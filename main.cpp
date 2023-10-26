#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#define CHUNK_SIZE 4096

bool isASCII(char c) {
    return (c >= 32 && c <= 126);
}

int main() {
    std::string filename;
    char* buffer = new char[CHUNK_SIZE];

    std::cout << "Enter the file path (e.g., D:\\Downloads\\memdump.mem): ";
    std::getline(std::cin, filename);

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "File open failed" << std::endl;
        delete[] buffer;
        return 1;
    }

    while (!file.eof()) {
        file.read(buffer, CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            std::string data(buffer, static_cast<size_t>(bytesRead));

            size_t pos1, pos2;

            pos1 = data.find("ms-shellactivity");
            if (pos1 != std::string::npos) {
                pos2 = data.find(".exe", pos1);
                if (pos2 != std::string::npos) {
                    std::string match = data.substr(pos1, pos2 - pos1 + 4);
                    match.erase(std::remove_if(match.begin(), match.end(), [](char c) { return !isASCII(c) || std::isspace(c); }), match.end());
                    std::cout << "File execution detected: " << match << std::endl;
                }
            }

            pos1 = data.find("IsInVirtualizationContainer");
            if (pos1 != std::string::npos) {
                pos2 = data.find(".exe", pos1);
                if (pos2 != std::string::npos) {
                    std::string match = data.substr(pos1, pos2 - pos1 + 4);
                    match.erase(std::remove_if(match.begin(), match.end(), [](char c) { return !isASCII(c) || std::isspace(c); }), match.end());
                    std::cout << "File execution detected: " << match << std::endl;
                }
            }

            pos1 = data.find("file:///");
            if (pos1 != std::string::npos) {
                pos2 = data.find(".exe", pos1);
                if (pos2 != std::string::npos) {
                    std::string match = data.substr(pos1, pos2 - pos1 + 4);
                    match.erase(std::remove_if(match.begin(), match.end(), [](char c) { return !isASCII(c) || std::isspace(c); }), match.end());
                    std::cout << "File execution detected: " << match << std::endl;
                }
            }

            pos1 = data.find("[{\"application\"");
            if (pos1 != std::string::npos) {
                pos2 = data.find(".exe", pos1);
                if (pos2 != std::string::npos) {
                    std::string match = data.substr(pos1, pos2 - pos1 + 4);
                    match.erase(std::remove_if(match.begin(), match.end(), [](char c) { return !isASCII(c) || std::isspace(c); }), match.end());
                    std::cout << "File execution detected: " << match << std::endl;
                }
            }
        }
    }

    delete[] buffer;
    return 0;
}
