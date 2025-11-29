
#include "loadFonts.h"

ImFont* g_RubikRegular = nullptr;
ImFont* g_RubikMedium = nullptr;
ImFont* g_RubikLarge = nullptr;

std::string getExeDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    return path.substr(0, path.find_last_of("\\/"));
}

std::string getFontPath(const std::string& relativePath) {
    return getExeDir() + "\\" + relativePath;
}

void LoadRubikFont(ImGuiIO& io) {
    std::string rubikFontPathStr = getFontPath("fonts\\rubik\\Rubik-Medium.ttf");
    const char* rubikFontPath = rubikFontPathStr.c_str();

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 1;
    config.PixelSnapH = true;

    static const ImWchar rubik_ranges[] = { 0x0020, 0x00FF, 0x0400, 0x052F, 0 };

    g_RubikRegular = io.Fonts->AddFontFromFileTTF(rubikFontPath, 16.0f, &config, rubik_ranges);
    if (!g_RubikRegular)
        std::cerr << "Failed to load Rubik font (regular)" << std::endl;

    g_RubikMedium = io.Fonts->AddFontFromFileTTF(rubikFontPath, 18.0f, &config, rubik_ranges);
    if (!g_RubikMedium)
        std::cerr << "Failed to load Rubik font (medium)" << std::endl;

    g_RubikLarge = io.Fonts->AddFontFromFileTTF(rubikFontPath, 28.0f, &config, rubik_ranges);
    if (!g_RubikLarge)
        std::cerr << "Failed to load Rubik font (large)" << std::endl;
}


void LoadFontAwesome(ImGuiIO& io)
{
    std::string fontAwesomePathStr = getFontPath("fonts\\fontawesome-free-6.7.2-desktop\\otfs\\Font Awesome 6 Free-Solid-900.otf");
    const char* fontAwesomePath = fontAwesomePathStr.c_str();

    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true; 
    config.PixelSnapH = true;
    
    static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 }; 

    if (!io.Fonts->AddFontFromFileTTF(fontAwesomePath, 16.0f, &config, icons_ranges))
        std::cerr << "Failed to load FontAwesome" << std::endl;
}
