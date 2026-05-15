#include "AudioLoader.h"
#include <fstream>

#include "decoder_libs/stb_vorbis.h"

#define MINIMP3_ONLY_MP3
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#include "decoder_libs/minimp3.h"

namespace AudioEngine
{
    bool AudioLoader::loadWavFile(const std::filesystem::path& path, Sound& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        const std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size < 44) return false;

        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size))
        {
            return false;
        }

        if (std::memcmp(data.data(), "RIFF", 4) != 0) return false;
        if (std::memcmp(data.data() + 8, "WAVE", 4) != 0) return false;

        bool foundFmt = false;
        bool foundData = false;

        uint16_t audioFormat = 0;
        uint16_t channels = 0;
        uint32_t sampleRate = 0;
        uint16_t bitsPerSample = 0;
        const uint8_t* pcmData = nullptr;
        uint32_t pcmBytes = 0;

        size_t offset = 12;
        while (offset + 8 <= data.size())
        {
            const char* chunkId = reinterpret_cast<const char*>(data.data() + offset);
            uint32_t chunkSize = readU32(data.data() + offset + 4);
            offset += 8;

            if (offset + chunkSize > data.size()) break;

            if (std::memcmp(chunkId, "fmt ", 4) == 0)
            {
                if (chunkSize < 16) return false;
                audioFormat = readU16(data.data() + offset + 0);
                channels = readU16(data.data() + offset + 2);
                sampleRate = readU32(data.data() + offset + 4);
                bitsPerSample = readU16(data.data() + offset + 14);
                foundFmt = true;
            }
            else if (std::memcmp(chunkId, "data", 4) == 0)
            {
                pcmData = data.data() + offset;
                pcmBytes = chunkSize;
                foundData = true;
            }

            offset += chunkSize;
            if (chunkSize & 1) offset++; // word alignment
        }

        if (!foundFmt || !foundData) return false;
        if (!(channels == 1 || channels == 2)) return false;
        if (!(audioFormat == 1 || audioFormat == 3)) return false;
        if (!(bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32)) return false;

        out.sampleRate = sampleRate;
        out.channels = channels;
        out.samples.clear();

        const size_t bytesPerFrame = (bitsPerSample / 8) * channels;
        if (bytesPerFrame == 0) return false;

        const size_t frameCount = pcmBytes / bytesPerFrame;
        out.samples.reserve(frameCount * channels);

        const uint8_t* p = pcmData;

        for (size_t f = 0; f < frameCount; f++)
        {
            for (uint16_t ch = 0; ch < channels; ch++)
            {
                float sample = 0.0f;

                if (audioFormat == 1)
                {
                    if (bitsPerSample == 8)
                    {
                        // Unsigned 8-bit PCM
                        uint8_t u = *p++;
                        sample = (float(int(u)) - 128.0f) / 128.0f;
                    }
                    else if (bitsPerSample == 16)
                    {
                        int16_t s = (int16_t)(p[0] | (p[1] << 8));
                        p += 2;
                        sample = (float)s / 32768.0f;
                    }
                    else if (bitsPerSample == 24)
                    {
                        int32_t v = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16));
                        if (v & 0x800000) v |= ~0xFFFFFF;
                        p += 3;
                        sample = (float)v / 8388608.0f;
                    }
                    else
                    {
                        // 32-bit PCM
                        int32_t s = (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
                        p += 4;
                        sample = (float)s / 2147483648.0f;
                    }
                }
                else if (audioFormat == 3)
                {
                    if (bitsPerSample != 32) return false;
                    float s;
                    std::memcpy(&s, p, sizeof(float));
                    p += 4;
                    sample = s;
                }

                out.samples.push_back(std::clamp(sample, -1.0f, 1.0f));
            }
        }

        return !out.samples.empty();
    }

    bool AudioLoader::loadOggFile(const std::filesystem::path& path, Sound& out)
    {
        int channels = 0;
        int sampleRate = 0;
        short* pcm = nullptr;
        int samplesPerChannel = stb_vorbis_decode_filename(path.string().c_str(), &channels, &sampleRate, &pcm);
        if (samplesPerChannel <= 0)
        {
            free(pcm);
            return false;
        }
        size_t totalSamples = size_t(samplesPerChannel) * size_t(channels);
        out.sampleRate = uint32_t(sampleRate);
        out.channels = uint16_t(channels);
        out.samples.reserve(totalSamples);
        for (size_t i = 0; i < totalSamples; i++)
        {
            float sample = float(pcm[i]) / 32768.0f;
            out.samples.push_back(std::clamp(sample, -1.0f, 1.0f));
        }
        free(pcm);
        return true;
    }

    bool AudioLoader::loadMp3File(const std::filesystem::path& path, Sound& out)
    {
        // Read the entire file into memory
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }
        std::vector<uint8_t> fileData(std::istreambuf_iterator<char>(file), {});

        mp3dec_t dec;
        mp3dec_init(&dec);

        mp3d_sample_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME]; // float[1152*2]
        mp3dec_frame_info_t info{};

        const uint8_t* ptr = fileData.data();
        int remaining = static_cast<int>(fileData.size());
        bool firstFrame = true;

        while (remaining > 0)
        {
            int samples = mp3dec_decode_frame(&dec, ptr, remaining, frame, &info);

            if (info.frame_bytes == 0)
                break; // no more sync

            ptr += info.frame_bytes;
            remaining -= info.frame_bytes;

            if (samples == 0)
                continue; // ID3 / reservoir frame, keep going

            if (firstFrame)
            {
                out.sampleRate = info.hz;
                out.channels = info.channels;
                firstFrame = false;
            }

            out.samples.insert(out.samples.end(), frame, frame + samples * info.channels);
        }

        return !out.samples.empty();
    }

    bool AudioLoader::loadAudioFile(FileExtension ext, const std::filesystem::path& path, Sound& out)
    {
        switch (ext)
        {
        case FileExtension::WAV:
            return loadWavFile(path, out);
        case FileExtension::OGG:
            return loadOggFile(path, out);
        case FileExtension::MP3:
            return loadMp3File(path, out);
        default:
            return false;
        }
    }

    FileExtension AudioLoader::getFileExtensionFromString(const std::string& ext)
    {
        if (ext == "wav" || ext == "WAV") return FileExtension::WAV;
        if (ext == "ogg" || ext == "OGG") return FileExtension::OGG;
        if (ext == "mp3" || ext == "MP3") return FileExtension::MP3;
		return FileExtension::UNKNOWN;
    }
}
