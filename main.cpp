#include <windows.h>
#include <Softpub.h>
#include <wincrypt.h>
#include <mscat.h>
#include <iostream>
#include <limits>
#include <tchar.h>
#include <fstream>
#include <string>
#include <array>
#include <unordered_set>
#include <memory>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "Crypt32.lib")

class TrustVerifyWrapper {
public:
    bool VerifyFileSignature(const std::wstring& filePath) {
        return CheckFileSignature(filePath, L"SHA256") == ERROR_SUCCESS;
    }

    DWORD CheckFileSignature(const std::wstring& aPePath, const std::wstring& aCatalogHashAlgo) {
        if (verifyFromFile(aPePath) == ERROR_SUCCESS) {
            return ERROR_SUCCESS;
        }
        return verifyFromCatalog(aPePath, aCatalogHashAlgo);
    }

private:
    DWORD verifyFromFile(const std::wstring& aPePath) {
        WINTRUST_FILE_INFO fileData;
        memset(&fileData, 0, sizeof(WINTRUST_FILE_INFO));
        fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
        fileData.pcwszFilePath = aPePath.c_str();
        fileData.hFile = NULL;
        fileData.pgKnownSubject = NULL;

        GUID guidAction = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA winTrustData;
        memset(&winTrustData, 0, sizeof(WINTRUST_DATA));
        winTrustData.cbStruct = sizeof(WINTRUST_DATA);
        winTrustData.dwUIChoice = WTD_UI_NONE;
        winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
        winTrustData.pFile = &fileData;

        return WinVerifyTrust(NULL, &guidAction, &winTrustData);
    }

    DWORD verifyFromCatalog(const std::wstring& aPePath, const std::wstring& aCatalogHashAlgo) {
        LONG lStatus = TRUST_E_NOSIGNATURE;
        GUID WintrustVerifyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        GUID DriverActionGuid = DRIVER_ACTION_VERIFY;
        HANDLE hFile;
        DWORD dwHash;
        BYTE bHash[100];
        HCATINFO hCatInfo = NULL;
        HCATADMIN hCatAdmin;

        hFile = CreateFileW(aPePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return GetLastError();
        }

        if (!CryptCATAdminAcquireContext2(
            &hCatAdmin,
            &DriverActionGuid,
            aCatalogHashAlgo.c_str(),
            NULL,
            0)) {
            CloseHandle(hFile);
            return GetLastError();
        }

        dwHash = sizeof(bHash);
        if (!CryptCATAdminCalcHashFromFileHandle2(
            hCatAdmin,
            hFile,
            &dwHash,
            bHash,
            0)) {
            CloseHandle(hFile);
            return GetLastError();
        }

        auto lHashWstr = ByteHashIntoWstring(bHash, dwHash);

        hCatInfo = CryptCATAdminEnumCatalogFromHash(hCatAdmin, bHash, dwHash, 0, NULL);

        if (!hCatInfo) {
            CryptCATAdminReleaseContext(hCatAdmin, 0);
            CloseHandle(hFile);
            return GetLastError();
        }

        lStatus = verifyTrustFromCatObject(hCatInfo, aPePath, lHashWstr);

        CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatInfo, 0);
        CryptCATAdminReleaseContext(hCatAdmin, 0);
        CloseHandle(hFile);

        return lStatus;
    }

    DWORD verifyTrustFromCatObject(HCATINFO aCatInfo, const std::wstring& aFileName, const std::wstring& aHash) {
        GUID WintrustVerifyGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

        WINTRUST_DATA wd = { 0 };
        WINTRUST_CATALOG_INFO wci = { 0 };

        CATALOG_INFO ci = { 0 };
        CryptCATCatalogInfoFromContext(aCatInfo, &ci, 0);

        memset(&wci, 0, sizeof(wci));
        wci.cbStruct = sizeof(WINTRUST_CATALOG_INFO);
        wci.pcwszCatalogFilePath = ci.wszCatalogFile;
        wci.pcwszMemberFilePath = aFileName.c_str();
        wci.pcwszMemberTag = aHash.c_str();

        memset(&wd, 0, sizeof(wd));
        wd.cbStruct = sizeof(WINTRUST_DATA);
        wd.fdwRevocationChecks = WTD_REVOKE_NONE;
        wd.dwUnionChoice = WTD_CHOICE_CATALOG;
        wd.pCatalog = &wci;
        wd.dwUIChoice = WTD_UI_NONE;
        wd.dwUIContext = WTD_UICONTEXT_EXECUTE;
        wd.fdwRevocationChecks = WTD_STATEACTION_VERIFY;
        wd.dwStateAction = WTD_STATEACTION_VERIFY;
        wd.dwProvFlags = 0;
        wd.hWVTStateData = NULL;
        wd.pwszURLReference = NULL;
        wd.pPolicyCallbackData = NULL;
        wd.pSIPClientData = NULL;
        wd.dwUIContext = 0;

        return WinVerifyTrust(NULL, &WintrustVerifyGuid, &wd);
    }

    std::wstring ByteHashIntoWstring(BYTE* aHash, size_t aHashLen) {
        if (!aHash || !aHashLen) {
            return L"";
        }

        auto lHashString = new WCHAR[aHashLen * 2 + 1];

        for (DWORD dw = 0; dw < aHashLen; ++dw) {
            wsprintfW(&lHashString[dw * 2], L"%02X", aHash[dw]);
        }

        std::wstring lHashWstr(lHashString);

        delete[] lHashString;

        return lHashWstr;
    }
};

// Define a constant CHUNK_SIZE for reading data in chunks (resulting in a faster data processing)
constexpr size_t MIN_CHUNK_SIZE = 330;

// An array to store lowercase conversions of characters
std::array<char, 256> lowercaseConversionTable;

// Text colors for better console output
#ifdef _WIN32
#define SET_TEXT_COLOR_BLUE() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 9)
#define SET_TEXT_COLOR_GREEN() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10)
#define SET_TEXT_COLOR_RED() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12)
#define SET_TEXT_COLOR_YELLOW() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14)
#define RESET_TEXT_COLOR() SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7)
#endif

// Function to initialize the lowercaseConversionTable array
void InitializeLowercaseConversionTable() {
    for (int i = 0; i < 256; ++i) {
        lowercaseConversionTable[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(i)));
    }
}

// Function to convert a character to lowercase using the lowercaseConversionTable
char ConvertToLowercase(char character) {
    // Ensure that the character is within the valid range (0-255)
    if (character >= 0 && character <= 255) {
        return lowercaseConversionTable[static_cast<unsigned char>(character)];
    }
    else {
        // Return the character as is if it's out of bounds
        return character;
    }
}

// Function to convert device paths to proper, logical paths with its drive letter
std::map<std::wstring, std::wstring> GetDosPathDevicePathMap()
{
    wchar_t devicePath[MAX_PATH] = { 0 };
    std::map<std::wstring, std::wstring> result;
    std::wstring dosPath = L"A:";

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter)
    {
        dosPath[0] = letter;
        if (QueryDosDeviceW(dosPath.c_str(), devicePath, MAX_PATH))
        {
            result[dosPath] = devicePath;
        }
    }
    return result;
}

// Function to process and clean up strings to be printed
void CleanStringForPrinting(std::string& inputString) {
    size_t length = inputString.size();
    size_t writeIndex = 0;
    for (size_t readIndex = 0; readIndex < length; ++readIndex) {
        char currentChar = inputString[readIndex];
        switch (currentChar) {
        case '%':
            if (readIndex + 2 < length) {
                switch (inputString[readIndex + 1]) {
                case '3':
                    if (inputString[readIndex + 2] == 'A') {
                        currentChar = ':';
                        readIndex += 2;
                    }
                    break;
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

// Function to process the memory image file
void ProcessMatchingString(std::string& match, std::unordered_set<std::string>& printedMatches, char outputChoice, std::unique_ptr<std::ostream>& output) {
    // Check if the match contains "HarddiskVolume" and replace it with the drive letter
    if (match.find("HarddiskVolume") != std::string::npos) {
        std::map<std::wstring, std::wstring> dosPathDevicePathMap = GetDosPathDevicePathMap();
        size_t pos = match.find("\\\\Device\\\\HarddiskVolume");
        if (pos != std::string::npos) {
            // Find the position of the numeric part
            size_t start = pos + 24;
            size_t end = start;
            while (end < match.length() && std::isdigit(match[end])) {
                end++;
            }

            // Extract the numeric part
            std::string volumePart = match.substr(start, end - start);

            // Trim and sanitize the extracted numeric part
            volumePart.erase(std::remove_if(volumePart.begin(), volumePart.end(), [](char c) { return !std::isdigit(c); }), volumePart.end());

            // Check if volumePart is a valid integer
            if (!volumePart.empty()) {
                int volNum = std::stoi(volumePart); // Convert the numeric part to an integer

                // Find the corresponding drive letter
                wchar_t driveLetter = 0;
                for (const auto& entry : dosPathDevicePathMap) {
                    if (entry.second.find(L"HarddiskVolume" + std::to_wstring(volNum)) != std::wstring::npos) {
                        driveLetter = entry.first[0];
                        break;
                    }
                }

                // Replace the numeric part of the device path with the correct drive letter and a colon
                if (driveLetter != 0) {
                    std::string replacement = std::string(1, static_cast<char>(driveLetter)) + ":";
                    match.replace(pos, end - pos, replacement);
                }
            }
        }
    }

    // Replace double backslashes with a single backslash.
    size_t doubleBackslashPos = match.find("\\\\");
    while (doubleBackslashPos != std::string::npos) {
        match.replace(doubleBackslashPos, 2, "\\");
        doubleBackslashPos = match.find("\\\\", doubleBackslashPos + 1);
    }


    // Check if the match contains "ProgramFiles" and replace it with a more human-readable format
    if (match.find("ProgramFiles(x86)") != std::string::npos) {
        size_t pos = match.find("ProgramFiles(x86)");
        match.replace(pos, 17, "Program Files (x86)");
    }
    else if (match.find("ProgramFiles") != std::string::npos) {
        size_t pos = match.find("ProgramFiles");
        match.replace(pos, 12, "Program Files");
    }

    if (match.find("file:///") != std::string::npos) {

        // Remove "file:///" prefix (first 8 characters)
        match = match.substr(8);
        CleanStringForPrinting(match); // Clean up the string further

        // Convert the match to lowercase for case-insensitive comparison
        std::string lowercaseMatch = match;
        std::transform(lowercaseMatch.begin(), lowercaseMatch.end(), lowercaseMatch.begin(), ::tolower);

        if (printedMatches.find(lowercaseMatch) == printedMatches.end() && match.length() <= 110 && match.length() > 4) {
            printedMatches.insert(lowercaseMatch); // Insert the lowercase match into the set to keep track of it

            // Print the modified match in the desired output.
            if (outputChoice == 'C' || outputChoice == 'c') {
                SET_TEXT_COLOR_GREEN(); // Set text color to green
                std::cout << "Accessed file: ";
                RESET_TEXT_COLOR(); // Reset text color
                std::cout << match << std::endl;
            }
            else if (outputChoice == 'F' || outputChoice == 'f') {
                (*output) << "Accessed file: " << match << std::endl;
            }
        }
    }

    CleanStringForPrinting(match); // Clean up the string further

    // Convert the match to lowercase for case-insensitive comparison
    std::string lowercaseMatch = match;
    std::transform(lowercaseMatch.begin(), lowercaseMatch.end(), lowercaseMatch.begin(), ::tolower);

    // Check if the lowercase match has not been previously printed and meets the length condition
    if (printedMatches.find(lowercaseMatch) == printedMatches.end() && match.length() <= 110 && match.length() > 4) {
        // Insert the lowercase match into the set to keep track of it
        printedMatches.insert(lowercaseMatch);

        // Print the original match in the desired output
        if (outputChoice == 'C' || outputChoice == 'c') {
            SET_TEXT_COLOR_BLUE(); // Set text color to blue
            std::cout << "Executed file: ";
            RESET_TEXT_COLOR(); // Reset text color so that the match is printed in white
            std::cout << match << std::endl;
        }
        else if (outputChoice == 'F' || outputChoice == 'f') {
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

    // Check if a filename is provided as a command-line argument, otherwise prompt the user to enter a file path
    if (argc > 1) {
        filename = argv[1];
    }
    else {
        std::cout << "Enter the file path of your memory image (Example: D:\\Downloads\\memdump.mem): ";
        std::getline(std::cin, filename);
    }

    char outputChoice;
    bool validChoice = false;

    // Prompt the user for the output choice (console or file) and validate the input
    while (!validChoice) {
        std::cout << "Do you want to print the matched strings to the console (C) or to a file (F)? ";
        std::cin >> outputChoice;

        // Clear the input buffer
        while (std::cin.get() != '\n') {
            // Continue reading characters and do nothing with them until a newline is found
        }

        if (outputChoice == 'C' || outputChoice == 'c' || outputChoice == 'F' || outputChoice == 'f') {
            validChoice = true;
        }
        else {
            std::cerr << "Invalid choice. Please enter 'C' for console or 'F' for file." << std::endl;
        }
    }

    std::ifstream file(filename, std::ios::binary | std::ios::in);

    // Check if the file can be opened; if not, display an error message
    if (!file.is_open()) {
        std::cerr << "Failed to open the file, probably because it is already opened by another application or you provided a wrong path." << std::endl;
        return 1;
    }

    // If the output choice is a file, open an output file for writing
    if (outputChoice == 'F' || outputChoice == 'f') {
        std::string outputFilePath = "memparser_results.txt";
        output = std::make_unique<std::ofstream>(outputFilePath);

        // Check if the output file can be opened; if not, display an error message
        if (!output->good()) {
            std::cerr << "Failed to open the output file. Try to select the 'C' option to print results to the console if this keeps happening." << std::endl;
            return 1;
        }
    }

    // Get the file size to determine the appropriate chunk size
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Determine the chunk size based on the file size
    size_t chunkSize = (fileSize > MIN_CHUNK_SIZE) ? MIN_CHUNK_SIZE : fileSize;
    buffer.resize(chunkSize);

    if (outputChoice == 'F' || outputChoice == 'f') {
        std::string outputFilePath = "memparser_results.txt";
        output = std::make_unique<std::ofstream>(outputFilePath);

        if (!output->good()) {
            std::cerr << "Failed to open the output file. Try to select the 'C' option to print results to the console if this keeps happening." << std::endl;
            return 1;
        }
    }

    bool done = false;

    // Process the memory image in chunks
    while (!done) {
        file.read(buffer.data(), chunkSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            std::string data(buffer.data(), static_cast<size_t>(bytesRead));
            // Append any remaining overlapData from the previous chunk to the current data.
            data = overlapData + data;
            // Clear the overlapData variable as it has been processed.
            overlapData.clear();

            /**
             * The overlapData variable is used to handle overlapping data between successive chunks of data read from a memory image file.
             * The program processes the memory image file in chunks for more efficient data processing.
             * overlapData is employed to ensure that any partial information at the end of one chunk is retained and combined with the data in the next chunk.
             * This prevents splitting and processing of incomplete information that might span across two chunks.
            */

            size_t pos1, pos2;

            /**
                Search for specific substrings in the data, process and print them.
                These searches are related to file access or execution evidence.
                Also, they handle different formats of the same information.
            */

            /**
             * "file:///" -> Explorer string (evidence of file accessed/opened).
             * "ImageName" -> SgrmBroker string.
             * "AppPath" -> CDPUserSvc and TextInputHost strings.
             * "!!" -> DPS string (indicating compilation time of an executable).
             * "\??\" -> Explorer and csrss string (evidence of execution).
             * ".exe" and ". e x e" -> ending of any substring (with or without spaces).
            */

            // The matching substrings are passed to ProcessMatchingString for cleaning and printing

            // Search for occurrences of "file:///" pattern in the input data
            pos1 = data.find("file:///");

            // If the pattern "file:///" is found in the data string:
            if (pos1 != std::string::npos) {
                // Extract a substring starting from the position of the pattern.
                auto dataSubstring = data.substr(pos1);

                // Search for the ".exe" pattern within the extracted substring.
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4,
                    [](char a, char b) {
                        return ConvertToLowercase(a) == ConvertToLowercase(b);
                    });

                // Check for additional occurrences of "file:///" between pos1 and it.
                bool hasFileOccurrences = false;
                for (auto it2 = dataSubstring.begin() + 7; it2 < it; ++it2) {
                    if (std::equal(it2, it2 + 7, "file:///")) {
                        hasFileOccurrences = true;
                        break;
                    }
                }

                // Check for the presence of special characters between "file:///" and ".exe".
                bool hasSpecialCharactersBetween = false;
                const std::string specialCharacters = "*\"<>?|:\\";
                for (auto it2 = dataSubstring.begin() + 7; it2 < it; ++it2) {
                    if (specialCharacters.find(*it2) != std::string::npos) {
                        hasSpecialCharactersBetween = true;
                        break;
                    }
                }

                // Check for the presence of a slash "/" between "file:///" and ".exe".
                bool hasSlashBetween = false;
                auto slashPosition = std::find(dataSubstring.begin() + 8, it, '/');
                if (slashPosition != it) {
                    hasSlashBetween = true;
                }

                // If there are no additional "file:///" between the initial "file:///" and ".exe",
                // and there are no special characters and there is a slash "/" between them:
                if (!hasFileOccurrences && !hasSpecialCharactersBetween && hasSlashBetween && it != dataSubstring.end()) {
                    // Calculate the end position of the matched substring.
                    size_t pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));

                    // Extract the matched string, including "file:///" and the ".exe" extension.
                    std::string match = data.substr(pos1, pos2 - pos1 + 4);

                    // Process the matching string using a function named ProcessMatchingString.
                    ProcessMatchingString(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "ImageName" pattern
            pos1 = data.find("\"ImageName\":\"");
            if (pos1 != std::string::npos) {
                auto dataSubstring = data.substr(pos1);
                auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe\"", ".exe\"" + 4, // Increment +4 to include the ".exe" extension.
                    [](char a, char b) {
                        return ConvertToLowercase(a) == ConvertToLowercase(b);
                    });

                // Check for the presence of special characters between "ImageName":" and ".exe".
                bool hasSpecialCharactersBetween = false;
                const std::string specialCharacters = "*\"<>?|:\\";
                for (auto it2 = dataSubstring.begin() + 13; it2 < it; ++it2) {
                    if (specialCharacters.find(*it2) != std::string::npos) {
                        hasSpecialCharactersBetween = true;
                        break;
                    }
                }

                if (it != dataSubstring.end() && !hasSpecialCharactersBetween) {
                    pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                    // Increment +13 here to skip the "ImageName":" part of the string, leaving only the device path.
                    std::string match = data.substr(pos1 + 13, pos2 - pos1 - 13 + 4);
                    ProcessMatchingString(match, printedMatches, outputChoice, output);
                }
            }

            // Handle the "AppPath" pattern
            pos1 = data.find("\"AppPath\":\"");
            if (pos1 != std::string::npos) {
                pos1 += 12; // Move past "AppPath":"
                auto dataSubstring = data.substr(pos1);

                // Check if the first character in dataSubstring is an alphabetic character
                if (dataSubstring.size() > 0 && std::isalpha(dataSubstring[0])) {
                    auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4, // Increment +4 here to include the ".exe" extension.
                        [](char a, char b) {
                            return ConvertToLowercase(a) == ConvertToLowercase(b);
                        });

                    if (it != dataSubstring.end()) {
                        pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                        std::string match = data.substr(pos1, pos2 - pos1 + 4); // Increment +4 here to include the ".exe" extension
                        ProcessMatchingString(match, printedMatches, outputChoice, output);
                    }
                }
            }

            // Search for "!!" in the input string, if not found, search for "! !"
            pos1 = data.find("!!");
            if (pos1 == std::string::npos) {
                pos1 = data.find("! !");
            }

            // If "!!" or "! !" is found in the input data
            if (pos1 != std::string::npos) {
                std::string searchString = ".exe!";
                std::string searchStringWithSpaces = ". e x e !";
                size_t pos2 = pos1;

                // Search for ".exe!" or ". e x e !" with spaces
                pos2 = data.find(searchString, pos1);
                if (pos2 == std::string::npos) {
                    pos2 = data.find(searchStringWithSpaces, pos1);
                }

                // If ".exe!" or ". e x e !" is found:
                if (pos2 != std::string::npos) {
                    // Calculate the start and end positions of the match
                    size_t start = (pos1 == data.find("!!") ? (pos1 + 2) : (pos1 + 4)); // Skip 2 characters if no spaces between characters were found, 4 otherwise
                    size_t endPos = pos2 + ((pos2 == data.find(searchStringWithSpaces, pos1)) ? 7 : 4);

                    /**
                     *
                     * This code block checks if a DPS string with a correct format was found to avoid false flagging corrupt data.
                     * I will take !!svchost.exe!2092/10/12:19:58:29! as an example to explain this part of the code.
                     * Each character is checked. For example, a DPS string will always have a digit in the first position after ".exe!" or ". e x e !".
                     * We can analyze if at a certain bit of the string, there is a digit, a colon, a slash, etc... to check if a DPS string was found.
                     *
                    */

                    bool CorrectFormat = false;
                    if (endPos < data.length() - 20) {
                        char c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17, c18, c19, c20;

                        if (pos2 == data.find(searchStringWithSpaces, pos1)) {
                            // If ". e x e !" was found, a DPS string with spaces between characters was found.
                            // Therefore we will have to skip spaces between characters
                            c1 = data[endPos + 2]; // 2
                            c2 = data[endPos + 4]; // 0
                            c3 = data[endPos + 6]; // 9
                            c4 = data[endPos + 8]; // 2
                            c5 = data[endPos + 10]; // /
                            c6 = data[endPos + 12]; // 1
                            c7 = data[endPos + 14]; // 0
                            c8 = data[endPos + 16]; // / 
                            c9 = data[endPos + 18]; // 1
                            c10 = data[endPos + 20]; // 2
                            c11 = data[endPos + 22]; // :
                            c12 = data[endPos + 24]; // 1
                            c13 = data[endPos + 26]; // 9
                            c14 = data[endPos + 28]; // :
                            c15 = data[endPos + 30]; // 5
                            c16 = data[endPos + 32]; // 8
                            c17 = data[endPos + 34]; // :
                            c18 = data[endPos + 36]; // 2
                            c19 = data[endPos + 38]; // 9
                            c20 = data[endPos + 40]; // !
                        }
                        else {
                            // If ".exe!" was found, use normal positions since there are no spaces between characters to skip
                            c1 = data[endPos + 1]; // 2
                            c2 = data[endPos + 2]; // 0
                            c3 = data[endPos + 3]; // 9
                            c4 = data[endPos + 4]; // 2
                            c5 = data[endPos + 5]; // /
                            c6 = data[endPos + 6]; // 1
                            c7 = data[endPos + 7]; // 0
                            c8 = data[endPos + 8]; // / 
                            c9 = data[endPos + 9]; // 1
                            c10 = data[endPos + 10]; // 2
                            c11 = data[endPos + 11]; // :
                            c12 = data[endPos + 12]; // 1
                            c13 = data[endPos + 13]; // 9
                            c14 = data[endPos + 14]; // :
                            c15 = data[endPos + 15]; // 5
                            c16 = data[endPos + 16]; // 8
                            c17 = data[endPos + 17]; // :
                            c18 = data[endPos + 18]; // 2
                            c19 = data[endPos + 19]; // 9
                            c20 = data[endPos + 20]; // !
                        }
                        // Check if the characters at these positions are digits and the last character is a slash
                        if (isdigit(c1) && isdigit(c2) && isdigit(c3) && isdigit(c4) && c5 == '/'
                            && isdigit(c6) && isdigit(c7) && c8 == '/' && isdigit(c9) && isdigit(c10)
                            && c11 == ':' && isdigit(c12) && isdigit(c13) && c14 == ':' && isdigit(c15)
                            && isdigit(c16) && c17 == ':' && isdigit(c18) && isdigit(c19) && c20 == '!') {
                            CorrectFormat = true;
                        }
                    }

                    // If a DPS string format was found succesfully:
                    if (CorrectFormat) {
                        std::string match = data.substr(start, endPos - start); // We extract the executable name
                        // Process the extracted string
                        ProcessMatchingString(match, printedMatches, outputChoice, output);
                    }
                }
            }

            pos1 = data.find("\\??\\");
            if (pos1 != std::string::npos) {
                pos1 += 4; // Move past \\??\\ //
                auto dataSubstring = data.substr(pos1);

                // Check if the substring has at least 3 characters (letter, colon, slash)
                if (dataSubstring.size() >= 3 && std::isalpha(dataSubstring[0]) && dataSubstring[1] == ':' && dataSubstring[2] == '/') {
                    auto it = std::search(dataSubstring.begin(), dataSubstring.end(), ".exe", ".exe" + 4, // Increment +4 here to include the ".exe" extension.
                        [](char a, char b) {
                            return std::tolower(a) == std::tolower(b);
                        });

                    if (it != dataSubstring.end()) {
                        pos2 = pos1 + static_cast<size_t>(std::distance(dataSubstring.begin(), it));
                        std::string match = data.substr(pos1 + 4, pos2 - pos1); // Increment +4 here to include the ".exe" extension
                        ProcessMatchingString(match, printedMatches, outputChoice, output);
                    }
                }
            }

            // Store the last part of data (220 characters) for overlap with the next chunk.
            // This ensures that any partial information at the end of the chunk is retained for the next iteration.
            overlapData = data.substr(data.size() - 220);
        }
        else {
            done = true;
        }
    }

    std::vector<std::string> nonDeletedFiles;

    // Check if any printed matches correspond to non-existing files and print a message
    for (const std::string& printedMatch : printedMatches) {
        if (std::isalpha(printedMatch[0]) && printedMatch.size() >= 3 &&
            printedMatch[1] == ':' && printedMatch[2] == '\\') {
            if (!std::ifstream(printedMatch)) {
                if (outputChoice == 'C' || outputChoice == 'c') {
                    SET_TEXT_COLOR_RED(); // Set text color to red
                    std::cout << "Deleted file (file could not be found): ";
                    RESET_TEXT_COLOR(); // Reset text color so that the match is printed in white
                    std::cout << printedMatch << std::endl;
                }
                else if (outputChoice == 'F' || outputChoice == 'f') {
                    (*output) << "Deleted file (file could not be found): " << printedMatch << std::endl;
                }
            }
            else {
                nonDeletedFiles.push_back(printedMatch); // Store non-deleted files
            }
        }
    }

    TrustVerifyWrapper wrapper;
    std::set<std::wstring> unsignedFiles;

    // Iterate through non-deleted files and check for digital signatures
    for (const std::string& nonDeletedFile : nonDeletedFiles) {
        std::wstring wideFilePath(nonDeletedFile.begin(), nonDeletedFile.end());
        if (!wrapper.VerifyFileSignature(wideFilePath)) {
            SET_TEXT_COLOR_YELLOW(); // Set text color to yellow
            std::wcout << L"Unsigned file detected: " << std::endl;
            RESET_TEXT_COLOR(); // Reset text color so that the match is printed in white
            std::wcout << wideFilePath << std::endl;
            unsignedFiles.insert(wideFilePath);
        }
    }

    // Close the input file
    file.close();

    // Wait for user input before exiting the program
    std::cout << "Scan finished. Press Enter to exit the program...";
    std::cin.get();

    return 0;
}
