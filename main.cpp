#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <memory>
#include <vector>
#include <limits>

constexpr size_t CHUNK_SIZE = 4096;

void ProcessResults(std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            if (str[i + 1] == '2') {
                if (str[i + 2] == '0') {
                    str[i] = ' ';
                    str.erase(i + 1, 2);
                } else if (str[i + 2] == '8') {
                    str[i] = '(';
                    str.erase(i + 1, 2);
                } else if (str[i + 2] == '9') {
                    str[i] = ')';
                    str.erase(i + 1, 2);
                }
            }
        } else if (str[i] == '/') {
            str[i] = '\\';
        } else if (!(str[i] >= 32 && str[i] <= 126) || std::isspace(str[i])) {
            str.erase(i, 1);
            --i;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string filename;
    std::vector<char> buffer(CHUNK_SIZE);
    std::string overlapData;
    std::unordered_set<std::string> printedMatches;
    std::unique_ptr<std::ostream> output;

    if (argc == 2) {
        filename = argv[1];
    } else {
        std::cout << "Enter the file path of your memory image (e.g., D:\\Downloads\\memdump.mem): ";
        std::getline(std::cin, filename);
    }

    char outputChoice;
    bool validChoice = false;

    while (!validChoice) {
        std::cout << "Do you want to print the matched strings to the console (C) or to a file (F)? ";
        std::cin >> outputChoice;

        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (outputChoice == 'C' || outputChoice == 'c' || outputChoice == 'F' || outputChoice == 'f') {
            validChoice = true;
        } else {
            std::cerr << "Invalid choice. Please enter 'C' for console or 'F' for file." << std::endl;
        }
    }

    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "File open failed" << std::endl;
        return 1;
    }

    if (outputChoice == 'F' || outputChoice == 'f') {
        std::string outputFilePath = "memdump_results.txt";
        output = std::make_unique<std::ofstream>(outputFilePath);

        if (!output->good()) {
            std::cerr << "Failed to open the output file. Try to select the 'C' option to print results to the console if this keeps happening." << std::endl;
            return 1;
        }
    }

    bool done = false;

    while (!done) {
        file.read(buffer.data(), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            std::string data(buffer.data(), static_cast<size_t>(bytesRead));
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

                    if (match.find("ProgramFiles(x86)") != std::string::npos) {
                        size_t pos = match.find("ProgramFiles(x86)");
                        match.replace(pos, 17, "Program Files (x86)");
                    } else if (match.find("ProgramFiles") != std::string::npos) {
                        size_t pos = match.find("ProgramFiles");
                        match.replace(pos, 12, "Program Files");
                    }

                    if (match.length() <= 110 && printedMatches.find(match) == printedMatches.end()) {
                        if (outputChoice == 'C' || outputChoice == 'c') {
                            std::cout << "File execution detected: " << match << std::endl;
                        } else if (outputChoice == 'F' || outputChoice == 'f') {
                            (*output) << "File execution detected: " << match << std::endl;
                        }
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

                    if (match.find("HarddiskVolume") != std::string::npos) {
                        size_t volumePos = match.find("\\\\Device");
                        if (volumePos != std::string::npos) {
                            char driveLetter = 'A' + match[volumePos + 24] - '1';
                            std::string driveLetterStr(1, driveLetter);
                            match.replace(volumePos, 25, driveLetterStr + ":");
                        }
                    }

                    size_t doubleBackslashPos = match.find("\\\\");
                    while (doubleBackslashPos != std::string::npos) {
                        match.replace(doubleBackslashPos, 2, "\\");
                        doubleBackslashPos = match.find("\\\\", doubleBackslashPos + 1);
                    }

                    if (match.length() <= 110 && printedMatches.find(match) == printedMatches.end()) {
                        if (outputChoice == 'C' or outputChoice == 'c') {
                            std::cout << "Executed file: " << match << std::endl;
                        } else if (outputChoice == 'F' or outputChoice == 'f') {
                            (*output) << "Executed file: " << match << std::endl;
                        }
                        printedMatches.insert(match);
                    }
                }
            }

            overlapData = data.substr(data.size() - 1645);
        } else {
            done = true;
        }
    }

    for (const std::string& printedMatch : printedMatches) {
        if (std::isalpha(printedMatch[0]) && printedMatch.size() >= 3 &&
            printedMatch[1] == ':' && printedMatch[2] == '\\') {
            if (!std::ifstream(printedMatch)) {
                if (outputChoice == 'C' || outputChoice == 'c') {
                    std::cout << "Deleted file (file could not be found): " << printedMatch << std::endl;
                } else if (outputChoice == 'F' || outputChoice == 'f') {
                    (*output) << "Deleted file (file could not be found): " << printedMatch << std::endl;
                }
            }
        }
    }

    file.close();

    std::cout << "Scan finished. Press Enter to exit the program...";
    std::cin.get();

    return 0;
}
