# ============================================================
#  Nebula Engine - Android NDK build (GLES 3.0)
#  Invoked automatically by Gradle via externalNativeBuild.
# ============================================================
cmake_minimum_required(VERSION 3.18.1)

project(NebulaEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(NEBULA_SOURCES
    engine/json/json.cpp
    engine/physics/physics_world.cpp
    engine/scene/scene.cpp
    engine/nebula_engine.cpp
    engine/render/gl_renderer.cpp
    engine/game/nebula_run.cpp
    jni/nebula_jni.cpp
)

add_library(nebula2d SHARED ${NEBULA_SOURCES})

target_include_directories(nebula2d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(nebula2d PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

# GLES3 + Android logging
find_library(log-lib log)
find_library(android-lib android)
find_library(gles3-lib GLESv3)
find_library(egl-lib EGL)

target_link_libraries(nebula2d PRIVATE
    ${log-lib}
    ${android-lib}
    ${gles3-lib}
    ${egl-lib}
)
