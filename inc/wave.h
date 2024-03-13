#pragma once

#include <stdint.h>
#include <fstream>

enum AudioFormat
{
	UNKNOWN,
	OTHER,
	PCM,
};

struct WavInfo
{
	uint32_t sampleRate;
	uint16_t bitsPerSample;
	uint16_t numChannels;
	AudioFormat audioFormat;
	uint32_t dataSize;

};

WavInfo getWavInfo(std::fstream &input);
int getNumSamples(const WavInfo &info);