#pragma once
#include <AL/al.h>
#include <AL/alc.h>
#include <vector>
#include <unordered_map>
#include <string>

class SoundManager
{
	bool initialized = false;

	ALCdevice* device = nullptr;
	ALCcontext* context = nullptr;

	std::unordered_map<std::string, ALuint> buffers;
	std::vector<ALuint> sources;
public:
	static SoundManager& getInstance();

	SoundManager();
	~SoundManager();

	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	bool loadWav(const std::string& name, const std::string& filename);
	bool loadOgg(const std::string& name, const std::string& filename);

	void play(const std::string& name, float pitch = 1.0f, float gain = 1.0f, bool loop = false);

	void update();

	bool isInitialized() const { return initialized; }
};

