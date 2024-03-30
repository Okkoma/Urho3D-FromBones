//
// Copyright (c) 2008-2022 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#ifdef URHO3D_VOLK
#define VK_NO_PROTOTYPES
#include <volk/volk.h>
#else
#include <vulkan/vulkan.h>
#endif

#ifdef URHO3D_VMA
#ifdef URHO3D_VOLK
    #define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#include "vma/vk_mem_alloc.h"
#endif

#include "../../Graphics/ConstantBuffer.h"
#include "../../Graphics/GraphicsDefs.h"
#include "../../Graphics/ShaderProgram.h"
#include "../../Graphics/RenderPath.h"
#include "../../Graphics/Texture2D.h"

#define NUMDESCRIPTORSETS 25

//#define ACTIVE_FRAMELOGDEBUG

namespace Urho3D
{

class Context;

typedef HashMap<Pair<ShaderVariation*, ShaderVariation*>, SharedPtr<ShaderProgram> > ShaderProgramMap;
typedef HashMap<unsigned, SharedPtr<ConstantBuffer> > ConstantBufferMap;


struct PhysicalDeviceInfo
{
    template <typename T> T* GetExtensionFeatures() const;
    template <typename T> T& GetOrCreateExtensionFeatures(VkStructureType featuretype);

    template <typename T> T* GetExtensionProperties() const;
    template <typename T> T& GetOrCreateExtensionProperties(VkStructureType propertytype);

    void CleanUp();

#ifndef URHO3D_VMA
    bool GetMemoryTypeIndex(uint32_t filter, VkMemoryPropertyFlags properties, uint32_t& memorytype) const;
#endif

    Collection extensionFeatures_;
    Collection extensionProperties_;
    VkPhysicalDevice device_;
    String name_;

    unsigned grQueueIndex_;
    unsigned prQueueIndex_;
    PODVector<unsigned> queueIndexes_;

    VkSurfaceCapabilitiesKHR surfaceCapabilities_;
    Vector<VkSurfaceFormatKHR> surfaceFormats_;
    Vector<VkPresentModeKHR> presentModes_;

    VkPhysicalDeviceFeatures features_;
    VkPhysicalDeviceProperties properties_;
#ifndef URHO3D_VMA
    VkPhysicalDeviceMemoryProperties memoryProperties_;
#endif
	VkPhysicalDeviceFeatures requireFeatures_;
};

struct PipelineInfo;

struct FrameData
{
    unsigned id_;

    /// Frame Outputs datas
    VkImage image_;
    VkImageView imageView_;

    /// SwapChain Synchronization Objects
//    VkSemaphore acquireSync_;
//    VkSemaphore releaseSync_;
    VkFence     submitSync_;

    VkCommandPool commandPool_;
    VkCommandBuffer commandBuffer_;

    bool textureDirty_;
    bool commandBufferBegun_;
    bool renderPassBegun_;
    VkPipeline lastPipelineBound_;

    PipelineInfo* lastPipelineInfoBound_;
    int renderPassIndex_;
    unsigned viewportIndex_;
    unsigned subpassIndex_;
};

enum
{
    PASS_CLEAR = 0,
    PASS_RENDER,
    PASS_PRESENT
};

struct RenderPassInfo
{
    RenderPassInfo() : type_(-1), key_(0), renderPass_(0), numColorAttachments_(0), numDepthAttachments_(0) { }

    int type_;
    unsigned key_;
    unsigned renderPathCommandIndex_;
    unsigned numColorAttachments_;
    unsigned numDepthAttachments_;

    VkRenderPass renderPass_;

    Vector<int> attachments_;
    Vector<VkClearValue> clearColors_;

    // Frame Buffers for each frame of the swapchain
    Vector<VkFramebuffer> framebuffers_;
};

enum
{
    RENDERSLOT_PRESENT = 0,
    RENDERSLOT_TARGET1,
    RENDERSLOT_TARGET2,
    RENDERSLOT_TARGET3,
    RENDERSLOT_TARGET4,
    RENDERSLOT_TARGET5,
    RENDERSLOT_TARGET6,
    RENDERSLOT_TARGET7,
    RENDERSLOT_TARGET8,
    RENDERSLOT_TARGET9,
    RENDERSLOT_TARGET10,
    RENDERSLOT_DEPTH,

    MAX_RENDERSLOTS
};

struct RenderAttachment
{
    int slot_, frame_;

    VkImage image_;
    VkImageView imageView_;
    VkSampler sampler_;

#ifndef URHO3D_VMA
    VkDeviceMemory memory_;
#else
    VmaAllocation memory_;
#endif

    SharedPtr<Texture2D> texture_;
};

struct RenderPathInfo
{
    SharedPtr<RenderPath> renderPath_;
    // Attachements by Slot and By FrameIndex
    Vector<RenderAttachment > renderAttachments_;
    Vector<RenderPassInfo* > passInfos_;
    HashMap<unsigned, unsigned > renderPathCommandIndexToRenderPassIndex_;
};

enum PipelineState
{
    PIPELINESTATE_BLENDMODE = 0,
    PIPELINESTATE_PRIMITIVE,
    PIPELINESTATE_COLORMASK,
    PIPELINESTATE_FILLMODE,
    PIPELINESTATE_CULLMODE,
    PIPELINESTATE_DEPTHTEST,
    PIPELINESTATE_DEPTHWRITE,
    PIPELINESTATE_STENCILTEST,
    PIPELINESTATE_STENCILMODE,
    PIPELINESTATE_SAMPLES,
    PIPELINESTATE_LINEWIDTH,
    PIPELINESTATE_MAX
};

struct DescriptorsGroupAllocation
{
    // Vulkan Descriptor Pool
    VkDescriptorPool pool_;

    // container of the Allocated DescriptorSets by the pool
    Vector<VkDescriptorSet> sets_;

    // current index in the container : points to the current DescriptorSet in use by the pipeline
    unsigned index_;
};

struct DescriptorsGroup
{
    // Set Id
    unsigned id_;

    // Describe the bindings of a DescriptorSet
    Vector<ShaderBind> bindings_;
    // Vulkan DescriptorSet Layout
    VkDescriptorSetLayout layout_;

    // DescriptorsGroupAllocation for each Frame
    Vector<DescriptorsGroupAllocation> setsByFrame_;
};

void ExtractStencilMode(int value, CompareMode& mode, StencilOp& pass, StencilOp& fail, StencilOp& zFail);
int StencilMode(CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail);


struct PipelineInfo
{
    PipelineInfo() :
        key_(StringHash::ZERO),
        renderPassKey_(0),
        pipelineStates_(0),
        stencilValue_(0),
        pipelineLayout_((VkPipelineLayout)VK_NULL_HANDLE),
        pipeline_((VkPipeline)VK_NULL_HANDLE),
        maxAllocatedDescriptorSets_(NUMDESCRIPTORSETS)
    { }

    PipelineInfo(const PipelineInfo& data) :
        key_(data.key_),
        renderPassKey_(data.renderPassKey_),
        pipelineStates_(data.pipelineStates_),
        stencilValue_(data.stencilValue_),
        vs_(data.vs_),
        ps_(data.ps_),
        vertexElementsTable_(data.vertexElementsTable_),
        pipelineLayout_(data.pipelineLayout_),
        pipeline_(data.pipeline_),
        maxAllocatedDescriptorSets_(data.maxAllocatedDescriptorSets_)
    { }

    StringHash key_;
    unsigned renderPassKey_;
    unsigned pipelineStates_;
    unsigned stencilValue_;
    SharedPtr<ShaderVariation> vs_;
    SharedPtr<ShaderVariation> ps_;
    Vector<PODVector<VertexElement> > vertexElementsTable_;

    VkPipelineLayout pipelineLayout_;
    VkPipeline pipeline_;
    unsigned maxAllocatedDescriptorSets_;
    Vector<DescriptorsGroup> descriptorsGroups_;
};

class PipelineBuilder
{
public:
    PipelineBuilder(GraphicsImpl* impl);

    void Reset();
    void CleanUp(bool shadermodules=true, bool vertexinfo=true, bool dynamicstates=true, bool colorblending=true);

    void AddShaderStage(ShaderVariation* variation, const String& entry = "main");
    void AddVertexBinding(unsigned binding=0, bool instance=false);
    void AddVertexElement(unsigned binding, const VertexElement& element);
    void AddVertexElements(unsigned binding, const PODVector<VertexElement>& elements);
    void AddVertexElements(const Vector<PODVector<VertexElement> >& elementsTable, const bool* instanceTable=0);
    void SetTopology(unsigned primitive=TRIANGLE_LIST, bool primitiveRestartEnable=false, unsigned flags=0);
    void SetViewportStates();
    void SetRasterization(unsigned fillMode, CullMode cullMode=CULL_CW, int linewidth=0);
    void SetDepthStencil(bool enable, int compare, bool write, bool stencil, int stencilmode, unsigned stencilvalue=0);
    void AddDynamicState(VkDynamicState state);
    void SetMultiSampleState(int samples=1);
    void SetColorBlend(bool enable=VK_FALSE, VkLogicOp logicOp=VK_LOGIC_OP_COPY, float b0=0.f, float b1=0.f, float b2=0.f, float b3=0.f);
    void AddColorBlendAttachment(int attachmentIndex, BlendMode blendMode=BLEND_ALPHA, unsigned colormask=0xF);

    void CreatePipeline(PipelineInfo* info);

    static const unsigned VULKAN_MAX_SHADER_STAGES = 2;
    static const unsigned VULKAN_MAX_VERTEX_BINDINGS = 4;
    static const unsigned VULKAN_MAX_VERTEX_ATTRIBUTES = 16;
    static const unsigned VULKAN_MAX_DYNAMIC_STATES = 8;
    static const unsigned VULKAN_MAX_COLOR_ATTACHMENTS = 4;

private:
    bool CreateDescriptors(PipelineInfo* info);

    unsigned numShaderStages_;
    unsigned numVertexBindings_;
    unsigned numVertexAttributes_;
    unsigned numDynamicStates_;
    unsigned numColorAttachments_;

    Vector<VkShaderModule > shaderModules_;
    Vector<PODVector<VertexElement> > vertexElementsTable_;
    VkVertexInputBindingDescription vertexBindings_[VULKAN_MAX_VERTEX_BINDINGS];
    VkVertexInputAttributeDescription vertexAttributes_[VULKAN_MAX_VERTEX_ATTRIBUTES];
    VkDynamicState dynamicStates_[VULKAN_MAX_DYNAMIC_STATES];
    VkPipelineColorBlendAttachmentState colorBlendAttachments_[VULKAN_MAX_COLOR_ATTACHMENTS];
    VkPipelineShaderStageCreateInfo shaderStages_[VULKAN_MAX_SHADER_STAGES];
    VkPipelineVertexInputStateCreateInfo vertexInputState_;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_;
    VkPipelineViewportStateCreateInfo viewportState_;
    VkPipelineRasterizationStateCreateInfo rasterizationState_;
    VkPipelineDepthStencilStateCreateInfo depthStencilState_;
    VkPipelineDynamicStateCreateInfo dynamicState_;
    VkPipelineMultisampleStateCreateInfo multiSampleState_;
    VkPipelineColorBlendStateCreateInfo colorBlendState_;

    GraphicsImpl* impl_;
    VkViewport viewport_;
    VkRect2D scissor_;
    bool viewportSetted_;
    const VkAllocationCallbacks* pAllocator_;
};

/// %Graphics subsystem implementation. Holds API-specific objects.

class URHO3D_API GraphicsImpl
{
    friend class Graphics;
    friend class PipelineBuilder;

public:
    GraphicsImpl();

    static const unsigned DefaultRenderPassWithTarget;
    static const unsigned DefaultRenderPassNoClear;

    static const unsigned ClearPass_1C_1DS;
    static const unsigned ClearPass_2C_1DS;
    static const unsigned RenderPass_1C_1DS;
    static const unsigned RenderPass_2C_1DS;
    static const unsigned PresentPass_1C;

    /// Setters
	// Configuration
	void AddInstanceExtension(const char* extension);
	void AddDeviceExtension(const char* gpuextension);
	void SetDefaultDevice(const String& device);

	// RenderPaths
    void AddRenderPassInfo(const String& attachmentconfig);
    void SetRenderPath(RenderPath* renderPath);
    void SetRenderPass(unsigned passindex);

	/// Pipelines
    PipelineInfo* RegisterPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, VertexBuffer** buffers);
    PipelineInfo* RegisterPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, unsigned numVertexTables, const PODVector<VertexElement>* vertexTables);

    void ResetToDefaultPipelineStates();
    void SetPipelineState(unsigned& pipelineStates, PipelineState state, unsigned value);
    bool SetPipeline(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned pipelineStates, VertexBuffer** vertexBuffers);
    void SetViewport(const IntRect& rect, unsigned index);
    /// Getters
    static PipelineInfo* GetPipelineInfo() { return pipelineInfo_; }
    static unsigned GetUBOPaddedSize(unsigned size);
    static VkFormat GetSwapChainFormat() { return swapChainInfo_.format; }
    static VkFormat GetDepthStencilFormat() { return depthStencilFormat_; }
    static int GetLineWidthIndex(float width);

    VkInstance GetInstance() const { return instance_; }

    VkDevice GetDevice() const { return device_; }
    static const PhysicalDeviceInfo& GetPhysicalDeviceInfo() { return physicalInfo_; }
    VkCommandPool GetCommandPool() const { return commandPool_; }
#ifdef URHO3D_VMA
    VmaAllocator GetAllocator() const { return allocator_; }
#endif

    FrameData& GetFrame() { return *frame_; }

    VkQueue GetGraphicsQueue() const { return graphicQueue_; }
    unsigned GetFrameIndex() const { return currentFrame_; }
    const VkExtent2D& GetSwapExtent() const { return swapChainExtent_; }

    const VkViewport& GetViewport() const { return viewport_; }
    const VkRect2D& GetScissor() const { return screenScissor_; }
    const VkRect2D& GetFrameScissor() const { return frameScissor_; }

    Texture2D* GetCurrentViewportTexture() const;

    const RenderPassInfo* GetRenderPassInfo(unsigned renderPassKey) const;

    unsigned GetPipelineState(unsigned pipelineStates, PipelineState state) const;
    unsigned GetDefaultPipelineStates() const;
    unsigned GetDefaultPipelineStates(PipelineState stateToModify, unsigned value);
    unsigned GetPipelineStateVariation(unsigned entrypipelineStates, PipelineState state, unsigned value);

    PipelineInfo* GetPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, unsigned stencilvalue=0) const;
    PipelineInfo* GetPipelineInfo(const StringHash& key) const;
    VkPipeline GetPipeline(const StringHash& key) const;

    int GetMaxCompatibleDescriptorSets(PipelineInfo* p1, PipelineInfo* p2) const;

    /// Dump
    String DumpPipelineStates(unsigned pipelineStates) const;
    void DumpRegisteredPipelineInfo() const;

private:
    bool CreateVulkanInstance(Context* context, const String& appname, SDL_Window* window, const Vector<String>& requestedLayers);
    bool CreateWindowSurface(SDL_Window* window);

    void CleanUpVulkan();
    void CleanUpRenderPasses();
    void CleanUpPipelines();
    void CleanUpSwapChain();

    bool CreateSwapChain(int width=0, int height=0, bool* srgb=0, bool* vsync=0, bool* triplebuffer=0);
    void UpdateSwapChain(int width=0, int height=0, bool* srgb=0, bool* vsync=0, bool* triplebuffer=0);

    void CreateImageAttachment(int slot, int frame, RenderAttachment& attachment, unsigned width, unsigned height);
    void DestroyAttachment(RenderAttachment& attachment);

    bool CreateRenderPasses(RenderPathInfo& renderPathInfo);
    bool CreateRenderPathAttachments(RenderPathInfo& renderPathInfo);
    bool CreateRenderPaths();

    void CreatePipelines();
    VkPipeline CreatePipeline(PipelineInfo* info);

    bool AcquireFrame();
    bool PresentFrame();

    Context* context_;
    SDL_Window* window_;
    Graphics* graphics_;

	String requireDevice_;
	PODVector<const char*> requireInstanceExts_;
	PODVector<const char*> requireDeviceExts_;
    bool validationLayersEnabled_;

#ifdef URHO3D_VMA
    VmaAllocator allocator_;
#endif
    /// Instance Objects
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debugMsg_;
    VkSurfaceKHR surface_, oldSurface_;

    unsigned vulkanApiVersion_;
    static PhysicalDeviceInfo physicalInfo_;
    static VkSurfaceFormatKHR swapChainInfo_;
    static VkFormat depthStencilFormat_;
    static PipelineInfo* pipelineInfo_;

    /// Devices & Queues Objects
    VkDevice device_;
    VkQueue graphicQueue_;
    VkQueue presentQueue_;
    VkCommandPool commandPool_;

    /// Objects States
    bool surfaceDirty_;
    bool swapChainDirty_;
    bool vertexElementsDirty_;
    bool vertexBuffersDirty_;
    bool indexBufferDirty_;
    bool pipelineDirty_;
    bool viewportDirty_;
    bool scissorDirty_;

    /// Vertex Buffers
    PODVector<VkBuffer> vertexBuffers_;
    PODVector<VkDeviceSize> vertexOffsets_;

    /// Presentation
    Vector<FrameData> frames_;
    FrameData* frame_;
    unsigned numFrames_;
    unsigned currentFrame_;
    VkPresentModeKHR presentMode_;
    VkExtent2D swapChainExtent_;
    VkSwapchainKHR swapChain_;

    /// Render Passes
    HashMap<unsigned, RenderPathInfo > renderPathInfos_;
    HashMap<unsigned, RenderPassInfo > renderPassInfos_;
    RenderPathInfo* renderPathInfo_;
    int renderPassIndex_;
    Texture2D* viewportTexture_;
    unsigned viewportIndex_;
    bool viewportChanged_;

    /// Pipelines
    PipelineBuilder pipelineBuilder_;
    IntRect viewportRects_[MAX_SHADER_VIEWPORTS];
    VkViewport viewport_;
    VkRect2D screenScissor_, frameScissor_;
    VkPipelineCache pipelineCache_;
    unsigned pipelineStates_;
    unsigned defaultPipelineStates_;
    unsigned stencilValue_;
    HashMap<StringHash, PipelineInfo > pipelinesInfos_;
    // indexed by vs,ps,states,stencilvalue
    HashMap<unsigned, HashMap<StringHash, HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > > > > pipelineInfoTable_;
    /// Test : New pipeline storage and hash
//    Vector<PipelineInfo> pipelinesInfos_;
//    HashMap<StringHash, Vector<PipelineInfo* > > vsPipelineInfos_;
//    HashMap<StringHash, Vector<PipelineInfo* > > psPipelineInfos_;

    /// Semaphore Pools
    //Vector<VkSemaphore> semaphorePool_;
    VkSemaphore presentComplete_;
    VkSemaphore renderComplete_;

    /// Constant Buffers
    ConstantBuffer* constantBuffers_[2][MAX_SHADER_PARAMETER_GROUPS];
    /// Constant buffer search map.
    ConstantBufferMap allConstantBuffers_;
    /// Currently dirty constant buffers.
    PODVector<ConstantBuffer*> dirtyConstantBuffers_;
    /// Shader programs.
    ShaderProgramMap shaderPrograms_;
    /// Shader program in use.
    ShaderProgram* shaderProgram_;
};

}
