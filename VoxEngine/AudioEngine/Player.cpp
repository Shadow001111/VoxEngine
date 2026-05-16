#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"

#include "Core/TracyProfiler.h"

#include <iostream>

namespace AudioEngine
{
    static inline float lerpf(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    bool Player::init(uint32_t deviceSampleRate, uint32_t deviceChannels, uint32_t maxVoices)
    {
        if (mInitialized) return true;

        mOutputSampleRate = deviceSampleRate;
        mOutputChannels = deviceChannels;
        mMaxVoices = maxVoices;

        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = mOutputChannels;
        config.sampleRate = mOutputSampleRate;
        config.dataCallback = &Player::dataCallback;
        config.pUserData = this;

        config.noClip = MA_TRUE;                    // disable built-in clipping
        config.noPreSilencedOutputBuffer = MA_TRUE; // skip memset(0) on buffer

        //config.periodSizeInMilliseconds = 10; // reduce latency (default is 100ms)

        ma_result result = ma_device_init(nullptr, &config, &mDevice);
        if (result != MA_SUCCESS)
        {
            std::cerr << "ma_device_init failed: " << result << "\n";
            return false;
        }

        result = ma_device_start(&mDevice);
        if (result != MA_SUCCESS)
        {
            std::cerr << "ma_device_start failed: " << result << "\n";
            ma_device_uninit(&mDevice);
            return false;
        }

        mInitialized = true;

        mVoices.resize(mMaxVoices);

        return true;
    }

    void Player::shutdown()
    {
        if (!mInitialized) return;
        ma_device_uninit(&mDevice);
        mInitialized = false;
    }

    std::optional<VoiceId> Player::play(Sound& sound, float volume, float pitch, float pan, bool loop)
    {
        TRACY_SCOPE_N("Play")

        std::lock_guard<std::mutex> lock(mVoiceMutex);

        auto freeIt = std::find_if(mVoices.begin(), mVoices.end(), [](const Voice& v)
            {
                return !v.active;
            });
        if (freeIt == mVoices.end())
        {
            return std::nullopt;
        }
        *freeIt = makeVoice(sound, volume, pitch, pan, loop);
        return freeIt->handle;
    }

    void Player::stop(VoiceId voiceHandle)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            if (v.active && v.handle == voiceHandle)
            {
                v.active = false;
                break;
            }
        }
    }

    void Player::stopAll()
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            v.active = false;
        }
    }

    bool Player::isActive(VoiceId voiceHandle)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            if (v.active && v.handle == voiceHandle)
            {
                return true;
            }
        }
        return false;
    }

    void Player::setVoiceVolume(VoiceId voiceHandle, float volume)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            if (v.active && v.handle == voiceHandle)
            {
                v.volume = std::max(0.0f, volume);
                break;
            }
        }
    }

    void Player::setVoicePitch(VoiceId voiceHandle, float pitch)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            if (v.active && v.handle == voiceHandle)
            {
                v.pitch = std::max(0.01f, pitch);
                break;
            }
        }
    }

    void Player::setVoicePan(VoiceId voiceHandle, float pan)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);
        for (auto& v : mVoices)
        {
            if (v.active && v.handle == voiceHandle)
            {
                v.setPan(pan);
                return;
            }
        }
    }

    Voice Player::makeVoice(Sound& sound, float volume, float pitch, float pan, bool loop)
    {
        Voice v;
        v.handle = mNextVoiceId++;
        v.sound = OptionalSoundReference(sound);
        v.active = true;
        v.loop = loop;
        v.volume = std::max(0.0f, volume);
        v.pitch = std::max(0.01f, pitch);
        v.setPan(pan);
        v.cursor = 0.0;
        return v;
    }

    void Player::mix(float* out, ma_uint32 frameCount)
    {
        TRACY_SCOPE_N("Mix")

        std::fill(out, out + frameCount * mOutputChannels, 0.0f);

        {
            std::lock_guard<std::mutex> lock(mVoiceMutex);

            for (auto& voice : mVoices)
            {
                if (!voice.active || !voice.sound.has_value()) continue;

                const Sound& sound = voice.sound.value();
                if (sound.channels == 0 || sound.samples.empty()) [[unlikely]]
                {
                    voice.active = false;
                    continue;
                }

                if (sound.channels == 1)
                {
                    mixMonoVoice(voice, sound, out, frameCount);
                }
                else
                {
                    mixStereoVoice(voice, sound, out, frameCount);
                }
            }
        }

        // Clip output to [-1, 1].
        // No need for SIMD, compiler already optimizes it
        const size_t totalSamples = static_cast<size_t>(frameCount) * mOutputChannels;
        for (size_t i = 0; i < totalSamples; i++)
        {
            out[i] = std::clamp(out[i], -1.0f, 1.0f);
        }
    }

    void Player::mixMonoVoice(Voice& voice, const Sound& sound, float* out, ma_uint32 frameCount)
    {
        TRACY_SCOPE_N("Mix mono voice")

        const uint32_t srcFrames = sound.frameCount();
        if (srcFrames == 0)
        {
            voice.active = false;
            return;
        }

        const float leftGain = voice.leftGain * voice.volume;
        const float rightGain = voice.rightGain * voice.volume;

        const double step = (double)sound.sampleRate / (double)mOutputSampleRate * (double)voice.pitch;

        for (ma_uint32 i = 0; i < frameCount; i++)
        {
            if (voice.cursor >= srcFrames)
            {
                if (voice.loop)
                {
                    voice.cursor = std::fmod(voice.cursor, (double)srcFrames);
                }
                else
                {
                    voice.active = false;
                    return;
                }
            }

            const uint32_t i0 = static_cast<uint32_t>(voice.cursor);
            const uint32_t i1 = (i0 + 1 < srcFrames) ? (i0 + 1) : i0;
            const float t = static_cast<float>(voice.cursor - (double)i0);

            const float a = sound.samples[i0];
            const float b = sound.samples[i1];
            const float mono = lerpf(a, b, t);

            const size_t dst = static_cast<size_t>(i) * mOutputChannels;
            out[dst + 0] += mono * leftGain;
            if (mOutputChannels > 1)
            {
                out[dst + 1] += mono * rightGain;
            }

            voice.cursor += step;
        }
    }

    void Player::mixStereoVoice(Voice& voice, const Sound& sound, float* out, ma_uint32 frameCount)
    {
        TRACY_SCOPE_N("Mix stereo voice")

        const uint32_t srcFrames = sound.frameCount();
        if (srcFrames == 0)
        {
            voice.active = false;
            return;
        }

        const float leftGain = voice.leftGain * voice.volume;
        const float rightGain = voice.rightGain * voice.volume;

        const double step = (double)sound.sampleRate / (double)mOutputSampleRate * (double)voice.pitch;

        for (ma_uint32 i = 0; i < frameCount; i++)
        {
            if (voice.cursor >= srcFrames)
            {
                if (voice.loop)
                {
                    voice.cursor = std::fmod(voice.cursor, (double)srcFrames);
                }
                else
                {
                    voice.active = false;
                    return;
                }
            }

            const uint32_t i0 = static_cast<uint32_t>(voice.cursor);
            const uint32_t i1 = (i0 + 1 < srcFrames) ? (i0 + 1) : i0;
            const float t = static_cast<float>(voice.cursor - (double)i0);

            const size_t base0 = static_cast<size_t>(i0) * 2;
            const size_t base1 = static_cast<size_t>(i1) * 2;

            const float l0 = sound.samples[base0 + 0];
            const float r0 = sound.samples[base0 + 1];
            const float l1 = sound.samples[base1 + 0];
            const float r1 = sound.samples[base1 + 1];

            const float sampleL = lerpf(l0, l1, t);
            const float sampleR = lerpf(r0, r1, t);

            const size_t dst = static_cast<size_t>(i) * mOutputChannels;
            out[dst + 0] += sampleL * leftGain;
            if (mOutputChannels > 1)
            {
                out[dst + 1] += sampleR * rightGain;
            }

            voice.cursor += step;
        }
    }
}
