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

namespace AudioEngine
{
    using VoiceId = uint32_t;

    struct Voice
    {
        VoiceId handle = 0;
        OptionalSoundReference sound;
        bool active = false;
        bool loop = false;

        float volume = 1.0f;    // 0..1+
        float pitch = 1.0f;     // 1.0 = normal speed
        float pan = 0.0f;       // -1 = left, 0 = center, +1 = right
        double cursor = 0.0;    // Source frame position
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

        VoiceId mNextVoiceId = 0; // TODO: Use free lists instead of incrementing IDs forever
    public:
        Player() = default;
        ~Player() { shutdown(); }

        bool init(uint32_t deviceSampleRate = 48000, uint32_t deviceChannels = 2, uint32_t maxVoices = 64);

        void shutdown();

        VoiceId play(Sound& sound, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f, bool loop = false);

        void stop(VoiceId voiceHandle);

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
    };
}
