
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "sample.h"

using namespace Svr;

// The vertex buffer bind id, used as a constant in various places in the sample
#define VERTEX_BUFFER_BIND_ID 0

// Sample Name
#define SAMPLE_NAME "Vulkan Sample: SVR Rendering"
#define SAMPLE_VERSION 1


#ifndef DEG_TO_RAD
#define DEG_TO_RAD (float)M_PI / 180.0f
#endif

#ifndef RAD_TO_DEG
#define RAD_TO_DEG (float)180.0f / M_PI
#endif


static bool gContextCreated = false;

static bool gFirstInit = false;
static bool gIsPaused = true;
static bool gSvrStarted = false;
static bool gRecreateContext = false;

int gRenderFieldOfView = 90;
svrHeadPoseState gCurrentPoseState;

extern PFN_vkGetSemaphoreFdKHR g_vkGetSemaphoreFdKHR;
extern PFN_vkGetFenceFdKHR g_vkGetFenceFdKHR;

VkFormat gEyeColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
VkFormat gEyeDepthFormat = VK_FORMAT_D16_UNORM;

// Potential Config Items
float   gObjectRadius = 6.0f;
bool    gObjectRotates = true;
bool    gObjectBobbles = false;
float   gBobbleAmount = 2.0f;
float   gBobbleSpeed = 2.0f;
bool    gBobbleHorizontal = false;
bool    gBobbleVertical = false;

// For the object bobble
float gTotalBobble = 0.0f;

unsigned int gLastUpdateTime = 0;

//-----------------------------------------------------------------------------
unsigned int GetTimeMS()
//-----------------------------------------------------------------------------
{
    struct timeval t;
    t.tv_sec = t.tv_usec = 0;

    if (gettimeofday(&t, NULL) == -1)
    {
        return 0;
    }

    return (unsigned int)(t.tv_sec * 1000LL + t.tv_usec / 1000LL);
}

void EndVR()
{
    if (gSvrStarted)
    {
        LOGI("Calling svrEndVr...");
        svrEndVr();
        gSvrStarted = false;
        LOGI("gSvrStarted = %d", gSvrStarted);
    }
}

void PauseVR()
{
    gIsPaused = true;
    LOGI("gIsPaused = %d", gIsPaused);
    if (gSvrStarted)
    {
        EndVR();
    }
}

void StartVR(android_app *pApp)
{
    if (pApp == NULL || pApp->window == NULL)
    {
        LOGE("StartVR returning because window has not been created!");
        return;
    }

    //Could be called again, if native window changes. End previous start first.
    LOGI("Calling EndVR() at the beginning of StartVR()...");
    EndVR();

    // LOGI("StartVR: pApp = 0x%x", pApp);
    // LOGI("StartVR: pApp->window = 0x%x", pApp->window);

    float fltWidth = (float)ANativeWindow_getWidth(pApp->window);
    float fltHeight = (float)ANativeWindow_getHeight(pApp->window);

    // Only start in Landscape Mode
    if (fltWidth >= fltHeight)
    {
        LOGI("Calling svrSetTrackingMode (0x%x)...", kTrackingRotation | kTrackingPosition);
        svrSetTrackingMode(kTrackingRotation | kTrackingPosition);

        svrBeginParams beginParams;
        memset(&beginParams, 0, sizeof(beginParams));

        beginParams.cpuPerfLevel = kPerfMedium;
        beginParams.gpuPerfLevel = kPerfMedium;
        beginParams.nativeWindow = pApp->window;
        beginParams.optionFlags = 0;
        beginParams.mainThreadId = gettid();

        LOGI("Calling svrBeginVr...");
        svrBeginVr(&beginParams);

        gSvrStarted = true;
        LOGI("gSvrStarted = %d", gSvrStarted);
    }
}


VkSample::VkSample()
    : VkSampleFramework(SAMPLE_NAME, SAMPLE_VERSION)
{
    // This is kind of stupid to hard-code this here, so hard-code it in the framework
    // this->SetUseValidation(true);

    mCurrentEyeBuffer = 0;
    mEyeBufferWidth = 1024; // Set until we get actual size from SVR
    mEyeBufferHeight = 1024;

    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        mEyeRenderSemaphore[whichBuffer] = 0;
    }   // Each Buffer

}
///////////////////////////////////////////////////////////////////////////////

VkSample::~VkSample()
{

}

///////////////////////////////////////////////////////////////////////////////

void VkSample::PointerDownEvent(int pointerId, float x, float y)
{
    // LOGI("Pointer %d: Down (%0.2f, %0.2f)", pointerId, x, y);

    // Recenter the pose
    svrRecenterPose();
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::PointerUpEvent(int pointerId, float x, float y)
{
    // LOGI("Pointer %d: Up   (%0.2f, %0.2f)", pointerId, x, y);
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::PointerMoveEvent(int pointerId, float x, float y)
{
    // LOGI("Pointer %d: Move (%0.2f, %0.2f)", pointerId, x, y);
}

///////////////////////////////////////////////////////////////////////////////


void VkSample::WindowResize(uint32_t newWidth, uint32_t newHeight)
{
    mWidth  = newWidth/2;
    mHeight = newHeight/2;
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitVertexBuffers()
{
    if (!MeshObject::LoadObj(this, "torus.obj", VERTEX_BUFFER_BIND_ID, &mMesh))
    {
        LOGE("Error loading sample mesh file");
        return;
    }
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitUniformBuffers()
{

    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {
            for (uint32_t whichObject = 0; whichObject < NUM_OBJECTS; whichObject++)
            {
                // Set some default values...
                mEyeVertData[whichObject].proj = mProjectionMatrix;
                mEyeVertData[whichObject].view = mViewMatrix;
                mEyeVertData[whichObject].model = glm::mat4(1.0f);

                mEyeFragData[whichObject].color = glm::vec4(0.75f, 0.0f, 0.75f, 1.0f);
                mEyeFragData[whichObject].ambient = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
                mEyeFragData[whichObject].eyePos = glm::vec4(mCameraPos.x, mCameraPos.y, mCameraPos.z, 1.0f);
                mEyeFragData[whichObject].lightPos = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                mEyeFragData[whichObject].lightColor = glm::vec4(1.0f, 1.0f, 0.8f, 1.0f);

                // ...and change whatever is specific per object
                switch (whichObject)
                {
                case 0:     // +X
                    mBaseObjectPos[whichObject] = glm::vec3(gObjectRadius, 0.0f, 0.0f);
                    mBaseObjectColor[whichObject] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    break;

                case 1:     // +Y
                    mBaseObjectPos[whichObject] = glm::vec3(0.0f, gObjectRadius, 0.0f);
                    mBaseObjectColor[whichObject] = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
                    break;

                case 2:     // +Z
                    mBaseObjectPos[whichObject] = glm::vec3(0.0f, 0.0f, gObjectRadius);
                    mBaseObjectColor[whichObject] = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                    break;

                case 3:     // -X
                    mBaseObjectPos[whichObject] = glm::vec3(-gObjectRadius, 0.0f, 0.0f);
                    mBaseObjectColor[whichObject] = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
                    break;

                case 4:     // -Y
                    mBaseObjectPos[whichObject] = glm::vec3(0.0f, -gObjectRadius, 0.0f);
                    mBaseObjectColor[whichObject] = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
                    break;

                case 5:     // -Z
                    mBaseObjectPos[whichObject] = glm::vec3(0.0f, 0.0f, -gObjectRadius);
                    mBaseObjectColor[whichObject] = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                    break;
                }

                float colorScale = 0.8f;
                mBaseObjectColor[whichObject] *= colorScale;
                mBaseObjectColor[whichObject][3] = 1.0;

                mEyeVertData[whichObject].model = glm::translate(glm::mat4(1.0f), mBaseObjectPos[whichObject]);
                mEyeFragData[whichObject].color = mBaseObjectColor[whichObject];

                mEyeBuffers[whichBuffer][whichEye].VertUniformBuffer[whichObject].InitBuffer(this, sizeof(mEyeVertData[whichObject]), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (const char*)&mEyeVertData[whichObject]);
                mEyeBuffers[whichBuffer][whichEye].FragUniformBuffer[whichObject].InitBuffer(this, sizeof(mEyeFragData[whichObject]), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (const char*)&mEyeFragData[whichObject]);
            }   // Each Object
        }   // Each Eye
    }   // Each Buffer
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitEyeLayouts()
{
    VkResult ret = VK_SUCCESS;

    // One vertext uniform buffer and one fragment uniform buffer
    VkDescriptorSetLayoutBinding uniformAndSamplerBinding[2] = {};

    uniformAndSamplerBinding[0].binding             = 0;
    uniformAndSamplerBinding[0].descriptorCount     = 1;
    uniformAndSamplerBinding[0].descriptorType      = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uniformAndSamplerBinding[0].stageFlags          = VK_SHADER_STAGE_VERTEX_BIT;
    uniformAndSamplerBinding[0].pImmutableSamplers  = nullptr;

    uniformAndSamplerBinding[1].binding             = 1;
    uniformAndSamplerBinding[1].descriptorCount     = 1;
    uniformAndSamplerBinding[1].descriptorType      = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    uniformAndSamplerBinding[1].stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
    uniformAndSamplerBinding[1].pImmutableSamplers  = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType             = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext             = nullptr;
    descriptorSetLayoutCreateInfo.bindingCount      = 2;
    descriptorSetLayoutCreateInfo.pBindings         = &uniformAndSamplerBinding[0];

    ret = vkCreateDescriptorSetLayout(mDevice, &descriptorSetLayoutCreateInfo, nullptr, &mEyeDescriptorLayout);
    VK_CHECK(!ret);

    // Our pipeline layout simply points to the empty descriptor layout.
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext                  = nullptr;
    pipelineLayoutCreateInfo.setLayoutCount         = 1;
    pipelineLayoutCreateInfo.pSetLayouts            = &mEyeDescriptorLayout;
    ret = vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, nullptr, &mEyePipelineLayout);
    VK_CHECK(!ret);
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitEyeDescriptorSet()
{
    // Set up the descriptor set
    VkResult err;
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType                 = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext                 = nullptr;
    descriptorSetAllocateInfo.descriptorPool        = mDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount    = 1;
    descriptorSetAllocateInfo.pSetLayouts = &mEyeDescriptorLayout;

    VkWriteDescriptorSet writes[2] = {};

    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {
            for (uint32_t whichObject = 0; whichObject < NUM_OBJECTS; whichObject++)
            {
                err = vkAllocateDescriptorSets(mDevice, &descriptorSetAllocateInfo, &mEyeBuffers[whichBuffer][whichEye].DescriptorSet[whichObject]);
                VK_CHECK(!err);
                // LOGI("Validation: Eye Descriptor Set [%d][%d][%d] = 0x%x", whichBuffer, whichEye, whichBuffer, (unsigned int)mEyeBuffers[whichBuffer][whichEye].DescriptorSet[whichObject]);

                VkDescriptorBufferInfo vertUniform = mEyeBuffers[whichBuffer][whichEye].VertUniformBuffer[whichObject].GetDescriptorInfo();
                writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[0].dstBinding = 0;
                writes[0].dstSet = mEyeBuffers[whichBuffer][whichEye].DescriptorSet[whichObject];
                writes[0].descriptorCount = 1;
                writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writes[0].pBufferInfo = &vertUniform;

                VkDescriptorBufferInfo fragUniform = mEyeBuffers[whichBuffer][whichEye].FragUniformBuffer[whichObject].GetDescriptorInfo();
                writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[1].dstBinding = 1;
                writes[1].dstSet = mEyeBuffers[whichBuffer][whichEye].DescriptorSet[whichObject];
                writes[1].descriptorCount = 1;
                writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writes[1].pBufferInfo = &fragUniform;

                vkUpdateDescriptorSets(mDevice, 2, &writes[0], 0, nullptr);
            }   // Each Object
        }   // Each Eye
    }   // Each Buffer

}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitEyeRenderPass()
{
    // The renderpass defines the attachments to the framebuffer object that gets
    // used in the pipeline. We have two attachments, the colour buffer, and the
    // depth buffer. The operations and layouts are set to defaults for this type
    // of attachment.

    LOGE("**************************************************");
    LOGE("Creating Eye Render Passes...");
    LOGE("**************************************************");
    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {

            LOGE("    EyeBuffer[%d][%d]: Color Format = 0x%08x; Depth Format = 0x%08x", whichBuffer, whichEye,
                                                            gEyeColorFormat,
                                                            gEyeDepthFormat);
            VkAttachmentDescription attachmentDescriptions[2];
            memset(attachmentDescriptions, 0, sizeof(attachmentDescriptions));

            attachmentDescriptions[0].flags = 0;
            attachmentDescriptions[0].format = gEyeColorFormat;
            attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescriptions[1].flags = 0;
            attachmentDescriptions[1].format = gEyeDepthFormat;
            attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // We have references to the attachment offsets, stating the layout type.
            VkAttachmentReference colorReference = {};
            colorReference.attachment = 0;
            colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthReference = {};
            depthReference.attachment = 1;
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // There can be multiple subpasses in a renderpass, but this example has only one.
            // We set the color and depth references at the grahics bind point in the pipeline.
            VkSubpassDescription subpassDescription = {};
            subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescription.flags = 0;
            subpassDescription.inputAttachmentCount = 0;
            subpassDescription.pInputAttachments = nullptr;
            subpassDescription.colorAttachmentCount = 1;
            subpassDescription.pColorAttachments = &colorReference;
            subpassDescription.pResolveAttachments = nullptr;
            subpassDescription.pDepthStencilAttachment = &depthReference;
            subpassDescription.preserveAttachmentCount = 0;
            subpassDescription.pPreserveAttachments = nullptr;

            // The renderpass itself is created with the number of subpasses, and the
            // list of attachments which those subpasses can reference.
            VkRenderPassCreateInfo renderPassCreateInfo = {};
            renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassCreateInfo.pNext = nullptr;
            renderPassCreateInfo.attachmentCount = 2;
            renderPassCreateInfo.pAttachments = attachmentDescriptions;
            renderPassCreateInfo.subpassCount = 1;
            renderPassCreateInfo.pSubpasses = &subpassDescription;
            renderPassCreateInfo.dependencyCount = 0;
            renderPassCreateInfo.pDependencies = nullptr;

            VkResult ret;
            ret = vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mEyeBuffers[whichBuffer][whichEye].RenderPass);
            VK_CHECK(!ret);
        }   // Each Eye
    }   // Each Buffer
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitDescriptorPool()
{
    //Create a pool with the amount of descriptors we require
    VkDescriptorPoolSize poolSize[2] = {};

    // Each descriptor has 2 uniform buffers for each of SVR_NUM_EYE_BUFFERS * SVR_NUM_EYES * NUM_OBJECTS descriptors

    poolSize[0].type                        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    poolSize[0].descriptorCount             = 2 * SVR_NUM_EYE_BUFFERS * SVR_NUM_EYES * NUM_OBJECTS + 10;    // +10 until we get rid of single output tests

    poolSize[1].type                        = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize[1].descriptorCount             = 5 * SVR_NUM_EYE_BUFFERS;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext          = nullptr;
    descriptorPoolCreateInfo.maxSets        = 2 * SVR_NUM_EYE_BUFFERS * SVR_NUM_EYES * NUM_OBJECTS;
    descriptorPoolCreateInfo.poolSizeCount  = 2;
    descriptorPoolCreateInfo.pPoolSizes     = poolSize;

    VkResult err;
    err = vkCreateDescriptorPool(mDevice, &descriptorPoolCreateInfo, NULL, &mDescriptorPool);
    VK_CHECK(!err);
    // LOGI("Validation: Descriptor Pool = 0x%x", (unsigned int)mDescriptorPool);
}

///////////////////////////////////////////////////////////////////////////////

void VkSample::InitEyeBuffers()
{
    LOGI("Creating %dx%d eye buffers...", mEyeBufferWidth, mEyeBufferHeight);
    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {
            ImageViewObject::CreateImageView(this, mEyeBufferWidth, mEyeBufferHeight, gEyeColorFormat,
                                                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mEyeBuffers[whichBuffer][whichEye].RenderTarget);

            ImageViewObject::CreateImageView(this, mEyeBufferWidth, mEyeBufferHeight, gEyeDepthFormat,
                                                VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mEyeBuffers[whichBuffer][whichEye].DepthTarget);

            TextureObject::CreateTexture(this, mEyeBufferWidth, mEyeBufferHeight, gEyeColorFormat,
                                                VK_IMAGE_USAGE_TRANSFER_DST_BIT, &mEyeBuffers[whichBuffer][whichEye].OutputTexture);

            VkImageView attachments[2] = {};
            VkFramebufferCreateInfo framebufferCreateInfo = {};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.pNext = nullptr;
            framebufferCreateInfo.renderPass = mEyeBuffers[whichBuffer][whichEye].RenderPass;
            framebufferCreateInfo.attachmentCount = 2;
            framebufferCreateInfo.pAttachments = attachments;
            framebufferCreateInfo.width = mEyeBufferWidth;
            framebufferCreateInfo.height = mEyeBufferHeight;
            framebufferCreateInfo.layers = 1;

            attachments[0] = mEyeBuffers[whichBuffer][whichEye].RenderTarget.GetView();
            attachments[1] = mEyeBuffers[whichBuffer][whichEye].DepthTarget.GetView();

            VkResult ret = vkCreateFramebuffer(mDevice, &framebufferCreateInfo, nullptr, &mEyeBuffers[whichBuffer][whichEye].FrameBuffer);
            VK_CHECK(!ret);
        }   // Each Eye
    }   // Each Buffer
}

void VkSample::InitEyePipelines()
{
    //This pipeline is the one which collects and outputs G-Buffer data
    VkShaderModule sh_def_vert = CreateShaderModuleFromAsset("model_v.spv");
    VkShaderModule sh_def_frag = CreateShaderModuleFromAsset("model_f.spv");

    VkPipelineVertexInputStateCreateInfo visci = mMesh.Buffer().CreatePipelineState();

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = sh_def_vert;
    shaderStages[0].pName  = "main";
    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = sh_def_frag;
    shaderStages[1].pName  = "main";

    // We define a simple viewport and scissor.
    VkPipelineViewportStateCreateInfo      vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    // Instead of statically defining our scissor and viewport as part of the
    // pipeline, we can state they are dynamic, providing them as part of the
    // command stream via command buffer recording operations.
    VkDynamicState dynamicStateList[2] = {};
    dynamicStateList[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStateList[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext                = nullptr;
    dynamicStateCreateInfo.dynamicStateCount    = 2;
    dynamicStateCreateInfo.pDynamicStates       = &dynamicStateList[0];

    // Ensure the colorWriteMask is valid for each attachment.
    VkPipelineColorBlendAttachmentState att_state[1] = {};
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo    cb = {};
    cb.sType                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount      = 1;
    cb.pAttachments         = &att_state[0];

    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {

            InitPipeline(VK_NULL_HANDLE, 2, &shaderStages[0], mEyePipelineLayout, mEyeBuffers[whichBuffer][whichEye].RenderPass, 0, &visci, nullptr,
                nullptr, &vp, nullptr, nullptr, nullptr, &cb, &dynamicStateCreateInfo, false, VK_NULL_HANDLE, &mEyeBuffers[whichBuffer][whichEye].Pipeline);

        }   // Each Eye
    }   // Each Buffer


    vkDestroyShaderModule(mDevice, sh_def_frag, nullptr);
    vkDestroyShaderModule(mDevice, sh_def_vert, nullptr);
}

void VkSample::InitEyeCommandBuffers()
{
    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {
            VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
            commandBufferAllocateInfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.pNext                 = nullptr;
            commandBufferAllocateInfo.commandPool           = mCommandPool;
            commandBufferAllocateInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commandBufferAllocateInfo.commandBufferCount    = 1;

            // Allocate a shared buffer for use in setup operations.
            VkResult ret = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, &mEyeBuffers[whichBuffer][whichEye].CommandBuffer);
            VK_CHECK(!ret);

            // vkBeginCommandBuffer should reset the command buffer, but Reset can be called
            // to make it more explicit.
            VkResult err;
            err = vkResetCommandBuffer(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, 0);
            VK_CHECK(!err);

            VkCommandBufferInheritanceInfo cmd_buf_hinfo = {};
            cmd_buf_hinfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            cmd_buf_hinfo.pNext                 = nullptr;
            cmd_buf_hinfo.renderPass            = VK_NULL_HANDLE;
            cmd_buf_hinfo.subpass               = 0;
            cmd_buf_hinfo.framebuffer           = VK_NULL_HANDLE;
            cmd_buf_hinfo.occlusionQueryEnable  = VK_FALSE;
            cmd_buf_hinfo.queryFlags            = 0;
            cmd_buf_hinfo.pipelineStatistics    = 0;

            VkCommandBufferBeginInfo cmd_buf_info = {};
            cmd_buf_info.sType                  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cmd_buf_info.pNext                  = nullptr;
            cmd_buf_info.flags                  = 0;
            cmd_buf_info.pInheritanceInfo       = &cmd_buf_hinfo;

            // By calling vkBeginCommandBuffer, cmdBuffer is put into the recording state.
            err = vkBeginCommandBuffer(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, &cmd_buf_info);
            VK_CHECK(!err);

            // When starting the render pass, we can set clear values.
            VkClearValue clear_values[2] = {};
            clear_values[0].color.float32[0]        = 0.25f;
            clear_values[0].color.float32[1]        = 0.25f;
            clear_values[0].color.float32[2]        = 0.25f;
            clear_values[0].color.float32[3]        = 1.0f;
            clear_values[1].depthStencil.depth      = 1.0f;
            clear_values[1].depthStencil.stencil    = 0;

            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType                      = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext                      = nullptr;
            rp_begin.renderPass                 = mEyeBuffers[whichBuffer][whichEye].RenderPass;
            rp_begin.framebuffer                = mEyeBuffers[whichBuffer][whichEye].FrameBuffer;
            rp_begin.renderArea.offset.x        = 0;
            rp_begin.renderArea.offset.y        = 0;
            rp_begin.renderArea.extent.width    = mEyeBuffers[whichBuffer][whichEye].OutputTexture.GetWidth();
            rp_begin.renderArea.extent.height   = mEyeBuffers[whichBuffer][whichEye].OutputTexture.GetHeight();
            rp_begin.clearValueCount            = 2;
            rp_begin.pClearValues               = clear_values;

            vkCmdBeginRenderPass(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            // Set our pipeline. This holds all major state
            // the pipeline defines, for example, that the vertex buffer is a triangle list.
            vkCmdBindPipeline(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mEyeBuffers[whichBuffer][whichEye].Pipeline);

            // Provide the dynamic state missing from our pipeline
            VkViewport viewport;
            memset(&viewport, 0, sizeof(viewport));
            viewport.width      = mEyeBuffers[whichBuffer][whichEye].RenderTarget.GetWidth();
            viewport.height     = mEyeBuffers[whichBuffer][whichEye].RenderTarget.GetHeight();
            viewport.minDepth   = 0.0f;
            viewport.maxDepth   = 1.0f;
            vkCmdSetViewport(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, 0, 1, &viewport);

            VkRect2D scissor;
            memset(&scissor, 0, sizeof(scissor));
            scissor.extent.width    = mEyeBuffers[whichBuffer][whichEye].RenderTarget.GetWidth();;
            scissor.extent.height   = mEyeBuffers[whichBuffer][whichEye].RenderTarget.GetHeight();
            scissor.offset.x        = 0;
            scissor.offset.y        = 0;
            vkCmdSetScissor(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, 0, 1, &scissor);

            // Bind our vertex buffer, with a 0 offset.
            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &mMesh.Buffer().GetBuffer(), offsets);

            //bind out descriptor set, which handles our uniforms and samplers
            uint32_t dynamicOffsets[1] = {0};
            for (uint32_t whichObject = 0; whichObject < NUM_OBJECTS; whichObject++)
            {
                vkCmdBindDescriptorSets(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        mEyePipelineLayout, 0, 1, &mEyeBuffers[whichBuffer][whichEye].DescriptorSet[whichObject], 1, dynamicOffsets);

                // Issue a draw command
                vkCmdDraw(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, mMesh.GetNumVertices(), 1, 0, 0);
            }   // Each Object

            // Now our render pass has ended.
            vkCmdEndRenderPass(mEyeBuffers[whichBuffer][whichEye].CommandBuffer);

            TargetToTexture(mEyeBuffers[whichBuffer][whichEye].CommandBuffer, mEyeBuffers[whichBuffer][whichEye].RenderTarget, mEyeBuffers[whichBuffer][whichEye].OutputTexture);

            // By ending the command buffer, it is put out of record mode.
            err = vkEndCommandBuffer(mEyeBuffers[whichBuffer][whichEye].CommandBuffer);
            VK_CHECK(!err);
        }   // Each Eye
    }   // Each Buffer
}

void VkSample::InitEyeSemaphors()
{
    LOGI("******************************");
    LOGI("Creating render complete semaphores...");
    LOGI("******************************");
    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        VkResult ret = VK_SUCCESS;

        // Need extra initialization structure since want to share the handle with OpenGL
        if (g_vkGetSemaphoreFdKHR == NULL)
        {
            LOGE("****************************************");
            LOGE("Error! Do NOT have function pointer for g_vkGetSemaphoreFdKHR()!");
            LOGE("****************************************");
        }
        else
        {
            VkExportSemaphoreCreateInfoKHR exportSemCreateInfo = {};
            memset(&exportSemCreateInfo, 0, sizeof(VkExportSemaphoreCreateInfoKHR));
            exportSemCreateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;
            exportSemCreateInfo.pNext = NULL;
            exportSemCreateInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.pNext = &exportSemCreateInfo;
            semaphoreCreateInfo.flags = 0;

            LOGI("******************************");
            LOGI("    Semaphore (%d of %d)...", whichBuffer + 1, SVR_NUM_EYE_BUFFERS);
            LOGI("******************************");
            ret = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mEyeRenderSemaphore[whichBuffer]);
            VK_CHECK(!ret);
        }
    }   // Each Buffer
    LOGI("******************************");
    LOGI("Render complete semaphores created");
    LOGI("******************************");
}

bool VkSample::InitSample()
{
    // At this point we need to verify that mSwapchainImageCount and SVR_NUM_EYE_BUFFERS work together.
    // We create mDescriptorSet[SVR_NUM_EYE_BUFFERS] but reference them using mSwapchainImageCount
    LOGI("******************************");
    LOGI("mSwapchainImageCount = %d; SVR_NUM_EYE_BUFFERS = %d", mSwapchainImageCount, SVR_NUM_EYE_BUFFERS);
    LOGI("******************************");

    // Get the SVR parameters
    svrDeviceInfo deviceInfo = svrGetDeviceInfo();

    LOGI("******************************");
    LOGI("SVR Device Info:");
    LOGI("******************************");
    LOGI("    Display Width = %d", deviceInfo.displayWidthPixels);
    LOGI("    Display Height = %d", deviceInfo.displayHeightPixels);
    LOGI("    Refresh Rate = %0.2f", deviceInfo.displayRefreshRateHz);
    LOGI("    Orientation = %d", deviceInfo.displayOrientation);
    LOGI("    Eye Width = %d", deviceInfo.targetEyeWidthPixels);
    LOGI("    Eye Height = %d", deviceInfo.targetEyeHeightPixels);
    LOGI("    Fov (X) = %0.2f", deviceInfo.targetFovXRad);
    LOGI("    Fov (Y) = %0.2f", deviceInfo.targetFovYRad);
    LOGI("    OS Version = %d", deviceInfo.deviceOSVersion);

    if (deviceInfo.targetEyeWidthPixels == 0)
    {
        deviceInfo.targetEyeWidthPixels = 1024;
        LOGI("    Eye Width => %d", deviceInfo.targetEyeWidthPixels);
    }

    if (deviceInfo.targetEyeHeightPixels == 0)
    {
        deviceInfo.targetEyeHeightPixels = 1024;
        LOGI("    Eye Height => %d", deviceInfo.targetEyeHeightPixels);
    }

    if (deviceInfo.targetFovXRad == 0)
    {
        deviceInfo.targetFovXRad = 3.14159f / 2.0f;
        LOGI("    Fov (X) => %0.2f", deviceInfo.targetFovXRad);
    }

    if (deviceInfo.targetFovYRad == 0)
    {
        deviceInfo.targetFovYRad = 3.14159f / 2.0f;
        LOGI("    Fov (Y) => %0.2f", deviceInfo.targetFovYRad);
    }


    mEyeBufferWidth = deviceInfo.targetEyeWidthPixels;
    mEyeBufferHeight = deviceInfo.targetEyeHeightPixels;

    gRenderFieldOfView = deviceInfo.targetFovXRad;

    // Initialize our matrices
    float aspect = (float)deviceInfo.targetEyeWidthPixels / (float)deviceInfo.targetEyeHeightPixels;
    mProjectionMatrix = glm::perspective(deviceInfo.targetFovXRad, aspect, 0.1f, 300.0f);

    mCameraPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 lookat(0.0f, 0.0f, -1.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    mViewMatrix =  glm::lookAtRH(mCameraPos, lookat, up);
    mModelMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -8.0f));

    InitUniformBuffers();
    InitVertexBuffers();
    InitDescriptorPool();

    InitEyeLayouts();
    InitEyeDescriptorSet();
    InitEyeRenderPass();
    InitEyeBuffers();
    InitEyePipelines();

    InitEyeCommandBuffers();

    InitEyeSemaphors();

    return true;
}

///////////////////////////////////////////////////////////////////////////////

void L_CreateLayout(float centerX, float centerY, float radiusX, float radiusY, svrLayoutCoords *pLayout)
{
    // This is always in screen space so we want Z = 0 and W = 1
    float lowerLeftPos[4] = { centerX - radiusX, centerY - radiusY, 0.0f, 1.0f };
    float lowerRightPos[4] = { centerX + radiusX, centerY - radiusY, 0.0f, 1.0f };
    float upperLeftPos[4] = { centerX - radiusX, centerY + radiusY, 0.0f, 1.0f };
    float upperRightPos[4] = { centerX + radiusX, centerY + radiusY, 0.0f, 1.0f };

    float lowerUVs[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    float upperUVs[4] = { 0.0f, 1.0f, 1.0f, 1.0f };

    memcpy(pLayout->LowerLeftPos, lowerLeftPos, sizeof(lowerLeftPos));
    memcpy(pLayout->LowerRightPos, lowerRightPos, sizeof(lowerRightPos));
    memcpy(pLayout->UpperLeftPos, upperLeftPos, sizeof(upperLeftPos));
    memcpy(pLayout->UpperRightPos, upperRightPos, sizeof(upperRightPos));

    memcpy(pLayout->LowerUVs, lowerUVs, sizeof(lowerUVs));
    memcpy(pLayout->UpperUVs, upperUVs, sizeof(upperUVs));
}

bool VkSample::Draw()
{
    VkResult err;

    VkCommandBuffer drawbuffers[25];
    uint32_t CmdBuffCount = 0;

    drawbuffers[CmdBuffCount++] = mEyeBuffers[mCurrentEyeBuffer][0].CommandBuffer;  // Left Eye
    drawbuffers[CmdBuffCount++] = mEyeBuffers[mCurrentEyeBuffer][1].CommandBuffer;  // Right Eye

    const VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext                    = nullptr;
    submitInfo.waitSemaphoreCount       = 0;    // 1;
    submitInfo.pWaitSemaphores          = NULL; // &mBackBufferSemaphore;
    submitInfo.pWaitDstStageMask        = &WaitDstStageMask;
    submitInfo.commandBufferCount       = CmdBuffCount;
    submitInfo.pCommandBuffers          = &drawbuffers[0];
    submitInfo.signalSemaphoreCount     = 1;
    submitInfo.pSignalSemaphores        = &mEyeRenderSemaphore[mCurrentEyeBuffer];

    // Need a fence that we can pass in as part of svrSubmitFrame
    VkExportFenceCreateInfoKHR exportFenceInfo;
    memset(&exportFenceInfo, 0, sizeof(VkExportFenceCreateInfoKHR));
    exportFenceInfo.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO_KHR;
    exportFenceInfo.pNext = NULL;
    exportFenceInfo.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

    VkFenceCreateInfo fenceInfo;
    memset(&fenceInfo, 0, sizeof(VkFenceCreateInfo));
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;;
    fenceInfo.pNext = &exportFenceInfo;
    fenceInfo.flags = 0;    // If we want to start signaled, pass in VK_FENCE_CREATE_SIGNALED_BIT

    VkFence currentFence;
    err = vkCreateFence(mDevice, &fenceInfo, NULL, &currentFence);
    VK_CHECK(!err);

    // Last parameter is a fence
    err = vkQueueSubmit(mQueue, 1, &submitInfo, currentFence);
    VK_CHECK(!err);

    // Must grab the interop handle to the semaphore AFTER it is submitted.
    // To clarify, this is the valid usage of exporting a fd from a semaphore :
    // If  handleType  refers to a handle type with temporary import semantics, semaphore  must be signaled, 
    // or have an associated  semaphore signal operation  pending execution.
    //  It will not work on our implementation if this is not followed.

    // This handle is temporary.  It will be closed on the GL side
    int GlFenceHandle = 0;

    if (g_vkGetFenceFdKHR == NULL)
    {
        LOGE("****************************************");
        LOGE("Error! Do NOT have function pointer for vkGetFenceFdKHR()!");
        LOGE("****************************************");
    }
    else
    {
        // LOGE("****************************************");
        // LOGE("Calling vkGetFenceFdKHR() = 0x%x", (int)g_vkGetFenceFdKHR);
        // LOGE("****************************************");
        VkFenceGetFdInfoKHR getFdInfo = {};
        memset(&getFdInfo, 0, sizeof(VkFenceGetFdInfoKHR));
        getFdInfo.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
        getFdInfo.pNext = NULL;
        getFdInfo.fence = currentFence;
        getFdInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR;

        err = g_vkGetFenceFdKHR(mDevice, &getFdInfo, &GlFenceHandle);
        // LOGE("****************************************");
        // LOGE("Back from vkGetFenceFdKHR(): Error = %d", err);
        // LOGE("****************************************");
        VK_CHECK(!err);
        // LOGE("****************************************");
        // LOGE("vkGetFenceFdKHR(): Gl Handle = %d", GlFenceHandle);
        // LOGE("****************************************");
    }

    // Can now delete the fence since we have a GL version
    vkDestroyFence(mDevice, currentFence, NULL);


    // Submit the frame
    static int gFrameIndex = 0;

    svrFrameParams frameParams;
    memset(&frameParams, 0, sizeof(frameParams));
    frameParams.frameIndex = gFrameIndex++;

    frameParams.renderLayers[0].imageType = kTypeVulkan;
    frameParams.renderLayers[0].imageHandle = mEyeBuffers[mCurrentEyeBuffer][kLeftEye].OutputTexture.GetGlHandle();    // GetSampler();
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[0].imageCoords);
    frameParams.renderLayers[0].eyeMask = kEyeMaskLeft;
    frameParams.renderLayers[0].vulkanInfo.memSize = mEyeBuffers[mCurrentEyeBuffer][kLeftEye].OutputTexture.mMemorySize;
    frameParams.renderLayers[0].vulkanInfo.width = mEyeBuffers[mCurrentEyeBuffer][kLeftEye].OutputTexture.mWidth;
    frameParams.renderLayers[0].vulkanInfo.height = mEyeBuffers[mCurrentEyeBuffer][kLeftEye].OutputTexture.mHeight;
    frameParams.renderLayers[0].vulkanInfo.numMips = mEyeBuffers[mCurrentEyeBuffer][kLeftEye].OutputTexture.mMips;
    frameParams.renderLayers[0].vulkanInfo.bytesPerPixel = 4;  // GL_RGBA8
    frameParams.renderLayers[0].vulkanInfo.renderSemaphore = GlFenceHandle; //  GlSemaphoreHandle;

    frameParams.renderLayers[1].imageType = kTypeVulkan;
    frameParams.renderLayers[1].imageHandle = mEyeBuffers[mCurrentEyeBuffer][kRightEye].OutputTexture.GetGlHandle();   // GetSampler();
    L_CreateLayout(0.0f, 0.0f, 1.0f, 1.0f, &frameParams.renderLayers[1].imageCoords);
    frameParams.renderLayers[1].eyeMask = kEyeMaskRight;
    frameParams.renderLayers[1].vulkanInfo.memSize = mEyeBuffers[mCurrentEyeBuffer][kRightEye].OutputTexture.mMemorySize;
    frameParams.renderLayers[1].vulkanInfo.width = mEyeBuffers[mCurrentEyeBuffer][kRightEye].OutputTexture.mWidth;
    frameParams.renderLayers[1].vulkanInfo.height = mEyeBuffers[mCurrentEyeBuffer][kRightEye].OutputTexture.mHeight;
    frameParams.renderLayers[1].vulkanInfo.numMips = mEyeBuffers[mCurrentEyeBuffer][kRightEye].OutputTexture.mMips;
    frameParams.renderLayers[1].vulkanInfo.bytesPerPixel = 4;  // GL_RGBA8
    frameParams.renderLayers[1].vulkanInfo.renderSemaphore = GlFenceHandle; //  GlSemaphoreHandle;

    frameParams.headPoseState = gCurrentPoseState;
    frameParams.fieldOfView = gRenderFieldOfView;   // Already in Radians *DEG_TO_RAD;
    frameParams.minVsyncs = 1;

    // LOGI("SumbitFrame(%d): Left = %d; Right = %d", mCurrentEyeBuffer, frameParams.renderLayers[0].imageHandle, frameParams.renderLayers[1].imageHandle);

    svrSubmitFrame(&frameParams);

    // Notes:
    // VkResult vkQueueSubmit(
    //     VkQueue                                     queue,
    //     uint32_t                                    submitCount,
    //     const VkSubmitInfo*                         pSubmits,
    //     VkFence                                     fence);
    // VkResult vkGetFenceStatus(
    //     VkDevice                                    device,
    //     VkFence                                     fence);


    // Go to the next eye buffer
    mCurrentEyeBuffer = (mCurrentEyeBuffer + 1) % SVR_NUM_EYE_BUFFERS;

    return true;
}


///////////////////////////////////////////////////////////////////////////////

bool VkSample::Update()
{
    // We want an explicit sync for this sample
    vkDeviceWaitIdle(mDevice);

    unsigned int TimeNow = GetTimeMS();
    if (gLastUpdateTime == 0)
        gLastUpdateTime = TimeNow;

    float elapsedTime = (float)(TimeNow - gLastUpdateTime) / 1000.0f;
    gLastUpdateTime = TimeNow;

    // Get the current head pose from SVR
    float predictedTimeMs = svrGetPredictedDisplayTime();
    gCurrentPoseState = svrGetPredictedHeadPose(predictedTimeMs);

    if (gCurrentPoseState.poseStatus & kTrackingPosition)
    {
        //Apply a scale to adjust for the scale of the content
        gCurrentPoseState.pose.position.x *= 2.0f;
        gCurrentPoseState.pose.position.y *= 2.0f;
        gCurrentPoseState.pose.position.z *= 2.0f;

        // TODO: Keep the base camera position around?
        gCurrentPoseState.pose.position.x += mCameraPos.x;
        gCurrentPoseState.pose.position.y += mCameraPos.y;
        gCurrentPoseState.pose.position.z += mCameraPos.z;
    }
    else
    {
        gCurrentPoseState.pose.position.x = mCameraPos.x;
        gCurrentPoseState.pose.position.y = mCameraPos.y;
        gCurrentPoseState.pose.position.z = mCameraPos.z;
    }

    glm::vec3 SvrCameraPos = glm::vec3(gCurrentPoseState.pose.position.x, gCurrentPoseState.pose.position.y, gCurrentPoseState.pose.position.z);

    float currentIpd = DEFAULT_IPD;
    float headHeight = DEFAULT_HEAD_HEIGHT;
    float headDepth = DEFAULT_HEAD_DEPTH;
    SvrGetEyeViewMatrices(gCurrentPoseState, false, currentIpd, headHeight, headDepth, mSvrViewMatrix[0], mSvrViewMatrix[1]);

    static float spinAngle = 0.1f;

    glm::mat4 rotation = glm::rotate(glm::mat4(), glm::radians(spinAngle), glm::vec3(1.0f, 1.0f, 0.0f));
    glm::mat4 model = mModelMatrix * rotation ;

    VkResult ret = VK_SUCCESS;
    uint8_t *pData;

    spinAngle += 2.0f;

    // LOGI("TotalBobble: %0.4f += %0.4f", gTotalBobble, elapsedTime * gBobbleSpeed);
    gTotalBobble += elapsedTime * gBobbleSpeed;
    if (gTotalBobble > 2.0f * M_PI)
        gTotalBobble -= 2.0f * M_PI;

    float oneBobble = gBobbleAmount * cosf(gTotalBobble);
    // LOGI("   oneBobble (Total = %0.4f): %0.4f", gTotalBobble, oneBobble);

    for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
    {
        // TODO: Make this rotate base on elapsedTime
        // float rotateAmount = elapsedTime * (360.0f / 5.0f);   // Rotating at 5 sec/rotation
        // float rotateAmount = 0.1f;

        for (uint32_t whichObject = 0; whichObject < NUM_OBJECTS; whichObject++)
        {
            mEyeVertData[whichObject].proj = mProjectionMatrix;
            mEyeVertData[whichObject].view = mSvrViewMatrix[whichEye];    //  mViewMatrix;

            if (gObjectBobbles)
            {
                // What is the current position
                glm::vec3 onePosition = mBaseObjectPos[whichObject];
                switch (whichObject)
                {
                case 0:     // +X
                    onePosition = glm::vec3(gObjectRadius, 0.0f, 0.0f);
                    break;
                case 1:     // +Y
                    onePosition = glm::vec3(0.0f, gObjectRadius, 0.0f);
                    break;
                case 2:     // +Z
                    onePosition = glm::vec3(0.0f, 0.0f, gObjectRadius);
                    break;
                case 3:     // -X
                    onePosition = glm::vec3(-gObjectRadius, 0.0f, 0.0f);
                    break;
                case 4:     // -Y
                    onePosition = glm::vec3(0.0f, -gObjectRadius, 0.0f);
                    break;
                case 5:     // -Z
                    onePosition = glm::vec3(0.0f, 0.0f, -gObjectRadius);
                    break;
                }

                if (gBobbleHorizontal)
                    onePosition.x += oneBobble;
                if (gBobbleVertical)
                    onePosition.y += oneBobble;

                mBaseObjectPos[whichObject] = onePosition;
            }   // Object Bobbles

            if (gObjectRotates)
            {
                mEyeVertData[whichObject].model = glm::translate(glm::mat4(1.0f), mBaseObjectPos[whichObject]);
                mEyeVertData[whichObject].model = glm::rotate(mEyeVertData[whichObject].model, glm::radians(spinAngle), normalize(glm::vec3(1.0f, 0.0f, 0.0f)));
            }
            else
            {
                mEyeVertData[whichObject].model = glm::translate(glm::mat4(1.0f), mBaseObjectPos[whichObject]);
                switch (whichObject)
                {
                case 0:     // +X
                    mEyeVertData[whichObject].model = glm::rotate(mEyeVertData[whichObject].model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                    break;

                case 1:     // +Y
                    break;

                case 2:     // +Z
                    mEyeVertData[whichObject].model = glm::rotate(mEyeVertData[whichObject].model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    break;

                case 3:     // -X
                    mEyeVertData[whichObject].model = glm::rotate(mEyeVertData[whichObject].model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                    break;

                case 4:     // -Y
                    break;

                case 5:     // -Z
                    mEyeVertData[whichObject].model = glm::rotate(mEyeVertData[whichObject].model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                    break;
                }
            }   // Objects don't rotate

            ret = vkMapMemory(mDevice, mEyeBuffers[mCurrentEyeBuffer][whichEye].VertUniformBuffer[whichObject].GetDeviceMemory(), 0, mEyeBuffers[mCurrentEyeBuffer][whichEye].VertUniformBuffer[whichObject].GetAllocationSize(), 0, (void**)&pData);
            memcpy(pData, &mEyeVertData[whichObject], sizeof(mEyeVertData[whichObject]));
            vkUnmapMemory(mDevice, mEyeBuffers[mCurrentEyeBuffer][whichEye].VertUniformBuffer[whichObject].GetDeviceMemory());

            mEyeFragData[whichObject].color = mBaseObjectColor[whichObject];
            mEyeFragData[whichObject].ambient = glm::vec4(0.25f, 0.25f, 0.25f, 1.0f);
            mEyeFragData[whichObject].eyePos = glm::vec4(SvrCameraPos.x, SvrCameraPos.y, SvrCameraPos.z, 1.0f);
            mEyeFragData[whichObject].lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            mEyeFragData[whichObject].lightColor = glm::vec4(1.0f, 1.0f, 0.8f, 1.0f);

            ret = vkMapMemory(mDevice, mEyeBuffers[mCurrentEyeBuffer][whichEye].FragUniformBuffer[whichObject].GetDeviceMemory(), 0, mEyeBuffers[mCurrentEyeBuffer][whichEye].FragUniformBuffer[whichObject].GetAllocationSize(), 0, (void**)&pData);
            memcpy(pData, &mEyeFragData[whichObject], sizeof(mEyeFragData[whichObject]));
            vkUnmapMemory(mDevice, mEyeBuffers[mCurrentEyeBuffer][whichEye].FragUniformBuffer[whichObject].GetDeviceMemory());
        }   // Each Object
    }   // Each Eye

    return true;
}

///////////////////////////////////////////////////////////////////////////////

bool VkSample::DestroySample()
{
    // Destroy descriptor pool
    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

    // ********************************
    // Eye Buffers
    // ********************************
    vkDestroyPipelineLayout(mDevice, mEyePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mEyeDescriptorLayout, nullptr);

    // Clean up entire eye buffer structure
    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        for (uint32_t whichEye = 0; whichEye < SVR_NUM_EYES; whichEye++)
        {
            mEyeBuffers[whichBuffer][whichEye].RenderTarget.Destroy();
            mEyeBuffers[whichBuffer][whichEye].DepthTarget.Destroy();
            mEyeBuffers[whichBuffer][whichEye].OutputTexture.Destroy();

            for (uint32_t whichObject = 0; whichObject < NUM_OBJECTS; whichObject++)
            {
                mEyeBuffers[whichBuffer][whichEye].VertUniformBuffer[whichObject].Destroy();
                mEyeBuffers[whichBuffer][whichEye].FragUniformBuffer[whichObject].Destroy();
            }   // Each Object

            vkDestroyFramebuffer(mDevice, mEyeBuffers[whichBuffer][whichEye].FrameBuffer, nullptr);
            vkDestroyRenderPass(mDevice, mEyeBuffers[whichBuffer][whichEye].RenderPass, nullptr);
            vkDestroyPipeline(mDevice, mEyeBuffers[whichBuffer][whichEye].Pipeline, nullptr);

            // How do we destroy descriptor set? 

            // How do we destroy command buffers?

        }   // Each Eye
    }   // Each Buffer

    for (uint32_t whichBuffer = 0; whichBuffer < SVR_NUM_EYE_BUFFERS; whichBuffer++)
    {
        if(mEyeRenderSemaphore[whichBuffer] != 0)
            vkDestroySemaphore(mDevice, mEyeRenderSemaphore[whichBuffer], 0);
        mEyeRenderSemaphore[whichBuffer] = 0;
    }   // Each Buffer

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// Begin Android Glue entry point
#include <android_native_app_glue.h>

// Shared state for our app.
struct EngineData
{
    struct android_app* app;
    int animating;
    VkSampleFramework* sample;
};

// Process the next main command.
static void CommandCallback(struct android_app* app, int32_t cmd)
{
    struct EngineData* pEngine = (EngineData*)app->userData;
    switch (cmd)
    {
        // Command from main thread: the AInputQueue has changed.  Upon processing
        // this command, android_app->inputQueue will be updated to the new queue
        // (or NULL).
    case APP_CMD_INPUT_CHANGED:
        LOGI("APP_CMD_INPUT_CHANGED");
        break;

        // Command from main thread: a new ANativeWindow is ready for use.  Upon
        // receiving this command, android_app->window will contain the new window
        // surface.
    case APP_CMD_INIT_WINDOW:
        LOGI("APP_CMD_INIT_WINDOW (Window = 0x%x)", (unsigned int)app->window);

        gFirstInit = true;
        if (!pEngine->sample->Initialize(pEngine->app->window))
        {
            LOGE("VkSample::Initialize Error");
            pEngine->sample->TearDown();
        }
        else
        {
            LOGI("VkSample::Initialize Success");
        }
        pEngine->animating = 1;
        break;

        // Command from main thread: the existing ANativeWindow needs to be
        // terminated.  Upon receiving this command, android_app->window still
        // contains the existing window; after calling android_app_exec_cmd
        // it will be set to NULL.
    case APP_CMD_TERM_WINDOW:
        LOGI("APP_CMD_TERM_WINDOW");
        gContextCreated = false;
        gFirstInit = false;
        pEngine->animating = 0;
        pEngine->sample->TearDown();
        break;

        // Command from main thread: the current ANativeWindow has been resized.
        // Please redraw with its new size.
    case APP_CMD_WINDOW_RESIZED:
        LOGI("APP_CMD_WINDOW_RESIZED");
        break;

        // Command from main thread: the system needs that the current ANativeWindow
        // be redrawn.  You should redraw the window before handing this to
        // android_app_exec_cmd() in order to avoid transient drawing glitches.
    case APP_CMD_WINDOW_REDRAW_NEEDED:
        LOGI("APP_CMD_WINDOW_REDRAW_NEEDED");
        break;

        // Command from main thread: the content area of the window has changed,
        // such as from the soft input window being shown or hidden.  You can
        // find the new content rect in android_app::contentRect.
    case APP_CMD_CONTENT_RECT_CHANGED:
        LOGI("APP_CMD_CONTENT_RECT_CHANGED");
        break;

        // Command from main thread: the app's activity window has gained
        // input focus.
    case APP_CMD_GAINED_FOCUS:
        LOGI("APP_CMD_GAINED_FOCUS");
        break;

        // Command from main thread: the app's activity window has lost
        // input focus.
    case APP_CMD_LOST_FOCUS:
        LOGI("APP_CMD_LOST_FOCUS");
        break;

        // Command from main thread: the current device configuration has changed.
    case APP_CMD_CONFIG_CHANGED:
        LOGI("APP_CMD_CONFIG_CHANGED");
        gContextCreated = false;
        gRecreateContext = true;
        break;

        // Command from main thread: the system is running low on memory.
        // Try to reduce your memory use.
    case APP_CMD_LOW_MEMORY:
        LOGI("APP_CMD_LOW_MEMORY");
        break;

        // Command from main thread: the app's activity has been started.
    case APP_CMD_START:
        LOGI("APP_CMD_START");
        break;

        // Command from main thread: the app's activity has been resumed.
    case APP_CMD_RESUME:
        LOGI("APP_CMD_RESUME");
        if (gFirstInit && gIsPaused)
        {
            if (!pEngine->sample->Initialize(pEngine->app->window))
            {
                LOGE("VkSample::Initialize Error");
                pEngine->sample->TearDown();
            }
            else
            {
                LOGI("VkSample::Initialize Success");
            }
            pEngine->animating = 1;
        }

        gIsPaused = false;
        LOGI("gIsPaused = %d", gIsPaused);
        break;

        // Command from main thread: the app should generate a new saved state
        // for itself, to restore from later if needed.  If you have saved state,
        // allocate it with malloc and place it in android_app.savedState with
        // the size in android_app.savedStateSize.  The will be freed for you
        // later.
    case APP_CMD_SAVE_STATE:
        LOGI("APP_CMD_SAVE_STATE");

        // Teardown, and recreate each time
        pEngine->animating = 0;
        pEngine->sample->TearDown();
        break;

        // Command from main thread: the app's activity has been paused.
    case APP_CMD_PAUSE:
        LOGI("APP_CMD_PAUSE");
        PauseVR();
        break;

        // Command from main thread: the app's activity has been stopped.
    case APP_CMD_STOP:
        LOGI("APP_CMD_STOP");
        PauseVR();
        break;

        // Command from main thread: the app's activity is being destroyed,
        // and waiting for the app thread to clean up and exit before proceeding.
    case APP_CMD_DESTROY:
        LOGI("APP_CMD_DESTROY");
        break;

    default:
        LOGI("Unknown APP_CMD_: %d", cmd);
        break;
    }
}

int InputCallback(android_app* pApp, AInputEvent* event)
{
    struct EngineData* pEngine = (EngineData*)pApp->userData;

    // Want touch events in [0,1] for width [Left, Right] and height [Bottom, Top]
    // Actually, we want screen size here, NOT surface size,
    // Touch events come in as screen size so where you are touching gets messed up.
    float fltWidth = (float)ANativeWindow_getWidth(pApp->window);
    float fltHeight = (float)ANativeWindow_getHeight(pApp->window);

    // A key is pressed
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
    {
        // See <AndroidNDK>\platforms\android-9\arch-arm\usr\include\android\input.h
        int iKeyCode = AKeyEvent_getKeyCode(event);
        int iKeyAction = AInputEvent_getType(event);

        switch (iKeyCode)
        {
        case AKEYCODE_HOME:     //  = 3,
            LOGI("KeyCode: AKEYCODE_HOME");
            break;
        case AKEYCODE_BACK:     //  = 4,
            LOGI("KeyCode: AKEYCODE_BACK");
            break;
        case AKEYCODE_MENU:     //  = 82,
            LOGI("KeyCode: AKEYCODE_MENU");
            break;
        default:
            LOGI("KeyCode: %d", iKeyCode);
            break;
        }

        switch (iKeyAction)
        {
        case AKEY_EVENT_ACTION_DOWN:
            //pFrmApp->GetInput().KeyDownEvent(iKeyCode);
            break;
        case AKEY_EVENT_ACTION_UP:
            //pFrmApp->GetInput().KeyUpEvent(iKeyCode);
            break;
        case AKEY_EVENT_ACTION_MULTIPLE:
            break;
        }

        return 0;
    }

    // Touch the screen
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        // See <AndroidNDK>\platforms\android-9\arch-arm\usr\include\android\input.h
        float fltOneX = 0.0f;
        float fltOneY = 0.0f;

        int iPointerAction = AMotionEvent_getAction(event);
        int iPointerIndx = (iPointerAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int iPointerID = AMotionEvent_getPointerId(event, iPointerIndx);
        int iAction = (iPointerAction & AMOTION_EVENT_ACTION_MASK);
        switch (iAction)
        {
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        case AMOTION_EVENT_ACTION_DOWN:
            fltOneX = AMotionEvent_getX(event, iPointerIndx);
            fltOneY = AMotionEvent_getY(event, iPointerIndx);
            pEngine->sample->PointerDownEvent(iPointerID, fltOneX / fltWidth, fltOneY / fltHeight);
            break;

        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_UP:
            fltOneX = AMotionEvent_getX(event, iPointerIndx);
            fltOneY = AMotionEvent_getY(event, iPointerIndx);
            pEngine->sample->PointerUpEvent(iPointerID, fltOneX / fltWidth, fltOneY / fltHeight);
            break;

        case AMOTION_EVENT_ACTION_MOVE:
        {
            int iHistorySize = AMotionEvent_getHistorySize(event);
            int iPointerCount = AMotionEvent_getPointerCount(event);
            for (int iHistoryIndx = 0; iHistoryIndx < iHistorySize; iHistoryIndx++)
            {
                for (int iPointerIndx = 0; iPointerIndx < iPointerCount; iPointerIndx++)
                {
                    iPointerID = AMotionEvent_getPointerId(event, iPointerIndx);
                    fltOneX = AMotionEvent_getHistoricalX(event, iPointerIndx, iHistoryIndx);
                    fltOneY = AMotionEvent_getHistoricalY(event, iPointerIndx, iHistoryIndx);
                }
            }

            for (int iPointerIndx = 0; iPointerIndx < iPointerCount; iPointerIndx++)
            {
                iPointerID = AMotionEvent_getPointerId(event, iPointerIndx);
                fltOneX = AMotionEvent_getX(event, iPointerIndx);
                fltOneY = AMotionEvent_getY(event, iPointerIndx);
                pEngine->sample->PointerMoveEvent(iPointerID, fltOneX / fltWidth, fltOneY / fltHeight);
            }

        }
            break;

        case AMOTION_EVENT_ACTION_CANCEL:
            LOGI("AMOTION_EVENT_ACTION_CANCEL: 0x%x", iPointerID);
            break;

        case AMOTION_EVENT_ACTION_OUTSIDE:
            LOGI("AMOTION_EVENT_ACTION_OUTSIDE: 0x%x", iPointerID);
            break;
        }

        return 1;
    }

    return 0;
}

// This is the main entry point of a native application that is using
// android_native_app_glue.  It runs in its own thread, with its own
// event loop for receiving input events and doing other things.
void android_main(struct android_app* pAppState)
{
    EngineData engine;
    memset(&engine, 0, sizeof(engine));

    // Make sure glue isn't stripped.
    // No longer need this in later NDKs
    //app_dummy();

    engine.sample = new VkSample();
    assert(engine.sample);


    svrInitParams initParams;
    initParams.javaVm = pAppState->activity->vm;
    (*pAppState->activity->vm).AttachCurrentThread(&initParams.javaEnv, NULL);
    initParams.javaActivityObject = pAppState->activity->clazz;

    LOGI("Initializing SVR...");
    if (svrInitialize(&initParams) != SVR_ERROR_NONE)
    {
        // Can't exit here since it yanks the rug out from the dialog box telling user SVR not supported :)
        //LOGE("android_main: svrInitialize failed, appication exiting");
        //return;
    }


    pAppState->userData = &engine;

    pAppState->onAppCmd = CommandCallback;
    pAppState->onInputEvent = InputCallback;

    engine.app = pAppState;
    // Give the assetManager instance to the sample, as to allow it
    // to load assets such as shaders and images from our project.
    engine.sample->SetAssetManager(pAppState->activity->assetManager);

    // loop waiting for stuff to do.
    while (1)
    {
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(engine.animating ? 0 : -1, nullptr, &events, (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != nullptr)
            {
                source->process(pAppState, source);
            }

            // Check if we are exiting.
            if (pAppState->destroyRequested != 0)
            {
                engine.sample->TearDown();
                // delete engine.sample;
                return;
            }
        }   // Looper poll

        if ((gIsPaused == false) && (gSvrStarted == false))
        {
            LOGI("gIsPaused = %d; gSvrStarted = %d", gIsPaused, gSvrStarted);
            StartVR(pAppState);
        }

        if (engine.animating && engine.sample->IsInitialized())
        {
            engine.sample->DrawFrame();
        }
    }
}
//END Android Glue
///////////////////////////////////////////////////////////////////////////////