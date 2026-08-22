cmake_minimum_required(VERSION 3.20)
project(SimpleChat)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)

# === FETCH_CONTENT ===
set(LIB_FOLDER_NAME Libraries)
set(INCLUDE_FOLDER_NAME Include)
set(LIB_LINK_FOLDER_NAME Lib)
set(FETCHED_CONTENT_FOLDER_NAME External)

#cmake varibales:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/Run)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/Run)

set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${FETCHED_CONTENT_FOLDER_NAME})
set(BUILD_STATIC_EXECUTEABLE OFF)

set(CMAKE_STRICT_STANDARD ON)

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

# === asio ===
FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-30-0
)
FetchContent_MakeAvailable(asio)

set(EXTERNAL_INCLUDE_PATHS
   ${asio_SOURCE_DIR}/asio/include
)

#vcpkg:
# find_package(lib CONFIG REQUIRED)

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
   ${EXTERNAL_INCLUDE_PATHS}
   ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${INCLUDE_FOLDER_NAME}
)

link_directories(
   ${CMAKE_SOURCE_DIR}/${LIB_FOLDER_NAME}/${LIB_LINK_FOLDER_NAME}
)

add_executable(Client
   ClientMain.cpp
   SRC/Networking/StreamedNet.cpp
)

add_executable(Server
   ServerMain.cpp
)

if(WIN32)
   target_link_libraries(Client PRIVATE
      ws2_32
      mswsock
   )
   target_compile_definitions(Client PRIVATE ASIO_STANDALONE)

   target_link_libraries(Server PRIVATE 
      ws2_32
      mswsock
   )
   target_compile_definitions(Server PRIVATE ASIO_STANDALONE) #NetStream_Enable_Log
endif()
