#ifndef FILES_H
#define FILES_H   

#include <shlobj.h> 
#include <shobjidl.h> 
#include <string>
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <codecvt>
#include <locale>
#include <vector>
#include <unordered_map>

#include "readtags.h"
#include "albumArt.h"

struct AudioMetadata {
    std::string title;
    std::string artist;
    std::string album;
    int year;
    std::string date_str;
    std::string plainLyrics; 
    GLuint albumArtTexture = 0;
};

std::string OpenFileDialog();
std::string OpenFolderDialogWithIFileDialog();

std::unordered_map<std::string, AudioMetadata> AddAudioFilesFromDirectory(const std::string& directory);
std::unordered_map<std::string, AudioMetadata> AddAudioFile(const std::string& filePath);

#endif