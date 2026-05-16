#include "ResourceManager.h"
#include <iostream>

namespace AudioEngine
{
    std::optional<SoundId> ResourceManager::loadSound(FileExtension ext, const std::filesystem::path& path)
    {
        auto sound = std::make_unique<Sound>();
        if (!AudioLoader::loadAudioFile(ext, path, *sound))
        {
            std::cerr << "Failed to load audio file: " << path << "\n";
            return std::nullopt;
        }

        std::lock_guard<std::mutex> lock(mResourceMutex);
        SoundId id = mNextSoundId++;
        mSounds[id] = std::move(sound);
        return id;
    }

    std::optional<SoundId> ResourceManager::loadSound(const std::filesystem::path& path)
    {
        auto ext = path.extension().string();
        if (ext.empty()) [[unlikely]]
        {
            std::cerr << "File has no extension: " << path << "\n";
            return std::nullopt;
        }

        // Remove the dot from the extension
        ext.erase(0, 1);
        FileExtension fileExt = AudioLoader::getFileExtensionFromString(ext);
        if (fileExt == FileExtension::UNKNOWN) [[unlikely]]
        {
            std::cerr << "Unsupported audio file extension: " << ext << "\n";
            return std::nullopt;
        }
		return loadSound(fileExt, path);
    }

    bool ResourceManager::unloadSound(SoundId soundId)
    {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        return mSounds.erase(soundId) > 0;
    }

    OptionalSoundReference ResourceManager::getSound(SoundId soundId)
    {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        auto it = mSounds.find(soundId);
        if (it != mSounds.end())
        {
			Sound* soundPtr = it->second.get();
            return soundPtr;
        }
        return std::nullopt;
    }
}
