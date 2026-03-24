#include <vulkan/vulkan_core.h>
#define VOLK_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <volk/volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "slang/slang.h"
#include "slang/slang-com-ptr.h"

#include <ktx.h>
#include <ktxvulkan.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

VkApplicationInfo appInfo {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Vulakning",
    .apiVersion = VK_API_VERSION_1_3
};

uint32_t instanceExtensionsCount{ 0 };
char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };

VkInstanceCreateInfo instanceCI {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledExtensionCount = instanceExtensionsCount,
    .ppEnabledExtensionNames = instanceExtensions,
};

chk(vkCreateInstance(&instanceCI, nullptr, &instance));

static inline void chk(VKResult result) {
    if (result != VK_SUCCESS) {
        std:cerr << "Vulkan call returned an error (" << result << ")#n";
        exit(result);
    }
}

int main() {
    std::cout << "hello there";
    return 0;
}
