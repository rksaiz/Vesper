#include "getlyrics.h"
#include <string>
#include <filesystem>

std::string GetExecutableDir() {
    char path[MAX_PATH];
    HMODULE hm = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&__FUNCTION__, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    std::string exePath = path;
    return std::filesystem::path(exePath).parent_path().string();
}

std::string exeDir = GetExecutableDir();
std::string caPath = exeDir + "\\cacert.pem";

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string FetchLyrics(const std::string& title, const std::string& artist) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string readBuffer;

    std::string url = std::string("https://lrclib.net/api/search?track_name=") +
                  curl_easy_escape(curl, title.c_str(), 0) +
                  "&artist_name=" +
                  curl_easy_escape(curl, artist.c_str(), 0);

    curl_easy_setopt(curl, CURLOPT_CAINFO, caPath.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vesper (https://github.com/rksaiz/Vesper)");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "Curl error: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    try {
        auto j = json::parse(readBuffer);
        if (j.is_array() && !j.empty()) {
            return j[0].value("plainLyrics", "");
        }
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    return "";
}
