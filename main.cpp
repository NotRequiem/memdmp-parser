#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <unordered_set>
#include <memory>
#include <vector>
#include <algorithm>
#include <limits>

// Define a constant CHUNK_SIZE for reading data in chunks.
constexpr size_t MIN_CHUNK_SIZE = 330;

// An array to store lowercase conversions of characters.
std::array<char, 256> toLowerTable;

// Function to initialize the toLowerTable array.
void InitializeToLowerTable() {
    for (int i = 0; i < 256; ++i) {
        toLowerTable[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(i)));
    }
}

// Function to convert a character to lowercase using the toLowerTable.
char ToLower(char c) {
    return toLowerTable[static_cast<unsigned char>(c)];
}

// Function to process and clean up strings to be printed.
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

// Function to process and clean up a matching string before printing it.
void ProcessMatch(std::string& match, std::unordered_set<std::string>& printedMatches, char outputChoice, std::unique_ptr<std::ostream>& output) {
    // Check if the match contains "HarddiskVolume" and replace it with the drive letter.
    if (match.find("HarddiskVolume") != std::string::npos) {
        size_t volumePos = match.find("\\\\Device");
        if (volumePos != std::string::npos) {
            char driveLetter = 'A' + match[volumePos + 24] - '1';
            std::string driveLetterStr(1, driveLetter);
            match.replace(volumePos, 25, driveLetterStr + ":");
        }
    }

    // Replace double backslashes with a single backslash.
    size_t doubleBackslashPos = match.find("\\\\");
    while (doubleBackslashPos != std::string::npos) {
        match.replace(doubleBackslashPos, 2, "\\");
        doubleBackslashPos = match.find("\\\\", doubleBackslashPos + 1);
    }

    // Clean up the string further.
    ProcessResults(match);

    // Check if the match contains "ProgramFiles" and replace it with a more human-readable format.
    if (match.find("ProgramFiles(x86)") != std::string::npos) {
        size_t pos = match.find("ProgramFiles(x86)");
        match.replace(pos, 17, "Program Files (x86)");
    } else if (match.find("ProgramFiles") != std::string::npos) {
        size_t pos = match.find("ProgramFiles");
        match.replace(pos, 12, "Program Files");
    }

    // Check the length of the match and print it if it's short and not previously printed.
    if (match.length() <= 110 && printedMatches.find(match) == printedMatches.end()) {
        if (outputChoice == 'C' || outputChoice == 'c') {
            std::cout << "Executed file: " << match << std::endl;
        } else if (outputChoice == 'F' || outputChoice == 'f') {
            (*output) << "Executed file: " << match << std::endl;
        }
        printedMatches.insert(match);
    }
}

int main(int argc, char* argv[]) {
    InitializeToLowerTable();

    std::string filename;
    std::vector<char> buffer;
    std::string overlapData;
    std::unordered_set<std::string> printedMatches;
    std::unique_ptr<std::ostream> output;

    // Check if a filename is provided as a command-line argument, otherwise prompt the user to enter a file path.
    if (argc > 1) {
        filename = argv[1];
    } else {
        std::cout << "Enter the file path of your memory image (Example: D:\\Downloads\\memdump.mem): ";
        std::getline(std::cin, filename);
    }

    char outputChoice;
    bool validChoice = false;

    // Prompt the user for the output choice (console or file) and validate the input.
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

    std::ifstream file(filename, std::ios::binary | std::ios::in);

    // Check if the file can be opened; if not, display an error message.
    if (!file.is_open()) {
        std::cerr << "Failed to open the file, probably because it is already opened by another application or you provided a wrong path." << std::endl;
        return 1;
    }

    // If the output choice is a file, open an output file for writing.
    if (outputChoice == 'F' || outputChoice == 'f') {
        std::string outputFilePath = "memdump_results.txt";
        output = std::make_unique<std::ofstream>(outputFilePath);

        // Check if the output file can be opened; if not, display an error message.
        if (!output->good()) {
            std::cerr << "Failed to open the output file. Try to select the 'C' option to print results to the console if this keeps happening." << std::endl;
            return 1;
        }
    }

    // Get the file size to determine the appropriate chunk size.
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Determine the chunk size based on the file size.
    size_t chunkSize = (fileSize > MIN_CHUNK_SIZE) ? MIN_CHUNK_SIZE : fileSize;
    buffer.resize(chunkSize);

    if (outputChoice == 'F' || outputChoice == 'f') {
        std::string outputFilePath = "memdump_results.txt";
        output = std::make_unique<std::ofstream>(outputFilePath);

        if (!output->good()) {
            std::cerr << "Failed to open the output file. Try to select the 'C' option to print results to the console if this keeps happening." << std::endl;
            return 1;
        }
    }

    bool done = false;

    // Process the memory image in chunks.

    while (!done) {
        file.read(buffer.data(), chunkSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            std::string data(buffer.data(), static_cast<size_t>(bytesRead));
            data = overlapData + data;
            overlapData.clear();

            size_t pos1, pos2;

            /** Search for specific substrings in the data, process and print them.
            These searches are related to file access or execution evidence.
            Also, they handle different formats of the same information.
            */

            /**
            "file:///" -> Explorer string (evidence of file accessed/opened)
            "ImageName" -> SgrmBroker string
            "AppPath" -> CDPUserSvc and TextInputHost strings
            "!!" -> DPS string (indicating compilation time of an executable)
            ".exe" and ". e x e" -> ending of any substring (with or without spaces)
            */

            // The matching substrings are passed to ProcessMatch for cleaning and printing.

            // Handle the "file:///" pattern and variations.
            pos1 = data.find("file:///");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 8, pos2 - pos1 - 8 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "ImageName" pattern.
            pos1 = data.find("ImageName");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 12, pos2 - pos1 - 12 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "AppPath" pattern.
            pos1 = data.find("AppPath");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ToLower(a) == ToLower(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 12, pos2 - pos1 - 12 + 4);
                    ProcessMatch(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "!!" pattern, indicating compilation time.
            pos1 = data.find("!!");
            if (pos1 == std::string::npos) {
                // If "!!" is not found, try searching for "! !"
                pos1 = data.find("! !");
            }

            if (pos1 != std::string::npos) {
                std::string searchString = ".exe!";
                std::string searchStringWithSpaces = ". e x e !";
                size_t pos2 = pos1;

                // Find ".exe!" or ". e x e !" with spaces
                pos2 = data.find(searchString, pos1);
                if (pos2 == std::string::npos) {
                    pos2 = data.find(searchStringWithSpaces, pos1);
                }

                if (pos2 != std::string::npos) {
                    // Ensure that there are 4 numbers and a slash (\) after the matched substring
                    size_t endPos = pos2 + (pos2 == data.find(searchStringWithSpaces, pos1) ? searchStringWithSpaces.size() : searchString.size());

                    while (endPos < data.length() && (std::isdigit(data[endPos]) || data[endPos] == ' ' || data[endPos] == '/')) {
                        ++endPos;
                    }

                    if (endPos - pos2 >= (pos2 == data.find(searchStringWithSpaces, pos1) ? searchStringWithSpaces.size() : searchString.size()) + 5) {
                        std::string match = data.substr(pos1, endPos - pos1);
                        ProcessMatch(match, printedMatches, outputChoice, output);
                    }
                }
            }
            // Store the last part of data (220 characters) for overlap with the next chunk.
            overlapData = data.substr(data.size() - 220);
        } else {
            done = true;
        }
    }

    // Check if any printed matches correspond to non-existing files and print a message.
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

    // Close the input file.
    file.close();

    // Display a message and wait for user input before exiting the program.
    std::cout << "Scan finished. Press Enter to exit the program...";
    std::cin.get();

    return 0;
}
