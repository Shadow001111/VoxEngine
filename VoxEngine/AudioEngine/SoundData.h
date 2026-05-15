#pragma once
#include <cstdint>
#include <vector>
#include <optional>

namespace AudioEngine
{
    struct Sound
    {
        uint32_t sampleRate = 0;
        uint16_t channels = 0;          // 1 or 2
        std::vector<float> samples;     // Interleaved, normalized [-1, 1]

        uint32_t frameCount() const noexcept {
            return channels ? static_cast<uint32_t>(samples.size() / channels) : 0;
        }
    };

    class OptionalSoundReference
    {
        Sound* sound;
    public:
        OptionalSoundReference() : sound(nullptr) {}
		OptionalSoundReference(std::nullopt_t) : sound(nullptr) {}
        OptionalSoundReference(Sound& s) : sound(&s) {}
        OptionalSoundReference(Sound* s) : sound(s) {}

        bool has_value() const noexcept { return sound != nullptr; }
        Sound& value() const { return *sound; }
    };
}