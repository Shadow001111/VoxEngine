#pragma once
#include "AudioLoader.h"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace AudioEngine
{
	using SoundId = uint32_t;

	class ResourceManager
	{
		std::unordered_map<SoundId, std::unique_ptr<Sound>> mSounds;
		std::mutex mResourceMutex;

		SoundId mNextSoundId = 1; // TODO: Use free lists instead of incrementing IDs forever
	public:
		// Loads a sound file and returns its SoundId. Returns 0 on failure.
		SoundId loadSound(FileExtension ext, const std::filesystem::path& path);

		// Overload that infers file extension from the path. Returns 0 on failure.
		SoundId loadSound(const std::filesystem::path& path);

		// Unloads a sound by its SoundId. Returns true on success, false if the SoundId was not found.
		bool unloadSound(SoundId soundId);

		// Retrieves a sound by its SoundId. Returns an optional reference to the sound, or std::nullopt if not found.
		OptionalSoundReference getSound(SoundId soundId);
	};
}

