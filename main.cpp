#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <set>

#define CHUNK_SIZE 4096

void ProcessResults(std::string& str) {
    size_t found = str.find("%20");
    while (found != std::string::npos) {
        str.replace(found, 3, " ");
        found = str.find("%20", found + 1);
    }

    found = str.find("%28");
    while (found != std::string::npos) {
        str.replace(found, 3, "(");
        found = str.find("%28", found + 1);
    }

    found = str.find("%29");
    while (found != std::string::npos) {
        str.replace(found, 3, ")");
        found = str.find("%29", found + 1);
    }

    std::replace(str.begin(), str.end(), '/', '\\');

    str.erase(std::remove_if(str.begin(), str.end(), [](char c) { return !(c >= 32 && c <= 126) || std::isspace(c); }), str.end());
}

int main() {
    std::string filename;
    char* buffer = new char[CHUNK_SIZE];
    std::string overlapData;
    std::set<std::string> printedMatches;

    std::cout << "Enter the file path of your memory image (e.g., D:\\Downloads\\memdump.mem): ";
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

            data = overlapData + data;
            overlapData.clear();

            size_t pos1, pos2;

            pos1 = data.find("file:///");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                        [](char a, char b) {
                            return std::tolower(a) == std::tolower(b);
                        });

                    if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                        std::string match = data.substr(pos1 + 8, pos2 - pos1 - 8 + 4);
                        ProcessResults(match);
                        if (match.length() <= 110 && printedMatches.find(match) == printedMatches.end()) {
                        std::cout << "File execution detected: " << match << std::endl;
                        printedMatches.insert(match);
                    }
                    }
                
            }

            pos1 = data.find("ImageName");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return std::tolower(a) == std::tolower(b);
                    });

                if (it != dataSubstring.end()) {
                pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 12, pos2 - pos1 - 12 + 4);
                    ProcessResults(match);
                    if (match.length() <= 110 && printedMatches.find(match) == printedMatches.end()) {
                        std::cout << "File execution detected: " << match << std::endl;
                        printedMatches.insert(match);
                    }
                }
            }

            overlapData = data.substr(data.size() - 1645);
        }
    }

    delete[] buffer;
    return 0;
}
