#include <cstring>
#include <iostream>
#include "wave.h"

WavInfo getWavInfo(std::fstream &input)
{
	const int startPos = input.tellg();
	input.seekg(0, std::ios::beg);

	WavInfo result;

	const int n = 44;
	char buffer[n];
    input.read(buffer, n);
	if (input.gcount() != n){
		std::cerr << "Failed to read info" << std::endl;
		result.audioFormat = AudioFormat::UNKNOWN;
	} else {
		uint16_t audioFormat;
		memcpy(&audioFormat, buffer + 20, 2);
		if (audioFormat == 1){
			result.audioFormat = AudioFormat::PCM;
		} else {
			result.audioFormat = AudioFormat::OTHER;
		}

		memcpy(&result.numChannels, buffer + 22, 2);
		memcpy(&result.sampleRate, buffer + 24, 4);
		memcpy(&result.bitsPerSample, buffer + 34, 2);
		memcpy(&result.dataSize, buffer + 40, 4);
	}

	input.seekg(startPos, std::ios::beg);
	
	return result;
}

int getNumSamples(const WavInfo &info)
{
	return info.dataSize / (info.numChannels * (info.bitsPerSample/2));
}