cmake_minimum_required(VERSION 3.20)
project(SimpleChat)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === FETCH_CONTENT ===
set(LIB_FOLDER_NAME Libraries)
set(INCLUDE_FOLDER_NAME Include)
set(LIB_LINK_FOLDER_NAME Lib)
set(FETCHED_CONTENT_FOLDER_NAME External/Fetch)

#cmake varibales:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/Run)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/Run)

set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${FETCHED_CONTENT_FOLDER_NAME})
set(BUILD_STATIC_EXECUTEABLE ON)

set(CMAKE_STRICT_STANDARD ON)
option(SIMPLECHAT_BUILD_UI "Build the SimpleChat UI application" ON)

#preConfig
if(CMAKE_STRICT_STANDARD)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)
   set(CMAKE_CXX_EXTENSIONS OFF)
   if (MSVC)
      # Strict ISO mode for MSVC
      add_compile_options(
         /permissive-
         /W3
      )
   else()
      # Strict ISO mode for GCC & Clang
      add_compile_options(
         -std=c++20
         -pedantic-errors
         # -Wall
      )
   endif()
endif()

#fetchExternals
# === WebView ===
if(SIMPLECHAT_BUILD_UI)
   FetchContent_Declare(
      webview
      GIT_REPOSITORY https://github.com/webview/webview
      GIT_TAG 0.12.0
   )

   set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
   set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
   set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
   set(WEBVIEW_BUILD_AMALGAMATION OFF CACHE BOOL "" FORCE)

   FetchContent_Declare(
      webview2
      URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
   )

   FetchContent_MakeAvailable(webview webview2)
endif()

#vcpkg:
# find_package(lib CONFIG REQUIRED)

#subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/External/NCore)

# Test
enable_testing()

# static build
if(BUILD_STATIC_EXECUTEABLE MATCHES ON) 
   if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
      set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
   elseif(MSVC)
      # MultiThreaded (Release) / MultiThreadedDebug (Debug)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
   endif()
endif()

include_directories(
   ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${INCLUDE_FOLDER_NAME}
)

link_directories(
   ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${LIB_LINK_FOLDER_NAME}
)

add_executable(CLIMain
   CLIMain.cpp
)

add_executable(Tests
   Test/main.cpp
)

add_executable(UIMain
   UIMain.cpp
)

if(WIN32)
   target_link_libraries(CLIMain PRIVATE NCore)

   target_include_directories(UIMain PRIVATE
      ${webview2_SOURCE_DIR}/build/native/include
   )

   target_link_libraries(UIMain PRIVATE
      webview::core
      NCore
   )

   target_link_libraries(Tests PRIVATE NCore)
   # target_compile_definitions(Tests PRIVATE NetStream_Enable_Log)
endif()

add_test(NAME Tests COMMAND Tests)

