#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <unordered_set>
#include <memory>
#include <vector>
#include <algorithm>
#include <limits>

// Define a constant CHUNK_SIZE for reading data in chunks (resulting in a faster data processing).
constexpr size_t MIN_CHUNK_SIZE = 330;

// An array to store lowercase conversions of characters.
std::array<char, 256> lowercaseConversionTable;

/** Platform-independent text color macros
 * This code defines platform-independent macros to set the text color based on the platform you are running the program on. 
 * It uses Windows Console API on Windows and ANSI escape codes on non-Windows platforms.
 */
 
#ifdef _WIN32
#define SET_TEXT_COLOR_RED() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12)
#define SET_TEXT_COLOR_BLUE() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 9)
#define RESET_TEXT_COLOR() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7)
#else
#define SET_TEXT_COLOR_RED() std::cout << "\033[31m"
#define SET_TEXT_COLOR_BLUE() std::cout << "\033[34m"
#define RESET_TEXT_COLOR() std::cout << "\033[0m"
#endif

// Function to initialize the lowercaseConversionTable array.
void InitializeLowercaseConversionTable() {
    for (int i = 0; i < 256; ++i) {
        lowercaseConversionTable[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(i)));
    }
}

// Function to convert a character to lowercase using the lowercaseConversionTable.
char ConvertToLowercase(char character) {
    return lowercaseConversionTable[static_cast<unsigned char>(character)];
}

// Function to process and clean up strings to be printed.
void CleanStringForPrinting(std::string& inputString) {
    size_t length = inputString.size();
    size_t writeIndex = 0;
    for (size_t readIndex = 0; readIndex < length; ++readIndex) {
        char currentChar = inputString[readIndex];
        switch (currentChar) {
            case '%':
                if (readIndex + 2 < length) {
                    switch (inputString[readIndex + 2]) {
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
            inputString[writeIndex] = currentChar;
            ++writeIndex;
        }
    }
    inputString.resize(writeIndex);
}

// Function to process and clean up a matching string before printing it.
void ProcessMatchingString(std::string& match, std::unordered_set<std::string>& printedMatches, char outputChoice, std::unique_ptr<std::ostream>& output) {
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
    CleanStringForPrinting(match);

    // Check if the match contains "ProgramFiles" and replace it with a more human-readable format.
    if (match.find("ProgramFiles(x86)") != std::string::npos) {
        size_t pos = match.find("ProgramFiles(x86)");
        match.replace(pos, 17, "Program Files (x86)");
    } else if (match.find("ProgramFiles") != std::string::npos) {
        size_t pos = match.find("ProgramFiles");
        match.replace(pos, 12, "Program Files");
    }

    // Convert the match to lowercase for case-insensitive comparison.
    std::string lowercaseMatch = match;
    std::transform(lowercaseMatch.begin(), lowercaseMatch.end(), lowercaseMatch.begin(), ::tolower);

    // Check if the lowercase match has not been previously printed and meets the length condition.
    if (printedMatches.find(lowercaseMatch) == printedMatches.end() && match.length() <= 110) {
        // Insert the lowercase match into the set to keep track of it.
        printedMatches.insert(lowercaseMatch);

        // Print the original match in the desired output.
        if (outputChoice == 'C' || outputChoice == 'c') {
            SET_TEXT_COLOR_BLUE(); // Set text color to blue
            std::cout << "Executed file: ";
            RESET_TEXT_COLOR(); // Reset text color
            std::cout << match << std::endl;
        } else if (outputChoice == 'F' || outputChoice == 'f') {
            (*output) << "Executed file: " << match << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    InitializeLowercaseConversionTable();

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
            // Append any remaining overlapData from the previous chunk to the current data.
            data = overlapData + data;
            // Clear the overlapData variable as it has been processed.
            overlapData.clear();

            size_t pos1, pos2;

            /** 
                Search for specific substrings in the data, process and print them.
                These searches are related to file access or execution evidence.
                Also, they handle different formats of the same information.
            */ 

            /** 
             * "file:///" -> Explorer string (evidence of file accessed/opened)
             * "ImageName" -> SgrmBroker string
             * "AppPath" -> CDPUserSvc and TextInputHost strings
             * "!!" -> DPS string (indicating compilation time of an executable)
             * ".exe" and ". e x e" -> ending of any substring (with or without spaces)
            */

            // The matching substrings are passed to ProcessMatchingString for cleaning and printing.

            // Search for occurrences of "file:///" pattern in the input data.
            pos1 = data.find("file:///");

            // If the pattern is found in the data string.
            if (pos1 != std::string::npos) {
                // Extract a substring starting from the position of the pattern.
                auto dataSubstring = data.substr(pos1);

                // Search for the ".exe" pattern within the extracted substring.
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ConvertToLowercase(a) == ConvertToLowercase(b);
                    });

                // If ".exe" is found within the substring.
                if (it != dataSubstring.end()) {
                    // Calculate the end position of the matched substring.
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));

                    // Extract the matched string, including "file:///" and the ".exe" extension.
                    std::string match = data.substr(pos1 + 8, pos2 - pos1 - 8 + 4);

                    // Process the matching string using a function named ProcessMatchingString.
                    ProcessMatchingString(match, printedMatches, outputChoice, output);
                }
            }

// The same process is repeated for two more patterns: "ImageName" and "AppPath".
// The code checks for the existence of these patterns in the data string and processes
// the matching substrings in a similar manner as described above.

            // Handle the "ImageName" pattern.
            pos1 = data.find("ImageName");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ConvertToLowercase(a) == ConvertToLowercase(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 12, pos2 - pos1 - 12 + 4);
                    ProcessMatchingString(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "AppPath" pattern.
            pos1 = data.find("AppPath");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ConvertToLowercase(a) == ConvertToLowercase(b);
                    });

                if (it != dataSubstring.end()) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    std::string match = data.substr(pos1 + 12, pos2 - pos1 - 12 + 4);
                    ProcessMatchingString(match, printedMatches, outputChoice, output);
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
                    // Calculate the start and end positions of the match
                    size_t start = (pos1 == data.find("!!") ? (pos1 + 2) : (pos1 + 4));
                    size_t endPos = pos2 + ((pos2 == data.find(searchStringWithSpaces, pos1)) ? 7 : 4);

                    if (endPos - start >= ((pos2 == data.find(searchStringWithSpaces, pos1)) ? 7 : 4)) {
                        std::string match = data.substr(start, endPos - start);
                        ProcessMatchingString(match, printedMatches, outputChoice, output);
                    }
                }
            }
            // Store the last part of data (220 characters) for overlap with the next chunk.
            // This ensures that any partial information at the end of the chunk is retained for the next iteration.
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
                    SET_TEXT_COLOR_RED(); // Set text color to red
                    std::cout << "Deleted file (file could not be found): ";
                    RESET_TEXT_COLOR(); // Reset text color
                    std::cout << printedMatch << std::endl;
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
