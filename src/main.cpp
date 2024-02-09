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

WavInfo getWavInfo(std::ifstream &input)
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
	std::ifstream iStream(fileName, std::ios::binary);
	if (!iStream.is_open()){
		std::cerr << "Failed to open file " << fileName << std::endl;
		return false;
	}	

	std::string fileNameWithoutSuffix = fileName.substr(0, fileName.size()-inputSuffix.size());
	
	std::string outputFileName = fileNameWithoutSuffix + ".mp3";
	std::ofstream oStream(outputFileName, std::ios::binary);
	if (!oStream.is_open()){
		std::cerr << "Failed to open file " << outputFileName << std::endl;
		iStream.close(); //TODO: use smart ptr
		return false;
	}

	WavInfo info = getWavInfo(iStream);
	if (info.audioFormat == AudioFormat::UNKNOWN){
		std::cerr << "Could not determine audio format of " << fileName  << std::endl;
		iStream.close(); //TODO: use smart ptr
		oStream.close(); //TODO: use smart ptr
		return false;
	}

	if (info.audioFormat != AudioFormat::PCM){
		std::cerr << fileName << " uses unsupported encoding"  << std::endl;
		iStream.close(); //TODO: use smart ptr
		oStream.close(); //TODO: use smart ptr
		return false;
	}

	if (info.bitsPerSample != 16){
		std::cerr << fileName << " uses unsupported bits per same" << info.bitsPerSample << std::endl;
		iStream.close(); //TODO: use smart ptr
		oStream.close(); //TODO: use smart ptr
		return false;
	}

	// TODO: get from header, header can be larger
    iStream.seekg(44, std::ios::beg);

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
		iStream.close(); //TODO: use smart ptr
		oStream.close(); //TODO: use smart ptr
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
        iStream.read(reinterpret_cast<char*>(pcm_buffer), 2 * sizeof(short int) * PCM_SIZE);
		std::size_t read = iStream.gcount() / (2 * sizeof(short int));
        if (!iStream)
            write = lame_encode_flush(lame, mp3_buffer, MP3_SIZE);
        else
            write = lame_encode_buffer_interleaved(lame, pcm_buffer, read, mp3_buffer, MP3_SIZE);

        oStream.write(reinterpret_cast<const char*>(mp3_buffer), write);
    } while (iStream);

   	lame_close(lame);
    iStream.close();
    oStream.close();

	return true;
}



void process(std::vector<std::string>* fileNames, std::string outputPath, int tId)
{
	for(;;){		
		std::unique_lock<std::mutex> guard(m);
		if (fileNames->size() == 0){
			break;
		}
		auto target = fileNames->back();
		fileNames->pop_back();
		guard.unlock();
		std::cout << "Started converting " << target << " (t" << tId << ")" <<std::endl;
		convertToMp3(target, outputPath);
		std::cout << "Done converting " << target << " (t" << tId << ")" <<std::endl;
	}
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
	int nThreads = getNCores();
	std::vector<std::thread> threads;
	for(int i = 0; i < nThreads; i++){  
		threads.push_back(std::thread(process, &fileNames, path, i));
	}

	for(int i = 0; i < nThreads; i++){ 
		threads[i].join();
	}

	return 0;
}