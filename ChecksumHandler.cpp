#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <utility>
#include <limits>
#include <iomanip>

// Define a struct to hold file change information
struct FileChangeInfo {
    std::string filePath;
    std::string changeType;
};


// Export Macro for DLL
#ifdef CHECKSUMHANDLER_EXPORTS
#define CHECKSUMHANDLER_API __declspec(dllexport)
#else  
#define CHECKSUMHANDLER_API __declspec(dllimport)
#endif

// Function declarations
int createChecksumFile(const std::string& path, const std::vector<std::string>& excludePatterns = {});
bool validateChecksumFile(const std::string& currPath, const std::string& newPath);
bool validateChecksumFile(const std::string& currPath, const std::string& newPath, std::vector<FileChangeInfo>& changedFiles);



/**
 * Memory Management:
 * The GetChangedFiles function allocates memory that must be freed by the caller
 * using the FreeChangedFiles function to prevent memory leaks.
 *
 * Example usage:
 *   char** filePaths = nullptr;
 *   char** changeTypes = nullptr;
 *   int count = 0;
 *   int result = GetChangedFiles(currPath, newPath, &filePaths, &changeTypes, &count);
 *   if (result > 0) {
 *     // Process the changes
 *     // ...
 *     // Free the memory when done
 *     FreeChangedFiles(filePaths, changeTypes, count);
 *   }
 */

// Function declarations with C linkage for export
extern "C" {
    CHECKSUMHANDLER_API int CreateChecksumFile(const char* path);
    CHECKSUMHANDLER_API bool ValidateChecksumFile(const char* currPath, const char* newPath);
    CHECKSUMHANDLER_API int GetChangedFiles(const char* currPath, const char* newPath, char*** filePathsOut, char*** changeTypesOut, int* count);
    CHECKSUMHANDLER_API void FreeChangedFiles(char** filePaths, char** changeTypes, int count);
}

// Internal C++ Function implementations
int calculateFileChecksum(const std::filesystem::path& filePath) {
    if (filePath.filename() == "checksum.txt") {
        return 0;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cout << "\n\033[1;33mWarning: Unable to open file for checksum: " << filePath << "\033[0m" << std::endl;
        return -1;
    }

    // CRC32 algorithm implementation (more reliable than the current approach)
    constexpr size_t bufferSize = 8192;  // 8KB Buffer
    char buffer[bufferSize];
    uint32_t crc = 0xFFFFFFFF;

    // Generate CRC32 lookup table once
    static uint32_t crcTable[256];
    static bool tableInitialized = false;

    if (!tableInitialized) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            }
            crcTable[i] = c;
        }
        tableInitialized = true;
    }

    while (file.read(buffer, bufferSize)) {
        for (size_t i = 0; i < static_cast<size_t>(file.gcount()); ++i) {
            crc = crcTable[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
    }

    // Process remaining bytes
    for (size_t i = 0; i < static_cast<size_t>(file.gcount()); ++i) {
        crc = crcTable[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
    }

    return ~crc; // Final XOR value
}
int createChecksumFile(const std::string& path, const std::vector<std::string>& excludePatterns) {
    // Validate that the path exists
   if (!std::filesystem::exists(path)) {
       std::cout << "\n\033[1;31mError: Path does not exist: " << path << "\033[0m" << std::endl;
       return -1;
   }

   // Create a Checksum File in Path
   std::filesystem::path checksumPath = std::filesystem::path(path) / "checksum.txt";
   std::ofstream checksumFile;
   try {
       checksumFile.open(checksumPath);
       if (!checksumFile.is_open()) {
           std::cout << "\n\033[1;31mError: Unable to create checksum file\033[0m" << std::endl;
           return -1;
       }
   }
   catch (const std::exception& e) {
       std::cout << "\n\033[1;31mException while creating checksum file: " << e.what() << "\033[0m" << std::endl;
       return -1;
   }

   int fileCount = 0;
   int errorCount = 0;

   // Loop Through Each Folder and File in Path, Calculate Checksum & Write to File
   std::cout << "\nCalculating checksums for files in " << path << "...\n";

   for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
       if (entry.is_regular_file()) {
           std::filesystem::path filePath = entry.path();

           // Skip the checksum file itself
           if (filePath.filename() == "checksum.txt") {
               continue;
           }

           // Skip files matching exclude patterns
           bool shouldExclude = false;
           for (const auto& pattern : excludePatterns) {
               if (filePath.string().find(pattern) != std::string::npos) {
                   shouldExclude = true;
                   break;
               }
           }

           if (shouldExclude) {
               continue;
           }

           int checksum = calculateFileChecksum(filePath);
           if (checksum != -1) {
               try {
                   checksumFile << filePath.string() << " " << checksum << std::endl;
                   if (checksumFile.fail()) {
                       std::cout << "\n\033[1;33mWarning: Failed to write checksum for: " << filePath.string() << "\033[0m" << std::endl;
                       errorCount++;
                   }
                   else {
                       fileCount++;
                       // Show progress every 10 files
                       if (fileCount % 10 == 0) {
                           std::cout << "." << std::flush;
                       }
                   }
               }
               catch (const std::exception& e) {
                   std::cout << "\n\033[1;33mException while writing checksum: " << e.what() << "\033[0m" << std::endl;
                   errorCount++;
               }
           }
           else {
               errorCount++;
           }
       }
   }

   checksumFile.close();

   // Print Success
   std::cout << "\n\033[1;32mChecksum File Created: " << checksumPath << "\033[0m" << std::endl;
   std::cout << "\033[1;32mProcessed " << fileCount << " files";
   if (errorCount > 0) {
       std::cout << " (" << errorCount << " files could not be read)";
   }
   std::cout << "\033[0m" << std::endl;

   // Return Success
   return 200;
}  

bool validateChecksumFile(const std::string& currPath, const std::string& newPath, std::vector<FileChangeInfo>& changedFiles) {
    try {
        std::cout << "\nValidating Files..." << std::endl;

        // Validate Both Paths Exist
        if (!std::filesystem::exists(currPath)) {
            std::cout << "\n\033[1;31mError: Current Path does not exist: " << currPath << "\033[0m" << std::endl;
            return false;
        }

        if (!std::filesystem::exists(newPath)) {
            std::cout << "\n\033[1;31mError: New Path does not exist: " << newPath << "\033[0m" << std::endl;
            return false;
        }

        // Check if paths are folders and look for checksum.txt
        std::filesystem::path currChecksumPath = currPath;
        std::filesystem::path newChecksumPath = newPath;

        if (std::filesystem::is_directory(currPath)) {
            currChecksumPath = std::filesystem::path(currPath) / "checksum.txt";
            if (!std::filesystem::exists(currChecksumPath)) {
                std::cout << "\n\033[1;31mError: checksum.txt not found in current folder: " << currPath << "\033[0m" << std::endl;
                return false;
            }
        }

        if (std::filesystem::is_directory(newPath)) {
            newChecksumPath = std::filesystem::path(newPath) / "checksum.txt";
            if (!std::filesystem::exists(newChecksumPath)) {
                std::cout << "\n\033[1;31mError: checksum.txt not found in new folder: " << newPath << "\033[0m" << std::endl;
                return false;
            }
        }

        // Read Checksum Files with progress indication
        std::cout << "\nReading and comparing checksum files..." << std::endl;

        // Open and validate checksum files
        std::ifstream currChecksumFile(currChecksumPath);
        std::ifstream newChecksumFile(newChecksumPath);

        if (!currChecksumFile.is_open()) {
            std::cout << "\n\033[1;31mError: Unable to open current checksum file: " << currChecksumPath << "\033[0m" << std::endl;
            return false;
        }

        if (!newChecksumFile.is_open()) {
            std::cout << "\n\033[1;31mError: Unable to open new checksum file: " << newChecksumPath << "\033[0m" << std::endl;
            currChecksumFile.close();
            return false;
        }

        // Create maps to store file checksums
        std::map<std::string, int> currFiles;
        std::map<std::string, int> newFiles;
        int parsedLineCount = 0;
        int validLineCount = 0;
        int errorLineCount = 0;

        // Read current checksum file
        std::string line;
        while (std::getline(currChecksumFile, line)) {
            parsedLineCount++;

            // Show progress indicator for large files
            if (parsedLineCount % 100 == 0) {
                std::cout << "." << std::flush;
            }

            size_t spacePos = line.find_last_of(' ');
            if (spacePos != std::string::npos) {
                std::string filePath = line.substr(0, spacePos);
                try {
                    int checksum = std::stoi(line.substr(spacePos + 1));
                    currFiles[filePath] = checksum;
                    validLineCount++;
                }
                catch (const std::exception& e) {
                    std::cout << "\n\033[1;31mError parsing checksum in current file (line " << parsedLineCount
                        << "): " << filePath << " (" << e.what() << ")\033[0m" << std::endl;
                    errorLineCount++;
                }
            }
            else if (!line.empty()) { // Ignore empty lines
                std::cout << "\n\033[1;33mWarning: Malformed line in current checksum file (line "
                    << parsedLineCount << "): " << line << "\033[0m" << std::endl;
                errorLineCount++;
            }
        }

        // Reset counters for the new file
        parsedLineCount = 0;
        int validNewLineCount = 0;

        // Read new checksum file
        while (std::getline(newChecksumFile, line)) {
            parsedLineCount++;

            // Show progress indicator for large files
            if (parsedLineCount % 100 == 0) {
                std::cout << "+" << std::flush;
            }

            size_t spacePos = line.find_last_of(' ');
            if (spacePos != std::string::npos) {
                std::string filePath = line.substr(0, spacePos);
                try {
                    int checksum = std::stoi(line.substr(spacePos + 1));
                    newFiles[filePath] = checksum;
                    validNewLineCount++;
                }
                catch (const std::exception& e) {
                    std::cout << "\n\033[1;31mError parsing checksum in new file (line " << parsedLineCount
                        << "): " << filePath << " (" << e.what() << ")\033[0m" << std::endl;
                    errorLineCount++;
                }
            }
            else if (!line.empty()) { // Ignore empty lines
                std::cout << "\n\033[1;33mWarning: Malformed line in new checksum file (line "
                    << parsedLineCount << "): " << line << "\033[0m" << std::endl;
                errorLineCount++;
            }
        }

        // File statistics
        std::cout << "\n\033[1;36mFile Statistics:\033[0m" << std::endl;
        std::cout << "Current file: " << validLineCount << " valid entries" << std::endl;
        std::cout << "New file: " << validNewLineCount << " valid entries" << std::endl;
        if (errorLineCount > 0) {
            std::cout << "Errors: " << errorLineCount << " lines had parsing issues" << std::endl;
        }

        // Close Checksum Files
        currChecksumFile.close();
        newChecksumFile.close();

        // Compare files and identify changes
        changedFiles.clear();
        std::cout << "\nComparing checksums..." << std::endl;

        // Find added and changed files
        for (const auto& [filePath, checksum] : newFiles) {
            auto it = currFiles.find(filePath);

            if (it == currFiles.end()) {
                // File was added
                changedFiles.push_back({ filePath, "ADDED" });
            }
            else if (it->second != checksum) {
                // File was changed (different checksum)
                changedFiles.push_back({ filePath, "CHANGED" });
            }
        }

        // Find deleted files
        for (const auto& [filePath, checksum] : currFiles) {
            if (newFiles.find(filePath) == newFiles.end()) {
                // File was deleted
                changedFiles.push_back({ filePath, "DELETED" });
            }
        }

        // Print summary
        if (changedFiles.empty()) {
            std::cout << "\n\033[1;32mChecksum Files Match - No Changes Detected\033[0m" << std::endl;
        }
        else {
            std::cout << "\n\033[1;33mChanges Detected:\033[0m" << std::endl;
            std::cout << "-------------------------" << std::endl;

            int addedCount = 0, deletedCount = 0, changedCount = 0;

            // Use categorized output for better readability when there are many changes
            if (changedFiles.size() > 20) {
                // Group changes by type for easier reading
                std::vector<std::string> addedFiles;
                std::vector<std::string> deletedFiles;
                std::vector<std::string> modifiedFiles;

                for (const auto& change : changedFiles) {
                    if (change.changeType == "ADDED") {
                        addedFiles.push_back(change.filePath);
                        addedCount++;
                    }
                    else if (change.changeType == "DELETED") {
                        deletedFiles.push_back(change.filePath);
                        deletedCount++;
                    }
                    else if (change.changeType == "CHANGED") {
                        modifiedFiles.push_back(change.filePath);
                        changedCount++;
                    }
                }

                // Show added files
                if (!addedFiles.empty()) {
                    std::cout << "\n\033[1;32mAdded Files (" << addedCount << "):\033[0m" << std::endl;
                    for (const auto& file : addedFiles) {
                        std::cout << "  " << file << std::endl;
                    }
                }

                // Show deleted files
                if (!deletedFiles.empty()) {
                    std::cout << "\n\033[1;31mDeleted Files (" << deletedCount << "):\033[0m" << std::endl;
                    for (const auto& file : deletedFiles) {
                        std::cout << "  " << file << std::endl;
                    }
                }

                // Show modified files
                if (!modifiedFiles.empty()) {
                    std::cout << "\n\033[1;33mModified Files (" << changedCount << "):\033[0m" << std::endl;
                    for (const auto& file : modifiedFiles) {
                        std::cout << "  " << file << std::endl;
                    }
                }
            }
            else {
                // For fewer changes, use the original line-by-line output
                for (const auto& change : changedFiles) {
                    if (change.changeType == "ADDED") {
                        std::cout << "\033[1;32m[ADDED]\033[0m " << change.filePath << std::endl;
                        addedCount++;
                    }
                    else if (change.changeType == "DELETED") {
                        std::cout << "\033[1;31m[DELETED]\033[0m " << change.filePath << std::endl;
                        deletedCount++;
                    }
                    else if (change.changeType == "CHANGED") {
                        std::cout << "\033[1;33m[CHANGED]\033[0m " << change.filePath << std::endl;
                        changedCount++;
                    }
                }
            }

            std::cout << "-------------------------" << std::endl;
            std::cout << "Summary: " << addedCount << " added, " << deletedCount << " deleted, " << changedCount << " changed" << std::endl;

            // Calculate change ratio for context
            double changeRatio = static_cast<double>(changedFiles.size()) /
                (std::max(currFiles.size(), newFiles.size()) > 0 ?
                    std::max(currFiles.size(), newFiles.size()) : 1) * 100.0;

            std::cout << "Change percentage: " << std::fixed << std::setprecision(2)
                << changeRatio << "% of files affected" << std::endl;
            std::cout.flush();
        }

        // Return true if there are no changes, false if there are changes
        return changedFiles.empty();
    }
    catch (const std::exception& e) {
        std::cout << "\n\033[1;31mUnexpected error during checksum validation: " << e.what() << "\033[0m" << std::endl;
        return false;
    }
}


// Overloaded Implementation
std::vector<FileChangeInfo> validateChecksumFileAndGetChanges(const std::string& currPath, const std::string& newPath) {
    std::vector<FileChangeInfo> changedFiles;
    validateChecksumFile(currPath, newPath, changedFiles);
    return changedFiles;
}

// Overloaded Implementation
bool validateChecksumFile(const std::string& currPath, const std::string& newPath) {
    std::vector<FileChangeInfo> changedFiles;
    return validateChecksumFile(currPath, newPath, changedFiles);
}


// Cross Compatibility for Clearing Console
void clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Exported C-compatible function implementations
#ifdef CHECKSUMHANDLER_EXPORTS
int CreateChecksumFile(const char* path) {
    return createChecksumFile(std::string(path), {});
}

bool ValidateChecksumFile(const char* currPath, const char* newPath) {
    return validateChecksumFile(std::string(currPath), std::string(newPath));
}

int GetChangedFiles(const char* currPath, const char* newPath, char*** filePathsOut, char*** changeTypesOut, int* count) {
    // Validate input parameters
    if (filePathsOut == nullptr || changeTypesOut == nullptr || count == nullptr) {
        return -1; // Invalid parameters
    }

    // Initialize output parameters
    *filePathsOut = nullptr;
    *changeTypesOut = nullptr;
    *count = 0;

    try {
        // Get changed files
        std::vector<FileChangeInfo> changedFiles = validateChecksumFileAndGetChanges(std::string(currPath), std::string(newPath));

        // Set the count
        *count = static_cast<int>(changedFiles.size());

        if (*count == 0) {
            return 0; // No changes, pointers remain null
        }

        // Allocate memory for file paths and change types
        *filePathsOut = (char**)malloc((*count) * sizeof(char*));
        if (*filePathsOut == nullptr) {
            return -2; // Memory allocation failure
        }

        *changeTypesOut = (char**)malloc((*count) * sizeof(char*));
        if (*changeTypesOut == nullptr) {
            free(*filePathsOut);
            *filePathsOut = nullptr;
            return -2; // Memory allocation failure
        }

        // Initialize pointers to null for safe cleanup in case of failure
        for (int i = 0; i < *count; i++) {
            (*filePathsOut)[i] = nullptr;
            (*changeTypesOut)[i] = nullptr;
        }

        // Copy data to output arrays
        for (int i = 0; i < *count; i++) {
            const FileChangeInfo& info = changedFiles[i];

            // Allocate and copy file path
            size_t pathLen = info.filePath.length() + 1;
            (*filePathsOut)[i] = (char*)malloc(pathLen * sizeof(char));
            if ((*filePathsOut)[i] == nullptr) {
                // Cleanup and return error
                FreeChangedFiles(*filePathsOut, *changeTypesOut, *count);
                *filePathsOut = nullptr;
                *changeTypesOut = nullptr;
                *count = 0;
                return -2; // Memory allocation failure
            }
            strcpy_s((*filePathsOut)[i], pathLen, info.filePath.c_str());

            // Allocate and copy change type
            size_t typeLen = info.changeType.length() + 1;
            (*changeTypesOut)[i] = (char*)malloc(typeLen * sizeof(char));
            if ((*changeTypesOut)[i] == nullptr) {
                // Cleanup and return error
                FreeChangedFiles(*filePathsOut, *changeTypesOut, *count);
                *filePathsOut = nullptr;
                *changeTypesOut = nullptr;
                *count = 0;
                return -2; // Memory allocation failure
            }
            strcpy_s((*changeTypesOut)[i], typeLen, info.changeType.c_str());
        }

        return 1; // Success
    }
    catch (const std::exception& e) {
        // Handle any exceptions
        if (*count > 0 && (*filePathsOut != nullptr || *changeTypesOut != nullptr)) {
            FreeChangedFiles(*filePathsOut, *changeTypesOut, *count);
        }
        *filePathsOut = nullptr;
        *changeTypesOut = nullptr;
        *count = 0;
        return -3; // Exception occurred
    }
}

void FreeChangedFiles(char** filePaths, char** changeTypes, int count) {
    if (filePaths != nullptr) {
        for (int i = 0; i < count; i++) {
            if (filePaths[i] != nullptr) {
                free(filePaths[i]);
            }
        }
        free(filePaths);
    }

    if (changeTypes != nullptr) {
        for (int i = 0; i < count; i++) {
            if (changeTypes[i] != nullptr) {
                free(changeTypes[i]);
            }
        }
        free(changeTypes);
    }
}
#endif

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

            std::vector<FileChangeInfo> changedFiles;
            bool result = validateChecksumFile(currPath, newPath, changedFiles);
            return result ? 0 : 1;  // Return 0 if no changes (validation passed), 1 if changes or error
        }

        // Invalid command or insufficient arguments
        else {
            std::cout << "\033[1;31mError: Invalid command or insufficient arguments.\033[0m" << std::endl;
            displayUsage(argv[0]);
            return 1;
        }
    }

   // Interactive Menu Mode
       // Interactive menu mode (no command-line arguments provided)
    // Create Loopable Menu
    while (true) {
        clearConsole();

        // Print Menu
        std::cout << "\033[1;34mChecksum Handler\033[0m\n";
        std::cout << "[1] \033[1;33mCreate Checksum File\033[0m\n";
        std::cout << "[2] \033[1;33mValidate Checksum\033[0m\n";
        std::cout << "[3] \033[1;31mExit\033[0m\n";
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

            // Input Only
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

            std::vector<FileChangeInfo> changedFiles;
            validateChecksumFile(str, strTwo, changedFiles);
            break;
        }

        case 3: {
            // Exit
            return 0;
        }

        default: {
            // Invalid Choice with better error message
            std::cout << "\033[1;31mInvalid Choice: '" << input << "'\033[0m" << std::endl;
            std::cout << "Please enter 1, 2, or 3." << std::endl;
            break;
        }
        }

        // Wait for user input
        std::cout << "\nPress Enter to continue...";
        std::string empty;
        std::getline(std::cin, empty);
    }
}