#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <array>
#include <unordered_set>
#include <memory>
#include <vector>
#include <algorithm>
#include <cctype>
#include <limits>

constexpr size_t CHUNK_SIZE = 330;

std::array<char, 256> toLowerTable;

void InitializeToLowerTable() {
    for (int i = 0; i < 256; ++i) {
        toLowerTable[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(i)));
    }
}

char ToLower(char c) {
    return toLowerTable[static_cast<unsigned char>(c)];
}

void ProcessResults(std::string& str) {
    size_t length = str.size();
    size_t writeIndex = 0;
    for (size_t readIndex = 0; readIndex < length; ++readIndex) {
        char currentChar = str[readIndex];
        switch (currentChar) {
            case '%':
                if (readIndex + 2 < length) {
                    switch (str[readIndex + 2]) {
                        case '0':
                            currentChar = ' ';
                            readIndex += 2;
                            break;
                        case '8':
                            currentChar = '(';
                            readIndex += 2;
                            break;
                        case '9':
                            currentChar = ')';
                            readIndex += 2;
                            break;
                    }
                }
                break;
            case '/':
                currentChar = '\\';
                break;
        }
        if ((currentChar >= 32 && currentChar <= 126) && !std::isspace(currentChar)) {
            str[writeIndex] = currentChar;
            ++writeIndex;
        }
    }
    str.resize(writeIndex);
}

void ProcessMatch(std::string_view match, std::unordered_set<std::string>& printedMatches, char outputChoice, std::unique_ptr<std::ostream>& output) {
    if (match.find("HarddiskVolume") != std::string_view::npos) {
        size_t volumePos = match.find("\\\\Device");
        if (volumePos != std::string_view::npos) {
            char driveLetter = 'A' + match[volumePos + 24] - '1';
            std::string driveLetterStr(1, driveLetter);
            std::string matchStr = std::string(match);
            matchStr.replace(volumePos, 25, driveLetterStr + ":");
            match = matchStr;
        }
    }

    size_t doubleBackslashPos = match.find("\\\\");
    while (doubleBackslashPos != std::string_view::npos) {
        std::string matchStr = std::string(match);
        matchStr.replace(doubleBackslashPos, 2, "\\");
        match = matchStr;
        doubleBackslashPos = match.find("\\\\", doubleBackslashPos + 1);
    }

    std::string matchStr(match);
    ProcessResults(matchStr);

    if (matchStr.find("ProgramFiles(x86)") != std::string::npos) {
        size_t pos = matchStr.find("ProgramFiles(x86)");
        matchStr.replace(pos, 17, "Program Files (x86)");
    } else if (matchStr.find("ProgramFiles") != std::string::npos) {
        size_t pos = matchStr.find("ProgramFiles");
        matchStr.replace(pos, 12, "Program Files");
    }

    if (matchStr.length() <= 110 && printedMatches.find(matchStr) == printedMatches.end()) {
        if (outputChoice == 'C' || outputChoice == 'c') {
            std::cout << "Executed file: " << matchStr << std::endl;
        } else if (outputChoice == 'F' || outputChoice == 'f') {
            (*output) << "Executed file: " << matchStr << std::endl;
        }
        printedMatches.insert(matchStr);
    }
}

int main(int argc, char* argv[]) {
    InitializeToLowerTable();

    std::string filename;
    std::vector<char> buffer(CHUNK_SIZE);
    std::string overlapData;
    std::unordered_set<std::string> printedMatches;
    std::unique_ptr<std::ostream> output;

    if (argc == 2) {
        filename = argv[1];
    } else {
        std::cout << "Enter the file path of your memory image (Example: D:\\Downloads\\memdump.mem): ";
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
        std::cerr << "Failed to open the file, probably because it is already opened by another application or you provided a wrong path." << std::endl;
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
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string_view match = dataSubstring.substr(8, pos2 - pos1 - 8 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            pos1 = data.find("ImageName");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string_view match = dataSubstring.substr(12, pos2 - pos1 - 12 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            pos1 = data.find("AppPath");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string_view match = dataSubstring.substr(12, pos2 - pos1 - 12 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            overlapData = data.substr(data.size() - 220);
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
