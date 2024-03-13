#pragma once

#include <vector>
#include <string>
#include <fstream>

std::vector<std::string> getFilesInDir(std::string path);
int getNCores();

struct fstream_closer {
    void operator()(std::fstream* stream) const {
        if (stream) {
            stream->close();
            delete stream;
        }
    }
};