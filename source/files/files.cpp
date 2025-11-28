#include "files.h"
#include <iostream>
#include <algorithm>
#include <set>

bool IsSupportedAudioFile(const std::filesystem::path& path) {
    static const std::set<std::string> supportedExtensions = {
        ".mp3", ".wav", ".flac", ".m4a", ".ogg", ".aac"
    };
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return supportedExtensions.find(ext) != supportedExtensions.end();
}

std::string OpenFileDialog() {
    wchar_t filePath[MAX_PATH] = { 0 };

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter =
        L"All Audio Files (*.mp3;*.wav;*.flac;*.ogg;*.aac;*.m4a;*.opus)\0*.mp3;*.wav;*.flac;*.ogg;*.aac;*.m4a;*.opus\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = L"Select an Audio File";

    if (!GetOpenFileNameW(&ofn)) {
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            std::cerr << "GetOpenFileNameW failed with error: " << error << std::endl;
        }
        return "";
    }

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(filePath);
}

std::string OpenFolderDialogWithIFileDialog() {
    std::string folderPath;
    IFileDialog* pFileDialog = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pFileDialog->GetOptions(&dwOptions);
        pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);

        hr = pFileDialog->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileDialog->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                    folderPath = converter.to_bytes(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
            pFileDialog->Release();
        }
    }
    return folderPath;
}

std::unordered_map<std::string, AudioMetadata> AddAudioFilesFromDirectory(const std::string& directory) {
    std::unordered_map<std::string, AudioMetadata> metadataMap;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (IsSupportedAudioFile(entry.path())) {
                std::string path = entry.path().u8string();

                if (metadataMap.find(path) == metadataMap.end()) {
                    std::string title, artist, album, date_str;
                    int year;
                    ReadAudioTags(path.c_str(), &title, &artist, &album, &year, &date_str);

                    metadataMap[path] = { title, artist, album, year, date_str };
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    return metadataMap;
}

std::unordered_map<std::string, AudioMetadata> AddAudioFile(const std::string& filePath) {
    std::unordered_map<std::string, AudioMetadata> metadataMap;

    try {
        std::filesystem::path path(filePath);
        if (IsSupportedAudioFile(path)) {
            std::string pathStr = path.u8string();

            if (metadataMap.find(pathStr) == metadataMap.end()) {
                std::string title, artist, album, date_str;
                int year;
                ReadAudioTags(pathStr.c_str(), &title, &artist, &album, &year, &date_str);

                metadataMap[pathStr] = { title, artist, album, year, date_str };
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    return metadataMap;
}
