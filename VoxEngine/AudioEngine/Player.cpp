#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace AudioEngine
{
    static float lerpf(float a, float b, float t)
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
        return true;
    }

    void Player::shutdown()
    {
        if (!mInitialized) return;
        ma_device_uninit(&mDevice);
        mInitialized = false;
    }

    VoiceId Player::play(Sound& sound, float volume, float pitch, float pan, bool loop)
    {
        std::lock_guard<std::mutex> lock(mVoiceMutex);

        if (mVoices.size() >= mMaxVoices)
        {
            auto freeIt = std::find_if(mVoices.begin(), mVoices.end(), [](const Voice& v)
                {
                    return !v.active;
                });
            if (freeIt == mVoices.end())
            {
                return 0;
            }
            *freeIt = makeVoice(sound, volume, pitch, pan, loop);
            return freeIt->handle;
        }

        Voice v = makeVoice(sound, volume, pitch, pan, loop);
        VoiceId handle = v.handle;
        mVoices.push_back(std::move(v));
        return handle;
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
                v.pan = std::clamp(pan, -1.0f, 1.0f);
                break;
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
        v.pan = std::clamp(pan, -1.0f, 1.0f);
        v.cursor = 0.0;
        return v;
    }

    void Player::mix(float* out, ma_uint32 frameCount)
    {
        std::fill(out, out + frameCount * mOutputChannels, 0.0f);

        std::lock_guard<std::mutex> lock(mVoiceMutex);

        constexpr float PI = 3.14159265358979323846f;

        for (auto& v : mVoices)
        {
            if (!v.active || !v.sound.has_value()) continue;

            const Sound& s = v.sound.value();
            if (s.channels == 0 || s.samples.empty())
            {
                v.active = false;
                continue;
            }

            const uint32_t srcFrames = s.frameCount();
            if (srcFrames == 0) {
                v.active = false;
                continue;
            }

            const float pan = std::clamp(v.pan, -1.0f, 1.0f);
            const float panNorm = (pan + 1.0f) * 0.5f;
            const float leftGain = std::cos(panNorm * (PI * 0.5f)) * v.volume;
            const float rightGain = std::sin(panNorm * (PI * 0.5f)) * v.volume;

            const double step = (double)s.sampleRate / (double)mOutputSampleRate * (double)v.pitch;

            for (ma_uint32 i = 0; i < frameCount; i++)
            {
                if (v.cursor >= srcFrames)
                {
                    if (v.loop)
                    {
                        v.cursor = std::fmod(v.cursor, (double)srcFrames);
                    }
                    else
                    {
                        v.active = false;
                        break;
                    }
                }

                const uint32_t i0 = static_cast<uint32_t>(v.cursor);
                const uint32_t i1 = (i0 + 1 < srcFrames) ? (i0 + 1) : i0;
                const float t = static_cast<float>(v.cursor - (double)i0);

                float sampleL = 0.0f;
                float sampleR = 0.0f;

                if (s.channels == 1)
                {
                    const float a = s.samples[i0];
                    const float b = s.samples[i1];
                    const float mono = lerpf(a, b, t);
                    sampleL = mono;
                    sampleR = mono;
                }
                else
                {
                    const size_t base0 = static_cast<size_t>(i0) * 2;
                    const size_t base1 = static_cast<size_t>(i1) * 2;

                    const float l0 = s.samples[base0 + 0];
                    const float r0 = s.samples[base0 + 1];
                    const float l1 = s.samples[base1 + 0];
                    const float r1 = s.samples[base1 + 1];

                    sampleL = lerpf(l0, l1, t);
                    sampleR = lerpf(r0, r1, t);
                }

                const size_t dst = static_cast<size_t>(i) * mOutputChannels;
                out[dst + 0] += sampleL * leftGain;
                if (mOutputChannels > 1)
                {
                    out[dst + 1] += sampleR * rightGain;
                }

                v.cursor += step;
            }
        }

        // Remove dead voices.
        mVoices.erase(
            std::remove_if(mVoices.begin(), mVoices.end(),
                [](const Voice& v) { return !v.active; }),
            mVoices.end()
        );

        // Clip output to [-1, 1].
        const size_t totalSamples = static_cast<size_t>(frameCount) * mOutputChannels;
        for (size_t i = 0; i < totalSamples; i++)
        {
            out[i] = std::clamp(out[i], -1.0f, 1.0f);
        }
    }
}