
#include "AudioManager.h"
#include "files.h"
#include <algorithm>

void AudioManager::AddFilesFromDirectory(const std::string& directory) {
    auto newMetadata = ::AddAudioFilesFromDirectory(directory);
    for (auto& [path, meta] : newMetadata) {
        if (std::find(audioFiles.begin(), audioFiles.end(), path) == audioFiles.end()) {
            audioFiles.push_back(path);
            metadataCache[path] = meta;
        }
    }
}
void AudioManager::AddFile(const std::string& filePath) {
    auto newMetadata = ::AddAudioFile(filePath);
    for (auto& [path, meta] : newMetadata) {
        if (std::find(audioFiles.begin(), audioFiles.end(), path) == audioFiles.end()) {
            audioFiles.push_back(path);
            metadataCache[path] = meta;
        }
    }
}