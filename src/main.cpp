#include <string>
#include <iostream>
#include <dirent.h>
#include <vector>
#include <stdio.h>
#include "unistd.h"
#include <thread>
#include <mutex>
#include <assert.h>
#include <exception>
#include <string.h>
#include <filesystem>
#include "lame/lame.h"

namespace fs = std::filesystem;

std::string inputSuffix = ".wav";

std::mutex m;

bool keepGoing = true;

int getWavSampleRate(FILE* pcm)
{
	int n = 28;
	uint8_t buffer[n];
	if (!fread (buffer,n,1,pcm)){
		return 0;
	}

	uint32_t rate = 0;
	memcpy(&rate, buffer + 24, 4);
	return rate;
}

int getNCores()
{
	return std::thread::hardware_concurrency();
}


bool convertToMp3(std::string fileName, std::string folder)
{	
	int read, write;
	FILE *pcm = fopen((fileName).c_str(), "rb"); 
	if (!pcm){
		std::cerr << "Error opening " << fileName << std::endl;
		exit(1);
	}
	fseek(pcm, 4*1024, SEEK_CUR);
	rewind(pcm);

	auto fileNameWithoutSuffix = fileName.substr(0, fileName.size()-inputSuffix.size());

	auto mp3 = fopen((fileNameWithoutSuffix + ".mp3").c_str(), "wb"); 
	if (!mp3){
		std::cerr << "Error opening " << fileNameWithoutSuffix << ".mp3 for writing" << std::endl;
		exit(3);
	}
	
	const int PCM_SIZE = 8192*3;
	const int MP3_SIZE = 8192*3;
	short int pcm_buffer[PCM_SIZE*2];
	unsigned char mp3_buffer[MP3_SIZE];

	lame_t lame = lame_init();
	lame_set_in_samplerate(lame, getWavSampleRate(pcm));
	lame_set_VBR(lame, vbr_default);
	lame_init_params(lame);

	int nTotalRead=0;

	do {
		read = fread(pcm_buffer, 2*sizeof(short int), PCM_SIZE, pcm);
		nTotalRead+=read*4;
		if (read == 0){
			write = lame_encode_flush(lame, mp3_buffer, MP3_SIZE);
		}else{
			write = lame_encode_buffer_interleaved(lame,pcm_buffer, read, mp3_buffer, MP3_SIZE);
		}

		fwrite(mp3_buffer, write, 1, mp3);
	} while (read != 0);

	lame_close(lame);
	fclose(mp3);
	fclose(pcm);
	return 0;
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