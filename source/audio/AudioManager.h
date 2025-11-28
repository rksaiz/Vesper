#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "files.h"

class AudioManager {
public:
    void AddFilesFromDirectory(const std::string& directory);
    void AddFile(const std::string& filePath);
    const std::vector<std::string>& GetAudioFiles() const { return audioFiles; }
    const std::unordered_map<std::string, AudioMetadata>& GetMetadataCache() const { return metadataCache; }

private:
    std::vector<std::string> audioFiles;
    std::unordered_map<std::string, AudioMetadata> metadataCache;
};