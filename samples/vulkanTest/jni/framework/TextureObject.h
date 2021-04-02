//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.
#pragma once

#include <assert.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <android/asset_manager.h>

class VkSampleFramework;

// Defines a simple object for creating and holding Vulkan image/view objects.
class ImageViewObject
{
public:
    ImageViewObject();
    ~ImageViewObject();

    static bool CreateImageView(VkSampleFramework* sample, uint32_t width, uint32_t height, VkFormat format, VkImageAspectFlags aspect,
                                VkImageLayout layout, VkImageUsageFlags usage, VkFlags memoryRequirmentFlags, ImageViewObject* textureObject);

    void SetLayoutImmediate(VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout);

    virtual bool Destroy();

    VkImageView GetView()
    {
        return mView;
    }

    VkImage GetImage()
    {
        return mImage;
    }

    VkImageLayout GetLayout()
    {
        return mImageLayout;
    }

    VkFormat GetFormat()
    {
        return mFormat;
    }

    uint32_t GetWidth()
    {
        return mWidth;
    }

    uint32_t GetHeight()
    {
        return mHeight;
    }
public:
    VkImage mImage;
    VkImageLayout mImageLayout;
    VkDeviceMemory mMem;
    VkImageView mView;
    VkSampleFramework* mSample;

    VkFormat mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
};

// Defines a simple object for creating and holding Vulkan texture objects.
// Supports loading from TGA files in Android Studio asset folder.
// Only supports R8G8B8A8 files and texture formats. Converts from TGA BGRA to RGBA.
class TextureObject: public ImageViewObject
{
public:
    TextureObject();
    ~TextureObject();

    static bool FromTGAFile(VkSampleFramework* sample, const char* filename, TextureObject* textureObject);

    static bool FromASTCFile(VkSampleFramework* sample, const char* filename, TextureObject* textureObject);


    static bool CreateTexture(VkSampleFramework* sample, uint32_t width, uint32_t height,
                                             VkFormat format, VkImageUsageFlags additionalUsage,
                                             TextureObject* textureObject);

    VkSampler GetSampler()
    {
        return mSampler;
    }

    int GetGlHandle()
    {
        return mGlHandle;
    }

    bool Destroy();

public:
    static uint8_t* LoadTGAFromMemory( const uint8_t* dataInMemory, uint32_t* pWidth, uint32_t* pHeight, VkFormat* pFormat);

    VkSampler   mSampler;

    int         mGlHandle;
    uint32_t    mMemorySize;
    uint32_t    mMips;
};