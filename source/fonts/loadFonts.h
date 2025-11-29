#ifndef LOADFONTS_H
#define LOADFONTS_H
#include "imgui.h"
#include <iostream>
#include <windows.h>
#include <string>
#include <filesystem>

extern ImFont* g_RubikRegular;
extern ImFont* g_RubikMedium;
extern ImFont* g_RubikLarge;

void LoadRubikFont(ImGuiIO& io);
void LoadFontAwesome(ImGuiIO& io);

#endif