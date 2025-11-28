#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <thread>
#include <atomic>
#include <optional>

#include "files.h"
#include "audiomanager.h"
#include "getlyrics.h"
#include "loadFonts.h"
#include "albumArt.h"
#include "AudioEngine.h"

void GuiLoop(GLFWwindow* window);