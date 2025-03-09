# checksum_handler

A utility for creating and validating file checksums to detect changes in directories.

## Features
- Create Checksum Files: Generate CRC32 checksums for all files in a directory
- Validate Checksums: Compare two checksum files to identify added, modified, or deleted files
- Multiple Interfaces:
	- Command-line interface for scripting
  	- Interactive menu for manual operation
  	- DLL exports for integration with other software

## Usage
### Command-line Interface
```
# Create a checksum file
ChecksumHandler create <folder_path> [exclude_pattern1] [exclude_pattern2] ...

# Compare checksums
ChecksumHandler validate <current_path> <new_path>

# Display help
ChecksumHandler help
```

## Examples

Creating a checksum file:
```
ChecksumHandler create C:\Projects\MyApp
```

Creating a checksum file with exclusions:
```
ChecksumHandler create C:\Projects\MyApp temp .git build
```

Validating changes:
```
ChecksumHandler validate C:\Projects\MyApp\v1 C:\Projects\MyApp\v2
```

## Interactive Menu

Run the program without arguments to enter interactive menu mode:
1. **Create a checksum file**: Generate a new checksum.txt file
	- Enter the Folder Path
	- Optionally specify exclude patterns (comma-separated)
2. **Validate checksums**: Compare two checksum files and identify changes
	- Enter the path to the original directory or checksum file
	- Enter the path to the new directory or checksum file
3. **Exit**: Close the program

## DLL Integration
The following functions are exported for use in other applications:
```c
// Create a checksum file in the specified path
int CreateChecksumFile(const char* path);

// Validate checksums between two paths and return true if no changes are detected
bool ValidateChecksumFile(const char* currPath, const char* newPath);

// Get detailed information about changed files
int GetChangedFiles(const char* currPath, const char* newPath, char*** filePathsOut, char*** changeTypesOut, int* count);

// Free memory allocated by GetChangedFiles
void FreeChangedFiles(char** filePaths, char** changeTypes, int count);
```

## Memoary Management Example
```c
char** filePaths = nullptr;
char** changeTypes = nullptr;
int count = 0;
int result = GetChangedFiles(currPath, newPath, &filePaths, &changeTypes, &count);
if (result > 0) {
  // Process the changes
  // ...
  // Free the memory when done
  FreeChangedFiles(filePaths, changeTypes, count);
}
```

## Change Detection
The program identifies three types of file changes:
- ADDED: Files present in the new directory but not in the original
- DELETED: Files present in the original directory but not in the new one
- CHANGED: Files present in both directories but with different checksums

For large change sets, the output is organized by change type for better readability and includes a change percentage calculation.

## Implementation Details
- Uses CRC32 algorithm for reliable file checksums
- Processes files recursively in directories
- Provides detailed error reporting and progress indicators
- Color-coded console output for better readability
- Cross-platform compatible console clearing

## Return Codes
- 0: Success (no changes for validation)
- 1: Error or changes detected during validation
- 200: Internal success code for checksum creation

## Build Information
- Built with C++20
- Supports both standalone executable and DLL builds
- Uses standard C++ libraries for file system operations
