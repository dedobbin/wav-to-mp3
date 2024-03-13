#include <iostream>
#include <thread>
#include <filesystem>
#include "common.h"

namespace fs = std::filesystem;

std::vector<std::string> getFilesInDir(std::string path)
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

int getNCores()
{
	return std::thread::hardware_concurrency();
}
