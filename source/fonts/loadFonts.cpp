
#include "loadFonts.h"

ImFont* g_RubikRegular = nullptr;
ImFont* g_RubikMedium = nullptr;
ImFont* g_RubikLarge = nullptr;

void LoadRubikFont(ImGuiIO& io) {
    const char* rubikFontPath = RUBIK_FONT_PATH;

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
    const char* fontAwesomePath = FONT_AWESOME_PATH;

    io.Fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true; 
    config.PixelSnapH = true;
    
    static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 }; 

    io.Fonts->AddFontFromFileTTF(fontAwesomePath, 16.0f, &config, icons_ranges);
}
