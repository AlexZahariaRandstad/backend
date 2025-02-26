/**
 * @file FileGuard.h
 * @author Alex Zaharia
 * @brief Utility class for file management
 * @version 0.1
 * @date 2025-02-26
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#ifndef FILE_GUARD_H
#define FILE_GUARD_H

#include <string>
#include <filesystem>

/**
 * @class FileGuard
 * @brief A RAII-based class to automatically manage file deletion upon destruction.
 * 
 * The "FileGuard" class ensures that a specified file is deleted 
 * when the object goes out of scope. This approach prevents orphaned 
 * temporary files.
 * 
 * Example usage:
 * When fileGuard goes out of scope, "temp.txt" is automatically deleted.
 * @code
 * {
 *     FileGuard fileGuard("temp.txt");
 * }
 * @endcode
 */
class FileGuard {
public:
    /**
     * @brief Constructs a FileGuard object to manage a specified file.
     * 
     * @param strFilePath The path of the file to be managed.
     */
    explicit FileGuard(const std::string& strFilePath);

    /**
     * @brief Destructor that deletes the managed file if it exists.
     */
    ~FileGuard();

    /**
     * @brief Deleted copy constructor to prevent duplicate management of the same file.
     */
    FileGuard(const FileGuard&) = delete;

    /**
     * @brief Deleted copy assignment operator to prevent multiple instances from managing the same file.
     */
    FileGuard& operator=(const FileGuard&) = delete;

    /**
     * @brief Deleted move constructor
     */
    FileGuard(FileGuard&&) = delete;

    /**
     * @brief Deleted move assignment operator
     */
    FileGuard& operator=(FileGuard&&) = delete;

private:
    std::string mStrFilePath;
};

#endif
