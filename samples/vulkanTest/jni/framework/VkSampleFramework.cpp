//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "VkSampleFramework.h"

// Need some defines for Validation layers
// #define USE_VALIDATION_LAYERS
// #define USE_VALIDATION_VERBOSE

// Functions we have to get
PFN_vkGetMemoryFdKHR                                g_vkGetMemoryFdKHR = NULL;
PFN_vkGetSemaphoreFdKHR                             g_vkGetSemaphoreFdKHR = NULL;
PFN_vkGetFenceFdKHR                                 g_vkGetFenceFdKHR = NULL;

PFN_vkGetPhysicalDeviceImageFormatProperties2KHR    g_vkGetPhysicalDeviceImageFormatProperties2KHR = NULL;


VkSampleFramework::VkSampleFramework(const char* name, uint32_t version)
{
    mSampleName     = name;
    mSampleVersion  = version;
    mWidth = 0;
    mHeight = 0;
    InitializeValues();
}

///////////////////////////////////////////////////////////////////////////////

VkSampleFramework::VkSampleFramework()
{
    mSampleName = "Unnamed Sample";
    mSampleVersion = 1;
    mWidth = 0;
    mHeight = 0;
    InitializeValues();
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitializeValues()
{
    // Instance Extensions
    mEnabledInstanceExtensionCount = 0;
    for(uint32_t whichOne = 0; whichOne < 16; whichOne++)
        mInstanceExtensionNames[whichOne] = NULL;

    // Instance Layers
    mEnabledInstanceLayerCount = 0;

    // Validation Callbacks
    mCreateDebugReportCallbackEXT = NULL;
    mDestroyDebugReportCallbackEXT = NULL;
    mDebugReportMessageCallback = NULL;
    mDebugReportCallback = (VkDebugReportCallbackEXT)NULL;

    // Android objects
    mAndroidWindow = NULL;
    mAssetManager = NULL;

    // Vulkan objects
    mInstance = VK_NULL_HANDLE;
    mpPhysicalDevices = NULL;
    mPhysicalDevice = VK_NULL_HANDLE;
    memset(&mPhysicalDeviceProperties, 0, sizeof(mPhysicalDeviceProperties));
    memset(&mPhysicalDeviceMemoryProperties, 0, sizeof(mPhysicalDeviceMemoryProperties));
    mDevice = VK_NULL_HANDLE;
    mPhysicalDeviceCount = 0;
    mQueueFamilyIndex = 0;
    mQueue = VK_NULL_HANDLE;
    mSurface = (VkSurfaceKHR)0;
    memset(&mSurfaceFormat, 0, sizeof(mSurfaceFormat));
    mCommandPool = VK_NULL_HANDLE;

    // This shared cmdBuffer is used for setup operations on e.g. textures
    mSetupCommandBuffer = VK_NULL_HANDLE;

    // Swapchain
    mSwapchain = VK_NULL_HANDLE;
    mSwapchainBuffers = NULL;
    mSwapchainCurrentIdx = 0;
    mSwapchainImageCount = 0;
    mDepthBuffers = NULL;

    // Vulkan Synchronization objects
    mBackBufferSemaphore = VK_NULL_HANDLE;
    mRenderCompleteSemaphore = VK_NULL_HANDLE;
    mFence = VK_NULL_HANDLE;

    // Render size
    mHeight = 0;
    mWidth = 0;

    // Time
    mFrameTimeBegin = 0;
    mFrameTimeEnd = 0;
    mFrameTimeDelta = 0;
    mFrameIdx = 0;

    // Logging
    mbLogFPS = true;

    // State
    mInitialized = false;
    mInitBegun = false;

    // Sample version and name (Already initialized above)
    // mSampleVersion;
    // mSampleName;

    // Memory Allocation
    mMemoryAllocator = NULL;
    mUseMemoryAllocator = false;

    // Validation
#ifdef USE_VALIDATION_LAYERS
    mUseValidation = true;
#else
    mUseValidation = false;
#endif

}

///////////////////////////////////////////////////////////////////////////////

VkSampleFramework::~VkSampleFramework()
{
    if (mInitBegun)
    {
        TearDown();
    }
}

///////////////////////////////////////////////////////////////////////////////

// Initializes the Vulkan subsystem to a default sample state.
bool VkSampleFramework::Initialize(ANativeWindow* window)
{
    VkResult ret = VK_SUCCESS;

    if (mInitBegun)
    {
        TearDown();
    }

    mWidth  = ANativeWindow_getWidth(window);
    mHeight = ANativeWindow_getHeight(window);
    WindowResize(mWidth,mHeight);

    // mInitBegun acts as a signal that a partial teardown may be
    // needed, regardless of mInitialized state.
    mInitBegun = true;

    // The android window to render to is passed, we must use this
    // later in the initialization sequence.
    mAndroidWindow = window;

    // Create the memory allocator
    mMemoryAllocator = new MemoryAllocator();
    mMemoryAllocator->SetSample(this);

    CreateInstance();
    GetPhysicalDevices();

    InitDevice();
    // DELETE_THIS InitSwapchain();
    InitCommandbuffers();
    InitSync();
    // DELETE_THIS InitSwapchainLayout();

    // Hand over the rest of initialization to the subclass
    InitSample();

    // We acquire the next swap chain image, in preparation for the render loop
    // DELETE_THIS SetNextBackBuffer();

    // Keep track of frame index and time
    mFrameIdx = 0;
    mFrameTimeBegin = GetTime();

    // Initialized!
    mInitialized = true;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

// Check for instance extensions that are supported by Vulkan
bool VkSampleFramework::CheckInstanceExtensions()
{
    VkResult ret = VK_SUCCESS;

    // Discover the number of extensions listed in the instance properties in order to allocate
    // a buffer large enough to hold them.
    uint32_t numInstanceExtensions = 0;
    ret = vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtensions, nullptr);
    VK_CHECK(!ret);
    VkExtensionProperties* instanceExtensions = nullptr;
    instanceExtensions = new VkExtensionProperties[numInstanceExtensions];

    // Now request instanceExtensionCount VkExtensionProperties elements be read into out buffer
    ret = vkEnumerateInstanceExtensionProperties(nullptr, &numInstanceExtensions, instanceExtensions);
    VK_CHECK(!ret);

    // We're looking for these 3 extensions
    VkBool32 surfaceExtFound            = 0;
    VkBool32 platformSurfaceExtFound    = 0;
    VkBool32 debugReportExtFound        = 0;

    // We require at least two extensions, VK_KHR_surface and VK_KHR_android_surface. If they are found,
    // add them to the extensionNames list that we'll use to initialize our instance with later.
    // If validation is enabled, another extension VK_EXT_debug_report is required.
    mEnabledInstanceExtensionCount = 0;

    for (uint32_t i = 0; i <numInstanceExtensions; i++)
    {
        if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instanceExtensions[i].extensionName))
        {
            surfaceExtFound = 1;
            mInstanceExtensionNames[mEnabledInstanceExtensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
        }

        if (!strcmp(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, instanceExtensions[i].extensionName))
        {
            platformSurfaceExtFound = 1;
            mInstanceExtensionNames[mEnabledInstanceExtensionCount++] = VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
        }

#ifdef USE_VALIDATION_LAYERS
        if (mUseValidation)
        {
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instanceExtensions[i].extensionName))
            {
                debugReportExtFound = 1;
                mInstanceExtensionNames[mEnabledInstanceExtensionCount++] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            }
        }
#endif // USE_VALIDATION_LAYERS
    }
    VK_CHECK(mEnabledInstanceExtensionCount < 16);

    if (!surfaceExtFound)
    {
        LOGE("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_SURFACE_EXTENSION_NAME" extension.");
        return false;
    }
    if (!platformSurfaceExtFound)
    {
        LOGE("vkEnumerateInstanceExtensionProperties failed to find the " VK_KHR_ANDROID_SURFACE_EXTENSION_NAME" extension.");
        return false;
    }

#ifdef USE_VALIDATION_LAYERS
    if (mUseValidation && !debugReportExtFound)
    {
        LOGE("vkEnumerateInstanceExtensionProperties failed to find the " VK_EXT_DEBUG_REPORT_EXTENSION_NAME" extension.");
        return false;
    }
#endif // USE_VALIDATION_LAYERS

    delete []instanceExtensions;

    return true;
}

//////////////////////////////////////////////////////////////////////////////

// As per Google: https://developer.android.com/ndk/guides/graphics/validation-layer.html
// these are the supported instance layers.
const char*  VkSampleFramework::mInstanceLayers[] =
{
        "VK_LAYER_GOOGLE_threading",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_core_validation",
        //"VK_LAYER_LUNARG_device_limits", is not include in Android NDK  13.1
        "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects",
};

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::CheckInstanceLayerValidation()
{
    // Determine the number of instance layers that Vulkan reports
    uint32_t numInstanceLayers = 0;
    vkEnumerateInstanceLayerProperties(&numInstanceLayers, nullptr);

    // Enumerate instance layers with valid pointer in last parameter
    VkLayerProperties* layerProperties = (VkLayerProperties*)malloc(numInstanceLayers * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&numInstanceLayers, layerProperties);

    LOGI("****************************************");
    LOGI("Found %d Validation Layers:", numInstanceLayers);
    for (uint32_t j = 0; j < numInstanceLayers; j++)
    {
        LOGI("    %s", layerProperties[j].layerName);
    }
    LOGI("****************************************");

    // Make sure the desired instance validation layers are available
    // NOTE:  These are not listed in an arbitrary order.  Threading must be
    //        first, and unique_objects must be last.  This is the order they
    //        will be inserted by the loader.
    mEnabledInstanceLayerCount =  sizeof(mInstanceLayers) / sizeof(mInstanceLayers[0]);
    for (uint32_t i = 0; i < mEnabledInstanceLayerCount; i++)
    {
        bool found = false;
        for (uint32_t j = 0; j < numInstanceLayers; j++)
        {
            if (strcmp(mInstanceLayers[i], layerProperties[j].layerName) == 0)
            {
                found = true;
            }
        }
        if (!found)
        {
            LOGE("Instance Layer not found: %s", mInstanceLayers[i]);
            return false;
        }
    }

    return true;
}

const char *L_ReportObjectString(VkDebugReportObjectTypeEXT objType)
{
    switch (objType)
    {
    case VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT:
        return "UNKNOWN";
    case VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT:
        return "INSTANCE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT:
        return "PHYSICAL_DEVICE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT:
        return "DEVICE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT:
        return "QUEUE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT:
        return "SEMAPHORE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT:
        return "COMMAND_BUFFER";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT:
        return "FENCE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT:
        return "DEVICE_MEMORY";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT:
        return "BUFFER";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT:
        return "IMAGE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT:
        return "EVENT";
    case VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT:
        return "QUERY_POOL";
    case VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT:
        return "BUFFER_VIEW";
    case VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT:
        return "IMAGE_VIEW";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT:
        return "SHADER_MODULE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT:
        return "PIPELINE_CACHE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT:
        return "PIPELINE_LAYOUT";
    case VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT:
        return "RENDER_PASS";
    case VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT:
        return "PIPELINE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT:
        return "DESCRIPTOR_SET_LAYOUT";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT:
        return "SAMPLER";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT:
        return "DESCRIPTOR_POOL";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT:
        return "DESCRIPTOR_SET";
    case VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT:
        return "FRAMEBUFFER";
    case VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT:
        return "COMMAND_POOL";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT:
        return "SURFACE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT:
        return "SWAPCHAIN";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT:
        return "DEBUG_REPORT_CALLBACK";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT:
        return "DISPLAY";
    case VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT:
        return "DISPLAY_MODE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_OBJECT_TABLE_NVX_EXT:
        return "OBJECT_TABLE";
    case VK_DEBUG_REPORT_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX_EXT:
        return "INDIRECT_COMMANDS_LAYOUT";
    }

    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////////
// Validation call back.  We Filter the message and log.
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
        VkDebugReportFlagsEXT msgFlags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObject, size_t location,
        int32_t msgCode, const char * pLayerPrefix,
        const char * pMsg, void * pUserData )
{
    // Flags are a bit-mask and it is unclear if more than one can be set
    char flagString[512];
    memset(flagString, 0, sizeof(flagString));
    sprintf(flagString, "Flags (0x%x): ", msgFlags);

#if defined(USE_VALIDATION_LAYERS) && !defined (USE_VALIDATION_VERBOSE)
    if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        // We don't care about information messages
        return VK_FALSE;
    }
#endif

    if(msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        sprintf(&flagString[strlen(flagString)], "Info ");
    if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
        sprintf(&flagString[strlen(flagString)], "Warn ");
    if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        sprintf(&flagString[strlen(flagString)], "Perf ");
    if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        sprintf(&flagString[strlen(flagString)], "Error ");
    if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        sprintf(&flagString[strlen(flagString)], "Debug ");

    const char *pObjType = L_ReportObjectString(objType);

    LOGI("Validation (%s: %s): [%s] (Code %d) : %s", pObjType, flagString, pLayerPrefix, msgCode, pMsg);

    // The callback returns a VkBool32 that indicates to the calling layer if the Vulkan call should be aborted or not. 
    // Applications should always return VK_FALSE so that they see the same behavior with and without validation layers enabled.
    // If the application returns VK_TRUE from its callback and the Vulkan call being aborted returns a VkResult, the layer 
    // will return VK_ERROR_VALIDATION_FAILED_EXT
    return VK_FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Using the ReportCallback Extensions, we'll create validation callbacks.
void VkSampleFramework::CreateValidationCallbacks()
{
    mCreateDebugReportCallbackEXT   = (PFN_vkCreateDebugReportCallbackEXT)  vkGetInstanceProcAddr( mInstance, "vkCreateDebugReportCallbackEXT");
    mDestroyDebugReportCallbackEXT  = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr( mInstance, "vkDestroyDebugReportCallbackEXT");
    mDebugReportMessageCallback     = (PFN_vkDebugReportMessageEXT)         vkGetInstanceProcAddr( mInstance, "vkDebugReportMessageEXT");

    VK_CHECK(mCreateDebugReportCallbackEXT);
    VK_CHECK(mDestroyDebugReportCallbackEXT);
    VK_CHECK(mDebugReportMessageCallback);

    // Create the debug report callback..
    VkDebugReportCallbackCreateInfoEXT dbgCreateInfo;
    dbgCreateInfo.sType         = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    dbgCreateInfo.pNext         = NULL;
    dbgCreateInfo.pfnCallback   = DebugReportCallback;
    dbgCreateInfo.pUserData     = NULL;
    dbgCreateInfo.flags         =   VK_DEBUG_REPORT_ERROR_BIT_EXT               |
                                    VK_DEBUG_REPORT_WARNING_BIT_EXT             |
                                    VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                                    #if defined(USE_VALIDATION_LAYERS) && defined (USE_VALIDATION_VERBOSE)
                                        VK_DEBUG_REPORT_INFORMATION_BIT_EXT         |
                                    #endif
                                    VK_DEBUG_REPORT_DEBUG_BIT_EXT;

    VkResult ret = mCreateDebugReportCallbackEXT(mInstance, &dbgCreateInfo, NULL, &mDebugReportCallback);
    VK_CHECK(!ret);
}

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::CreateInstance()
{
    VkResult ret = VK_SUCCESS;

    VK_CHECK(CheckInstanceExtensions());

    // We specify the Vulkan version our application was built with,
    // as well as names and versions for our application and engine,
    // if applicable. This allows the driver to gain insight to what
    // is utilizing the vulkan driver, and serve appropriate versions.
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType               = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext               = nullptr;
    applicationInfo.pApplicationName    = mSampleName;
    applicationInfo.applicationVersion  = mSampleVersion;
    applicationInfo.pEngineName         = "VkSample";
    applicationInfo.engineVersion       = 1;
    applicationInfo.apiVersion          = VK_API_VERSION_1_0;

    // Check the validation layers if we're using them
    mEnabledInstanceLayerCount = 0;
#ifdef USE_VALIDATION_LAYERS
    if (mUseValidation && !CheckInstanceLayerValidation())
    {
        return false;
    }
#endif // USE_VALIDATION_LAYERS

    // Creation information for the instance points to details about
    // the application, and also the list of extensions to enable.
    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType                    = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext                    = nullptr;
    instanceCreateInfo.pApplicationInfo         = &applicationInfo;
    instanceCreateInfo.enabledLayerCount        = mEnabledInstanceLayerCount;
    instanceCreateInfo.ppEnabledLayerNames      = mEnabledInstanceLayerCount==0 ? nullptr : mInstanceLayers;
    instanceCreateInfo.enabledExtensionCount    = mEnabledInstanceExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames  = mInstanceExtensionNames;

    // The main Vulkan instance is created with the creation infos above.
    // We do not specify a custom memory allocator for instance creation.
    ret = vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);

    // Vulkan API return values can expose further information on a failure.
    // For instance, INCOMPATIBLE_DRIVER may be returned if the API level
    // an application is built with, exposed through VkApplicationInfo, is
    // newer than the driver present on a device.
    if (ret == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        LOGE("Cannot find a compatible Vulkan installable client driver: vkCreateInstance Failure");
        return false;
    }
    else if (ret == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        LOGE("Cannot find a specified extension library: vkCreateInstance Failure");
        return false;
    }
    else
    {
        VK_CHECK(!ret);
    }

    // Query for functions we may need
    if (g_vkGetMemoryFdKHR == NULL)
    {
        LOGI("**************************************************");
        LOGI("Initializing Function: vkGetMemoryFdKHR");
        LOGI("**************************************************");
        g_vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetInstanceProcAddr(mInstance, "vkGetMemoryFdKHR");
        if (!g_vkGetMemoryFdKHR)
        {
            LOGE("Failed to get proc address for vkGetMemoryFdKHR!!");
        }
    }

    if (g_vkGetSemaphoreFdKHR == NULL)
    {
        LOGI("**************************************************");
        LOGI("Initializing Function: vkGetSemaphoreFdKHR");
        LOGI("**************************************************");
        g_vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)vkGetInstanceProcAddr(mInstance, "vkGetSemaphoreFdKHR");
        if (!g_vkGetSemaphoreFdKHR)
        {
            LOGE("Failed to get proc address for vkGetSemaphoreFdKHR!!");
        }
    }

    if (g_vkGetFenceFdKHR == NULL)
    {
        LOGI("**************************************************");
        LOGI("Initializing Function: vkGetFenceFdKHR");
        LOGI("**************************************************");
        g_vkGetFenceFdKHR = (PFN_vkGetFenceFdKHR)vkGetInstanceProcAddr(mInstance, "vkGetFenceFdKHR");
        if (!g_vkGetFenceFdKHR)
        {
            LOGE("Failed to get proc address for vkGetFenceFdKHR!!");
        }
    }

    if (g_vkGetPhysicalDeviceImageFormatProperties2KHR == NULL)
    {
        LOGI("**************************************************");
        LOGI("Initializing Function: vkGetPhysicalDeviceImageFormatProperties2KHR");
        LOGI("**************************************************");
        g_vkGetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceImageFormatProperties2KHR");
        if (!g_vkGetPhysicalDeviceImageFormatProperties2KHR)
        {
            LOGE("Failed to get proc address for vkGetPhysicalDeviceImageFormatProperties2KHR!!");
        }
    }



    // Create the callbacks if we're using them
#ifdef USE_VALIDATION_LAYERS
    if (mUseValidation)
    {
        CreateValidationCallbacks();
    }
#endif // USE_VALIDATION_LAYERS

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::GetPhysicalDevices()
{
    VkResult ret = VK_SUCCESS;

    // Query number of physical devices available
    ret = vkEnumeratePhysicalDevices(mInstance, &mPhysicalDeviceCount, nullptr);
    VK_CHECK(!ret);

    if (mPhysicalDeviceCount == 0)
    {
        LOGE("No physical devices detected.");
        return false;
    }

    // Allocate space the the correct number of devices, before requesting their data
    mpPhysicalDevices = new VkPhysicalDevice[mPhysicalDeviceCount];
    ret = vkEnumeratePhysicalDevices(mInstance, &mPhysicalDeviceCount, mpPhysicalDevices);
    VK_CHECK(!ret);

    // For purposes of this sample, we simply use the first device.
    mPhysicalDevice = mpPhysicalDevices[0];

    // By querying the device properties, we learn the device name, amongst
    // other details.
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mPhysicalDeviceProperties);

    LOGI("Vulkan Device: %s", mPhysicalDeviceProperties.deviceName);

    // Get Memory information and properties - this is required later, when we begin
    // allocating buffers to store data.
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mPhysicalDeviceMemoryProperties);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitDevice()
{
    VkResult ret = VK_SUCCESS;

    // Akin to when creating the instance, we can query extensions supported by the physical device
    // that we have selected to use.
    uint32_t deviceExtensionCount = 0;
    VkExtensionProperties *device_extensions = nullptr;
    ret = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deviceExtensionCount, nullptr);
    VK_CHECK(!ret);

    VkBool32 swapchainExtFound = 0;
    VkExtensionProperties* deviceExtensions = new VkExtensionProperties[deviceExtensionCount];
    ret = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &deviceExtensionCount, deviceExtensions);
    VK_CHECK(!ret);

    // For our example, we require the swapchain device extension, which is used to present backbuffers efficiently
    // to the users screen.
    uint32_t enabledExtensionCount = 0;
    const char* extensionNames[16] = {0};
    for (uint32_t i = 0; i < deviceExtensionCount; i++)
    {
        if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, deviceExtensions[i].extensionName))
        {
            swapchainExtFound = 1;
            extensionNames[enabledExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        }
        VK_CHECK(enabledExtensionCount < 16);
    }
    if (!swapchainExtFound)
    {
        LOGE("vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME " extension: vkCreateInstance Failure");

        // Always attempt to enable the swapchain
        extensionNames[enabledExtensionCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }

    // Don't need to call this since everything is sent directly to SVR.
    // If validation is enabled we MIGHT want to call it.  Calling it 
    // when not testing validation layer seems to cause other problems
    // so it is disabled here.
    // InitSurface();

    // Before we create our main Vulkan device, we must ensure our physical device
    // has queue families which can perform the actions we require. For this, we request
    // the number of queue families, and their properties.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);
    VkQueueFamilyProperties* queueProperties = new VkQueueFamilyProperties[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, queueProperties);
    VK_CHECK(queueFamilyCount >= 1);

    // We query each queue family in turn for the ability to support the android surface
    // that was created earlier. We need the device to be able to present its images to
    // this surface, so it is important to test for this.
    VkBool32* supportsPresent = new VkBool32[queueFamilyCount];
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        // This actually causes a message if validation layer is enabled because "mSurface"
        // has not been initialized by InitSurface()
        // LOGI("Validation: Queue %d asking for surface (0x%x) support", i, (unsigned int)mSurface);
        vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &supportsPresent[i]);
    }

    // Search for a graphics queue, and ensure it also supports our surface. We want a
    // queue which can be used for both, as to simplify operations.
    uint32_t queueIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        if ((queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (supportsPresent[i] == VK_TRUE)
            {
                queueIndex = i;
                break;
            }
        }
    }

    delete [] supportsPresent;
    delete [] queueProperties;

    if (queueIndex == UINT32_MAX)
    {
        VK_CHECK("Could not obtain a queue family for both graphics and presentation." && 0);
    }

    // We have identified a queue family which both supports our android surface,
    // and can be used for graphics operations.
    mQueueFamilyIndex = queueIndex;

    // As we create the device, we state we will be creating a queue of the
    // family type required. 1.0 is the highest priority and we use that.
    float queuePriorities[1] = { 1.0 };
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
    deviceQueueCreateInfo.sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext             = nullptr;
    deviceQueueCreateInfo.queueFamilyIndex  = mQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount        = 1;
    deviceQueueCreateInfo.pQueuePriorities  = queuePriorities;

    // Now we pass the queue create info, as well as our requested extensions,
    // into our DeviceCreateInfo structure.
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType                      = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext                      = nullptr;
    deviceCreateInfo.queueCreateInfoCount       = 1;
    deviceCreateInfo.pQueueCreateInfos          = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount          = 0;
    deviceCreateInfo.ppEnabledLayerNames        = nullptr;
    deviceCreateInfo.enabledExtensionCount      = enabledExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames    = extensionNames;

    // Create the device.
    ret = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, nullptr, &mDevice);
    VK_CHECK(!ret);

    // Obtain the device queue that we requested.
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitSurface()
{
    VkResult ret = VK_SUCCESS;

    // At this point, we create the android surface. This is because we want to
    // ensure our device is capable of working with the created surface object.
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext     = nullptr;
    surfaceCreateInfo.flags     = 0;
    surfaceCreateInfo.window    = mAndroidWindow;

    ret = vkCreateAndroidSurfaceKHR(mInstance, &surfaceCreateInfo, nullptr, &mSurface);
    VK_CHECK(!ret);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitSwapchain() 
{
#ifdef DELETE_THIS
    VkResult ret = VK_SUCCESS;
    // By querying the supported formats of our surface, we can ensure that
    // we use one that the device can work with.
    uint32_t formatCount;
    ret = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
    VK_CHECK(!ret);

    VkSurfaceFormatKHR *surfFormats = new VkSurfaceFormatKHR[formatCount];
    ret = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount,
                                               surfFormats);
    VK_CHECK(!ret);

    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned. For the purposes of this sample,
    // we use the first format returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        mSurfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        mSurfaceFormat.colorSpace = surfFormats[0].colorSpace;
    }
    else {
        mSurfaceFormat = surfFormats[0];
    }
    delete[] surfFormats;

    // Now we obtain the surface capabilities, which contains details such as width and height.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    ret = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface,
                                                    &surfaceCapabilities);
    VK_CHECK(!ret);

    //Unless already set, use the current surface extents as width/height
    if (mWidth == 0) {
        mWidth = surfaceCapabilities.currentExtent.width;
    }

    if (mHeight == 0) {
        mHeight = surfaceCapabilities.currentExtent.height;
    }

    // Now that we have selected formats and obtained ideal surface dimensions,
    // we create the swapchain. We use FIFO mode, which is always present. This
    // mode has a queue of images internally, that will be presented to the screen.
    // The swapchain will be created and expose the number of images created
    // in the queue, which will be at least the number specified in minImageCount.
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = mSurface;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    swapchainCreateInfo.imageFormat = mSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = mSurfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent.width = mWidth;
    swapchainCreateInfo.imageExtent.height = mHeight;
    swapchainCreateInfo.imageUsage = surfaceCapabilities.supportedUsageFlags;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;

    ret = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, nullptr, &mSwapchain);
    VK_CHECK(!ret);

    // Query the number of swapchain images. This is the number of images in the internal queue.
    ret = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mSwapchainImageCount, nullptr);
    VK_CHECK(!ret);

    LOGI("Swapchain Image Count: %d\n", mSwapchainImageCount);

    // Now we can retrieve these images, as to use them in rendering as our framebuffers.
    VkImage *pSwapchainImages = new VkImage[mSwapchainImageCount];
    ret = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mSwapchainImageCount, pSwapchainImages);
    VK_CHECK(!ret);

    // We prepare our own representation of the swapchain buffers, for keeping track
    // of resources during rendering.
    mSwapchainBuffers = new SwapchainBuffer[mSwapchainImageCount];
    VK_CHECK(mSwapchainBuffers);

    // From the images obtained from the swapchain, we create image views.
    // This gives us context into the image.
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext = nullptr;
    imageViewCreateInfo.format = mSurfaceFormat.format;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.flags = 0;

    for (uint32_t i = 0; i < mSwapchainImageCount; i++)
    {
        // We create an Imageview for each swapchain image, and track
        // the view and image in our swapchainBuffers object.
        mSwapchainBuffers[i].image = pSwapchainImages[i];
        imageViewCreateInfo.image = pSwapchainImages[i];

        VkResult err = vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr,
                                         &mSwapchainBuffers[i].view);
        VK_CHECK(!err);
    }

    // At this point, we have the references now in our swapchainBuffer object
    delete [] pSwapchainImages;

    // Now we create depth buffers for our swapchain images, which form part of
    // our framebuffers later.
    mDepthBuffers = new DepthBuffer[mSwapchainImageCount];
    for (int i = 0; i < mSwapchainImageCount; i++)
    {
        const VkFormat depthFormat = VK_FORMAT_D16_UNORM;

        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext           = nullptr;
        imageCreateInfo.imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format          = depthFormat;
        imageCreateInfo.extent.width    = mWidth;
        imageCreateInfo.extent.height   = mHeight;
        imageCreateInfo.extent.depth    = 1;
        imageCreateInfo .mipLevels      = 1;
        imageCreateInfo .arrayLayers    = 1;
        imageCreateInfo .samples        = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage           = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageCreateInfo .flags          = 0;

        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo .sType                          = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo .pNext                          = nullptr;
        imageViewCreateInfo .image                          = VK_NULL_HANDLE;
        imageViewCreateInfo.format                          = depthFormat;
        imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = 1;
        imageViewCreateInfo.flags                           = 0;
        imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;

        VkMemoryRequirements mem_reqs;
        VkResult  err;
        bool  pass;

        mDepthBuffers[i].format = depthFormat;

        // Create the image with details as imageCreateInfo
        err = vkCreateImage(mDevice, &imageCreateInfo, nullptr, &mDepthBuffers[i].image);
        VK_CHECK(!err);

        // discover what memory requirements are for this image.
        vkGetImageMemoryRequirements(mDevice, mDepthBuffers[i].image, &mem_reqs);

        // Allocate memory according to requirements
        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType                            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext                            = nullptr;
        memoryAllocateInfo.allocationSize                   = 0;
        memoryAllocateInfo.memoryTypeIndex                  = 0;
        memoryAllocateInfo.allocationSize                   = mem_reqs.size;
        pass = GetMemoryTypeFromProperties(mem_reqs.memoryTypeBits, 0, &memoryAllocateInfo.memoryTypeIndex);
        VK_CHECK(pass);

        err = vkAllocateMemory(mDevice, &memoryAllocateInfo, nullptr, &mDepthBuffers[i].mem);
        VK_CHECK(!err);

        // Bind memory to the image
        err = vkBindImageMemory(mDevice, mDepthBuffers[i].image, mDepthBuffers[i].mem, 0);
        VK_CHECK(!err);

        // Create the view for this image
        imageViewCreateInfo.image = mDepthBuffers[i].image;
        err = vkCreateImageView(mDevice, &imageViewCreateInfo, nullptr, &mDepthBuffers[i].view);
        VK_CHECK(!err);
    }
#endif // DELETE_THIS
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::SetImageLayout(VkImage image, VkCommandBuffer cmdBuffer, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcMask, VkPipelineStageFlags dstMask, uint32_t mipLevel, uint32_t mipLevelCount)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType                            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext                            = nullptr;
    imageMemoryBarrier.oldLayout                        = oldLayout;
    imageMemoryBarrier.newLayout                        = newLayout;
    imageMemoryBarrier.image                            = image;
    imageMemoryBarrier.subresourceRange.aspectMask      = aspect;
    imageMemoryBarrier.subresourceRange.baseMipLevel    = mipLevel;
    imageMemoryBarrier.subresourceRange.levelCount      = mipLevelCount;
    imageMemoryBarrier.subresourceRange.baseArrayLayer  = 0;
    imageMemoryBarrier.subresourceRange.layerCount      = 1;
    imageMemoryBarrier.srcAccessMask                    = 0;
    imageMemoryBarrier.dstAccessMask                    = 0;
    imageMemoryBarrier.srcQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex              = VK_QUEUE_FAMILY_IGNORED;

    if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    }
    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }
    if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        // Ensures reads can be made
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // Ensures writes can be made
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        // Ensure writes have completed
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        // Ensure writes have completed
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        // Make sure any Copy or CPU writes to image are flushed
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }
    if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    }

    // Barrier on image memory, with correct layouts set.
    vkCmdPipelineBarrier(cmdBuffer, srcMask /*VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT*/, dstMask /*VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT*/, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitSwapchainLayout()
{
#ifdef DELETE_THIS
    VkResult err;

    // Set the swapchain layout to present
    for (uint32_t i = 0; i < mSwapchainImageCount; i++)
    {
        VkCommandBuffer &cmdBuffer = mSwapchainBuffers[i].cmdBuffer;

        // vkBeginCommandBuffer should reset the command buffer, but Reset can be called
        // to make it more explicit.
        err = vkResetCommandBuffer(cmdBuffer, 0);
        VK_CHECK(!err);

        VkCommandBufferInheritanceInfo cmd_buf_hinfo = {};
        cmd_buf_hinfo.sType                                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        cmd_buf_hinfo.pNext                                 = nullptr;
        cmd_buf_hinfo.renderPass                            = VK_NULL_HANDLE;
        cmd_buf_hinfo.subpass                               = 0;
        cmd_buf_hinfo.framebuffer                           = VK_NULL_HANDLE;
        cmd_buf_hinfo.occlusionQueryEnable                  = VK_FALSE;
        cmd_buf_hinfo.queryFlags                            = 0;
        cmd_buf_hinfo.pipelineStatistics                    = 0;

        VkCommandBufferBeginInfo cmd_buf_info = {};
        cmd_buf_info.sType                                  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext                                  = nullptr;
        cmd_buf_info.flags                                  = 0;
        cmd_buf_info.pInheritanceInfo                       = &cmd_buf_hinfo;

        // By calling vkBeginCommandBuffer, cmdBuffer is put into the recording state.
        err = vkBeginCommandBuffer(cmdBuffer, &cmd_buf_info);
        VK_CHECK(!err);

        SetImageLayout(mSwapchainBuffers[i].image, cmdBuffer, VK_IMAGE_ASPECT_COLOR_BIT,                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
        SetImageLayout(mDepthBuffers[i].image,     cmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

        err = vkEndCommandBuffer(cmdBuffer);
        VK_CHECK(!err);

        // Submit the queue
        const VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {};
        submitInfo.sType                                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                                    = nullptr;
        submitInfo.waitSemaphoreCount                       = 0;
        submitInfo.pWaitSemaphores                          = nullptr;
        submitInfo.pWaitDstStageMask                        = &WaitDstStageMask;
        submitInfo.commandBufferCount                       = 1;
        submitInfo.pCommandBuffers                          = &mSwapchainBuffers[i].cmdBuffer;
        submitInfo.signalSemaphoreCount                     = 0;
        submitInfo.pSignalSemaphores                        = nullptr;
        err = vkQueueSubmit(mQueue, 1, &submitInfo, mFence);
        VK_CHECK(!err);
        err = vkWaitForFences(mDevice, 1, &mFence, true, 0xFFFFFFFF);
        VK_CHECK(!err);
        err= vkResetFences(mDevice, 1, &mFence);
        VK_CHECK(!err);
    }
#endif // DELETE_THIS
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitCommandbuffers()
{
    VkResult ret = VK_SUCCESS;
    // Command buffers are allocated from a pool; we define that pool here and create it.
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType                     = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext                     = nullptr;
    commandPoolCreateInfo.queueFamilyIndex          = mQueueFamilyIndex;
    commandPoolCreateInfo.flags                     = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ret = vkCreateCommandPool(mDevice, &commandPoolCreateInfo, nullptr, &mCommandPool);
    VK_CHECK(!ret);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext                 = nullptr;
    commandBufferAllocateInfo.commandPool           = mCommandPool;
    commandBufferAllocateInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount    = 1;

    // Create render command buffers, one per swapchain image.
    // DELETE_THIS for (uint32_t i=0; i < mSwapchainImageCount; i++)
    // DELETE_THIS {
    // DELETE_THIS     ret = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &mSwapchainBuffers[i].cmdBuffer);
    // DELETE_THIS     VK_CHECK(!ret);
    // DELETE_THIS }

    // Allocate a shared buffer for use in setup operations.
    ret = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &mSetupCommandBuffer);
    VK_CHECK(!ret);
}

void VkSampleFramework::TargetToTexture(VkCommandBuffer commandBuffer, ImageViewObject& sourceTarget, TextureObject& destTexture)
{
    // Copy render target images into textures, for use as samplers in shaders
    SetImageLayout(sourceTarget.GetImage(), commandBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL/*, VK_ACCESS_SHADER_READ_BIT*/);
    SetImageLayout( destTexture.GetImage(), commandBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit imageBlit;
    memset(&imageBlit, 0, sizeof(imageBlit));
    imageBlit.srcSubresource.layerCount = 1;
    imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageBlit.srcOffsets[0]             = {0};
    imageBlit.srcOffsets[1].x           = sourceTarget.GetWidth();
    imageBlit.srcOffsets[1].y           = sourceTarget.GetHeight();
    imageBlit.srcOffsets[1].z           = 1;

    imageBlit.dstSubresource.layerCount = 1;
    imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // imageBlit.dstOffsets[0]             = {0};
    imageBlit.dstOffsets[1].x           = destTexture.GetWidth();
    imageBlit.dstOffsets[1].y           = destTexture.GetHeight();
    imageBlit.dstOffsets[1].z           = 1;

    vkCmdBlitImage(commandBuffer, sourceTarget.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   destTexture.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &imageBlit, VK_FILTER_LINEAR);

    SetImageLayout( destTexture.GetImage(), commandBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    SetImageLayout(sourceTarget.GetImage(), commandBuffer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitPipeline(VkPipelineCache                                pipelineCache,
                                     uint32_t                                       stageCount,
                                     const VkPipelineShaderStageCreateInfo*         stages,
                                     VkPipelineLayout                               layout,
                                     VkRenderPass                                   renderPass,
                                     uint32_t                                       subpass,
                                     const VkPipelineVertexInputStateCreateInfo*    vertexInputState,
                                     const VkPipelineInputAssemblyStateCreateInfo*  inputAssemblyState,
                                     const VkPipelineTessellationStateCreateInfo*   tessellationState,
                                     const VkPipelineViewportStateCreateInfo*       viewportState,
                                     const VkPipelineRasterizationStateCreateInfo*  rasterizationState,
                                     const VkPipelineMultisampleStateCreateInfo*    multisampleState,
                                     const VkPipelineDepthStencilStateCreateInfo*   depthStencilState,
                                     const VkPipelineColorBlendStateCreateInfo*     colorBlendState,
                                     const VkPipelineDynamicStateCreateInfo*        dynState,
                                     bool                                           bAllowDerivation,
                                     VkPipeline                                     deriveFromPipeline,
                                     VkPipeline* pipeline)
{
    VkResult   err;

    // default to a basic pipeline structure, however, overload relevant info structures using
    // user-passed state.
    assert(pipeline);
    assert(stages);
    assert(vertexInputState);

    // Our vertex buffer describes a triangle list.
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology                                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // State for rasterization, such as polygon fill mode is defined.
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode                              = VK_POLYGON_MODE_FILL;
    rs.cullMode                                 = VK_CULL_MODE_NONE;
    rs.frontFace                                = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable                         = VK_FALSE;
    rs.rasterizerDiscardEnable                  = VK_FALSE;
    rs.depthBiasEnable                          = VK_FALSE;
    rs.lineWidth                                = 1.0f;

    // For this example we do not do blending, so it is disabled.
    VkPipelineColorBlendAttachmentState att_state[1] = {};
    att_state[0].colorWriteMask                 = 0xf;
    att_state[0].blendEnable                    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount                          = 1;
    cb.pAttachments                             = &att_state[0];

    // We define a simple viewport and scissor. It does not change during rendering
    // in this sample.
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount                            = 1;
    vp.scissorCount                             = 1;

    VkViewport viewport = {};
    viewport.height                             = (float) mHeight;
    viewport.width                              = (float) mWidth;
    viewport.minDepth                           = (float) 0.0f;
    viewport.maxDepth                           = (float) 1.0f;
    vp.pViewports = &viewport;

    VkRect2D scissor = {};
    scissor.extent.width                        = mWidth;
    scissor.extent.height                       = mHeight;
    scissor.offset.x                            = 0;
    scissor.offset.y                            = 0;
    vp.pScissors = &scissor;

    // Standard depth and stencil state is defined
    VkPipelineDepthStencilStateCreateInfo  ds = {};
    ds.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable                          = VK_TRUE;
    ds.depthWriteEnable                         = VK_TRUE;
    ds.depthCompareOp                           = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable                    = VK_FALSE;
    ds.back.failOp                              = VK_STENCIL_OP_KEEP;
    ds.back.passOp                              = VK_STENCIL_OP_KEEP;
    ds.back.compareOp                           = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable                        = VK_FALSE;
    ds.front                                    = ds.back;

    // We do not use multisample
    VkPipelineMultisampleStateCreateInfo   ms = {};
    ms.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pSampleMask                              = nullptr;
    ms.rasterizationSamples                     = VK_SAMPLE_COUNT_1_BIT;

    // Set up the flags
    VkPipelineCreateFlags flags = 0;
    if (bAllowDerivation)
    {
        flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    }
    if (deriveFromPipeline != VK_NULL_HANDLE)
    {
        flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    }

    // Out graphics pipeline records all state information, including our renderpass
    // and pipeline layout. We do not have any dynamic state in this example.
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType                    = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.flags                    = flags;
    pipelineCreateInfo.layout                   = layout;
    pipelineCreateInfo.pVertexInputState        = vertexInputState;
    pipelineCreateInfo.pInputAssemblyState      = (inputAssemblyState   != nullptr)? inputAssemblyState : &ia;
    pipelineCreateInfo.pRasterizationState      = (rasterizationState   != nullptr)? rasterizationState : &rs;
    pipelineCreateInfo.pColorBlendState         = (colorBlendState      != nullptr)? colorBlendState    : &cb;
    pipelineCreateInfo.pMultisampleState        = (multisampleState     != nullptr)? multisampleState   : &ms;
    pipelineCreateInfo.pViewportState           = (viewportState        != nullptr)? viewportState      : &vp;
    pipelineCreateInfo.pDepthStencilState       = (depthStencilState    != nullptr)? depthStencilState  : &ds;
    pipelineCreateInfo.pStages                  = stages;
    pipelineCreateInfo.renderPass               = renderPass;
    pipelineCreateInfo.pDynamicState            = dynState;
    pipelineCreateInfo.stageCount               = stageCount;
    pipelineCreateInfo.basePipelineHandle       = (deriveFromPipeline != VK_NULL_HANDLE) ? deriveFromPipeline : VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex        = -1; // indicates this field isn't used
    pipelineCreateInfo.subpass                  = subpass;

    err = vkCreateGraphicsPipelines(mDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, pipeline);
    VK_CHECK(!err);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitPipeline(VkPipelineCache                                pipelineCache,
                                     VkPipelineVertexInputStateCreateInfo*          visci,
                                     VkPipelineLayout                               pipelineLayout,
                                     VkRenderPass                                   renderPass,
                                     VkPipelineRasterizationStateCreateInfo*        providedRS,
                                     VkShaderModule                                 vertShaderModule,
                                     VkShaderModule                                 fragShaderModule,
                                     bool                                           bAllowDerivation,
                                     VkPipeline                                     deriveFromPipeline,
                                     VkPipeline*                                    pipeline)
{
    VkResult   err;

    // Create a basic pipeline structure with two shader stages, using the supplied cache.
    assert(pipeline);

    // Our vertex buffer describes a triangle list.
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology                                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // State for rasterization, such as polygon fill mode is defined.
    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode                              = VK_POLYGON_MODE_FILL;
    rs.cullMode                                 = VK_CULL_MODE_NONE;
    rs.frontFace                                = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthClampEnable                         = VK_FALSE;
    rs.rasterizerDiscardEnable                  = VK_FALSE;
    rs.depthBiasEnable                          = VK_FALSE;
    rs.lineWidth                                = 1.0f;

    // For this example we do not do blending, so it is disabled.
    VkPipelineColorBlendAttachmentState att_state[1] = {};
    att_state[0].colorWriteMask                 = 0xf;
    att_state[0].blendEnable                    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount                          = 1;
    cb.pAttachments                             = &att_state[0];

    // We define a simple viewport and scissor. It does not change during rendering
    // in this sample.
    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount                            = 1;
    vp.scissorCount                             = 1;

    VkViewport viewport = {};
    viewport.height                             = (float) mHeight;
    viewport.width                              = (float) mWidth;
    viewport.minDepth                           = (float) 0.0f;
    viewport.maxDepth                           = (float) 1.0f;
    vp.pViewports                               = &viewport;

    VkRect2D scissor = {};
    scissor.extent.width                        = mWidth;
    scissor.extent.height                       = mHeight;
    scissor.offset.x                            = 0;
    scissor.offset.y                            = 0;
    vp.pScissors = &scissor;

    // Standard depth and stencil state is defined
    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable                          = VK_TRUE;
    ds.depthWriteEnable                         = VK_TRUE;
    ds.depthCompareOp                           = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable                    = VK_FALSE;
    ds.back.failOp                              = VK_STENCIL_OP_KEEP;
    ds.back.passOp                              = VK_STENCIL_OP_KEEP;
    ds.back.compareOp                           = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable                        = VK_FALSE;
    ds.front                                    = ds.back;

    // We do not use multisample
    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType                                    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pSampleMask                              = nullptr;
    ms.rasterizationSamples                     = VK_SAMPLE_COUNT_1_BIT;

    // We define two shader stages: our vertex and fragment shader.
    // they are embedded as SPIR-V into a header file for ease of deployment.
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType                       = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage                       = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module                      = vertShaderModule;
    shaderStages[0].pName                       = "main";
    shaderStages[1].sType                       = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage                       = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module                      = fragShaderModule;
    shaderStages[1].pName                       = "main";

    // Set up the flags
    VkPipelineCreateFlags flags = 0;
    if (bAllowDerivation)
    {
        flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    }
    if (deriveFromPipeline != VK_NULL_HANDLE)
    {
        flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    }

    // Out graphics pipeline records all state information, including our renderpass
    // and pipeline layout. We do not have any dynamic state in this example.
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType                    = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.flags                    = flags;
    pipelineCreateInfo.layout                   = pipelineLayout;
    pipelineCreateInfo.pVertexInputState        = visci;
    pipelineCreateInfo.pInputAssemblyState      = &ia;
    pipelineCreateInfo.pRasterizationState      = (providedRS != nullptr)? providedRS : &rs;
    pipelineCreateInfo.pColorBlendState         = &cb;
    pipelineCreateInfo.pMultisampleState        = &ms;
    pipelineCreateInfo.pViewportState           = &vp;
    pipelineCreateInfo.pDepthStencilState       = &ds;
    pipelineCreateInfo.pStages                  = &shaderStages[0];
    pipelineCreateInfo.renderPass               = renderPass;
    pipelineCreateInfo.pDynamicState            = nullptr;
    pipelineCreateInfo.stageCount               = 2; //vertex and fragment
    pipelineCreateInfo.basePipelineHandle       = (deriveFromPipeline != VK_NULL_HANDLE) ? deriveFromPipeline : VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex        = -1; // indicates this field isn't used

    err = vkCreateGraphicsPipelines(mDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, pipeline);
    VK_CHECK(!err);
}

///////////////////////////////////////////////////////////////////////////////

VkShaderModule VkSampleFramework::CreateShaderModuleFromAsset(const char* asset)
{
    // Get the image file from the Android asset manager
    AAsset* file = AAssetManager_open(GetAssetManager(), asset, AASSET_MODE_BUFFER);
    assert(file);

    const uint32_t file_size = AAsset_getLength(file);
    const uint32_t* file_buffer = (const uint32_t*)AAsset_getBuffer(file);
    assert(file_buffer);

    VkShaderModule module = CreateShaderModule(file_buffer, file_size);
    AAsset_close(file);

    return module;
}

///////////////////////////////////////////////////////////////////////////////

VkShaderModule VkSampleFramework::CreateShaderModule(const uint32_t* code, uint32_t size)
{
    VkShaderModule module;
    VkResult  err;

    // Creating a shader is very simple once it's in memory as compiled SPIR-V.
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType                  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext                  = nullptr;
    moduleCreateInfo.codeSize               = size;
    moduleCreateInfo.pCode                  = code;
    moduleCreateInfo.flags                  = 0;
    err = vkCreateShaderModule(mDevice, &moduleCreateInfo, nullptr, &module);
    VK_CHECK(!err);

    return module;
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::InitSync()
{
    VkResult ret = VK_SUCCESS;

    // We have semaphores for rendering and backbuffer signalling.
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType               = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext               = nullptr;
    semaphoreCreateInfo.flags               = 0;
    ret = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mBackBufferSemaphore);
    VK_CHECK(!ret);

    ret = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mRenderCompleteSemaphore);
    VK_CHECK(!ret);

    // Create a fence to use for syncing the layout changes
    VkFenceCreateInfo fenceCreateInfo= {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    ret = vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mFence);
    VK_CHECK(!ret);
}

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::GetMemoryTypeFromProperties( uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex)
{
    VK_CHECK(typeIndex != nullptr);
    // Search memtypes to find first index with those properties..There are 32 possible combinations of memory types
    for (uint32_t i = 0; i < 32; i++)
    {
        if ((typeBits & 1) == 1)
        {
            // Type is available, does it match user properties?
            if ((mPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags &
                 requirements_mask) == requirements_mask)
            {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::SetNextBackBuffer()
{
    VkResult ret = VK_SUCCESS;

    // Get the next image to render to, then queue a wait until the image is ready
    ret  = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mBackBufferSemaphore, VK_NULL_HANDLE, &mSwapchainCurrentIdx);
    if (ret == VK_ERROR_OUT_OF_DATE_KHR)
    {
        LOGW("VK_ERROR_OUT_OF_DATE_KHR not handled in sample");
    }
    else if (ret == VK_SUBOPTIMAL_KHR)
    {
        LOGW("VK_SUBOPTIMAL_KHR not handled in sample");
    }
    VK_CHECK(!ret);
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::PresentBackBuffer()
{
    VkResult ret = VK_SUCCESS;

    // Use WSI to present. The semaphore chain used to signal rendering
    // completion allows the operation to wait before the present is
    // completed.
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType                       = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount              = 1;
    presentInfo.pSwapchains                 = &mSwapchain;
    presentInfo.pImageIndices               = &mSwapchainCurrentIdx;
    presentInfo.waitSemaphoreCount          = 1;
    presentInfo.pWaitSemaphores             = &mRenderCompleteSemaphore;

    ret = vkQueuePresentKHR(mQueue, &presentInfo);
    VK_CHECK(!ret);

    // Obtain the back buffer for the next frame.
    SetNextBackBuffer();
}

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::TearDown()
{
    if (!mInitBegun)
    {
        return false;
    }

    if (mMemoryAllocator)
    {
        delete mMemoryAllocator;
        mMemoryAllocator = NULL;
    }

    DestroySample();

    // Destroy all resources for swapchain images, and depth buffers
    // DELETE_THIS for (uint32_t i = 0; i < mSwapchainImageCount; i++)
    // DELETE_THIS {
    // DELETE_THIS     vkFreeCommandBuffers(mDevice, mCommandPool, 1, &mSwapchainBuffers[i].cmdBuffer);
    // DELETE_THIS     vkDestroyImageView(  mDevice, mSwapchainBuffers[i].view,  nullptr);
    // DELETE_THIS     vkDestroyImage(      mDevice, mDepthBuffers[i].image,     nullptr);
    // DELETE_THIS     vkDestroyImageView(  mDevice, mDepthBuffers[i].view,      nullptr);
    // DELETE_THIS     vkFreeMemory(        mDevice, mDepthBuffers[i].mem,       nullptr);
    // DELETE_THIS }
    // Destroy the swapchain
    // DELETE_THIS vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

    // DELETE_THIS delete [] mSwapchainBuffers;
    // DELETE_THIS delete [] mDepthBuffers;

    // Destroy pools
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    // Destroy sync
    vkDestroySemaphore(mDevice, mBackBufferSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mRenderCompleteSemaphore, nullptr);
    vkDestroyFence(mDevice, mFence, nullptr);

    // destroy the callback
    if (mDebugReportCallback)
    {
        mDestroyDebugReportCallbackEXT(mInstance, mDebugReportCallback, nullptr);
    }

    // Destroy the device, surface and instance
    vkDestroyDevice(mDevice, nullptr);

    // We don't always create the surface so conditionally release it
    if(mSurface != (VkSurfaceKHR)0)
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    mSurface = (VkSurfaceKHR)0;

    vkDestroyInstance(mInstance, nullptr);

    delete [] mpPhysicalDevices;

    mInitialized = false;
    mInitBegun = false;

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::DrawFrame()
{
    if (!mInitialized)
    {
        return;
    }

    // Update at a suitable time.
    Update();

    Draw();

    // DELETE_THIS PresentBackBuffer();

    // Calculate FPS below, displaying every 30 frames
    mFrameTimeEnd = GetTime();
    if (mFrameIdx % 30 == 0)
    {
        mFrameTimeDelta = mFrameTimeEnd - mFrameTimeBegin;
        double fps = 1000000000.0/((double)mFrameTimeDelta );
        if (mbLogFPS)
        {
            // No need to log this, SVR is logging submit frames
            // LOGI("FPS: %0.2f", fps);
        }
    }

    mFrameIdx++;
    mFrameTimeBegin = GetTime();

    // uncomment to invoke teardown logic
    //if (mFrameIdx==100)
    //{
    //  ShutDown();
    //}
}

///////////////////////////////////////////////////////////////////////////////

void VkSampleFramework::ShutDown()
{
    VkResult ret = VK_SUCCESS;
    ret = vkDeviceWaitIdle(mDevice);
    VK_CHECK(!ret);
    TearDown();
    exit(0);
};

///////////////////////////////////////////////////////////////////////////////

uint64_t VkSampleFramework::GetTime()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t) now.tv_sec*1000000000LL + now.tv_nsec;
}

///////////////////////////////////////////////////////////////////////////////

bool VkSampleFramework::LoadFileBuffer(const char* pszFileName, void** ppBuffer, uint32_t* pBufferSize)
{

    // Make sure file exists
    FILE* file  = fopen(pszFileName, "rb");
    if (!file)
    {
        LOGW("LoadFileBuffer - unable to open file for reading: %s", pszFileName);
        return false;
    }

    void*   pBuffer = NULL;
    size_t  nBufferSize = 0;

    // grab size of file
    if (file)
    {
        fseek(file, 0, SEEK_END);
        nBufferSize = ftell(file);
        fseek(file, 0, SEEK_SET);
    }

    // allocate buffer
    pBuffer = malloc(nBufferSize);
    if (ppBuffer == NULL)
    {
        LOGW("LoadFileBuffer - unable to allocate buffer: %d bytes", nBufferSize);
        return false;
    }

    // read the buffer
    uint32_t nRead = fread(pBuffer, sizeof(char), nBufferSize, file);
    if (nRead != nBufferSize)
    {
        free(pBuffer);
        *ppBuffer = NULL;
        fclose(file);
        LOGW("LoadFileBuffer - unable to read buffer: %d bytes", nBufferSize);
        return false;
    }

    // close file, set return values
    fclose(file);
    *ppBuffer = pBuffer;
    *pBufferSize = nBufferSize;

    return true;
}
#include <errno.h>
bool VkSampleFramework::SaveFileBuffer(const char* pszFileName, void* pBuffer, uint32_t bufferSize)
{
    // Make sure file can be written to
    FILE* file  = fopen(pszFileName, "wb");
    if (!file)
    {
        int e = errno;
        LOGW("SaveFileBuffer - unable to open file for writing: %s", pszFileName);
        return false;
    }

    // write the buffer
    uint32_t nWrite = fwrite(pBuffer, sizeof(char), bufferSize, file);
    if (nWrite != bufferSize)
    {
        LOGW("LoadFileBuffer - unable to write buffer: %d bytes", bufferSize);
        fclose(file);
        return false;
    }

    // close file
    fclose(file);

    return true;
}