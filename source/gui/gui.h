#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "GuiLoop.h"
#include <iostream>

#include "loadFonts.h"

void SetupImGuiStyle();
void SetupImGui(GLFWwindow* window);
void glfw_error_callback(int error, const char* description);