#pragma once_

#include "VkSampleframework.h"
#include <stdlib.h>

#define GLM_FORCE_CXX03
#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshObject.h"

#include "svrApi.h"
#include "svrUtil.h"

// Need a number of buffers to cycle through for submission
#define SVR_NUM_EYE_BUFFERS     2   // Currently mSwapchainImageCount is 2 on device.  If this is larger
                                    // than swap chain count there is judder on main screen because rendering
                                    // to buffer that is not associated with swap image.
#define SVR_NUM_EYES            2

#define NUM_OBJECTS             6

struct EyeVertData
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 model;
};

struct EyeFragData
{
    glm::vec4 color;
    glm::vec4 ambient;
    glm::vec4 eyePos;
    glm::vec4 lightPos;
    glm::vec4 lightColor;
};

struct EyeBufferData
{
    ImageViewObject     RenderTarget;
    ImageViewObject     DepthTarget;
    TextureObject       OutputTexture;

    BufferObject        VertUniformBuffer[NUM_OBJECTS];
    BufferObject        FragUniformBuffer[NUM_OBJECTS];

    VkFramebuffer       FrameBuffer;
    VkRenderPass        RenderPass;
    VkPipeline          Pipeline;
    VkDescriptorSet     DescriptorSet[NUM_OBJECTS];
    VkCommandBuffer     CommandBuffer;

};

class VkSample: public VkSampleFramework
{
public:
    VkSample();
    virtual ~VkSample();

public:
    void	PointerDownEvent(int pointerId, float x, float y);
    void	PointerUpEvent(int pointerId, float x, float y);
    void	PointerMoveEvent(int pointerId, float x, float y);

protected:

    bool InitSample();
    bool Update();
    bool Draw();
    bool DestroySample();
    void WindowResize(uint32_t newWidth, uint32_t newHeight);

    void InitDescriptorPool();
    void InitEyeRenderPass();

    void InitEyeLayouts();
    void InitEyeDescriptorSet();
    void InitEyeBuffers();
    void InitEyePipelines();
    void InitEyeCommandBuffers();
    void InitEyeSemaphors();

    void InitVertexBuffers();
    void InitUniformBuffers();

    VkDescriptorPool    mDescriptorPool;

    // Only need one mesh
    MeshObject      mMesh;

    glm::mat4       mProjectionMatrix;
    glm::mat4       mViewMatrix;
    glm::mat4       mModelMatrix;
    glm::vec3       mCameraPos;

    glm::mat4       mSvrViewMatrix[kNumEyes];

    // Set up for each object
    EyeVertData             mEyeVertData[NUM_OBJECTS];
    EyeFragData             mEyeFragData[NUM_OBJECTS];

    glm::vec3               mBaseObjectPos[NUM_OBJECTS];
    glm::vec4               mBaseObjectColor[NUM_OBJECTS];

    // ********************************
    // Eye Buffers
    // ********************************
    // Keep track of current eye buffer being rendered
    int                     mCurrentEyeBuffer;

    // Data for each eye buffer
    uint32_t                mEyeBufferWidth;
    uint32_t                mEyeBufferHeight;

    VkDescriptorSetLayout   mEyeDescriptorLayout;
    VkPipelineLayout        mEyePipelineLayout;

    EyeBufferData           mEyeBuffers[SVR_NUM_EYE_BUFFERS][SVR_NUM_EYES];

    VkSemaphore             mEyeRenderSemaphore[SVR_NUM_EYE_BUFFERS];

};

