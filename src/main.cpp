#ifndef VOLK_IMPLEMENTATION
#define VOLK_IMPLEMENTATION
#endif
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>

#include <volk/volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>

#ifndef VMA_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#endif
#include <vk_mem_alloc.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <slang.h>
#include <slang-com-ptr.h>

#ifndef TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#endif
#include <tiny_obj_loader.h>

#include <ktx.h>
#include <ktxvulkan.h>

bool updateSwapchain{ false };

// Basic checking functions
static inline void chk(VkResult result) {
    if (result != VK_SUCCESS) {
        std::cerr << "Vulkan call returned an error (" << result << ")\n";
        exit(result);
    }
}

static inline void chkSwapchain(VkResult result) {
    if (result < VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            updateSwapchain = true;
            return;
        }

        std::cerr << "Vulkan call returned an error (" << result << ")\n";
        exit(result);
    }
}

static inline void chk(bool result) {
    if (result != true) {
        std::cerr << "Call returned an error (" << result << ")\n";
        exit(result);
    }
}

int main(int argc, char* argv[]) {
    // SDL and Volk Initialisation
    chk(SDL_Init(SDL_INIT_VIDEO));
    chk(SDL_Vulkan_LoadLibrary(NULL));
    volkInitialize();

    // Information about the application
    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkaning",
        .apiVersion = VK_API_VERSION_1_3
    };

    // Creating and loading an instance of Vulkan, with the help of SDL3 to get platform specific extensions sorted
    /*
        An "implementation" of Vulkan is something that implements the Vulkan API, which could technically be anything,
        but is usually about GPUs.
    */
    VkInstance instance{ VK_NULL_HANDLE };
    uint32_t instanceExtensionsCount{ 0 };
    char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };
    
    VkInstanceCreateInfo instanceCI {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = instanceExtensionsCount,
        .ppEnabledExtensionNames = instanceExtensions,
    };

    chk(vkCreateInstance(&instanceCI, nullptr, &instance));
    volkLoadInstance(instance);

    // Getting a list of devices that are Vulkan capable
    /*
        Calling lists twice is common for Vulkan as it is a C-API. The first call here is for the amount of devices and
        the second call is for the actual data of those devices.
    */
    uint32_t deviceCount{ 0 };
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
    std::vector<VkPhysicalDevice> devices(deviceCount);
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    // Commandline argument to switch Vulkan devices
    uint32_t deviceIndex{ 0 };
    if (argc > 1) {
        deviceIndex = std::stoi(argv[1]);
        assert(deviceIndex < deviceCount);
    }

    // This displays information about the selected device.
    /*
        The number suffix dictates that this is a new version of a function/variable. The old one cannot be directly 
        replaced as it would break API compatibility.
    */
    VkPhysicalDeviceProperties2 deviceProperties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::cout << "Selected device: " << deviceProperties.properties.deviceName << '\n';

    // Finding a graphics queue
    /*
        Work gets submitted to a Queue instead of directly to a device. Queues are organised into Queue Families, with
        each family describing a set of queues with common functionality. We are doing graphics work so we just need
        the first queue family with graphics support.
        In some cases, a device could have graphics, compute, etc in different Queue Families, in which case you'd need
        to synchronise between the Queues. Also, devices without a Queue Family supporting graphics are very rare.
    */
    VkQueue queue{ VK_NULL_HANDLE };
    uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());

    uint32_t queueFamily{ 0 };
    for (size_t i{ 0 }; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamily = i;
            break;
        }
    }

    chk(SDL_Vulkan_GetPresentationSupport(instance, devices[deviceIndex], queueFamily));

    // Referencing the found queue family
    const float qfpriorities{ 1.0f };
    VkDeviceQueueCreateInfo queueCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &qfpriorities,
    };

    // Setup logical device
    /*
        Vulkan differenetiates between physical and logical devices. The former is the actual device, and the latter is
        a handle to the device's Vulkan implementation which the application will interact with.
        Requesting the features and extensions we want to use is important. As we are on Vulkan 1.3 we only need to
        request VK_KHR_swapchain to present something to the screen.
    */
    const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    /*
        pNext is a common struct member, which allows creating a linked list of structures that are passed into a
        function call. The sType member of each structure helps indentify each structure's type.
    */
    VkPhysicalDeviceVulkan12Features enabledVk12Features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true,
    };

    const VkPhysicalDeviceVulkan13Features enabledVk13Features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    const VkPhysicalDeviceFeatures enabledVk10Features {
        .samplerAnisotropy = VK_TRUE,
    };

    // Create logical device
    VkDevice device{ VK_NULL_HANDLE };
    VkDeviceCreateInfo deviceCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledVk13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &enabledVk10Features,
    };

    chk(vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &device));

    // Ask for a queue from the logical device
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    // Setup the VulkanMemoryAllocator (VMA)
    /*
        VMA provides an allocator used to allocate memory from. This only needs to be done once. We just need to pass
        in pointers to some common Vulkan functions, our Vulkan instance and device (we also enable support for buffer
        device address (flags)).
    */
    VmaAllocator allocator{ VK_NULL_HANDLE };
    VmaVulkanFunctions vkFunctions {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage,
    };

    VmaAllocatorCreateInfo allocatorCI {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = devices[deviceIndex],
        .device = device,
        .pVulkanFunctions = &vkFunctions,
        .instance = instance,
    };

    chk(vmaCreateAllocator(&allocatorCI, &allocator));

    // Creating a window and surface
    /*
        Creating these is platform specific, but SDL allows us to do it in a more cross-platform manner.
    */
    VkSurfaceKHR surface{ VK_NULL_HANDLE };
    SDL_Window* window = SDL_CreateWindow("Vulkaning", 1280u, 720u, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    chk(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));
    glm::ivec2 windowSize{};
    chk(SDL_GetWindowSize(window, &windowSize.x, &windowSize.y));

    // Get the properties of the surface that was just made
    VkSurfaceCapabilitiesKHR surfaceCaps{};
    chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[deviceIndex], surface, &surfaceCaps));

    // Finding the swapchain extent
    /*
        A swapchain is a series of images storing colour info that you enqueue to the presentation engine of the
        operating system. You first need to get the extent of the swapchain, but on Wayland this has the special value
        0xFFFFFFFF, which indicates the surface size will basically be determined by the size of the surface.
    */
    VkSwapchainKHR swapchain{ VK_NULL_HANDLE };
    VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };

    // Wayland specific code
    if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
        swapchainExtent = {
            .width = static_cast<uint32_t>(windowSize.x),
            .height = static_cast<uint32_t>(windowSize.y),
        };
    }

    // Creating a swapchain
    /*
        Colour format VK_FORMAT_B8G8R8A8_SRGB in the colour space VK_COLORSPACE_SRGB_NONLINEAR_KHR is guaranteed to be
        available everywhere. Anything else would require checks for support. minImageCount differs between GPUs which
        is why we provide the GPUs info collected from the surface. presentMode defines the way images are presented to
        the screen. VK_PRESENT_MODE_FIFO_KHR is a V-synced mode and is the only mode available everywhere.
    */
    const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
    VkSwapchainCreateInfoKHR swapchainCI {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = surfaceCaps.minImageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent {
            .width = swapchainExtent.width,
            .height = swapchainExtent.height,
        },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
    };

    chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

    // Requesting images from the swapchain
    /*
        Images are owned by the swapchain, not the application. As such, instead of creating the images on our own, we
        request them from the swapchain, getting at least as many images as set in minImageCount.
    */
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    uint32_t imageCount{ 0 };

    chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    swapchainImages.resize(imageCount);
    chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));
    swapchainImageViews.resize(imageCount);

    for (int i{ 0 }; i < imageCount; i++) {
        VkImageViewCreateInfo viewCI {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageFormat,
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        chk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
    }

    // Depth Attachment
    /*
        To render 3D objects properly we need to use depth testing. In order to depth test we need a depth attachment.
        VK_FORMAT_D32_SFLOAT_S8_UINT or VK_FORMAT_D24_UNORM_S8_UINT must be supported for a device at a minimum.
    */
    VkImage depthImage;
    std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
    for (VkFormat& format : depthFormatList) {
        VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(devices[deviceIndex], format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFormat = format;
            break;
        }
    }

    assert(depthFormat != VK_FORMAT_UNDEFINED);

    // Getting depth image information
    /*
        VM_IMAGE_TILING_OPTIMAL makes sure the image is stored optimally for the GPU. initialLayout is irrelevent to us
        so it's undefined. We need to set our use cases for the image, which we set to 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, since we will use it for depth attachment for the final render
        output.
    */
    VkImageCreateInfo depthImageCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent {
            .width = static_cast<uint32_t>(windowSize.x),
            .height = static_cast<uint32_t>(windowSize.y),
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    // Allocate memory for the depth image
    /*
        Memory allocation for images and buffers in Vulkan is very verbose but often very similar to VMA. VMA also
        handles selecting the correct memory types and usage flags via VMA_MEMORY_USAGE_AUTO, but you could do it 
        yourself if needed.  VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT tells VMA to create a separate memory 
        allocation for a resource, which is recommended for big images for example.
        We only need a single image even if we do double buffering as only the GPU accesses the image and the GPU can
        only ever write to a single depth image at a time.
    */
    VmaAllocation depthImageAllocation;
    VmaAllocationCreateInfo allocCI {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    chk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));

    // Create a depth view
    /*
        Images in Vulkan are accessed through views, giving more flexibility and access patterns for the same image. 
        You could  theoretically separate image views for images with multiple layers, with each one accessing an image
        in different ways. subresourceRange specifies the part of the image we want to access via the view, and 
        aspectMask refers to what part of the image we want to access.
    */
    VkImageView depthImageView;
    VkImageViewCreateInfo depthViewCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    chk(vkCreateImageView(device, &depthViewCI, nullptr, &depthImageView));

    // Mesh data for a 3D model
    /*
        Tinyobjloader loads the .obj files for us. Obj is an outdated format but it is very simple so it will work for
        our usecase.
    */
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    // Get the paths to loading the .obj and .mtl from any path
    std::string objDir = SDL_GetBasePath();
    std::string mtlDir = SDL_GetBasePath();
    objDir += "res/Joker.obj";
    mtlDir += "res/";

    std::string objWarn;
    std::string objErr;
    bool objSuccess = tinyobj::LoadObj(&attrib, &shapes, &materials, &objWarn, &objErr, objDir.c_str(), mtlDir.c_str());

    if (!objSuccess) {
        std::cout << "TinyObj returned an error: (" << objErr << ")\n";
        exit(1);
    } else if (!objWarn.empty()) {
        std::cout << "TinyObj warnings: " << objWarn <<'\n';
    }

    // Load vertex and index data
    /*
        To use interleaved vertex attributes (3 floats of position follows by normal vector and uv etc), we convert the
        values provided by tinyobj.
        Vulkan flips the y-axis so we must flip it ourselves so thing don't appear upside down.
    */
    const VkDeviceSize indexCount{ shapes[0].mesh.indices.size() };
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};

    for (auto& index : shapes[0].mesh.indices) {
        Vertex v {
            .pos = {
                attrib.vertices[index.vertex_index * 3], 
                -attrib.vertices[index.vertex_index * 3 + 1],
                attrib.vertices[index.vertex_index * 3 + 2],
            },
            .normal = {
                attrib.normals[index.normal_index * 3], 
                -attrib.normals[index.normal_index * 3 + 1],
                attrib.normals[index.normal_index * 3 + 2],
            },
            .uv = {
                attrib.texcoords[index.texcoord_index * 2], 
                1.0 - attrib.texcoords[index.texcoord_index * 2 + 1],
            },
        };

        vertices.push_back(v);
        indices.push_back(indices.size());
    }

    // Upload interleaved data to the GPU
    /*
        We are putting both vertices and indices into the same buffer, so that is why we added them.
    */
    VkDeviceSize vBufSize{ sizeof(Vertex) * vertices.size() };
    VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
    VkBufferCreateInfo bufferCI {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vBufSize + iBufSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    };

    // Upload this data to the GPU
    /*
        The flags VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT and
        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT allows us to get a memeory type that is located in
        the GPU and also accessible by the host. This used to only be possible on systems where the CPU and GPU memory
        were unified, but thanks to ReBAR dedicated GPUs can do this too.
        Without this we'd need to create a "staging" buffer on the host, copy data to that buffer and then submit a
        buffer copy from staging to the GPU side buffer using a command buffer. (Way more code.)
    */
    VmaAllocation vBufferAllocation{ VK_NULL_HANDLE };
    VkBuffer vBuffer{ VK_NULL_HANDLE };
    VmaAllocationCreateInfo vBufferAllocCI {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo vBufferAllocInfo{};
    chk(vmaCreateBuffer(allocator, &bufferCI, &vBufferAllocCI, &vBuffer, &vBufferAllocation, &vBufferAllocInfo));

    // Copying data into VRAM
    /*
        The buffer is persistently mapped thanks to VMA_ALLOCATION_CREATE_MAPPED_BIT, so it's a simple copy.
    */
    memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufSize);
    memcpy(((char*)vBufferAllocInfo.pMappedData) + vBufSize, indices.data(), iBufSize);

    // Frames in Flight
    /*
        For better performance we can use  Vulkan to make the CPU and GPU work in parallel when possible. So while the
        GPU is busy, the CPU can work on the next package.
        A preresquisite for that is to multiply resource shared by the CPU and GPU. This is what frames in flight are
        for. More frames means more latency, so you need a balance between parallelism and latency. We use it as a
        dimensions for all future resources that are shared.
    */
    constexpr uint32_t maxFramesInFlight{ 2 };

    // Shader data
    /*
        Stored in a single structure and laid out consecutively, so it can easily be copied to the matching GPU
        structure. Depending on the data types and arrangement used, layouts can look the same but actaully be
        different due to how shading languages align struct members. Other than manually aligning/padding structures,
        you could use Vulkans VK_EXT_scalar_block_layout or the Vulkan 1.2 core feature to help this.
    */
    struct ShaderData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model[3];
        glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
        uint32_t selected{ 1 };
    };
    
    ShaderData shaderData{};

    // Shader data buffers
    /*
        We want to add data to our shaders from the CPU side. For this we can create buffers that can be written by the
        CPU and read by the GPU. The data in these buffers stays constant across all shader invoations for a drawcall -
        an important guarantee for the GPU.
    */
    struct ShaderDataBuffer {
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VmaAllocationInfo allocationInfo{};
        VkBuffer buffer{ VK_NULL_HANDLE };
        VkDeviceAddress deviceAddress{};
    };

    // Accessing buffers
    /*
        Previously to access buffers you would need to use descriptors, which were complicated and slightly limiting,
        but in Vulkan 1.3 you can now use Buffer Device Addressing. You can access buffers via their address in the
        shader via pointer syntax.
        We need to make one shader data buffer per frame in flight, as mentioned before, and creation is similar to the
        previous buffer.
        Buffers for smaller amounts of data don't have to be stored in the GPU's VRAM, as it won't have that much of a
        performance impact.
    */
    std::array<ShaderDataBuffer, maxFramesInFlight> shaderDataBuffers;

    for (auto i{ 0 }; i < maxFramesInFlight; i++) {
        VkBufferCreateInfo uBufferCI {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(ShaderData),
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        };

        VmaAllocationCreateInfo uBufferAllocCI {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        chk(vmaCreateBuffer(allocator, &uBufferCI, &uBufferAllocCI, &shaderDataBuffers[i].buffer, &shaderDataBuffers[i].allocation, &shaderDataBuffers[i].allocationInfo));

        /*
            To access the buffer in a shader, we get and store its device address
        */
        VkBufferDeviceAddressInfo uBufferBdaInfo {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = shaderDataBuffers[i].buffer,
        };

        shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &uBufferBdaInfo);
    }

    // Synchronisation objects
    /*
        Unlike other APIs wse need to make sure that access to GPU resources are properly guarded to avoid read/write
        hazards (e.g. CPU writing to memory in use by the GPU). This is similar to multithreading but even harder since
        you are dealing between 2 different pieces of processors. Validation layers are helpful here.
        There are many types of synchronisations. We will be using:
        Fences - signaling work is done from GPU to CPU so you can make sure a shared resource is free for the CPU
        Binary Semaphores - controls access to recoures on the GPU-side only. Helpful to ensure proper ordering.
        Pipeline Barriers - controls resources within a GPU queue. We use them for layout transitions of images.
        The former 2 are objects we have to create and store, and the latter is issued as commands.
    */
    VkSemaphoreCreateInfo semaphoreCI {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fenceCI {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    std::array<VkFence, maxFramesInFlight> fences;
    std::array<VkSemaphore, maxFramesInFlight> presentSemaphores;
    std::vector<VkSemaphore> renderSemaphores;

    /*
        Fences are created in a signalled state by setting the VK_FENCE_CREATE_SIGNALED_BIT flag, otherwise the first
        wait for such a fence would timeout. One fence per frame is needed for syncing between GPU and CPU.
        Semaphores need to match the amount of swapchain images.
        For complex sync setups, Timeline Semaphores can help reduce the verbosity, They add a semaphore type with a
        counter value that can be increased and waited on and also be queried by the CPU to replace fences.
    */
    for (auto i{ 0 }; i < maxFramesInFlight; i++) {
        chk(vkCreateFence(device, &fenceCI, nullptr, &fences[i]));
        chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &presentSemaphores[i]));
    }

    renderSemaphores.resize(swapchainImages.size());
    for (auto& semaphore : renderSemaphores) {
        chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
    }


    // Allocate command buffers from pool
    /*
        To issue commands in Vulkan, you must record your commands into a buffer and submit them to a queue. Command
        buffers are allocated from a command pool which helps the driver optimise allocations.
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT lets us implicity resent command buffers when recording them.
        We also have to specifiy the queue family this command buffer will be submitted to.
        Command pools are cheap to create and let you record command buffers from multiple threads. Complex
        applications can often have multiple as a result.
    */
    VkCommandPool commandPool{ VK_NULL_HANDLE };
    std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers;

    VkCommandPoolCreateInfo commandPoolCI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamily,
    };

    chk(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cbAllocCI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = maxFramesInFlight,
    };

    chk(vkAllocateCommandBuffers(device, &cbAllocCI, commandBuffers.data()));

    // Loading textures
    /*
        Textures are images just like the swapchain or depth.
        We are using the KTX image format as it stores images in GPU native formats, saving us having to decompress
        or convert, and it also supports GPU specific features like mipmaps.
    */
    struct Texture {
        VmaAllocation allocation{ VK_NULL_HANDLE };
        VkImage image{ VK_NULL_HANDLE };
        VkImageView view{ VK_NULL_HANDLE };
        VkSampler sampler{ VK_NULL_HANDLE };
    };

    std::array<Texture, 1> textures{};
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (int i{ 0 }; i < textures.size(); i++) {
        ktxTexture* ktxTexture{ nullptr };
        std::string filename = SDL_GetBasePath();
        filename +=  "res/texture.ktx";
        ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        
        // Creating the texture image
        /*
            This is similar to the depth image, but we use the Vk format from the ktxTexture and we need mip levels now.
        */
        VkImageCreateInfo texImgCI {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(ktxTexture),
            .extent = {
                .width = ktxTexture->baseWidth,
                .height = ktxTexture->baseHeight,
                .depth = 1,
            },
            .mipLevels = ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        
        VmaAllocationCreateInfo texImageAllocCI {
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        chk(vmaCreateImage(allocator, &texImgCI, &texImageAllocCI, &textures[i].image, &textures[i].allocation, nullptr));
        
        // Creating the texture image view
        /*
            This is similar to the depth image, but we want to access the whole image including mip levels in this case.
        */
        VkImageViewCreateInfo texViewCI {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = textures[i].image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texImgCI.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = ktxTexture->numLevels,
                .layerCount = 1,
            },
        };
        
        chk(vkCreateImageView(device, &texViewCI, nullptr, &textures[i].view));
        
        // Creating an intermediate buffer
        /*
            This buffer will be a temporary source for a buffer-to-image copy, so the only flag we need is
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT. We need this buffer as we cannot do a simple memcpy() into the image as
            optimal tiling stores texels in a hardware-specific layout and we cannot convert to that. Using this buffer we
            will send commands to the GPU to copy this buffer to the image instead, doing the conversion as a result.
        */
        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VmaAllocationInfo imgSrcAllocInfo{};
        VkBufferCreateInfo imgSrcBufferCI {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = static_cast<uint32_t>(ktxTexture->dataSize),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        
        VmaAllocationCreateInfo imgSrcAllocCI {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        
        chk(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo));
        memcpy(imgSrcAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);
        
        // Command buffer to copy the image from the buffer to the GPU
        /*
            A fence is also created that's used to wait for the command buffer to finish execution.
        */
        VkFenceCreateInfo fenceOneTimeCI {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        
        VkFence fenceOneTime{};
        chk(vkCreateFence(device, &fenceOneTimeCI, nullptr, &fenceOneTime));
        VkCommandBuffer cbOneTime{};
        VkCommandBufferAllocateInfo cbOneTimeAI {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .commandBufferCount = 1,
        };
        
        chk(vkAllocateCommandBuffers(device, &cbOneTimeAI, &cbOneTime));
        
        // Recording the commands to copy the image
        /*
            That hardware specific layout for optimal tiled images on the GPU also defines what operations are possible
            with an image. So using a pipeline barrier we change said layout depending on what we want to do. We first
            transition all mip levels of the texture image from the undefined layout to a layout that allows us to transfer
            data to it, and then copy those from the temporary buffer to the image using vkCmdCopyBufferToImage. Finally we
            convert the mip levels to a layout we can read from a shader, and we submit the buffer to the queue.
            Extensions VK_EXT_host_image_copy and VK_KHR_unified_image_layouts make this process easier but aren't widely
            supported.
        */
        VkCommandBufferBeginInfo cbOneTimeBI {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        
        chk(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
        VkImageMemoryBarrier2 barrierTexImage {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = ktxTexture->numLevels,
                .layerCount = 1,
            },
        };
        
        VkDependencyInfo barrierTexInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierTexImage,
        };
        
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        std::vector<VkBufferImageCopy> copyRegions{};
        for (int i{ 0 }; i < ktxTexture->numLevels; i++) {
            ktx_size_t mipOffset{0};
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = static_cast<uint32_t>(i),
                    .layerCount = 1,
                },
                .imageExtent {
                    .width = ktxTexture->baseWidth >> i,
                    .height = ktxTexture->baseHeight >> i,
                    .depth = 1,
                },
            });
        }
        
        vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
        VkImageMemoryBarrier2 barrierTexRead {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = ktxTexture->numLevels,
                .layerCount = 1,
            },
        };
        
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        chk(vkEndCommandBuffer(cbOneTime));
        VkSubmitInfo oneTimeSI {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cbOneTime,
        };
        
        chk(vkQueueSubmit(queue, 1, &oneTimeSI, fenceOneTime));
        chk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
        
        vkDestroyFence(device, fenceOneTime, nullptr);
        vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);
        
        // Define texture sampler settings for shaders
        VkSamplerCreateInfo samplerCI {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = static_cast<float>(ktxTexture->numLevels),
        };
        
        chk(vkCreateSampler(device, &samplerCI, nullptr, &textures[i].sampler));
        
        ktxTexture_Destroy(ktxTexture);
        textureDescriptors.push_back({
            .sampler = textures[i].sampler,
            .imageView = textures[i].view,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        });
    }

    // Descriptors for the image
    /*
        Descriptors represent shader resources so GPUs can access them in the shader. This is one of the most verbose 
        parts of Vulkan (using Descriptor Indexing will simplify this a bit however). Through this we can do a
        "bindless" setup, where all textures are put into 1 large array and indexed in the shader, instead of making 
        descriptor sets for each texture. (I am only using 1 texture so I only need to make one either way).
        We are only making descriptors for images, so we only have 1 binding. A call to vkCreateDescriptorSetLayout 
        creates the descriptor set layout with our config. We will allocate the descriptor and the defining shader 
        interface at pipeline creation.
        We are using a combined image and sampler, but there are scenarios where you could separate them, and in those
        cases you can use 2 pool sizes, one for sampled images and another for samplers.
    */
    VkDescriptorSetLayout descriptorSetLayoutTex{ VK_NULL_HANDLE };
    VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &descVariableFlag,
    };

    VkDescriptorSetLayoutBinding descLayoutBindingTex {
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = static_cast<uint32_t>(textures.size()), // We only use 1 texture
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutCreateInfo descLayoutTexCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descBindingFlags,
        .bindingCount = 1,
        .pBindings = &descLayoutBindingTex,
    };
    
    chk(vkCreateDescriptorSetLayout(device, &descLayoutTexCI, nullptr, &descriptorSetLayoutTex));

    // Create descriptor pool
    /*
        Similar to a command pool. The number of descriptor types we want to allocate must be specified here upfront.
        We need as many descriptors for combined image and samplers as we load textures. We specify the amount of 
        descriptor sets we want to allocate via maxSets. This is due to using descriptor indexing and it's only ever 
        accessed by the GPU (so no need for MarFramesInFlight). Allocations beyond the requested pool size will fail.
    */
    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
    VkDescriptorPoolSize poolSize {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = static_cast<uint32_t>(textures.size()), // We only use 1 texture
    };

    VkDescriptorPoolCreateInfo descPoolCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    chk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));

    // Allocating the descriptor set from the pool
    /*
        While the descriptor set layout defines the interface, the descriptor contains the actual descriptor data. Sets
        and layouts are split because you can mix layouts and reuse them for different descriptor sets.
    */
    VkDescriptorSet descriptorSetTex{ VK_NULL_HANDLE };
    uint32_t variableDescCount{ static_cast<uint32_t>(textures.size()) }; // We only use 1 texture
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variableDescCount,
    };

    VkDescriptorSetAllocateInfo texDescSetAlloc {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = &variableDescCountAI,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayoutTex,
    };

    chk(vkAllocateDescriptorSets(device, &texDescSetAlloc, &descriptorSetTex));

    // Fill the descriptor set with data
    /*
        Calling vkUpdateDescriptorSets will put the information of the descriptors for the textures
        (textureDescriptors) into the first (and only) binding slot of the descriptor set.
    */
    VkWriteDescriptorSet writeDescSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSetTex,
        .dstBinding = 0,
        .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = textureDescriptors.data(),
    };

    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

    // Loading shaders
    /*
        You could compile Slang shaders before bringing them into the project as Vulkan expects SPIR-V, but compiling
        at runtime makes updating shaders easier.
    */
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    slang::createGlobalSession(slangGlobalSession.writeRef());

    // Defining compilation settings
    /*
        We use SPIR-V 1.4 as it was the baseline since Vulkan 1.2. We use a major column layout to match our GLM
        library for constructing matrices later.
    */
    std::array<slang::TargetDesc, 1> slangTargets {
        slang::TargetDesc {
            .format = SLANG_SPIRV,
            .profile = slangGlobalSession->findProfile("spirv_1_4"),
        },
    };

    std::array<slang::CompilerOptionEntry, 1> slangOptions{
        slang::CompilerOptionEntry {
            .name = slang::CompilerOptionName::EmitSpirvDirectly,
            .value = {
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = 1,
            },
        },
    };

    slang::SessionDesc slangSessionDesc {
        .targets = slangTargets.data(),
        .targetCount = SlangInt(slangTargets.size()),
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries = slangOptions.data(),
        .compilerOptionEntryCount = static_cast<uint32_t>(slangOptions.size()),
    };

    Slang::ComPtr<slang::ISession> slangSession;
    slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());

    // Compile shader
    /*
        Gets a SPIR-V representation of the Slang shader by loading a textual shader and using getTargetCode to compile
        all entry points in the shader.
    */
    std::string shaderFile = SDL_GetBasePath();
    shaderFile += "/res/shader.slang";

    Slang::ComPtr<slang::IModule> slangModule {
        slangSession->loadModuleFromSource("triangle", shaderFile.c_str(), nullptr, nullptr)
    };

    Slang::ComPtr<ISlangBlob> spirv;
    slangModule->getTargetCode(0, spirv.writeRef());

    // Loading the shader
    /*
        For Vulkan 1.3 we need to create a shader module (a container for compiled SPIR-V shaders).
        The Vulkan extension VK_KHR_maintenance5 extension, which became core in Vulkan 1.4, allows direct passing of
        VkShaderModuleCreateInfo to the pipeline's shader stage create info (and subsequently deprecated shader
        modules).
    */
    VkShaderModuleCreateInfo shaderModuleCI {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv->getBufferSize(),
        .pCode = reinterpret_cast<const uint32_t*>(spirv->getBufferPointer()),
    };

    VkShaderModule shaderModule{};
    chk(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));

    // Setting up the graphics pipeline
    /*
        Where OpenGL was a huge state machine whose state could be changed at any time, Vulkan changes this by 
        introducing pipeline state objects. These provide a full set of pipeline state in a "compiled" pipeline object,
        allowing the driver a chance to optimise them. This also allows for pipeline object creation in e.g. a separate
        thread. If you need a different pipeline state you have to create a new pipeline state object. Vulkan has 
        pipeline types for specific usecases like grephics, compute, raytracing, etc.
        We want a graphics pipeline. We first will create a pipeline layout, defining the interface between the
        pipeline and our shaders. Pipeline Layouts are separate objects so you can mix and match them with other 
        pipelines.
        pushConstantRange defines the range of values that can be directly pushed to the shader without having to go
        through a buffer. The descriptor set layouts define the interface the shader resources (in our case just the 
        one layout for passing the texture image descriptors)
    */
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    VkPushConstantRange pushConstantRange {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(VkDeviceAddress),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayoutTex,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange,
    };

    chk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // Vertex binding
    /*
        We need to specify the vertex structure we previously made into Vulkan terms. We use a single vertex buffer, so
        we require one vertex binding point. The stide matches the size of the vertex structure as our vertices are
        stored directly adjacent in memory. inputRate is per-vertex so the data pointer advances for every vertex read.
    */
    VkVertexInputBindingDescription vertexBinding {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    // Vertex attributes
    /*
        We specify how the vertex attributes for position, normals and texture coordinates are laid out in memory. This
        matches out CPU-side vertex structure.
        Another option for accessing vertices in the shader is buffer device address. This way you can skip traditional
        vertex attributes and manually fetch them in the shader using pointers. This is called "vertex pulling", but on
        some devices this can be slower, so we don't do it here.
    */
    std::vector<VkVertexInputAttributeDescription> vertexAttributes {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal), },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv), },
    };

    // Pipeline state for the vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        .pVertexAttributeDescriptions = vertexAttributes.data(),
    };

    // Input assembly state for vertex data
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    // Shaders and pipeline stage match ups
    /*
        If you wanted to use different shaders you'd need to create multiple pipelines. VK_EXT_shader_objects adds more
        flexibility to this part of the API.
    */
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages {
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shaderModule,
        .pName = "main" },
    { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shaderModule,
        .pName = "main" },
    };

    // Viewport state
    /*
        We want the viewport (and scissor) to be a dynamic state so we don't need to recreate the pipeline if anything
        about the viewport changes e.g. when resizing the window.
    */
    VkPipelineViewportStateCreateInfo viewportState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates.data(),
    };

    // Depth buffering
    VkPipelineDepthStencilStateCreateInfo depthStencilState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };

    // Enable dynamic rendering
    /*
        Removes the need of render passes, but as this was added later in Vulkans life it needs to be passed through
        pNext.
    */
    VkPipelineRenderingCreateInfo renderingCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &imageFormat,
        .depthAttachmentFormat = depthFormat,
    };

    // Blending, Rasterization and Multisampling settings
    VkPipelineColorBlendAttachmentState blendAttachment { .colorWriteMask = 0xF };
    VkPipelineColorBlendStateCreateInfo colourBlendState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Graphics pipeline creation
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkGraphicsPipelineCreateInfo pipelineCI {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCI,
        .stageCount = 2,
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colourBlendState,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
    };

    chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    // Render loop
    uint64_t lastTime{ SDL_GetTicks() };
    bool quit{ false };
    uint32_t imageIndex{ 0 };
    uint32_t frameIndex{ 0 };
    glm::vec3 objectRotations[3]{};
    glm::vec3 camPos{ 0.0f, 0.0f, -6.0f };
    while (!quit) {
        // Wait on fence
        /*
            We want to have the CPU start recording the next command buffer while the GPU is doing the previous one. So
            we wait for fence of the last frame the GPU has worked on to finish execution.
            We have no requirements regarding how long graphics operations are allowed to take, so the timeout is
            UINT64_MAX.
        */
        chk(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
        chk(vkResetFences(device, 1, &fences[frameIndex]));

        // Acquire next image
        /*
            We need to ask/aquire the swapchain for the next index to be used in this frame. A special check function
            is used here in case of VK_ERROR_OUT_OF_DATE when the surface becomes incompatible with the swapchain. If
            this happens, we recreate the swapchain for the next frame.
        */
        chkSwapchain(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex));

        // Update shader data
        /*
            Need up-to-date user inputs for the next frame. This is safe to do after waiting for the fence. We update
            matrices from current data using glm, and then copy it to the shader data buffer's persistent memory.
        */
        shaderData.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y), 0.1f, 32.0f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        for (int i { 0 }; i < 3; i++) {
            auto instancePos = glm::vec3(static_cast<float>(i-1) * 3.0f, 0.0f, 0.0f);
            shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }

        memcpy(shaderDataBuffers[frameIndex].allocationInfo.pMappedData, &shaderData, sizeof(ShaderData));
        
        // Reset command buffer
        /*
            We will record the commands for a single render frame into the command buffer.
            You could pre-record command buffers and reuse them until something changes that would require re-recording
            but this makes things very complicated as you'd need to implement update logic that works with CPU/GPU
            parallelism. Also, recording command buffers is quite fast and can be offloaded to another thread if needed
            so recording them every frame is fine.
            Commands recorded into a command buffer start with vkCmd. They are not directly executed, they only go off
            when submitted to a queue (GPU timeline), which is why they don't return a result. Don't mix these commands
            with instantly executed CPU timeline instructions!
            Command buffers have a lifecycle we must follow. e.g. you can record commands when it's in the executable
            state (validation layers can help here).
            We first move the command buffer into the initial state, by resetting it (safe due to waiting for fence so
            it's no longer in the pending state).
        */
        auto cb = commandBuffers[frameIndex];
        chk(vkResetCommandBuffer(cb, 0));

        // Start recording command buffer
        /*
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT makes the command buffer move to the invalid state after
            execution which can be used as an optimisation for drivers. After using vkBeginCommand Buffer, the
            command buffer moves into the recording state.
        */
        VkCommandBufferBeginInfo cbBI {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        chk(vkBeginCommandBuffer(cb, &cbBI));

        // Command Buffer inputs
        /*
            During rendering, colour information will be written to the current swapchain image and depth information
            will be written to the depth image. Optimal tiled images need to be in the correct layout for their
            intended use case, so the first step is to issue layout transitions for both of these.
            Image Memory Barriers transition layouts and make sure they happen at the right pipeline stage. 
            srcStageMask is the pipeline stage to wait on, srcAccessMask defines writes to be made available. dstStage
            and Access mask define where and what writes to be made visible. Available and visible doesn't mean the
            same thing. Available means data is ready for future memory operations, and visible means data is actually 
            visible to reads from the consuming stages.
            The first barrier transitions the current swapchain image to a layout so that we can use it as a colour
            attachment for rendering. The second barrier transitions the depth image to a layout that can be used as a
            depth attachment for rendering.
            vkCmdPipelineBarrier2 inssets those 2 barriers into the current command buffer.
        */
        std::array<VkImageMemoryBarrier2, 2> outputBarriers {
            VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = swapchainImages[imageIndex],
                .subresourceRange {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            }, VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = depthImage,
                .subresourceRange {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            },
        };

        VkDependencyInfo barrierDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = outputBarriers.data(),
        };

        vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);

        // Defining how to use the attachments
        /*
            We setup an VkRenderingAttachmentInfo for the swapchain used as colour attachment and depth image used as
            depth attachment. Each render pass will clear them to their clearValue due to loadOp. storeOp for colour
            attachment keeps its contents, but we don't need depth information once we're done rendering so we don't
            care after.
        */
        VkRenderingAttachmentInfo colourAttachmentInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainImageViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{ .color{ 0.0f, 0.0f,0.0f, 1.0f } },
        };
    
        VkRenderingAttachmentInfo depthAttachmentInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthImageView,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue{ .depthStencil = { 1.0f, 0 } },
        };

        // Start dynamic render pass instance
        VkRenderingInfo renderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea{ .extent {
                .width = static_cast<uint32_t>(windowSize.x),
                .height = static_cast<uint32_t>(windowSize.y),
            }},
            .layerCount = 1, 
            .colorAttachmentCount = 1,
            .pColorAttachments = &colourAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo,
        };

        vkCmdBeginRendering(cb, &renderingInfo);

        // Defining a viewport (and scissor)
        VkViewport vp {
            .width = static_cast<float>(windowSize.x),
            .height = static_cast<float>(windowSize.y),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        vkCmdSetViewport(cb, 0, 1, &vp);
        VkRect2D scissor{ .extent {
            .width = static_cast<uint32_t>(windowSize.x),
            .height = static_cast<uint32_t>(windowSize.y),
        }};
        vkCmdSetScissor(cb, 0, 1, &scissor);

        // Binding the resources for 3D objects
        /*
            This includes the graphics pipeline, the vertex and fragment shaders, and the descriptor set for the 
            texture images and the vertex and index buffers of the 3D mesh.
            For the shader data buffer we used the buffer device address so we pass the address of the current frames
            shader data buffer via a push constant to the shaders.
            These vkCmd calls (and others) set the current command buffer state, so they persist across multiple draw
            calls inside this command buffer. So if you wanted to change 1 thing you only need to add it while keeping
            the rest the same.
        */
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize vOffset{ 0 };
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetTex, 0, nullptr);
        vkCmdBindVertexBuffers(cb, 0, 1, &vBuffer, &vOffset);
        vkCmdBindIndexBuffer(cb, vBuffer, vBufSize, VK_INDEX_TYPE_UINT16);
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &shaderDataBuffers[frameIndex].deviceAddress);

        // Draw command and finish render pass
        vkCmdDrawIndexed(cb, indexCount, 3, 0, 0, 0);
        vkCmdEndRendering(cb);

        // Transition the swapchain image to a layout to present
        VkImageMemoryBarrier2 barrierPresent {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchainImages[imageIndex],
            .subresourceRange {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VkDependencyInfo barrierPresentDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierPresent,
        };

        vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);

        // End the command buffer recording
        chk(vkEndCommandBuffer(cb));

        // Submit command buffer
        /*
            To execute the commands recorded we need to submit the command buffer to a queue. For this program we only
            have a single graphics queue to submit to (but real applications may have multiple queues and more
            submission patterns).
            The semaphore in pWaitSemaphores makes sure the submitted command buffer(s) wont' start executing before 
            the presentation of the current frame has finished. The pipeline stage in pWaitDstStageMask will make that
            wait happen at the colour attachment output stage of the pipeline, so the GPU might already start doing 
            work on parts of the pipeline that come before this e.g. fetching vertices. The signal semaphore in 
            pSignalSemaphores on the other hand is a semaphore that's signalled by the GPU once command buffer 
            execution has completed. This combination makes sure no read/write hazards occur from the GPU reading or
            writing to resoures still in use.
            We need to decouple the two semaphore types (By using frameIndex and imageIndex) as vkQueuePresentKHR has
            no way to signal anything without an extension (and that isn't even available anywhere).
            Submissions can have multiple wait and signal semaphores and wait stages, which can be very complicated.
            Keep synchronisation scope as narrow as possible to allow the GPU to overlap work and detect errors through
            validation layers.
        */
        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentSemaphores[frameIndex],
            .pWaitDstStageMask = &waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &cb,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderSemaphores[imageIndex],
        };

        chk(vkQueueSubmit(queue, 1, &submitInfo, fences[frameIndex]));

        // Calculate frame index for the next loop
        frameIndex = (frameIndex + 1) % maxFramesInFlight;

        // Present image
        /*
            Calling vkQueuePresentKHR will enqueue the image for presentation after waiting for the render semaphore,
            guaranteeing the image won't be presented until after our rendering commands are finished.
        */
        VkPresentInfoKHR presentInfo {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderSemaphores[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex,
        };

        chkSwapchain(vkQueuePresentKHR(queue, &presentInfo));

        // Poll events
        float elapsedTime{ (SDL_GetTicks() - lastTime) / 1000.0f };
        lastTime = SDL_GetTicks();
        for (SDL_Event event; SDL_PollEvent(&event);) {
            // Exit loop if the application is about to close
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
                break;
            }

            // Rotate the selected object with mouse drag
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    objectRotations[shaderData.selected].x -= static_cast<float>(event.motion.yrel) * elapsedTime;
                    objectRotations[shaderData.selected].y += static_cast<float>(event.motion.xrel) * elapsedTime;
                }
            }

            // Zooming in with the mouse wheel
            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                camPos.z += static_cast<float>(event.wheel.y) * elapsedTime * 10.0f;
            }

            // Select active model instance
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS) {
                    shaderData.selected = (shaderData.selected < 2) ? shaderData.selected + 1 : 0;
                }

                if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS) {
                    shaderData.selected = (shaderData.selected > 0) ? shaderData.selected - 1 : 2;
                }
            }

            // Window resize
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                updateSwapchain = true;
            }
        }

        if (updateSwapchain) {
            updateSwapchain = false;
            SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

            vkDeviceWaitIdle(device);
            chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[deviceIndex], surface, &surfaceCaps));
            swapchainCI.oldSwapchain = swapchain;
            swapchainCI.imageExtent = {
                .width = static_cast<uint32_t>(windowSize.x),
                .height = static_cast<uint32_t>(windowSize.y)
            };

            chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

            for (auto i = 0; i < imageCount; i++) {
                vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            }

            chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
            swapchainImages.resize(imageCount);
            chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));
            swapchainImageViews.resize(imageCount);

            for (auto i = 0; i < imageCount; i++) {
                VkImageViewCreateInfo viewCI {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = swapchainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = imageFormat,
                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
                };

                chk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
            }

            vkDestroySwapchainKHR(device, swapchainCI.oldSwapchain, nullptr);
            vmaDestroyImage(allocator, depthImage, depthImageAllocation);
            vkDestroyImageView(device, depthImageView, nullptr);
            depthImageCI.extent = {
                .width = static_cast<uint32_t>(windowSize.x),
                .height = static_cast<uint32_t>(windowSize.y),
                .depth = 1
            };

            VmaAllocationCreateInfo allocCI {
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };

            chk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));

            VkImageViewCreateInfo viewCI {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = depthImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depthFormat,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
            };

            chk(vkCreateImageView(device, &viewCI, nullptr, &depthImageView));
        }
    }
    
    // Cleanup
    /* 
        Of course you could let the Operating System sort this all out for you... but don't do that.
        You must destroy VMA allocator Vulkan device and instance last, but the rest you can do in any order.
    */
    chk(vkDeviceWaitIdle(device));

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImageView(device, textures[0].view, nullptr);
    vkDestroySampler(device, textures[0].sampler, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTex, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);

    for (int i{ 0 }; i < renderSemaphores.size(); i++) {
        vkDestroySemaphore(device, renderSemaphores[i], nullptr);
    }
    
    for (int i{ 0 }; i < swapchainImageViews.size(); i++) {
    	vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }

    vmaDestroyImage(allocator, depthImage, depthImageAllocation);
    vmaDestroyImage(allocator, textures[0].image, textures[0].allocation);
    vmaDestroyBuffer(allocator, vBuffer, vBufferAllocation);

    for (int i{ 0 }; i < maxFramesInFlight; i++) {
        vkDestroyFence(device, fences[i], nullptr);
        vkDestroySemaphore(device, presentSemaphores[i], nullptr);
        vmaDestroyBuffer(allocator, shaderDataBuffers[i].buffer, shaderDataBuffers[i].allocation);
    }
    
    vmaDestroyAllocator(allocator);

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}
