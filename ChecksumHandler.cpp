#include ".\CS_Handler\checksum.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <utility>
#include <limits>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "CS_Handler.lib")

// Cross Compatibility for Clearing Console
void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Display command-line usage information
void displayUsage(const std::string& programName) {
    std::cout << "\033[1;34mChecksum Handler - Command Line Usage:\033[0m" << std::endl;
    std::cout << "  " << programName << " create <folder_path> [exclude_pattern1] [exclude_pattern2] ..." << std::endl;
    std::cout << "      Creates a checksum file in the specified folder." << std::endl;
    std::cout << "      Optional: Specify patterns to exclude files containing these patterns." << std::endl;
    std::cout << std::endl;
    std::cout << "  " << programName << " validate <current_path> <new_path>" << std::endl;
    std::cout << "      Validates checksums between two paths and reports changes." << std::endl;
    std::cout << std::endl;
    std::cout << "  " << programName << " changes <current_path> <new_path>" << std::endl;
    std::cout << "      Shows detailed changes between two checksum files." << std::endl;
    std::cout << std::endl;
    std::cout << "  " << programName << " help" << std::endl;
    std::cout << "      Displays this help information." << std::endl;
    std::cout << std::endl;
    std::cout << "  Without arguments: Starts in interactive menu mode." << std::endl;
}

int main(int argc, char* argv[])
{
    // Check for command-line arguments
    if (argc > 1) {
        std::string command = argv[1];

        // Convert command to lowercase for case-insensitive comparison
        std::transform(command.begin(), command.end(), command.begin(),
            [](unsigned char c) { return std::tolower(c); });

        // Help command or invalid arguments
        if (command == "help" || command == "--help" || command == "-h") {
            displayUsage(argv[0]);
            return 0;
        }

        // Create command
        else if (command == "create" && argc >= 3) {
            std::string path = argv[2];

            // Handle exclude patterns from args 3+ if present
            std::vector<std::string> excludePatterns;
            for (int i = 3; i < argc; i++) {
                excludePatterns.push_back(argv[i]);
            }

            // Show command info
            std::cout << "\033[1;34mCommand: Create checksum file\033[0m" << std::endl;
            std::cout << "Path: " << path << std::endl;
            if (!excludePatterns.empty()) {
                std::cout << "Exclude patterns: ";
                for (const auto& pattern : excludePatterns) {
                    std::cout << pattern << " ";
                }
                std::cout << std::endl;
            }

            int result = createChecksumFile(path, excludePatterns);
            return (result == 200) ? 0 : 1;  // Return 0 for success, 1 for error
        }

        // Validate command
        else if ((command == "validate" || command == "verify") && argc >= 4) {
            std::string currPath = argv[2];
            std::string newPath = argv[3];

            // Show command info
            std::cout << "\033[1;34mCommand: Validate checksums\033[0m" << std::endl;
            std::cout << "Current Path: " << currPath << std::endl;
            std::cout << "New Path: " << newPath << std::endl;

            // Use simple validation that returns true if no changes
            bool result = validateChecksumFile(currPath, newPath);
            return result ? 0 : 1;  // Return 0 if no changes (validation passed), 1 if changes or error
        }

        // Changes command - new feature using getChecksumFileChanges
        else if (command == "changes" && argc >= 4) {
            std::string currPath = argv[2];
            std::string newPath = argv[3];

            // Show command info
            std::cout << "\033[1;34mCommand: Show detailed changes\033[0m" << std::endl;
            std::cout << "Current Path: " << currPath << std::endl;
            std::cout << "New Path: " << newPath << std::endl;

            // Use getChecksumFileChanges with true to print results
            std::vector<FileChangeInfo> changes = getChecksumFileChanges(currPath, newPath, true);

            // Return change count for scripting
            return changes.empty() ? 0 : static_cast<int>(std::min(changes.size(), static_cast<size_t>(255)));
        }

        // Invalid command or insufficient arguments
        else {
            std::cout << "\033[1;31mError: Invalid command or insufficient arguments.\033[0m" << std::endl;
            displayUsage(argv[0]);
            return 1;
        }
    }

    // Interactive Menu Mode
    while (true) {
        clearConsole();

        // Print Menu
        std::cout << "\033[1;34mChecksum Handler\033[0m\n";
        std::cout << "[1] \033[1;33mCreate Checksum File\033[0m\n";
        std::cout << "[2] \033[1;33mValidate Checksum\033[0m\n";
        std::cout << "[3] \033[1;32mDetailed Changes\033[0m\n";
        std::cout << "[4] \033[1;31mExit\033[0m\n";
        std::cout << "\nEnter Option: ";

        // Improved menu input handling
        std::string input;
        std::getline(std::cin, input);

        int choice = 0;
        try {
            // Attempt to convert input to integer
            choice = std::stoi(input);
        }
        catch (const std::exception&) {
            // Handle invalid input (non-numeric)
            choice = 0; // Will trigger default case
        }

        clearConsole();

        // Check Choice
        switch (choice) {
        case 1: {
            // User Input of Folder Path
            std::cout << "\nEnter Folder Path: ";
            std::string str;
            std::getline(std::cin, str);

            // Optional: Ask for exclude patterns
            std::cout << "Enter exclude patterns (comma separated, or press Enter for none): ";
            std::string patternsInput;
            std::getline(std::cin, patternsInput);

            std::vector<std::string> excludePatterns;
            if (!patternsInput.empty()) {
                size_t pos = 0;
                std::string token;
                while ((pos = patternsInput.find(',')) != std::string::npos) {
                    token = patternsInput.substr(0, pos);
                    // Trim whitespace
                    token.erase(0, token.find_first_not_of(" \t"));
                    token.erase(token.find_last_not_of(" \t") + 1);
                    if (!token.empty()) {
                        excludePatterns.push_back(token);
                    }
                    patternsInput.erase(0, pos + 1);
                }
                // Add the last token
                patternsInput.erase(0, patternsInput.find_first_not_of(" \t"));
                patternsInput.erase(patternsInput.find_last_not_of(" \t") + 1);
                if (!patternsInput.empty()) {
                    excludePatterns.push_back(patternsInput);
                }
            }

            createChecksumFile(str, excludePatterns);
            break;
        }

        case 2: {
            // Get Current Checksum
            std::cout << "\nEnter Current Checksum Path: ";
            std::string str;
            std::getline(std::cin, str);

            // Get New Checksum - using consistent string input method
            std::cout << "\nEnter New Checksum Path: ";
            std::string strTwo;
            std::getline(std::cin, strTwo);

            // Use simple validation
            bool result = validateChecksumFile(str, strTwo);

            // Success message is already printed by the validateChecksumFile function
            if (result) {
                std::cout << "\n\033[1;32mValidation completed successfully - Files match!\033[0m" << std::endl;
            }
            break;
        }

        case 3: {
            // Get Current Checksum
            std::cout << "\nEnter Current Checksum Path: ";
            std::string str;
            std::getline(std::cin, str);

            // Get New Checksum
            std::cout << "\nEnter New Checksum Path: ";
            std::string strTwo;
            std::getline(std::cin, strTwo);

            // Get detailed changes with printing enabled
            std::vector<FileChangeInfo> changes = getChecksumFileChanges(str, strTwo, true);

            // Results are already printed by the function with printResults=true
            std::cout << "\nFound " << changes.size() << " total changes." << std::endl;
            break;
        }

        case 4: {
            // Exit
            return 0;
        }

        default: {
            // Invalid Choice with better error message
            std::cout << "\033[1;31mInvalid Choice: '" << input << "'\033[0m" << std::endl;
            std::cout << "Please enter 1, 2, 3, or 4." << std::endl;
            break;
        }
        }

        // Wait for user input
        std::cout << "\nPress Enter to continue...";
        std::string empty;
        std::getline(std::cin, empty);
    }
}