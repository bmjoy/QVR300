//                  Copyright (c) 2016 QUALCOMM Technologies Inc.
//                              All Rights Reserved.

#include "VkSampleFramework.h"


extern PFN_vkGetMemoryFdKHR                             g_vkGetMemoryFdKHR;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2KHR g_vkGetPhysicalDeviceImageFormatProperties2KHR;


///////////////////////////////////////////////////////////////////////////////
ImageViewObject::ImageViewObject()
{
    mWidth = 0;
    mHeight = 0;
    mSample = NULL;
    mImage = VK_NULL_HANDLE;
    mMem = VK_NULL_HANDLE;
    mView = VK_NULL_HANDLE;

    mImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    mFormat = VK_FORMAT_UNDEFINED;

}
///////////////////////////////////////////////////////////////////////////////

bool ImageViewObject::Destroy()
{
    if (!mSample)
    {
        //Uninitialized
        return false;
    }

    if (!mSample->IsInitialized())
    {
        //If the sample has already been torn down, bail out.
        return false;
    }

    vkDestroyImage(mSample->GetDevice(), mImage, nullptr);
    vkFreeMemory(mSample->GetDevice(), mMem, nullptr);
    vkDestroyImageView(mSample->GetDevice(), mView, nullptr);

    mSample = nullptr;
    return true;
}

///////////////////////////////////////////////////////////////////////////////

ImageViewObject::~ImageViewObject()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

void ImageViewObject::SetLayoutImmediate(VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkResult err = VK_SUCCESS;

    // We use a shared command buffer for setup operations to change layout.
    // Reset the setup command buffer
    vkResetCommandBuffer(mSample->GetSetupCommandBuffer(), 0);

    VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
    commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    commandBufferInheritanceInfo.pNext = NULL;
    commandBufferInheritanceInfo.renderPass = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.subpass = 0;
    commandBufferInheritanceInfo.framebuffer = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;
    commandBufferInheritanceInfo.queryFlags = 0;
    commandBufferInheritanceInfo.pipelineStatistics = 0;

    VkCommandBufferBeginInfo setupCmdsBeginInfo;
    setupCmdsBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    setupCmdsBeginInfo.pNext = NULL;
    setupCmdsBeginInfo.flags = 0;
    setupCmdsBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

    // Begin recording to the command buffer.
    vkBeginCommandBuffer(mSample->GetSetupCommandBuffer(), &setupCmdsBeginInfo);

    mSample->SetImageLayout(mImage, mSample->GetSetupCommandBuffer(), aspect, oldLayout, newLayout, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,0);

    //SetLayout(mSample->GetSetupCommandBuffer(), aspect, oldLayout, newLayout );

    // We are finished recording operations.
    vkEndCommandBuffer(mSample->GetSetupCommandBuffer());

    VkCommandBuffer buffers[1];
    buffers[0] = mSample->GetSetupCommandBuffer();

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buffers[0];
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    // Submit to our shared greaphics queue.
    err = vkQueueSubmit(mSample->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
    assert(!err);

    // Wait for the queue to become idle.
    err = vkQueueWaitIdle(mSample->GetGraphicsQueue());
    assert(!err);
}

///////////////////////////////////////////////////////////////////////////////

bool ImageViewObject::CreateImageView(VkSampleFramework* sample, uint32_t width, uint32_t height, VkFormat format, VkImageAspectFlags aspect,
                                      VkImageLayout layout, VkImageUsageFlags usage, VkFlags memoryRequirementFlags, ImageViewObject* imageViewObj)
{
    VkResult   err;
    bool   pass;

    if (!imageViewObj || !sample )
    {
        return false;
    }

    imageViewObj->mFormat = format;
    imageViewObj->mSample = sample;
    imageViewObj->mWidth = width;
    imageViewObj->mHeight = height;
    imageViewObj->mImageLayout = layout;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = NULL;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = imageViewObj->mFormat;
    imageCreateInfo.extent.depth = 1.0f;
    imageCreateInfo.extent.height = imageViewObj->mHeight;
    imageCreateInfo.extent.width = imageViewObj->mWidth;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.flags = 0;

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;
    memoryAllocateInfo.allocationSize = 0;
    memoryAllocateInfo.memoryTypeIndex = 0;

    VkMemoryRequirements mem_reqs;

    err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, NULL, &imageViewObj->mImage);
    assert(!err);

    vkGetImageMemoryRequirements(sample->GetDevice(), imageViewObj->mImage, &mem_reqs);

    memoryAllocateInfo.allocationSize  = mem_reqs.size;
    pass = sample->GetMemoryTypeFromProperties( mem_reqs.memoryTypeBits, memoryRequirementFlags, &memoryAllocateInfo.memoryTypeIndex);
    assert(pass);

    // allocate memory
    err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &imageViewObj->mMem);
    assert(!err);

    // bind memory
    err = vkBindImageMemory(sample->GetDevice(), imageViewObj->mImage, imageViewObj->mMem, 0);
    assert(!err);


    // Transition to the required layout
    imageViewObj->SetLayoutImmediate(aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout);

    // Create the image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext = NULL;
    viewCreateInfo.image = imageViewObj->mImage;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = imageViewObj->mFormat;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask = aspect;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.flags = 0;

    err = vkCreateImageView(sample->GetDevice(), &viewCreateInfo, NULL, &imageViewObj->mView);
    assert(!err);

    // All relevant objects created
    return true;
}

///////////////////////////////////////////////////////////////////////////////

uint8_t* TextureObject::LoadTGAFromMemory( const uint8_t* dataInMemory, uint32_t* pWidth, uint32_t* pHeight, VkFormat* pFormat )
{
    struct TARGA_HEADER
    {
        uint8_t   IDLength, ColormapType, ImageType;
        uint8_t   ColormapSpecification[5];
        uint16_t  XOrigin, YOrigin;
        uint16_t  ImageWidth, ImageHeight;
        uint8_t   PixelDepth;
        uint8_t   ImageDescriptor;
    };

    TARGA_HEADER header;

    assert(pWidth);
    assert(pHeight);
    assert(pFormat);
    assert(dataInMemory);

    header = *(TARGA_HEADER*)dataInMemory;
    uint8_t nPixelSize = header.PixelDepth / 8;
    (*pWidth)  = header.ImageWidth;
    (*pHeight) = header.ImageHeight;
    (*pFormat) = nPixelSize == 3 ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;

    dataInMemory += sizeof(TARGA_HEADER);

    uint8_t* pBits = new uint8_t[ nPixelSize * header.ImageWidth * header.ImageHeight ];
    memcpy(pBits, dataInMemory, nPixelSize * header.ImageWidth * header.ImageHeight );

    // Convert the image from BGRA to RGBA
    uint8_t* p = pBits;
    for (uint32_t y = 0; y < header.ImageHeight; y++)
    {
        for (uint32_t x = 0; x < header.ImageWidth; x++)
        {
            uint8_t temp = p[2];
            p[2] = p[0];
            p[0] = temp;
            p += nPixelSize;
        }
    }

    return pBits;
}

///////////////////////////////////////////////////////////////////////////////

TextureObject::TextureObject()
{
    mSample         = NULL;
    mImage          = VK_NULL_HANDLE;
    mImageLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    mMem            = VK_NULL_HANDLE;
    mView           = VK_NULL_HANDLE;
    mFormat         = VK_FORMAT_UNDEFINED;
    mWidth          = 0;
    mHeight         = 0;


    mSampler = VK_NULL_HANDLE;
    mGlHandle = -1;
    mMemorySize = 0;
    mMips = 0;

}

///////////////////////////////////////////////////////////////////////////////

TextureObject::~TextureObject()
{
    Destroy();
}

///////////////////////////////////////////////////////////////////////////////

bool TextureObject::Destroy()
{
    if (!mSample)
    {
        //Uninitialized texture, bail out
        return false;
    }

    if (!mSample->IsInitialized())
    {
        //If the sample has already been torn down, bail out.
        return false;
    }

    // Spec says you have to call close on this
    // if (mGlHandle >= 0)
    //     fclose(mGlHandle);
    mGlHandle = -1;

    vkDestroySampler(mSample->GetDevice(), mSampler, nullptr);

    return ImageViewObject::Destroy();;
}

///////////////////////////////////////////////////////////////////////////////

bool TextureObject::FromTGAFile(VkSampleFramework* sample, const char* filename, TextureObject* textureObject)
{
    if (!textureObject || !sample || !filename)
    {
        return false;
    }

    // Get the image file from the Android asset manager
    AAsset* file = AAssetManager_open(sample->GetAssetManager(), filename, AASSET_MODE_BUFFER);
    assert(file);

    const uint8_t* file_buffer = (const uint8_t*)AAsset_getBuffer(file);
    assert(file_buffer);

    uint32_t* imageData = (uint32_t*)LoadTGAFromMemory(file_buffer, &textureObject->mWidth, &textureObject->mHeight, &textureObject->mFormat);

    AAsset_close(file);

    // Only one format is supported here.
    assert(textureObject->mFormat == VK_FORMAT_R8G8B8A8_UNORM);
    VkResult   err;
    bool   pass;

    textureObject->mSample = sample;

    // Initialize the CreateInfo structure
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType               = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext               = NULL;
    imageCreateInfo.imageType           = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format              = textureObject->mFormat;
    imageCreateInfo.extent.depth        = 1;
    imageCreateInfo.extent.width        = textureObject->mWidth;
    imageCreateInfo.extent.height       = textureObject->mHeight;
    imageCreateInfo.mipLevels           = 1;
    imageCreateInfo.arrayLayers         = 1;
    imageCreateInfo.samples             = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling              = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage               = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.flags               = 0;
    imageCreateInfo.sharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout       = VK_IMAGE_LAYOUT_PREINITIALIZED;

    VkMemoryRequirements mem_reqs;

    // Initialize the memory allocation structure
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext            = NULL;
    memoryAllocateInfo.allocationSize   = 0;
    memoryAllocateInfo.memoryTypeIndex  = 0;

    // Reset the setup command buffer
    vkResetCommandBuffer(sample->GetSetupCommandBuffer(), 0);

    VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
    commandBufferInheritanceInfo.sType                  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    commandBufferInheritanceInfo.pNext                  = NULL;
    commandBufferInheritanceInfo.renderPass             = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.subpass                = 0;
    commandBufferInheritanceInfo.framebuffer            = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.occlusionQueryEnable   = VK_FALSE;
    commandBufferInheritanceInfo.queryFlags             = 0;
    commandBufferInheritanceInfo.pipelineStatistics     = 0;

    VkCommandBufferBeginInfo setupCmdsBeginInfo;
    setupCmdsBeginInfo.sType                            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    setupCmdsBeginInfo.pNext                            = NULL;
    setupCmdsBeginInfo.flags                            = 0;
    setupCmdsBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

    // Begin recording to the command buffer.
    vkBeginCommandBuffer(sample->GetSetupCommandBuffer(), &setupCmdsBeginInfo);

    // For now, just supporting a single miplevel...
    // DO NOT CHANGE THIS SINCE BELOW ONLY ASSUMES 1 LEVEL!
    int numMipLevels = 1;

    // Per miplevel values
    VkImage mipImages[numMipLevels];
    VkDeviceMemory  mipMemory[numMipLevels];
    uint32_t mipWidth[numMipLevels];
    uint32_t mipHeight[numMipLevels];
    for (uint32_t level = 0; level < numMipLevels; level++)
    {
        // DO NOT CHANGE THIS SINCE ABOVE ONLY ASSUMES 1 LEVEL!
        mipWidth[level] = textureObject->mWidth;
        mipHeight[level] = textureObject->mHeight;
    }


    // For each mip level
    for (uint32_t level = 0; level < numMipLevels; level++)
    {
        // Set the image size for this mip level
        imageCreateInfo.extent.width  = mipWidth[level];
        imageCreateInfo.extent.height = mipHeight[level];
        imageCreateInfo.extent.depth  = 1;

        // Create the mip level image
        err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, NULL, &mipImages[level]);
        assert(!err);

        vkGetImageMemoryRequirements(sample->GetDevice(), mipImages[level], &mem_reqs);

        memoryAllocateInfo.allocationSize   = mem_reqs.size;
        pass = sample->GetMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAllocateInfo.memoryTypeIndex);
        assert(pass);

        // allocate memory
        err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &mipMemory[level]);
        assert(!err);

        // bind memory
        err = vkBindImageMemory(sample->GetDevice(), mipImages[level], mipMemory[level], 0);
        assert(!err);

        // copy image data to the mip memory
        VkImageSubresource subRes = {};
        subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSubresourceLayout subResLayout;
        vkGetImageSubresourceLayout(sample->GetDevice(), mipImages[level], &subRes, &subResLayout);
        void *data;
        err = vkMapMemory(sample->GetDevice(), mipMemory[level], 0, mem_reqs.size, 0, &data);
        assert(!err);
        memcpy(data, imageData, mem_reqs.size);
        vkUnmapMemory(sample->GetDevice(), mipMemory[level]);

        // Change the mip image layout to transfer src
        sample->SetImageLayout(mipImages[level],
                               sample->GetSetupCommandBuffer(),
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               VK_IMAGE_LAYOUT_PREINITIALIZED,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, level, numMipLevels);
    }
    // Free the TGA image data
    delete imageData;

    // Setup texture as blit target
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.mipLevels = numMipLevels;
    imageCreateInfo.extent.width = textureObject->mWidth;
    imageCreateInfo.extent.height = textureObject->mHeight;
    imageCreateInfo.extent.depth = 1;



    // Create the image
    err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, nullptr, &textureObject->mImage);
    assert(!err);

    // Get the memory requirements
    vkGetImageMemoryRequirements(sample->GetDevice(), textureObject->mImage, &mem_reqs);

    // Get the memory type
    memoryAllocateInfo.allocationSize   = mem_reqs.size;
    pass = sample->GetMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex);
    assert(pass);

    // allocate memory
    err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &textureObject->mMem);
    assert(!err);

    // bind memory
    err = vkBindImageMemory(sample->GetDevice(), textureObject->mImage, textureObject->mMem, 0);
    assert(!err);

    // Change image layout to Transfer_DST so it can be filled.
    sample->SetImageLayout(textureObject->mImage,
                           sample->GetSetupCommandBuffer(),
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           VK_IMAGE_LAYOUT_PREINITIALIZED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, numMipLevels);

    // Copy mip levels
    for (uint32_t level = 0; level < numMipLevels; level++)
    {
        // Create a region for the image blit
        VkImageCopy region = {};

        region.srcSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.baseArrayLayer    = 0;
        region.srcSubresource.mipLevel          = 0;
        region.srcSubresource.layerCount        = 1;
        region.srcOffset.x                      = 0;
        region.srcOffset.y                      = 0;
        region.srcOffset.z                      = 0;
        region.dstSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.baseArrayLayer    = 0;
        region.dstSubresource.mipLevel          = level;
        region.dstSubresource.layerCount        = 1;
        region.dstOffset.x                      = 0;
        region.dstOffset.y                      = 0;
        region.dstOffset.z                      = 0;
        region.extent.width                     = mipWidth[level];
        region.extent.height                    = mipHeight[level];
        region.extent.depth                     = 1;

        // Put image copy for this mip level into command buffer
        vkCmdCopyImage(sample->GetSetupCommandBuffer(),
                       mipImages[level],
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       textureObject->mImage,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    sample->SetImageLayout(textureObject->mImage,
                           sample->GetSetupCommandBuffer(),
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, numMipLevels);

    // We are finished recording operations.
    vkEndCommandBuffer(sample->GetSetupCommandBuffer());

    // Prepare to submit the command buffer
    VkCommandBuffer buffers[1];
    buffers[0] = sample->GetSetupCommandBuffer();

    VkSubmitInfo submit_info;
    submit_info.sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext                   = NULL;
    submit_info.waitSemaphoreCount      = 0;
    submit_info.pWaitSemaphores         = NULL;
    submit_info.pWaitDstStageMask       = NULL;
    submit_info.commandBufferCount      = 1;
    submit_info.pCommandBuffers         = &buffers[0];
    submit_info.signalSemaphoreCount    = 0;
    submit_info.pSignalSemaphores       = NULL;

    // Submit to our shared graphics queue.
    err = vkQueueSubmit(sample->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
    assert(!err);

    // Wait for the queue to become idle.
    err = vkQueueWaitIdle(sample->GetGraphicsQueue());
    assert(!err);

    // Cleanup the mip structures
    for (uint32_t level = 0; level < numMipLevels; level++)
    {
        vkDestroyImage(sample->GetDevice(), mipImages[level], nullptr);
        vkFreeMemory(  sample->GetDevice(), mipMemory[level], nullptr);
    }

    // Now create a sampler for this image, with required details
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType                     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext                     = nullptr;
    samplerCreateInfo.magFilter                 = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter                 = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode                = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias                = 0.0f;
    samplerCreateInfo.anisotropyEnable          = VK_FALSE;
    samplerCreateInfo.maxAnisotropy             = 0;
    samplerCreateInfo.compareOp                 = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                    = 0.0f;
    samplerCreateInfo.maxLod                    = 0.0f;
    samplerCreateInfo.borderColor               = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates   = VK_FALSE;

    err = vkCreateSampler(sample->GetDevice(), &samplerCreateInfo, NULL, &textureObject->mSampler);
    assert(!err);

    // Create the image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType                            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                            = NULL;
    viewCreateInfo.image                            = VK_NULL_HANDLE;
    viewCreateInfo.viewType                         = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                           = textureObject->mFormat;
    viewCreateInfo.components.r                     = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                     = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                     = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                     = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel    = 0;
    viewCreateInfo.subresourceRange.levelCount      = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
    viewCreateInfo.subresourceRange.layerCount      = 1;
    viewCreateInfo.flags                            = 0;
    viewCreateInfo.image                            = textureObject->mImage;

    err = vkCreateImageView(sample->GetDevice(), &viewCreateInfo, NULL, &textureObject->mView);
    assert(!err);

    // All relevant objects created
    return true;}

///////////////////////////////////////////////////////////////////////////////

bool TextureObject::FromASTCFile(VkSampleFramework* sample, const char* filename, TextureObject* textureObject)
{
    if (!textureObject || !sample || !filename)
    {
        return false;
    }

    // Get the image file from the Android asset manager
    AAsset* file = AAssetManager_open(sample->GetAssetManager(), filename, AASSET_MODE_BUFFER);
    assert(file);

    uint32_t file_buffer_size = AAsset_getLength(file);
    uint8_t* file_buffer = ( uint8_t*)AAsset_getBuffer(file);

    assert(file_buffer);

    // The ASTC image assets have a basic header, followed by raw data.
    struct astc_header
    {
        uint8_t magic[4];
        uint8_t blockdim_x;
        uint8_t blockdim_y;
        uint8_t blockdim_z;
        uint8_t xsize[3];
        uint8_t ysize[3];
        uint8_t zsize[3];
    };

    astc_header* header = (astc_header*)file_buffer;
    uint8_t* astc_data = file_buffer + sizeof(astc_header);
    uint32_t astc_data_size = file_buffer_size - sizeof(astc_header);

    textureObject->mWidth   = header->xsize[0] + ((uint32_t)header->xsize[1]<<8) + ((uint32_t)header->xsize[2]<<16);
    textureObject->mHeight  = header->ysize[0] + ((uint32_t)header->ysize[1]<<8) + ((uint32_t)header->ysize[2]<<16);

    // Do not support 3d textures
    assert(header->blockdim_z==1);

    // We map the block dimensions to vulkan formats
    // More mappings can be added here. More information on supported formats can be found in the
    // Vulkan Specification
    if (header->blockdim_x == 8 && header->blockdim_y == 8)
    {
        textureObject->mFormat = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    }
    else if (header->blockdim_x == 4 && header->blockdim_y == 4)
    {
        textureObject->mFormat = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    }
    else if (header->blockdim_x == 10 && header->blockdim_y == 10)
    {
        textureObject->mFormat = VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    }
    else
    {
        LOGE("Whilst loading %s: Unsupported block dimensions %dx%d", filename, header->blockdim_x, header->blockdim_y);
        assert("Unsupported block dimensions"&&0);
    }

    VkResult   err;
    bool   pass;

    textureObject->mSample = sample;


    // Initialize the Create Info structure
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType               = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext               = NULL;
    imageCreateInfo.imageType           = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format              = textureObject->mFormat;
    imageCreateInfo.extent.depth        = 1;
    imageCreateInfo.extent.height       = textureObject->mHeight;
    imageCreateInfo.extent.width        = textureObject->mWidth;
    imageCreateInfo.mipLevels           = 1;
    imageCreateInfo.arrayLayers         = 1;
    imageCreateInfo.samples             = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling              = VK_IMAGE_TILING_LINEAR;
    imageCreateInfo.usage               = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.flags               = 0;
    imageCreateInfo.sharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout       = VK_IMAGE_LAYOUT_PREINITIALIZED;

    // Initialize the memory allocation structure
    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext            = NULL;
    memoryAllocateInfo.allocationSize   = 0;
    memoryAllocateInfo.memoryTypeIndex  = 0;

    // Create a source image that will help us load the texture
    VkImage srcImage;
    err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, NULL, &srcImage);
    assert(!err);

    // Get the memory requirements for this source image
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(sample->GetDevice(), srcImage, &mem_reqs);
    memoryAllocateInfo.allocationSize  = mem_reqs.size;
    pass = sample->GetMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAllocateInfo.memoryTypeIndex);
    assert(pass);

    // Allocate and bind the memory for the source image
    VkDeviceMemory srcMem;
    err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &srcMem);
    assert(!err);
    err = vkBindImageMemory(sample->GetDevice(), srcImage, srcMem, 0);
    assert(!err);

    // Map the memory and copy to it, and then unmap
    void *data;
    err = vkMapMemory(sample->GetDevice(), srcMem, 0, memoryAllocateInfo.allocationSize, 0, &data);
    assert(!err);
    assert(memoryAllocateInfo.allocationSize >= astc_data_size );
    memcpy( data, astc_data, astc_data_size);
    vkUnmapMemory(sample->GetDevice(), srcMem);

    // Setup texture as blit (destination) target ( and also to be subsequently used as a sampled texture
    imageCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.mipLevels   = 1;
    imageCreateInfo.extent.width = textureObject->mWidth;
    imageCreateInfo.extent.height = textureObject->mHeight;
    imageCreateInfo.extent.depth = 1;

    // Create the image
    err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, nullptr, &textureObject->mImage);
    assert(!err);

    // Get the memory requirements for the image
    vkGetImageMemoryRequirements(sample->GetDevice(), textureObject->mImage, &mem_reqs);
    memoryAllocateInfo.allocationSize   = mem_reqs.size;
    pass = sample->GetMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex);
    assert(pass);

    // allocate and bind memory for the image
    err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &textureObject->mMem);
    assert(!err);
    err = vkBindImageMemory(sample->GetDevice(), textureObject->mImage, textureObject->mMem, 0);
    assert(!err);

    // Reset the setup command buffer
    vkResetCommandBuffer(sample->GetSetupCommandBuffer(), 0);

    VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
    commandBufferInheritanceInfo.sType                  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    commandBufferInheritanceInfo.pNext                  = NULL;
    commandBufferInheritanceInfo.renderPass             = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.subpass                = 0;
    commandBufferInheritanceInfo.framebuffer            = VK_NULL_HANDLE;
    commandBufferInheritanceInfo.occlusionQueryEnable   = VK_FALSE;
    commandBufferInheritanceInfo.queryFlags             = 0;
    commandBufferInheritanceInfo.pipelineStatistics     = 0;

    VkCommandBufferBeginInfo setupCmdsBeginInfo;
    setupCmdsBeginInfo.sType                            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    setupCmdsBeginInfo.pNext                            = NULL;
    setupCmdsBeginInfo.flags                            = 0;
    setupCmdsBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

    // Begin recording to the command buffer.
    vkBeginCommandBuffer(sample->GetSetupCommandBuffer(), &setupCmdsBeginInfo);

    // Change image layout to Transfer_DST so it can be filled.
    sample->SetImageLayout(textureObject->mImage, sample->GetSetupCommandBuffer(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1);

    // Change source image layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
    sample->SetImageLayout(srcImage, sample->GetSetupCommandBuffer(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1);

    // Create a region for the image blit
    VkImageCopy region = {};

    region.srcSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.baseArrayLayer    = 0;
    region.srcSubresource.mipLevel          = 0;
    region.srcSubresource.layerCount        = 1;
    region.srcOffset.x                      = 0;
    region.srcOffset.y                      = 0;
    region.srcOffset.z                      = 0;
    region.dstSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.baseArrayLayer    = 0;
    region.dstSubresource.mipLevel          = 0;
    region.dstSubresource.layerCount        = 1;
    region.dstOffset.x                      = 0;
    region.dstOffset.y                      = 0;
    region.dstOffset.z                      = 0;
    region.extent.width                     = textureObject->mWidth;
    region.extent.height                    = textureObject->mHeight;
    region.extent.depth                     = 1;

    // Put image copy into command buffer
    vkCmdCopyImage(sample->GetSetupCommandBuffer(), srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureObject->mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Change the layout of the image to shader read only
    sample->SetImageLayout(textureObject->mImage, sample->GetSetupCommandBuffer(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1);

    // We are finished recording operation
    vkEndCommandBuffer(sample->GetSetupCommandBuffer());

    // Prepare to submit the command buffer
    VkCommandBuffer buffers[1];
    buffers[0] = sample->GetSetupCommandBuffer();

    VkSubmitInfo submit_info;
    submit_info.sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext                   = NULL;
    submit_info.waitSemaphoreCount      = 0;
    submit_info.pWaitSemaphores         = NULL;
    submit_info.pWaitDstStageMask       = NULL;
    submit_info.commandBufferCount      = 1;
    submit_info.pCommandBuffers         = &buffers[0];
    submit_info.signalSemaphoreCount    = 0;
    submit_info.pSignalSemaphores       = NULL;

    // Submit to our shared graphics queue.
    err = vkQueueSubmit(sample->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);
    assert(!err);

    // Wait for the queue to become idle.
    err = vkQueueWaitIdle(sample->GetGraphicsQueue());
    assert(!err);

    // Cleanup
    vkDestroyImage(sample->GetDevice(), srcImage, nullptr);
    vkFreeMemory(sample->GetDevice(), srcMem, nullptr);

    // Update the layout so we remember it..
    textureObject->mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Now create a sampler for this image, with required details
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType                         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext                         = nullptr;
    samplerCreateInfo.magFilter                     = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter                     = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode                    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU                  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV                  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW                  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias                    = 0.0f;
    samplerCreateInfo.anisotropyEnable              = VK_FALSE;
    samplerCreateInfo.maxAnisotropy                 = 0;
    samplerCreateInfo.compareOp                     = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                        = 0.0f;
    samplerCreateInfo.maxLod                        = 0.0f;
    samplerCreateInfo.borderColor                   = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates       = VK_FALSE;
    err = vkCreateSampler(sample->GetDevice(), &samplerCreateInfo, NULL, &textureObject->mSampler);
    assert(!err);

    // Create the image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType                            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                            = NULL;
    viewCreateInfo.viewType                         = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                           = textureObject->mFormat;
    viewCreateInfo.components.r                     = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                     = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                     = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                     = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel    = 0;
    viewCreateInfo.subresourceRange.levelCount      = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
    viewCreateInfo.subresourceRange.layerCount      = 1;
    viewCreateInfo.flags                            = 0;
    viewCreateInfo.image                            = textureObject->mImage;

    err = vkCreateImageView(sample->GetDevice(), &viewCreateInfo, NULL, &textureObject->mView);
    assert(!err);

    AAsset_close(file);

    // All relevant objects created
    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool TextureObject::CreateTexture(VkSampleFramework* sample, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags additionalUsage, TextureObject* textureObject)
{
    if (!textureObject || !sample)
    {
        return false;
    }

    LOGE("****************************************");
    LOGE("Creating %dx%d texture...", width, height);
    LOGE("****************************************");

    textureObject->mFormat = format;
    textureObject->mSample      = sample;
    textureObject->mWidth       = width;
    textureObject->mHeight      = height;
    textureObject->mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    textureObject->mMips        = 1;    // This is hardcoded below!

    VkResult   err;
    bool   pass;

    textureObject->mSample = sample;

    // VkImageCreateInfo->VkExternalMemoryImageCreateInfoKHR->VkDedicatedAllocationImageCreateInfoNV)
    // Since this texture object needs to be shared with OpenGL, 
    // we need an extra structure of data.
    // VkDedicatedAllocationImageCreateInfoNV dedicatedImageProps = {};
    // memset(&dedicatedImageProps, 0, sizeof(VkDedicatedAllocationImageCreateInfoNV));
    // dedicatedImageProps.sType = VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV;
    // dedicatedImageProps.pNext = NULL;
    // dedicatedImageProps.dedicatedAllocation = VK_TRUE;

    VkExternalMemoryImageCreateInfoKHR extMemImgCreateInfo = {};
    memset(&extMemImgCreateInfo, 0, sizeof(VkExternalMemoryImageCreateInfoKHR));
    extMemImgCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
    extMemImgCreateInfo.pNext = NULL;   //  &dedicatedImageProps;
    extMemImgCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;


    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext = &extMemImgCreateInfo;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = textureObject->mFormat;
    imageCreateInfo.extent.depth = 1.0f;
    imageCreateInfo.extent.height = textureObject->mHeight;
    imageCreateInfo.extent.width = textureObject->mWidth;
    imageCreateInfo.mipLevels = 1;    // Don't change this without changing "textureObject->mMips = 1;"

    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | additionalUsage;
    imageCreateInfo.flags = 0;

    // Before actually creating the image, we need a few more structures
    VkPhysicalDeviceExternalImageFormatInfoKHR DeviceExternalImageFormatInfo = {};
    DeviceExternalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR;
    DeviceExternalImageFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VkPhysicalDeviceImageFormatInfo2KHR DeviceImageFormatInfo = {};
    DeviceImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR;
    DeviceImageFormatInfo.format = imageCreateInfo.format;
    DeviceImageFormatInfo.type = imageCreateInfo.imageType;
    DeviceImageFormatInfo.tiling = imageCreateInfo.tiling;
    DeviceImageFormatInfo.usage = imageCreateInfo.usage;
    DeviceImageFormatInfo.flags = imageCreateInfo.flags;
    DeviceImageFormatInfo.pNext = &DeviceExternalImageFormatInfo;

    VkExternalImageFormatPropertiesKHR ExternalImageFormatProperties = {};
    ExternalImageFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR;

    VkImageFormatProperties2KHR ImageFormatProperties = {};
    ImageFormatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR;
    ImageFormatProperties.pNext = &ExternalImageFormatProperties;

    err = g_vkGetPhysicalDeviceImageFormatProperties2KHR(sample->GetPhysicalDevice(), &DeviceImageFormatInfo, &ImageFormatProperties);
    assert(!err);

    // Check for properties of the memory
    bool isExportable = ((ExternalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR) != 0);
    bool canBeOpaque = ((ExternalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR) != 0);
    bool RequireDedicatedOnly = ((ExternalImageFormatProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR) != 0);

    VkExternalMemoryImageCreateInfoKHR ExternalMemoryImageCreateInfo = {};
    ExternalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR;
    ExternalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
    imageCreateInfo.pNext = &ExternalMemoryImageCreateInfo;

    // Now that other structures are added, create the image
    err = vkCreateImage(sample->GetDevice(), &imageCreateInfo, NULL, &textureObject->mImage);
    assert(!err);

    // Need the memory requirements for the new image
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(sample->GetDevice(), textureObject->mImage, &mem_reqs);

    uint32 MemoryTypeIndex = 0;
    pass = sample->GetMemoryTypeFromProperties(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &MemoryTypeIndex);
    assert(pass);

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = NULL;    //  &dedicatedProps;
    memoryAllocateInfo.memoryTypeIndex = MemoryTypeIndex;   //  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    memoryAllocateInfo.allocationSize = mem_reqs.size;

    textureObject->mMemorySize = (uint32_t)memoryAllocateInfo.allocationSize;

    // Since this texture object needs to be shared with OpenGL, 
    // we need an extra structure of data.
    VkExportMemoryAllocateInfoKHR extMemCreateInfo = {};
    memset(&extMemCreateInfo, 0, sizeof(VkExportMemoryAllocateInfoKHR));
    extMemCreateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    extMemCreateInfo.pNext = NULL;  //  &dedicatedProps;
    extMemCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    // Set this as next until we require dedicated props
    memoryAllocateInfo.pNext = &extMemCreateInfo;

    VkMemoryDedicatedAllocateInfoKHR dedicatedProps = {};
    memset(&dedicatedProps, 0, sizeof(VkMemoryDedicatedAllocateInfoKHR));
    if (RequireDedicatedOnly)
    {
        dedicatedProps.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
        dedicatedProps.image = textureObject->mImage;

        dedicatedProps.pNext = memoryAllocateInfo.pNext;
        memoryAllocateInfo.pNext = &dedicatedProps;
    }


    err = vkAllocateMemory(sample->GetDevice(), &memoryAllocateInfo, NULL, &textureObject->mMem);
    assert(!err);

    err = vkBindImageMemory(sample->GetDevice(), textureObject->mImage, textureObject->mMem, 0);
    assert(!err);

    // Get a handle that can be passed to GL
    if (g_vkGetMemoryFdKHR == NULL)
    {
        LOGE("****************************************");
        LOGE("Error! Do NOT have function pointer for vkGetMemoryFdKHR()!");
        LOGE("****************************************");
    }
    else if (g_vkGetMemoryFdKHR != NULL)
    {
        // LOGE("****************************************");
        // LOGE("Calling vkGetMemoryFdKHR() = 0x%x", (int)g_vkGetMemoryFdKHR);
        // LOGE("****************************************");
        VkMemoryGetFdInfoKHR getFdInfo = {};
        memset(&getFdInfo, 0, sizeof(VkMemoryGetFdInfoKHR));
        getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        getFdInfo.pNext = NULL;
        getFdInfo.memory = textureObject->mMem;
        getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

        err = g_vkGetMemoryFdKHR(sample->GetDevice(), &getFdInfo, &textureObject->mGlHandle);
        // LOGE("****************************************");
        // LOGE("Back from vkGetMemoryFdKHR(): Error = %d", err);
        // LOGE("****************************************");
        assert(!err);
        //LOGE("****************************************");
        //LOGE("Back from assert on vkGetMemoryFdKHR()");
        //LOGE("****************************************");
    }

    // Change the layout of the image to shader read only
    textureObject->SetLayoutImmediate(VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, textureObject->mImageLayout );

    // Now create a sampler for this image, with required details
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType                     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext                     = nullptr;
    samplerCreateInfo.magFilter                 = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter                 = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode                = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW              = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias                = 0.0f;
    samplerCreateInfo.anisotropyEnable          = VK_FALSE;
    samplerCreateInfo.maxAnisotropy             = 0;
    samplerCreateInfo.compareOp                 = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod                    = 0.0f;
    samplerCreateInfo.maxLod                    = 0.0f;
    samplerCreateInfo.borderColor               = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCreateInfo.unnormalizedCoordinates   = VK_FALSE;

    err = vkCreateSampler(sample->GetDevice(), &samplerCreateInfo, NULL, &textureObject->mSampler);
    assert(!err);

    // Create the image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType                            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.pNext                            = NULL;
    viewCreateInfo.viewType                         = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format                           = textureObject->mFormat;
    viewCreateInfo.components.r                     = VK_COMPONENT_SWIZZLE_R;
    viewCreateInfo.components.g                     = VK_COMPONENT_SWIZZLE_G;
    viewCreateInfo.components.b                     = VK_COMPONENT_SWIZZLE_B;
    viewCreateInfo.components.a                     = VK_COMPONENT_SWIZZLE_A;
    viewCreateInfo.subresourceRange.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCreateInfo.subresourceRange.baseMipLevel    = 0;
    viewCreateInfo.subresourceRange.levelCount      = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer  = 0;
    viewCreateInfo.subresourceRange.layerCount      = 1;
    viewCreateInfo.flags                            = 0;
    viewCreateInfo.image                            = textureObject->mImage;

    err = vkCreateImageView(sample->GetDevice(), &viewCreateInfo, NULL, &textureObject->mView);
    assert(!err);

    // All relevant objects created
    return true;
}