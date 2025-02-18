#include "FileGuard.h"

FileGuard::FileGuard(const std::string& strFilePath) : mStrFilePath(strFilePath) {}

FileGuard::~FileGuard() {
    if (std::filesystem::exists(mStrFilePath)) {
        std::filesystem::remove(mStrFilePath);
    }
}
