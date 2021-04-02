#pragma once

// This definition enables the Android extensions
#define VK_USE_PLATFORM_ANDROID_KHR

// This definition allows prototypes of Vulkan API functions,
// rather than dynamically loading entrypoints to the API manually.
#define VK_PROTOTYPES

#include <vulkan/vulkan.h>

class VkSampleFramework;

class MemoryAllocator
{
public:
    MemoryAllocator();
    ~MemoryAllocator();
    VkResult AllocateMemory(VkMemoryAllocateInfo* pInfo, VkDeviceMemory* pMem, VkDeviceSize* pOffset);
    void FreeMemory(VkDeviceMemory* pMem, VkDeviceSize* pOffset);

    void SetSample(VkSampleFramework* pSampleFramework){ mSampleFramework = pSampleFramework;};
private:
    VkSampleFramework* mSampleFramework;

    static const int32_t NUM_MEMORY_TYPES = 32;
    static const int32_t ALLOCATION_SIZE = 1024 * 1024 * 50;

    VkDeviceMemory  mDeviceMemory[NUM_MEMORY_TYPES];
    VkDeviceSize    mDeviceMemoryOffset[NUM_MEMORY_TYPES];
};

