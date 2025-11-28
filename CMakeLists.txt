cmake_minimum_required(VERSION 4.0)
project(Vesper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# set fonts path
set(FONT_AWESOME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fonts/fontawesome-free-6.7.2-desktop/otfs/Font Awesome 6 Free-Solid-900.otf")
add_definitions(-DFONT_AWESOME_PATH="${FONT_AWESOME_PATH}")
set(RUBIK_FONT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fonts/rubik/Rubik-Medium.ttf")
add_definitions(-DRUBIK_FONT_PATH="${RUBIK_FONT_PATH}")

# -----------------------------
# 3rdparty
# -----------------------------
set(THIRD_PARTY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/include")
set(THIRD_PARTY_LIB_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lib")
set(THIRD_PARTY_BIN_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bin")
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

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  kissfft
  GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
  GIT_TAG        master
)

set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
set(KISSFFT_TEST  OFF CACHE BOOL "" FORCE)
set(KISSFFT_OPENMP OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(kissfft)

if(EXISTS "${kissfft_SOURCE_DIR}/test/CMakeLists.txt")
    file(READ "${kissfft_SOURCE_DIR}/test/CMakeLists.txt" content)
    string(REPLACE "pkg_check_modules(FFTW fftw3)" "# pkg_check_modules(FFTW fftw3)" content "${content}")
    string(REPLACE "find_package(PkgConfig REQUIRED)" "# find_package(PkgConfig REQUIRED)" content "${content}")
    file(WRITE "${kissfft_SOURCE_DIR}/test/CMakeLists.txt" "${content}")
endif()

find_package(OpenGL REQUIRED)

# -----------------------------
# Targets
# -----------------------------
add_executable(${PROJECT_NAME} 
    source/core
    source/files 
    source/fonts 
    source/gui 
    source/audio 
    source/tags 
    source/lyrics
    )

target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(OPENAL_INCLUDE_DIR "${THIRD_PARTY_INCLUDE_DIR}/OpenAL")
set(OPENAL_LIBRARY "${THIRD_PARTY_LIB_DIR}/OpenAL/OpenAL32.lib")

set(LIBCURL_LIB ${LIBCURL_LIBRARY_DIR}/libcurl.dll.a)

# Include
target_include_directories(${PROJECT_NAME} PRIVATE
    ${THIRD_PARTY_INCLUDE_DIR} 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${OPENAL_INCLUDE_DIR}    
    ${GLEW_INCLUDE_DIR}
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

target_link_libraries(${PROJECT_NAME}  PRIVATE
    ${OPENAL_LIBRARY}
    ${GLEW_DLL}
    ${LIBCURL_DLL}
    glfw
    OpenGL::GL
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    nlohmann_json::nlohmann_json
    ${LIBCURL_LIB}
    ${KISSFFT_LIBRARY}    
)

# copy postinstall

file(GLOB FFMPEG_DLLS "${FFMPEG_LIBRARY_DIR}/*.dll")
foreach(dll ${FFMPEG_DLLS})
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${dll}"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

if(EXISTS ${GLEW_DLL})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLEW_DLL}"
    "glew32.dll"
    COMMENT "Copying glew32.dll to bin directory")
    else()
    message(FATAL_ERROR "glew32.dll not found at ${GLEW_DLL}. Please ensure it is placed in bin/.")
endif()

if(EXISTS ${LIBCURL_DLL})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${LIBCURL_DLL}"
    "libcurl-x64.dll"
    COMMENT "Copying libcurl-x64.dll to bin directory")
    else()
    message(FATAL_ERROR "libcurl-x64.dll not found at ${LIBCURL_DLL}. Please ensure it is placed in bin/.")
endif()

if(EXISTS 3rdparty/cacert.pem)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "3rdparty/cacert.pem"
    "cacert.pem"
    COMMENT "Copying cacert.pem to bin directory")
    else()
    message(FATAL_ERROR "cacert.pem not found at 3rdparty/cacert.pem. Please ensure it is placed in bin/.")
endif()

install(FILES 
    ${FFMPEG_DLLS}
    ${GLEW_DLL}
    ${LIBCURL_DLL}
    ${THIRD_PARTY_BIN_DIR}/OpenAL32.dll
    3rdparty/cacert.pem
    DESTINATION release
)

add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)

install(TARGETS Vesper DESTINATION release)

file(GLOB EXTRA_DLLS "${THIRD_PARTY_BIN_DIR}/*.dll")

install(FILES ${EXTRA_DLLS}
    DESTINATION release
)



find_program(MINGW_GXX_EXECUTABLE NAMES g++)
if(MINGW_GXX_EXECUTABLE)
    get_filename_component(MINGW_DIR ${MINGW_GXX_EXECUTABLE} DIRECTORY)
    get_filename_component(MINGW_DIR ${MINGW_DIR} DIRECTORY)

    set(MINGW_BIN_DIR "${MINGW_DIR}/bin")

    foreach(lib libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
        if(EXISTS "${MINGW_BIN_DIR}/${lib}")
            install(FILES "${MINGW_BIN_DIR}/${lib}" DESTINATION release)
        endif()
    endforeach()
endif()
