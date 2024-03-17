#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include "lame/lame.h"
#include "wave.h"
#include "common.h"
#include "lame.h"

const std::string inputSuffix = "wav";

WavInfo last_used_settings;
std::mutex settings_consistency_mtx;
// Number of threads currently working
int n_converting = 0;
std::mutex n_converting_mtx;

bool convertToMp3(const std::string& fileName, const std::string& folder)
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
		std::cerr << fileName << ": Unsupported bits per sample " << info.bitsPerSample << std::endl;
		return false;
	}

	if (info.numChannels != 2){
		std::cerr << fileName << ": Unsupported number of channels " << info.numChannels << std::endl;
		return false;
	}

	// TODO: get from header, header can be larger
    iStream->seekg(44, std::ios::beg);

	LameWrapper lame(lame_init());

	settings_consistency_mtx.lock();
	if (last_used_settings.sampleRate != info.sampleRate || last_used_settings.numChannels != info.numChannels){
		
		// If we want new settings, we wait for all threads using current settings to finish.
		// All new incoming threads are waiting for for us now through settings_consistency_mtx.
		// This is not ideal because we are blocking all threads but changing settings is not thread safe
		while (true){
			n_converting_mtx.lock();
			if (n_converting == 0){
				n_converting_mtx.unlock();
				break;
			}
			n_converting_mtx.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		lame_set_in_samplerate(lame.get(), info.sampleRate);
		//TODO: should this be same as input? or can i decide?
		lame_set_out_samplerate(lame.get(), 44100); 
		lame_set_VBR(lame.get(), vbr_default);
		lame_set_num_channels(lame.get(), info.numChannels);
		//lame_set_brate(lame, 192);
		int ret = lame_init_params(lame.get());
		if (ret < 0) {
			settings_consistency_mtx.unlock();
			return false;
		}
		last_used_settings = info;
	}
	n_converting_mtx.lock();
	n_converting ++;
	n_converting_mtx.unlock();
	settings_consistency_mtx.unlock();
			
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
            write = lame_encode_flush(lame.get(), mp3_buffer, MP3_SIZE);
        else
            write = lame_encode_buffer_interleaved(lame.get(), pcm_buffer, read, mp3_buffer, MP3_SIZE);

        oStream->write(reinterpret_cast<const char*>(mp3_buffer), write);
    } while (iStream->good());

	return true;
}

void process(std::string fileName, std::string outputPath, int tId)
{
	std::cout << "Started converting " << fileName << " (t" << tId << ")" <<std::endl;
	convertToMp3(fileName, outputPath);
	n_converting_mtx.lock();
	n_converting --;
	n_converting_mtx.unlock();
	std::cout << "Done converting " << fileName << " (t" << tId << ")" <<std::endl;
}

int main(int argc, char* argv[])
{
	if (argc < 2){
		std::cout << "Provide path to folder as param." <<std::endl;
		return 1;
	}
	std::string path = argv[1];

	auto fileNames = getFilesInDir(path);
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
}