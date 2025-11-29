cmake_minimum_required(VERSION 4.0)
project(Vesper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/release")
# -----------------------------
# 3rdparty
# -----------------------------
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")
set(THIRD_PARTY_INCLUDE_DIR "${THIRD_PARTY_DIR}/include")
set(THIRD_PARTY_LIB_DIR     "${THIRD_PARTY_DIR}/lib")
set(THIRD_PARTY_BIN_DIR     "${THIRD_PARTY_DIR}/bin")

set(GLEW_DLL                "${THIRD_PARTY_BIN_DIR}/glew32.dll")
set(LIBCURL_DLL             "${THIRD_PARTY_BIN_DIR}/libcurl-x64.dll")

set(FFMPEG_INCLUDE_DIR ${THIRD_PARTY_INCLUDE_DIR}/ffmpeg)
set(FFMPEG_LIBRARY_DIR ${THIRD_PARTY_LIB_DIR}/ffmpeg)
set(LIBCURL_INCLUDE_DIR ${THIRD_PARTY_INCLUDE_DIR}/libcurl)
set(LIBCURL_LIBRARY_DIR ${THIRD_PARTY_LIB_DIR}/libcurl)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h PATHS ${FFMPEG_INCLUDE_DIR} NO_DEFAULT_PATH)
find_library(AVCODEC_LIBRARY NAMES avcodec avcodec-61 PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h PATHS ${FFMPEG_INCLUDE_DIR} NO_DEFAULT_PATH)
find_library(AVFORMAT_LIBRARY NAMES avformat avformat-61 PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h PATHS ${FFMPEG_INCLUDE_DIR} NO_DEFAULT_PATH)
find_library(AVUTIL_LIBRARY NAMES avutil avutil-59 PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)
find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h PATHS ${FFMPEG_INCLUDE_DIR} NO_DEFAULT_PATH)
find_library(SWRESAMPLE_LIBRARY NAMES swresample swresample-5 PATHS ${FFMPEG_LIBRARY_DIR} NO_DEFAULT_PATH)

# -----------------------------
# FetchContent
# -----------------------------
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# JSON
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# KISSFFT
FetchContent_Declare(
  kissfft
  GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
  GIT_TAG master
)

set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
set(KISSFFT_TEST  OFF CACHE BOOL "" FORCE)
set(KISSFFT_OPENMP OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(kissfft)

# OpenGL
find_package(OpenGL REQUIRED)

# -----------------------------
# Executable
# -----------------------------
add_executable(${PROJECT_NAME} 
    source/core/main.cpp 
    source/files/files.cpp
    source/fonts/loadFonts.cpp
    source/gui/gui.cpp source/gui/GuiLoop.cpp
    source/audio/AudioManager.cpp source/audio/AudioEngine.cpp
    source/tags/readtags.cpp source/tags/albumArt.cpp
    source/lyrics/getlyrics.cpp
)

# ImGui sources
target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE
    ${THIRD_PARTY_INCLUDE_DIR} 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${THIRD_PARTY_INCLUDE_DIR}/OpenAL
    ${THIRD_PARTY_INCLUDE_DIR}/GLEW
    source/core
    source/files 
    source/fonts 
    source/gui 
    source/audio 
    source/tags 
    source/lyrics
    ${AVCODEC_INCLUDE_DIR}
    ${AVFORMAT_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${SWRESAMPLE_INCLUDE_DIR}
    ${nlohmann_json_SOURCE_DIR}/include/nlohmann
    ${LIBCURL_INCLUDE_DIR}
    ${kissfft_SOURCE_DIR}    
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${THIRD_PARTY_LIB_DIR}/OpenAL/OpenAL32.lib
    ${GLEW_DLL}
    glfw
    OpenGL::GL
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    nlohmann_json::nlohmann_json
    ${THIRD_PARTY_LIB_DIR}/libcurl/libcurl.dll.a
    ${KISSFFT_LIBRARY}
)

# -----------------------------
# Copy DLLs & Fonts after build
# -----------------------------
set(INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/release")

file(MAKE_DIRECTORY "${INSTALL_DIR}")

function(copy_files TARGET FILES)
    foreach(f ${FILES})
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${f}" "${INSTALL_DIR}/"
        )
    endforeach()
endfunction()

# FFMPEG DLLs
set(FFMPEG_DLLS
    "${THIRD_PARTY_BIN_DIR}/avcodec-61.dll"
    "${THIRD_PARTY_BIN_DIR}/avdevice-61.dll"
    "${THIRD_PARTY_BIN_DIR}/avfilter-10.dll"
    "${THIRD_PARTY_BIN_DIR}/avformat-61.dll"
    "${THIRD_PARTY_BIN_DIR}/avutil-59.dll"
    "${THIRD_PARTY_BIN_DIR}/swresample-5.dll"
)

foreach(dll ${FFMPEG_DLLS})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${dll}" "${INSTALL_DIR}/"
    )
endforeach()

# GLEW / OpenAL
copy_files(${PROJECT_NAME} ${GLEW_DLL} "${THIRD_PARTY_BIN_DIR}/OpenAL32.dll")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${LIBCURL_DLL}" "${INSTALL_DIR}/"
)

# fonts
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
    "${INSTALL_DIR}/fonts"
)

# Curl cert
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cacert.pem"
    "${INSTALL_DIR}/"
)

# Install exe
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_DIR})

# -----------------------------
# copy MinGW dlls
# -----------------------------
find_program(MINGW_GXX_EXECUTABLE NAMES g++)
if(MINGW_GXX_EXECUTABLE)
    get_filename_component(MINGW_DIR ${MINGW_GXX_EXECUTABLE} DIRECTORY)
    get_filename_component(MINGW_DIR ${MINGW_DIR} DIRECTORY)
    set(MINGW_BIN_DIR "${MINGW_DIR}/bin")
    foreach(lib libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libsoxr.dll libgomp-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${lib}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${lib}" "${INSTALL_DIR}/"
            )
        endif()
    endforeach()
endif()
