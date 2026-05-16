#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX
#define NOSOCKET
#define NOCRYPT

#define MA_NO_DECODING
#define MA_NO_ENCODING

#define MA_NO_ENGINE            // high-level ma_engine API
#define MA_NO_RESOURCE_MANAGER  // file/asset management system
#define MA_NO_NODE_GRAPH        // effect/mixing node graph
#define MA_NO_GENERATION        // waveform + noise generators

#include "miniaudio/miniaudio.h"

#include "SoundData.h"

#include <mutex>
#include <cmath>
#include <algorithm>

namespace AudioEngine
{
    using VoiceId = uint32_t;

    struct Voice
    {
        OptionalSoundReference sound;
        VoiceId handle = 0;
        bool active = false;
        bool loop = false;

        float volume = 1.0f;
        float pitch = 1.0f;
        float leftGain = 1.0f; // Not multiplied by volume
        float rightGain = 1.0f; // Not multiplied by volume
        double cursor = 0.0;

        void setPan(float pan) noexcept
        {
            constexpr float PI = 3.14159265358979323846f;
            constexpr float HALF_PI = PI * 0.5;

            pan = std::clamp(pan, -1.0f, 1.0f);
            const float panNorm = (pan + 1.0f) * 0.5f;
            const float panNormPi = panNorm * HALF_PI;
            leftGain = std::cos(panNormPi);
            rightGain = std::sin(panNormPi);
        }
    };

    class Player
    {
        ma_device mDevice{};
        bool mInitialized = false;
        uint32_t mOutputSampleRate = 48000;
        uint32_t mOutputChannels = 2;
        uint32_t mMaxVoices = 64;

        std::mutex mVoiceMutex;
        std::vector<Voice> mVoices;

        VoiceId mNextVoiceId = 0;
    public:
        Player() = default;
        ~Player() { shutdown(); }

        bool init(uint32_t deviceSampleRate = 48000, uint32_t deviceChannels = 2, uint32_t maxVoices = 64);
        void shutdown();

        std::optional<VoiceId> play(Sound& sound, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f, bool loop = false);

        void stop(VoiceId voiceHandle);
        void stopAll();
        //void pause(VoiceId voiceHandle);

        bool isActive(VoiceId voiceHandle);

        void setVoiceVolume(VoiceId voiceHandle, float volume);
        void setVoicePitch(VoiceId voiceHandle, float pitch);
        void setVoicePan(VoiceId voiceHandle, float pan);
    private:
        static void dataCallback(ma_device* device, void* output, const void* /*input*/, ma_uint32 frameCount)
        {
            auto* self = static_cast<Player*>(device->pUserData);
            self->mix(static_cast<float*>(output), frameCount);
        }

        Voice makeVoice(Sound& sound, float volume, float pitch, float pan, bool loop);

        void mix(float* out, ma_uint32 frameCount);
        void mixMonoVoice(Voice& voice, const Sound& sound, float* out, ma_uint32 frameCount);
        void mixStereoVoice(Voice& voice, const Sound& sound, float* out, ma_uint32 frameCount);
    };
}
