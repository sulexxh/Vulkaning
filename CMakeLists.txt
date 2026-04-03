cmake_minimum_required(VERSION 3.23)

project(Vulkaning LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/app")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/app")
set(CMAKE_CURRENT_BINARY_DIR "${PROJECT_SOURCE_DIR}/bin/app")

# SDL3 Compilation flags
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)

set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER ON CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
set(SDL_HAPTIC ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
set(SDL_SENSOR ON CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_ASSEMBLY OFF CACHE BOOL "" FORCE)

# KTX Compilation flags
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)

# Add libraries
add_subdirectory(lib/SDL3 EXCLUDE_FROM_ALL)
add_subdirectory(lib/GLM EXCLUDE_FROM_ALL)
add_subdirectory(lib/vma EXCLUDE_FROM_ALL)
add_subdirectory(lib/vk-bootstrap EXCLUDE_FROM_ALL)
add_subdirectory(lib/TinyObjLoader EXCLUDE_FROM_ALL)
add_subdirectory(lib/volk EXCLUDE_FROM_ALL)
add_subdirectory(lib/ktx EXCLUDE_FROM_ALL)
find_package(slang REQUIRED)
find_package(Vulkan REQUIRED)

# Enable Wayland for Vulkan
if (LINUX)
    set(CMAKE_CXX_FLAG "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WAYLAND_KHR")
endif()

# Create final executable
add_executable(Vulkaning)

# Add source code to executable
target_include_directories(Vulkaning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Vulkaning PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_sources(Vulkaning PRIVATE
    src/main.cpp
    src/vkEngine.cpp        src/vkEngine.h
)

# Make the executable look for shared libraries in its own directory
if (APPLE)
    set_target_properties(Vulkaning PROPERTIES
        BUILD_RPATH "@loader_path"
    )
elseif (UNIX)
    set_target_properties(Vulkaning PROPERTIES
        BUILD_RPATH "$ORIGIN"
    )
endif()

# Link libraries to executable
target_link_libraries(Vulkaning PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    VulkanMemoryAllocator
    vk-bootstrap
    volk
    glm
    tinyobjloader
    slang::slang
    ktx
)

# Move some shared libraries and files to the same folder as the executable
add_custom_command(TARGET Vulkaning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:Vulkaning>
)

add_custom_command(TARGET Vulkaning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:vk-bootstrap>
        $<TARGET_FILE_DIR:Vulkaning>
)

add_custom_command(TARGET Vulkaning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ktx>
        $<TARGET_FILE_DIR:Vulkaning>
)

add_custom_command(TARGET Vulkaning POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_SOURCE_DIR}/res
        ${CMAKE_CURRENT_BINARY_DIR}/res
)

# Enable clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
