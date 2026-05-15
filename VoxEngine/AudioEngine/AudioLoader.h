#pragma once
#include "SoundData.h"
#include <filesystem>

namespace AudioEngine
{
    enum class FileExtension
    {
        UNKNOWN,
        WAV,
        OGG,
        MP3
    };

    class AudioLoader
    {
        static uint16_t readU16(const uint8_t* p) noexcept {
            return uint16_t(p[0] | (p[1] << 8));
        }

        static uint32_t readU32(const uint8_t* p) noexcept {
            return uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
        }
    public:
        static bool loadWavFile(const std::filesystem::path& path, Sound& out);
        static bool loadOggFile(const std::filesystem::path& path, Sound& out);
        static bool loadMp3File(const std::filesystem::path& path, Sound& out);

        static bool loadAudioFile(FileExtension ext, const std::filesystem::path& path, Sound& out);

        static FileExtension getFileExtensionFromString(const std::string& ext);
    };
}

