#pragma once

#include <string>
#include <filesystem>
#include <vector>

#ifdef CS_HANDLER_EXPORTS
#define CS_HANDLER_API __declspec(dllexport)
#else
#define CS_HANDLER_API __declspec(dllimport)
#endif

// Define a struct to hold file change information
struct FileChangeInfo {
    std::string filePath;
    std::string changeType;
};

// Calculate checksum for a file
CS_HANDLER_API int calculateFileChecksum(const std::filesystem::path& filePath);

// Create a checksum file
CS_HANDLER_API int createChecksumFile(const std::string& path, const std::vector<std::string>& excludePatterns = {});

// Validate checksum files
CS_HANDLER_API bool validateChecksumFile(const std::string& currPath, const std::string& newPath);

// Function to retrieve changes with return value
CS_HANDLER_API std::vector<FileChangeInfo> getChecksumFileChanges(const std::string& currPath, const std::string& newPath, bool printResults = false);

// Export functions with C linkage
extern "C" {
    CS_HANDLER_API int CalculateChecksum(const char* filePath);
    CS_HANDLER_API int CreateChecksumFile(const char* path);
    CS_HANDLER_API bool ValidateChecksumFile(const char* currPath, const char* newPath);
    CS_HANDLER_API int GetChangedFiles(const char* currPath, const char* newPath, char*** filePathsOut, char*** changeTypesOut, int* count);
    CS_HANDLER_API void FreeChangedFiles(char** filePaths, char** changeTypes, int count);
}