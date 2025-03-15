#include "pch.h"
#include "checksum.h"
#include <iostream>
#include <fstream>
#include <map>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>  // Add this for OpenSSL 3.0+ EVP interface
#include <openssl/err.h>  // Add this for error handling
#include <openssl/opensslv.h> // Add this to check OpenSSL version

extern int createChecksumFile(const std::string& path, const std::vector<std::string>& excludePatterns);
extern bool validateChecksumFile(const std::string& currPath, const std::string& newPath);
extern std::vector<FileChangeInfo> getChecksumFileChanges(const std::string& currPath, const std::string& newPath, bool printResults);


// Internal C++ Function implementation
int calculateFileChecksum(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cout << "\n\033[1;33mWarning: Unable to open file for checksum: " << filePath << "\033[0m" << std::endl;
        return -1;
    }

    constexpr size_t bufferSize = 8192;  // 8KB Buffer
    unsigned char buffer[bufferSize];
    unsigned char hash[SHA256_DIGEST_LENGTH];

    // OpenSSL 3.0+ compatible implementation
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // Use the EVP interface which is preferred in OpenSSL 3.0+
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cout << "\n\033[1;31mError: Could not create message digest context\033[0m" << std::endl;
        return -1;
    }

    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
        EVP_MD_CTX_free(mdctx);
        std::cout << "\n\033[1;31mError: Could not initialize digest\033[0m" << std::endl;
        return -1;
    }

    while (file.read(reinterpret_cast<char*>(buffer), bufferSize)) {
        if (1 != EVP_DigestUpdate(mdctx, buffer, file.gcount())) {
            EVP_MD_CTX_free(mdctx);
            std::cout << "\n\033[1;31mError: Could not update digest\033[0m" << std::endl;
            return -1;
        }
    }

    // Process remaining bytes
    if (file.gcount() > 0) {
        if (1 != EVP_DigestUpdate(mdctx, buffer, file.gcount())) {
            EVP_MD_CTX_free(mdctx);
            std::cout << "\n\033[1;31mError: Could not update digest with remaining bytes\033[0m" << std::endl;
            return -1;
        }
    }

    unsigned int len = SHA256_DIGEST_LENGTH;
    if (1 != EVP_DigestFinal_ex(mdctx, hash, &len)) {
        EVP_MD_CTX_free(mdctx);
        std::cout << "\n\033[1;31mError: Could not finalize digest\033[0m" << std::endl;
        return -1;
    }

    EVP_MD_CTX_free(mdctx);
#else
    // Legacy implementation for OpenSSL < 3.0
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    while (file.read(reinterpret_cast<char*>(buffer), bufferSize)) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    // Process remaining bytes
    if (file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    SHA256_Final(hash, &sha256);
#endif

    int result = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | hash[3];
    return result;
}

// C-compatible exported function implementation
extern "C" {
    int CalculateChecksum(const char* filePath) {
        return calculateFileChecksum(std::filesystem::path(filePath));
    }
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
    std::filesystem::path basePath = std::filesystem::absolute(path);

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
                    // relative path
                    std::filesystem::path relativePath = std::filesystem::relative(filePath, basePath);
                    checksumFile << relativePath.string() << " " << checksum << std::endl;

                    if (checksumFile.fail()) {
                        std::cout << "\n\033[1;33mWarning: Failed to write checksum for: " << filePath.string() << "\033[0m" << std::endl;
                        errorCount++;
                    }
                    else {
                        fileCount++;
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
                ((std::max)(currFiles.size(), newFiles.size()) > 0 ?
                    (std::max)(currFiles.size(), newFiles.size()) : 1) * 100.0;

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


// Overload Implementations
bool validateChecksumFile(const std::string& currPath, const std::string& newPath) {
    std::vector<FileChangeInfo> changedFiles;
    return validateChecksumFile(currPath, newPath, changedFiles);
}


std::vector<FileChangeInfo> getChecksumFileChanges(const std::string& currPath, const std::string& newPath, bool printResults) {
    std::vector<FileChangeInfo> changes;
    validateChecksumFile(currPath, newPath, changes);
    return changes;
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
        // Get changed files - FIXED VERSION
        std::vector<FileChangeInfo> changedFiles;
        getChecksumFileChanges(std::string(currPath), std::string(newPath), changedFiles, false);

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

            // Use memcpy instead of strcpy_s
            memcpy((*filePathsOut)[i], info.filePath.c_str(), pathLen);

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

            // Use memcpy instead of strcpy_s
            memcpy((*changeTypesOut)[i], info.changeType.c_str(), typeLen);
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