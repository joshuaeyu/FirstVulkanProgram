#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
//#include <vulkan/vulkan.h>

//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES    // GLM can handle UBO alignment requirements but breaks down for nested structs
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // OpenGL uses -1.0 to 1.0
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <set>
#include <limits>
#include <fstream>
#include <array>
#include <chrono>

/*
 Linking - General / Runpath Search Paths   for .dylib      same as -Wl,-rpath,
 Search Paths / Library Search Paths        for .a          same as -L
 Search Paths / Header Search Paths         for #include    same as -I
 
 Also need to set environment variables VK_ICD_FILENAMES and VK_LAYER_PATH in
 the Xcode scheme or in the current shell if launching from the command line.
 
 Configure layer settings using vk_layer_settings.txt
 */

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const int MAX_FRAMES_IN_FLIGHT = 2;

const bool DYNAMIC_VIEWPORT_SCISSOR = true;

const bool SEPARATE_TRANSFER_QUEUE_FAMILY = true;

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

const std::string SOURCE_PATH = "/Users/joshuayu/Documents/Programming/Vulkan/FirstVulkanProgram/FirstVulkanProgram/";

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func)
        func(instance, debugMessenger, pAllocator);
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> transferFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
        } else {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        return bindingDescription;
    }
    
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
        
        return attributeDescriptions;
    };
};

const std::vector<Vertex> vertices = {
    // Top square
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    // Bottom square
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    // Top square
    0, 1, 2, 2, 3, 0,
    // Bottom square
    4, 5, 6, 6, 7, 4
};

struct UniformBufferObject {
    // Remember alignment requirements
    // - Scalars: N (= 4 bytes given 32-bit floats)
    // - vec2: 2N
    // - vec3, vec4: 4N
    // - Nested structs: Aligned by base of its members rounded up to a multiple of 16 bytes (4N)
    // - mat4: same as vec4
    glm::vec2 foo;
    alignas(16) glm::mat4 model;
    glm::mat4 view; // good practice to be explicit and specify alignas(16) anyway
    glm::mat4 proj; // good practice to be explicit and specify alignas(16) anyway
};

class HelloTriangleApplication {
public:    
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
        
        // Severity can be: verbose (diagnostic), info, warning, error
        // Message type can be: general, validation, performance
        // Callback data contains: message, objects, object count
        
        std::cerr << "~~Validation layer: " << pCallbackData->pMessage << std::endl;
        
        // Return value indicates if the Vulkan call which triggered this
        // validation layer message should be aborted. Only return true
        // if testing the validation layer itself
        return VK_FALSE;
    }
    
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
private:
    GLFWwindow *window;
    // Instance
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    // Device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkQueue transferQueue;
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    bool portabilitySubsetExtSupported = false;
    // Swapchain
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    // Graphics pipeline
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    // Drawing
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkCommandPool graphicsCommandPool;
    VkCommandPool transferCommandPool;
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    std::vector<VkCommandBuffer> transferCommandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;
    // Vertex and index buffers
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    // Uniform buffers
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    // Texture
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
    // Depth buffer
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "FirstVulkanProgram", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetWindowSizeCallback(window, windowSizeCallback);
        glfwSetWindowIconifyCallback(window, windowIconifyCallback);
    }
    void initVulkan() {
        // Instance
        createInstance();
        setupDebugMessenger();
        // Physical device and logical device
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        // Swapchain
        createSwapchain();
        createImageViews();
        // Pipeline
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPools();
        // Depth
        createDepthResources();
        createFramebuffers();
        // Texture
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        // Buffers
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        // Descriptors
        createDescriptorPool();
        createDescriptorSets();
        // Command buffers and drawing
        createCommandBuffers();
        createSyncObjects();
    }
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }
    void cleanup() {
        cleanupSwapchain();
        
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);  // Free once buffer is no longer used (i.e., destroyed)
        }
        
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);  // Free once buffer is no longer used (i.e., destroyed)
        
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);  // Free once buffer is no longer used (i.e., destroyed)
        
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyFence(device, inFlightFences[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
        
        vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
        vkDestroyCommandPool(device, transferCommandPool, nullptr);

        vkDestroyDevice(device, nullptr);
        
        if (enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    
    static void windowSizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
    static void windowIconifyCallback(GLFWwindow* window, int iconified) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
    void cleanupSwapchain() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        
        for (auto framebuffer : swapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        
        for (auto& imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }
    
    
    // ================ createInstance() ================
    void createInstance() {
        if (glfwVulkanSupported())
            std::cout << "Vulkan is supported." << std::endl;
        else {
            throw std::runtime_error("Vulkan is not supported, exiting!");
        }
        
        if (enableValidationLayers) {
            if (checkValidationLayerSupport())
                std::cout << "All requested validation layers are available." << std::endl;
            else
                throw std::runtime_error("Validation layers requested, but not available!");
        }

        // ----- Application info -----
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        // ----- Instance create info -----
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // Validation layers (if requested)
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            
            createInfo.pNext = nullptr;
        }
        
        // Supported extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
        
        // Required extensions
        std::vector<const char*> requiredExtensions = getRequiredExtensions(); // GLFW required extensions
        requiredExtensions.push_back("VK_KHR_get_physical_device_properties2"); // Required by device extension VK_KHR_portability_subset
        
        // Check all required extensions are supported
        for (const auto& required : requiredExtensions) {
            const auto it = std::find_if(extensions.begin(), extensions.end(), [&required](VkExtensionProperties ext){ return strcmp(required, ext.extensionName) == 0; });
            if (it == extensions.end()) {
                throw std::runtime_error("Required extension " + std::string(required) + " is not supported!");
            }
        }
        std::cout << "All required extensions are supported." << std::endl;
        
        requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        
        // ----- Create instance -----
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance! Error code " + std::to_string(result) );
        }
    }
    // Checks that all validationLayers are supported
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        
        for (const char* layerName : validationLayers) {
            const auto it = std::find_if(availableLayers.begin(), availableLayers.end(), [&layerName](VkLayerProperties l){ return strcmp(layerName, l.layerName) == 0; });
            if (it == availableLayers.end())
                return false;
        }
        
        return true;
    }
    // Returns all extensions required (by GLFW)
    std::vector<const char*> getRequiredExtensions(bool print = false) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        // VK_KHR_surface, VK_EXT_metal_surface
        
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        
        if (print) {
            std::cout << "Required extensions:" << std::endl;
            for (const auto& extension : extensions) {
                std::cout << '\t' << extension << std::endl;
            }
        }
        
        return extensions;
    }
    
    // ================ setupDebugMessenger() ================
    void setupDebugMessenger() {
        if (!enableValidationLayers)
            return;
        
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        
        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
            throw std::runtime_error("Failed to setup debug messenger!");
    }
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        createInfo.pUserData = nullptr;
    }
    
    // ================ createSurface() ================
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }
    
    // ================ pickPhysicalDevice() ================
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                if (portabilitySubsetExtSupported) {
                    deviceExtensions.push_back("VK_KHR_portability_subset");
                }
                break;
            }
            // Can also rate device suitability based on its properties and pick the best one!
        }
        
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }
    bool isDeviceSuitable(VkPhysicalDevice device, bool print = false) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        
        if (print)
            std::cout << deviceProperties.deviceName << std::endl;
        
        // Queue families (graphics, presentation)
        QueueFamilyIndices indices = findQueueFamilies(device);
        
        // Device extensions
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        
        // Swap chain surface formats and present modes
        bool swapchainAdequate = false;
        if (extensionsSupported) {
            SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
            swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
        }
        
        // Physical device features (e.g., anisotropy)
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
        
        return indices.isComplete() && extensionsSupported && swapchainAdequate && supportedFeatures.samplerAnisotropy;
    }
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices{};
        
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Graphics and transfer
            if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
                if (!indices.graphicsFamily.has_value() && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphicsFamily = i;
                }
                // Look for a queue family that supports transfer that isn't the current graphics queue
                if (indices.graphicsFamily.has_value() && indices.graphicsFamily.value() != i && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                    indices.transferFamily = i;
                }
            } else {
                if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    indices.graphicsFamily = i;
                }
            }
            // Presentation
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }
            i++;
        }
        
        return indices;
    }
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
            if (strcmp(extension.extensionName, "VK_KHR_portability_subset") == 0) {
                portabilitySubsetExtSupported = true;
            }
        }
        
        return requiredExtensions.empty();
    }
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) {
        SwapchainSupportDetails details;
        
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        
        return details;
    }
    
    // ================ createLogicalDevice() ================
    void createLogicalDevice() {
        // Queue families
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            uniqueQueueFamilies.insert(indices.transferFamily.value());
        }
        
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        // Physical device features (used to request features)
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        
        // Logical device create info (queues, features, extensions)
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        
        // Create device
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }
        
        // Get device queues
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            vkGetDeviceQueue(device, indices.transferFamily.value(), 0, &transferQueue);
        }
    }
    
    // ================ createSwapChain() ================
    void createSwapchain() {
        // Query capabilities, surface formats, present modes of physical device
        SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice);
        
        // Pick surface format and present mode
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
        
        // Determine image count and extent based on capabilities
        uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }
        VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);
        
        // Create swap chain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface; // Based on window library (GLFW)
        createInfo.minImageCount = imageCount; // Based on capabilities
        createInfo.imageFormat = surfaceFormat.format; // Want SRGBA
        createInfo.imageExtent = extent; // Based on capabilities
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        QueueFamilyIndices queueFamilies = findQueueFamilies(physicalDevice);
        std::set<uint32_t> uniqueQueueFamilyIndices = {queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value()};
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            uniqueQueueFamilyIndices.insert(queueFamilies.transferFamily.value());
        }
        std::vector<uint32_t> queueFamilyIndices(uniqueQueueFamilyIndices.begin(), uniqueQueueFamilyIndices.end());
        if (queueFamilyIndices.size() > 1) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 1;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        
        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        
        createInfo.presentMode = presentMode; // Want MAILBOX
        createInfo.clipped = VK_TRUE;
        
        createInfo.oldSwapchain = VK_NULL_HANDLE;   // if swap chain becomes invalid/unoptimized (e.g., window is resized)
        
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }
        
        // Store actual swap chain image structs (actual image count may be more than the minImageCount we requested)
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
        
        // Store surface format and extent for later use
        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent = extent;
        
    }
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // Match current Vulkan-corrected window resolution
            return capabilities.currentExtent;
        } else {
            // Pick any resolution
            // We match the framebuffer size instead of window size because of Apple's Retina display
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            
            return actualExtent;
        }
    }
    
    // ================ createImageViews() ================
    void createImageViews() {
        // Create swap chain image views to be used by corresponding framebuffers
        
        // Swap chain contains images, each with a corresponding image view
        // Render pass renders to framebuffers, which each have an image view attachment
        
        swapchainImageViews.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            swapchainImageViews[i] = createImageView(swapchainImages[i], swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }
    
    // ================ createRenderPass() ================
    void createRenderPass() {
        // Color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling yet
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear previous color from framebuffer before drawing
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store rendered contents in memory
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Image layout needs to be suitable for next operation (i.e., presentation using the swap chain)        colorAttachment.format = swapchainImageFormat;
        
        // Depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling yet
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear previous color from framebuffer before drawing
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Store rendered contents in memory
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Image layout needs to be suitable for next operation
        
        // Subpass
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // Attachment index in VkAttachmentDescription array of VkRenderPassCreateInfo (createInfo.pAttachments)
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Attachment will be used as a color buffer
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1; // Attachment index in VkAttachmentDescription array of VkRenderPassCreateInfo (createInfo.pAttachments)
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Attachment will be used as a depth-stencil buffer
        
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Graphics, ray tracing, or compute
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef; // Color attachment is at index 0 -> layout(location = 0) out vec4 outColor in fragment shader
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;
        
        // Can instead specify VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT as a wait stage for VkQueueSubmit()
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // The implicit subpass before the render pass
        dependency.dstSubpass = 0; // Index of our subpass above
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Wait for swap chain to finish reading from the color attachment, and for late fragment tests to finish (which are after fragment shading), before accessing the attachments
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // After waiting, write to the color attachment and depth-stencil attachment (during early fragment tests which are before fragment shading)
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }
    
    // ================ createDescriptorSetLayout() ================
    void createDescriptorSetLayout() {
        // Used in descriptor sets; contains descriptor set layout bindings
        // Must be used in pipeline layout
        // For uniform buffers, image samplers, etc.
        
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;  // for image sampling
        
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;  // for image sampling
        
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout)) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }
    
    // ================ createGraphicsPipeline() ================
    void createGraphicsPipeline() {
        // ===== Shader modules =====
        auto vertShaderCode = readFile(SOURCE_PATH + "shaders/vert.spv");
        auto fragShaderCode = readFile(SOURCE_PATH + "shaders/frag.spv");
        
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
        
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // function in shader to invoke
        vertShaderStageInfo.pSpecializationInfo = nullptr;  // for (optional) optimizations
        
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main"; // function in shader to invoke
        fragShaderStageInfo.pSpecializationInfo = nullptr;  // for (optional) optimizations
        
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // ===== Fixed-function states =====
        
        // --- Vertex input ---
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        // --- Input assembly ---
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;    // For _STRIP topologies, allows breaking up of primitives using a special element buffer index 0xFFFF or 0xFFFFFF
        
        // --- Viewport and scissor ---
        // Static (specify now)
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchainExtent.width);   // Swapchain size may differ from window size!
        viewport.height = static_cast<float>(swapchainExtent.height); // Swapchain size may differ from window size!
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0,0};
        scissor.extent = swapchainExtent;
        
        // Dynamic (specify in command buffer)
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        
        VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        
        // Static or dynamic viewport state
        VkPipelineViewportStateCreateInfo viewportStateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.scissorCount = 1;
        if (!DYNAMIC_VIEWPORT_SCISSOR) {
            viewportStateInfo.pViewports = &viewport;
            viewportStateInfo.pScissors = &scissor;
        }
        
        // --- Rasterizer ---
        VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
        rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationInfo.depthClampEnable = VK_FALSE; // Don't clamp, discard
        rasterizationInfo.rasterizerDiscardEnable = VK_FALSE; // Yes, please rasterize
        rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationInfo.lineWidth = 1.0f;
        rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // glm::perspective flips y coordinate!
        rasterizationInfo.depthBiasEnable = VK_FALSE; // No, don't alter depth values
        rasterizationInfo.depthBiasClamp = 0.0f;
        rasterizationInfo.depthBiasConstantFactor = 0.0f;
        rasterizationInfo.depthBiasSlopeFactor = 0.0f;
        
        // --- Multisampling ---
        VkPipelineMultisampleStateCreateInfo multisampleInfo{};
        multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleInfo.sampleShadingEnable = VK_FALSE;
        multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleInfo.minSampleShading = 1.0f;
        multisampleInfo.pSampleMask = nullptr;
        multisampleInfo.alphaToCoverageEnable = VK_FALSE;
        multisampleInfo.alphaToOneEnable = VK_FALSE;
    
        // --- Depth and stencil testing ---
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
        depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilInfo.depthTestEnable = VK_TRUE; // Test if new fragments should be discarded
        depthStencilInfo.depthWriteEnable = VK_TRUE; // Write depth values of passing fragments to the depth buffer
        depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE; // Can allow testing for bounds [minDepthBounds, maxDepthBounds]
        depthStencilInfo.minDepthBounds = 0.0f; // optional
        depthStencilInfo.maxDepthBounds = 1.0f; // optional
        depthStencilInfo.stencilTestEnable = VK_FALSE;
        depthStencilInfo.front = {}; // optional
        depthStencilInfo.back = {}; // optional
        
        // --- Color blending ---
        // Per framebuffer settings
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // Disable blending
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        // Global settings
        VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
        colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendInfo.logicOpEnable = VK_FALSE; // Disable bitwise blending; would supersede per-framebuffer blend settings
        colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;
        colorBlendInfo.blendConstants[0] = 0.0f;
        colorBlendInfo.blendConstants[1] = 0.0f;
        colorBlendInfo.blendConstants[2] = 0.0f;
        colorBlendInfo.blendConstants[3] = 0.0f;
        
        // ===== Pipeline layout =====
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{}; // For uniforms
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;  // uniform buffer, image sampler
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
        
        // ===== Graphics pipeline =====
        VkGraphicsPipelineCreateInfo graphicsPipelineInfo{};
        graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineInfo.stageCount = 2;
        graphicsPipelineInfo.pStages = shaderStages;
        graphicsPipelineInfo.pVertexInputState = &vertexInputInfo;
        graphicsPipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        graphicsPipelineInfo.pViewportState = &viewportStateInfo;
        graphicsPipelineInfo.pRasterizationState = &rasterizationInfo;
        graphicsPipelineInfo.pMultisampleState = &multisampleInfo;
        graphicsPipelineInfo.pDepthStencilState = &depthStencilInfo;
        graphicsPipelineInfo.pColorBlendState = &colorBlendInfo;
        if (DYNAMIC_VIEWPORT_SCISSOR) {
            graphicsPipelineInfo.pDynamicState = &dynamicStateInfo;
        } else {
            graphicsPipelineInfo.pDynamicState = nullptr;
        }
        graphicsPipelineInfo.layout = pipelineLayout;
        graphicsPipelineInfo.renderPass = renderPass;
        graphicsPipelineInfo.subpass = 0; // Index of subpass where this graphics pipeline will be used
        graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // If creating a pipeline derivative
        graphicsPipelineInfo.basePipelineIndex = -1; // If creating a pipeline derivative
        
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
        
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
    }
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
        
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
        
        return shaderModule;
    }
    
    // ================ createFramebuffers() ================
    void createFramebuffers() {
        // Create swap chain framebuffers to be used by render pass
        
        swapchainFramebuffers.resize(swapchainImageViews.size());
        
        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {swapchainImageViews[i], depthImageView}; // Note: Color attachment differs per swap chain image, but the same depth image can be used for all framebuffers since only a single subpass runs at a time. We need multiple swap chain images to buffer presentation, but only need a single depth image since only one framebuffer (and swap chain image) is being written to at a time.
            
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass; // Must be a compatible render pass (i.e., same number and type of attachments)
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapchainExtent.width;
            framebufferInfo.height = swapchainExtent.height;
            framebufferInfo.layers = 1;
            
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }
    
    // ================ createCommandPool() ================
    void createCommandPools() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        
        VkCommandPoolCreateInfo graphicsCommandPoolInfo{};
        graphicsCommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        graphicsCommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Since we will record a command buffer every frame, want to be able to reset and rerecord over it
        graphicsCommandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); // Command pools only support command buffers submitted on a single type of queue; in this case, the graphics queue family
        
        if (vkCreateCommandPool(device, &graphicsCommandPoolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
        
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            VkCommandPoolCreateInfo transferCommandPoolInfo{};
            transferCommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            transferCommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Since we will record a command buffer every frame, want to be able to reset and rerecord over it. TRANSIENT_BIT allows implementation to optimize memory allocation for short-lived buffers (i.e., the staging buffer)
            transferCommandPoolInfo.queueFamilyIndex = queueFamilyIndices.transferFamily.value(); // Command pools only support command buffers submitted on a single type of queue; in this case, the transfer queue family
            
            if (vkCreateCommandPool(device, &transferCommandPoolInfo, nullptr, &transferCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool!");
            }
        }
    }
    
    // ================ createDepthResources() ================
    void createDepthResources() {
        // Find format supported by physical device
        // Could alternatively hardcode VK_FORMAT_D32_SFLOAT because it is very commonly supported
        VkFormat format = findDepthFormat();
        
        // Create image and image view
        createImage(swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT);
        
        // Transition out of _UNDEFINED layout
        transitionImageLayout(depthImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
    VkFormat findDepthFormat() {
        return findSupportedFormat(
           {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
           VK_IMAGE_TILING_OPTIMAL,
           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
            if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
                // If linear tiling requested
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) {
                // If optimal tiling requested
                return format;
            }
        }
        throw std::runtime_error("Failed to find supported format!");
    }
    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
    
    // ================ createTextureImage() ================
    void createTextureImage() {
        // Load image data from file
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load((SOURCE_PATH + "textures/texture.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        
        if (!pixels) {
            throw std::runtime_error("Failed to load texture image!");
        }
        
        // Copy image data into staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);
        
        stbi_image_free(pixels);
        
        // Create texture image
        createImage(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
        
        // Transition image layout from its initial value (undefined) to transfer destination optimal
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // Copy staging buffer to texture image
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        // Transition image layout now for shader access
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        // Clean up
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory ) {
        // Create image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling; // How texels are laid out
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Whether texels are discarded or preserved on the first transition; can only be _UNDEFINED or _PREINITIALIZED
        imageInfo.usage = usage; // Will copy buffer into this image, and will sample it from fragment shader
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            QueueFamilyIndices queueFamilies = findQueueFamilies(physicalDevice);
            std::array<uint32_t, 2> queueFamilyIndices = {queueFamilies.graphicsFamily.value(), queueFamilies.transferFamily.value()};

            imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT; // If graphics and transfer families are separate
            imageInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            imageInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }
        else
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Only will be used by one queue family (graphics+transfer)
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // For multi-sampling; only for attachments
        imageInfo.flags = 0;
        
        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }
        
        // Allocate memory for the image
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device, image, &memoryRequirements);
        
        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties); // Review this
        
        if (vkAllocateMemory(device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }
        
        // Bind allocated memory to the image
        vkBindImageMemory(device, image, imageMemory, 0);
    }
    VkCommandBuffer beginSingleTimeCommands() {
        // Allocate command buffer
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            allocateInfo.commandPool = transferCommandPool; // May want to create a separate command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT set
        } else {
            allocateInfo.commandPool = graphicsCommandPool; // May want to create a separate command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT set
        }
        allocateInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
        
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        return commandBuffer;
    }
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        // End recording command buffer
        vkEndCommandBuffer(commandBuffer);
        
        // Submit command buffer to appropriate queue, then free it
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(transferQueue); // Could also use vkWaitForFences
            vkFreeCommandBuffers(device, transferCommandPool, 1, &commandBuffer);
        } else {
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue); // Could also use vkWaitForFences
            vkFreeCommandBuffers(device, graphicsCommandPool, 1, &commandBuffer);
        }
    }
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        // Execute the transition using a command buffer
        
        // 1. Allocate and begin
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // 2. Record
        // Define a pipeline barrier (memory dependency) for the image
        VkImageMemoryBarrier barrier{}; // A pipeline barrier; generally used for synchronization (finish write before read), but can be used to transition image layouts and transfer queue family ownership. VkBufferMemoryBarrier also exists for buffers.
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout; // or VK_IMAGE_LAYOUT_UNDEFINED
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Earliest possible pipeline stage
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // Not an actual stage; a pseudo-stage where transfers happen
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // Not an actual stage; a pseudo-stage where transfers happen
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }
        
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier); // Specify pipeline stages which occur before the barrier, and stages which wait on the barrier
        
        // 3. End, submit, and free
        endSingleTimeCommands(commandBuffer);
    }
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        // Execute the copy using a command buffer
        
        // 1. Allocate and begin
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // 2. Record
        // Copy buffer region to image region
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0; // 0 indicates tightly packed; nonzero if there are padding bytes between rows of the image
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region); // Assumes image has already been transitioned to the specified layout
        
        // 3. End, submit, and free
        endSingleTimeCommands(commandBuffer);
    }
    
    // ================ createTextureImageView() ================
    void createTextureImageView() {
        textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
//        createInfo.components.r = VK_COMPONENT_SWIZZLE_R; // Unneeded because VK_COMPONENT_SWIZZLE_IDENTITY = 0
//        createInfo.components.g = VK_COMPONENT_SWIZZLE_G; // Unneeded because VK_COMPONENT_SWIZZLE_IDENTITY = 0
//        createInfo.components.b = VK_COMPONENT_SWIZZLE_B; // Unneeded because VK_COMPONENT_SWIZZLE_IDENTITY = 0
//        createInfo.components.a = VK_COMPONENT_SWIZZLE_A; // Unneeded because VK_COMPONENT_SWIZZLE_IDENTITY = 0
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }
        
        return imageView;
    }
    
    // ================ createTextureSampler() ================
    void createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // Can be black, white, or transparent
        samplerInfo.anisotropyEnable = VK_TRUE;
        
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        
        samplerInfo.unnormalizedCoordinates = VK_FALSE; // We want [0, 1) not [0, texWidth)
        
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS; // Used for PCF on shadow maps
        
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        
        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }
    
    // ================ createVertexBuffer() ================
    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        
        // Create/allocate the staging buffer - on CPU
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        // Fill the staging buffer
        // - Can also fill the vertex buffer directly if HOST_COHERENT_BIT and HOST_VISIBLE_BIT were set on it. The HOST_COHERENT_BIT ensures allocated memory in memory heap matches the mapped memory (i.e., there are no delays due to caching). Can use VkFlushMappedMemoryRanges and VkInvalidateMappedMemoryRanges to control transfer to GPU. Otherwise, transfer to GPU occurs in the background and specification guarantees that this is completed as of the next VkQueueSubmit call.
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);  // Temporarily map bufferMemory to data ptr. Can also use VK_WHOLE_SIZE?
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);
        
        // Create/allocate vertex buffer - local to GPU
        // - Can't map, but can transfer (copy) data into it
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        
        // Copy from staging buffer (CPU) to vertex buffer (GPU)
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
        
        // Free staging buffer memory
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            QueueFamilyIndices queueFamilies = findQueueFamilies(physicalDevice);
            std::array<uint32_t, 2> queueFamilyIndices = {queueFamilies.graphicsFamily.value(), queueFamilies.transferFamily.value()};

            bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        } else {
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Exclusive access by graphics queue
        }
        
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer!");
        }
        
        // Allocate memory for the buffer
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
        
        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);
        
        // Realistically, should combine buffers and call this on groups of buffers (i.e., objects) so that the maxMemoryAllocationCount physical device limit isn't reached
        // - Can use VulkanMemoryAllocator library for this
        if (vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate vertex buffer memory!");
        }
        
        // Bind the allocated memory to the buffer
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties)) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        // Execute the copy using a command buffer
        
        // 1. Allocate and begin recording command buffer
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        // 2. Record command buffer
        // Copy buffer region to buffer region
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        
        // 3. End recording command buffer, submit, then free it
        endSingleTimeCommands(commandBuffer);
    }
    
    // ================ createIndexBuffer() ================
    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        
        // Create/allocate the staging buffer - on CPU
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        // Fill the staging buffer
        // - Can also fill the index buffer directly if HOST_COHERENT_BIT and HOST_VISIBLE_BIT were set on it. The HOST_COHERENT_BIT ensures allocated memory in memory heap matches the mapped memory (i.e., there are no delays due to caching). Can use VkFlushMappedMemoryRanges and VkInvalidateMappedMemoryRanges to control transfer to GPU. Otherwise, transfer to GPU occurs in the background and specification guarantees that this is completed as of the next VkQueueSubmit call.
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);  // Temporarily map bufferMemory to data ptr. Can also use VK_WHOLE_SIZE?
        memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);
        
        // Create/allocate index buffer - local to GPU
        // - Can't map, but can transfer (copy) data into it
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
        
        // Copy from staging buffer (CPU) to index buffer (GPU)
        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
        
        // Free staging buffer memory
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    
    // ================ createCommandBuffer() ================
    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
            
            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);   // "Persistent" mapping
        }
    }
    
    void createDescriptorPool() {
        // Contains descriptor sets
        // Used for uniform buffers, image samplers, etc.
        
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); // Depending on driver, may allow allocation beyond this count. Should use Best Practices Validation to avoid exceeding this limit!
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); // Depending on driver, may allow allocation beyond this count. Should use Best Practices Validation to avoid exceeding this limit!
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }
    void createDescriptorSets() {
        // Reside in descriptor pools; use descriptor set layouts
        // Used for uniform buffers, image samplers, etc.
        
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = descriptorPool;
        allocateInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocateInfo.pSetLayouts = layouts.data();
        
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            // Uniform buffer data
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject); // or VK_WHOLE_SIZE
            
            // Image sampler data
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;
            
            // Update descriptor set i
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            // Uniform buffer descriptor info
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0; // The static binding we specified in the descriptor set layout
            descriptorWrites[0].dstArrayElement = 0; // More relevant if we had an array of descriptors (UBOs)
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1; // How many array elements to update
            descriptorWrites[0].pBufferInfo = &bufferInfo;
            descriptorWrites[0].pImageInfo = nullptr; // For descriptors that refer to image data
            descriptorWrites[0].pTexelBufferView = nullptr; // For descriptors that refer to buffer views
            // Image sampler descriptor info
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1; // The static binding we specified in the descriptor set layout
            descriptorWrites[1].dstArrayElement = 0; // More relevant if we had an array of descriptors
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1; // How many array elements to update
            descriptorWrites[1].pBufferInfo = nullptr;
            descriptorWrites[1].pImageInfo = &imageInfo; // For descriptors that refer to image data
            descriptorWrites[1].pTexelBufferView = nullptr; // For descriptors that refer to buffer views
            
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr); // Can also use to copy descriptors to each other
        }
    }
    
    // ================ createCommandBuffers() ================
    void createCommandBuffers() {
        graphicsCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        
        VkCommandBufferAllocateInfo graphicsAllocateInfo{};
        graphicsAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        graphicsAllocateInfo.commandPool = graphicsCommandPool;
        graphicsAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Can be submitted directly to a queue, can't be called from other command buffers
        graphicsAllocateInfo.commandBufferCount = static_cast<uint32_t>(graphicsCommandBuffers.size());
        
        if (vkAllocateCommandBuffers(device, &graphicsAllocateInfo, graphicsCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
        
        if (SEPARATE_TRANSFER_QUEUE_FAMILY) {
            transferCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            
            VkCommandBufferAllocateInfo transferAllocateInfo{};
            transferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            transferAllocateInfo.commandPool = transferCommandPool;
            transferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Can be submitted directly to a queue, can't be called from other command buffers
            transferAllocateInfo.commandBufferCount = static_cast<uint32_t>(transferCommandBuffers.size());
            
            if (vkAllocateCommandBuffers(device, &transferAllocateInfo, transferCommandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate command buffers!");
            }
        }
    }
    
    // ================ createSyncObjects() ================
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
               
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state so that the very first VkWaitForFences call doesn't block

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
                || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create semaphores for a frame!");
            
            if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fence for a frame!");
            }
        }
    }
    
    // ================ drawFrame() ================
    void drawFrame() {
        // Many Vulkan API calls for the GPU are asynchronous.
        // Therefore, synchronization of GPU execution must be explicitly specified to enforce order of operations.
        // - VkSemaphore - specify order of GPU operations (GPU blocks for GPU)
        // - VkFence - synchronize CPU and GPU (CPU blocks for GPU)
        
        // 1. Wait for previous frame to finish (when previous command buffer finishes execution)
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); // Blocks
        
        // 2. Acquire an image from the swap chain
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Can't check framebufferResized here because then imageAvailableSemaphore is already signaled
            // I.e., this frame's vkQueueSubmit would never get to wait for (and subsequently reset) imageAvailableSemaphore
            recreateSwapchain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }
        vkResetFences(device, 1, &inFlightFences[currentFrame]); // To prevent deadlock, don't reset fence (put back in unsignaled state) until we know vkQueueSubmit will be called and signal it
        
        // ~. Update uniform buffer
        updateUniformBuffer(currentFrame);
        
        // 3. Record a command buffer to draw the scene onto that image
        vkResetCommandBuffer(graphicsCommandBuffers[currentFrame], 0);
        recordCommandBuffer(graphicsCommandBuffers[currentFrame], imageIndex);
        
        // 4. Submit the recorded command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        // GPU must wait to write colors to the image until it is available, but is allowed to execute other pipeline stages anytime
        // Can also include VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT instead of creating a subpass source dependency on the swap chain image as done currently in createRenderPass()
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];
        
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        // Submit current frame's command buffer to graphics queue, then signal...
        // - semaphore for current frame's (at index 'currentFrame') image (at arbitrary index 'imageIndex') to be presented
        // - fence for next frame (at index 'currentFrame') to begin acquiring an image (at arbitrary index 'imageIndex') and recording its command buffer (at index 'currentFrame')
        // I.e., frame and swapchain image/framebuffer are decoupled; allows for x frames in flight and y swap chain images/framebuffers to be used
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }
        
        // 5. Present the swap chain image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapchains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Allows you to check presentation result of each swap chain
        
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            // Some platforms don't trigger VK_ERROR_OUT_OF_DATE_KHR after window resize
            // Therefore, need to also explicitly check based on GLFW's callback
            recreateSwapchain();
            framebufferResized = false;
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }
        
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        // Begin recording command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr; // Only for secondary command buffers
        
        if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
        renderPassBeginInfo.renderArea.offset = {0,0};
        renderPassBeginInfo.renderArea.extent = swapchainExtent;
        
        std::array<VkClearValue, 2> clearValues;
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassBeginInfo.pClearValues = clearValues.data(); // Used for VK_ATTACHMENT_LOAD_OP_CLEAR of the framebuffer's color and depth attachments
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // Embed render pass commands in primary command buffer without executing any secondary command buffers
        
        // Graphics
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        
        // Viewport and scissor
        if (DYNAMIC_VIEWPORT_SCISSOR) {
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(swapchainExtent.width);
            viewport.height = static_cast<float>(swapchainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            
            VkRect2D scissor{};
            scissor.offset = {0,0};
            scissor.extent = swapchainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        }
        
        // Bind buffers
        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
        
        // Draw!
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0); // Use vkCmdDraw for non-indexed drawing
        
        // End render pass
        vkCmdEndRenderPass(commandBuffer);
        
        // End command buffer
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end recording command buffer!");
        }
    }
    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;   // OpenGL's y clip coordinate is flipped?
        
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));  // For frequently changing values, use push constants instead of a UBO this way
    }
    void recreateSwapchain() {
        int iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED);
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        while (iconified == GLFW_TRUE || width == 0 || height == 0) {
            glfwWaitEvents();
            iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED);
            glfwGetWindowSize(window, &width, &height);
        }
        
        vkDeviceWaitIdle(device);
        
        cleanupSwapchain();
        
        createSwapchain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
    }
    
    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file!");
        }
        
        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        
        file.close();
        
        return buffer;
    }
};

int main() {
    HelloTriangleApplication app;
    
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
