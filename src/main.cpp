#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <exception>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "lame/lame.h"

namespace fs = std::filesystem;

std::string inputSuffix = ".wav";

std::mutex m;

bool keepGoing = true;

struct fstream_closer {
    void operator()(std::fstream* stream) const {
        if (stream) {
            stream->close();
            delete stream;
        }
    }
};

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

int getNCores()
{
	return std::thread::hardware_concurrency();
}


// TODO: use const &
bool convertToMp3(std::string fileName, std::string folder)
{
	using unique_fstream = std::unique_ptr<std::fstream, fstream_closer>;

	unique_fstream iStream(new std::fstream(fileName, std::ios::in | std::ios::binary));
	if (!iStream->is_open()){
		std::cerr << "Failed to open file " << fileName << std::endl;
		return false;
	}	

	std::string fileNameWithoutSuffix = fileName.substr(0, fileName.size()-inputSuffix.size());
	
	std::string outputFileName = fileNameWithoutSuffix + ".mp3";
	unique_fstream oStream(new std::fstream(outputFileName, std::ios::out | std::ios::binary));
	if (!oStream->is_open()){
		std::cerr << "Failed to open file " << outputFileName << std::endl;
		return false;
	}

	WavInfo info = getWavInfo(*iStream);
	if (info.audioFormat == AudioFormat::UNKNOWN){
		std::cerr << fileName << ": Could not determine audio format, aborting" << std::endl;
		return false;
	}

	if (info.audioFormat != AudioFormat::PCM){
		std::cerr << fileName << ": Unsupported encoding"  << std::endl;
		return false;
	}

	if (info.bitsPerSample != 16){
		std::cerr << fileName << ": Unsupported bits per sample" << info.bitsPerSample << std::endl;
		return false;
	}

	// TODO: get from header, header can be larger
    iStream->seekg(44, std::ios::beg);

	//TODO: move so is not initialized each call
	lame_t lame = lame_init();
	lame_set_in_samplerate(lame, info.sampleRate);
	//TODO: should this be same as input? or can i decide?
	lame_set_out_samplerate(lame, 44100); 
	lame_set_VBR(lame, vbr_default);
	lame_set_num_channels(lame, info.numChannels);
	//lame_set_brate(lame, 192);

	int ret = lame_init_params(lame);
	if (ret < 0) {
		lame_close(lame);
		return false;
	}

	const int bufferSize = getNumSamples(info);

	// TODO: use buffer size
    int read, write;
    const int PCM_SIZE = 8192;
    const int MP3_SIZE = 8192;

    short int pcm_buffer[PCM_SIZE*2];
    unsigned char mp3_buffer[MP3_SIZE];

    do {
        iStream->read(reinterpret_cast<char*>(pcm_buffer), 2 * sizeof(short int) * PCM_SIZE);
            read = iStream->gcount() / (2 * sizeof(short int));
		if (!iStream.get())
            write = lame_encode_flush(lame, mp3_buffer, MP3_SIZE);
        else
            write = lame_encode_buffer_interleaved(lame, pcm_buffer, read, mp3_buffer, MP3_SIZE);

        oStream->write(reinterpret_cast<const char*>(mp3_buffer), write);
    } while (iStream.get());

   	lame_close(lame);

	return true;
}



void process(std::string fileName, std::string outputPath, int tId)
{
	std::cout << "Started converting " << fileName << " (t" << tId << ")" <<std::endl;
	convertToMp3(fileName, outputPath);
	std::cout << "Done converting " << fileName << " (t" << tId << ")" <<std::endl;
}

std::vector<std::string> getInputFiles(std::string path)
{
	std::vector<std::string> result;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (fs::is_regular_file(entry)) {
                std::cout << entry.path().filename() << std::endl;
				result.push_back(path + "/" +entry.path().filename().string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
	return result;
}

int main(int argc, char* argv[])
{
	if (argc < 2){
		std::cout << "Provide path to folder as param." <<std::endl;
		return 1;
	}
	std::string path = argv[1];

	auto fileNames = getInputFiles(path);
	std::vector<std::thread> threads;

    // Create threads and add them to the vector
    for (int i = 0; i < fileNames.size(); i++) {
		std::string fileName = fileNames.at(i);
		threads.push_back(std::thread(process, fileName, path, i++));

    }

    // Join all the threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;

	return 0;
}