#include <iostream>
#include <fstream>
#include <string>
#include <regex>

#define CHUNK_SIZE 4096

int main() {
    std::string filename;
    char buffer[CHUNK_SIZE];

    std::cout << "Enter the file path (e.g., D:\\Downloads\\memdump.mem): ";
    std::getline(std::cin, filename);

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "File open failed" << std::endl;
        return 1;
    }

    std::regex pattern("ms-shellactivity:.+\\.exe");

    while (!file.eof()) {
        file.read(buffer, CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            std::string data(buffer, static_cast<size_t>(bytesRead));
            std::sregex_iterator it(data.begin(), data.end(), pattern);
            std::sregex_iterator end;

            while (it != end) {
                std::smatch match = *it;
                std::cout << "File execution detected: " << match.str() << std::endl;
                ++it;
            }
        }
    }

    return 0;
}
