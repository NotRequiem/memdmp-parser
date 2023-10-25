#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <regex>

int main() {
    FILE* file;
    FILE* output_file;
    char filename[MAX_PATH];
    char output_filename[] = "memdump_output.txt";
    char line[4096];

    printf("Enter the file path (e.g., D:\\Downloads\\memdump.mem): ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';

    file = fopen(filename, "r");
    if (file == NULL) {
        perror("File open failed");
        return 1;
    }

    output_file = fopen(output_filename, "w");
    if (output_file == NULL) {
        perror("Output file open failed");
        return 1;
    }

    const char* pattern1 = "\"displayText\":.+\"activationUri\":.+\"ms-shellactivity:\".+\"appDisplayName:\".+\\.exe\"";
    const char* pattern2 = "{\"ProcessId\":.+\"WindowId\":.+\"IsInVirtualizationContainer\":(false|true).+\"AppId\":.+\".+\\.exe\"";

    std::regex regex1(pattern1);
    std::regex regex2(pattern2);

    while (fgets(line, sizeof(line), file) != NULL) {
        fprintf(output_file, "%s", line);

        if (std::regex_search(line, regex1)) {
            printf("Match (SgrmBroker Pattern): %s", line);
        }
        if (std::regex_search(line, regex2)) {
            printf("Match (TextInputHost Pattern): %s", line);
        }
    }

    fclose(file);
    fclose(output_file);

    return 0;
}
