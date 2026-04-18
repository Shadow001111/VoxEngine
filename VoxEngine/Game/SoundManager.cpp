#include "SoundManager.h"
#include <iostream>
#include <fstream>
#include "stb_vorbis.c"

#include "Game/TracyProfiler.h"

#include "Game/ProfileCategories.h"

static bool loadWavFile(const std::string& filename, ALuint* bufferOut)
{
	std::ifstream f(filename, std::ios::binary);
	if (!f) return false;

	// Read headers
	char riff[4]; f.read(riff, 4);
	f.ignore(4); // chunk size
	char wave[4]; f.read(wave, 4);

	char fmt[4]; f.read(fmt, 4);

	uint32_t fmtSize;
	f.read(reinterpret_cast<char*>(&fmtSize), 4);

	uint16_t audioFormat;
	uint16_t channels;
	uint32_t sampleRate;
	uint32_t byteRate;
	uint16_t blockAlign;
	uint16_t bits;

	f.read(reinterpret_cast<char*>(&audioFormat), 2);
	f.read(reinterpret_cast<char*>(&channels), 2);
	f.read(reinterpret_cast<char*>(&sampleRate), 4);
	f.read(reinterpret_cast<char*>(&byteRate), 4);
	f.read(reinterpret_cast<char*>(&blockAlign), 2);
	f.read(reinterpret_cast<char*>(&bits), 2);

	if (fmtSize > 16)
		f.ignore(fmtSize - 16);

	// Data chunk
	char dataHeader[4];
	f.read(dataHeader, 4);

	uint32_t dataSize;
	f.read(reinterpret_cast<char*>(&dataSize), 4);

	std::vector<char> data(dataSize);
	f.read(data.data(), dataSize);

	ALenum format = 0;
	if (bits == 8)
		format = (channels == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8);
	else if (bits == 16)
		format = (channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16);
	else
		return false;

	alGenBuffers(1, bufferOut);
	alBufferData(*bufferOut, format, data.data(), dataSize, sampleRate);

	return true;
}

static bool loadOggFile(const std::string& filename, ALuint* bufferOut)
{
	int channels = 0;
	int sampleRate = 0;
	short* pcm = nullptr;

	int samplesPerChannel = stb_vorbis_decode_filename(filename.c_str(), &channels, &sampleRate, &pcm);

	if (samplesPerChannel <= 0)
	{
		free(pcm);
		return false;
	}

	size_t totalSamples = size_t(samplesPerChannel) * size_t(channels);
	size_t dataSize = totalSamples * sizeof(short);

	ALenum format = (channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16);

	alGenBuffers(1, bufferOut);
	alBufferData(*bufferOut, format, pcm, dataSize, sampleRate);

	free(pcm); // stb_vorbis allocs with malloc

	return true;
}

SoundManager& SoundManager::getInstance()
{
	static SoundManager instance;
	return instance;
}

SoundManager::SoundManager()
{
	device = alcOpenDevice(nullptr);
	if (!device)
	{
		std::cerr << "[Sound] Failed to open device.\n";
		return;
	}

	context = alcCreateContext(device, nullptr);
	if (!context || !alcMakeContextCurrent(context))
	{
		std::cerr << "[Sound] Failed to create context.\n";
		alcCloseDevice(device);
		device = nullptr;
	}

	initialized = true;
}

SoundManager::~SoundManager()
{
	for (ALuint src : sources)
		alDeleteSources(1, &src);
	sources.clear();

	for (auto& p : buffers)
		alDeleteBuffers(1, &p.second);
	buffers.clear();

	if (context)
	{
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(context);
		context = nullptr;
	}

	if (device)
	{
		alcCloseDevice(device);
		device = nullptr;
	}
}

bool SoundManager::loadWav(const std::string& name, const std::string& filename)
{
	if (buffers.contains(name))
		return true; // already loaded

	ALuint buffer;
	if (!loadWavFile(filename, &buffer))
	{
		std::cerr << "[Sound] Failed to load WAV: " << filename << "\n";
		return false;
	}

	buffers[name] = buffer;
	return true;
}

bool SoundManager::loadOgg(const std::string& name, const std::string& filename)
{
	if (buffers.contains(name))
		return true; // already loaded

	ALuint buffer;
	if (!loadOggFile(filename, &buffer))
	{
		std::cerr << "[Sound] Failed to load OGG: " << filename << "\n";
		return false;
	}

	buffers[name] = buffer;
	return true;
}

void SoundManager::play(const std::string& name, float pitch, float gain, bool loop)
{
	auto it = buffers.find(name);
	if (it == buffers.end()) {
		std::cerr << "[Sound] Unknown sound: " << name << "\n";
		return;
	}

	ALuint source;
	alGenSources(1, &source);

	alSourcei(source, AL_BUFFER, it->second);
	alSourcef(source, AL_PITCH, pitch);
	alSourcef(source, AL_GAIN, gain);
	alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);

	alSourcePlay(source);

	sources.push_back(source);
	return;
}

void SoundManager::update()
{
	TRACY_SCOPE("SoundManager update", ProfileCategory::General);

	for (size_t i = 0; i < sources.size();)
	{
		ALint state;
		alGetSourcei(sources[i], AL_SOURCE_STATE, &state);

		if (state != AL_PLAYING)
		{
			alDeleteSources(1, &sources[i]);
			sources.erase(sources.begin() + i);
		}
		else
		{
			i++;
		}
	}
}
