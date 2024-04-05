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

#include "../../Precompiled.h"

#include "../../Core/Context.h"

#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/VertexBuffer.h"
#include "../../Graphics/IndexBuffer.h"
#include "../../Graphics/Technique.h"

#include "../../IO/Log.h"

#include "../../IO/File.h"

#include "../../DebugNew.h"

#include <SDL/SDL.h>
#include <SDL/SDL_vulkan.h>

#ifdef URHO3D_VMA
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#endif

namespace Urho3D
{

const uint64_t TIME_OUT = 1000;
//const uint64_t TIME_OUT = UINT64_MAX;

const unsigned MaxFrames = 3;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if (GraphicsImpl::GetPipelineInfo())
    {
        const PipelineInfo& info = *GraphicsImpl::GetPipelineInfo();

        String s;
        s.AppendWithFormat("key=%u states=%u stencilValue=%u %s vs=%s ps=%s \n", info.key_.Value(), info.pipelineStates_, info.stencilValue_,
                           info.vs_ ? info.vs_->GetName().CString() : "null", info.vs_ ? info.vs_->GetDefines().CString() : "null",
                           info.ps_ ? info.ps_->GetDefines().CString() : "null");

        URHO3D_LOGERRORF("Vulkan Validation : pipelineInfo %s %s", s.CString(), pCallbackData->pMessage);
    }
    else
    {
        URHO3D_LOGERRORF("Vulkan Validation : %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

const VkFormat ELEMENT_TYPE_VKFORMAT[] =
{
    VK_FORMAT_R32_SINT,             // TYPE_INT
    VK_FORMAT_R32_SFLOAT,           // TYPE_FLOAT
    VK_FORMAT_R32G32_SFLOAT,        // TYPE_VECTOR2
    VK_FORMAT_R32G32B32_SFLOAT,     // TYPE_VECTOR3
    VK_FORMAT_R32G32B32A32_SFLOAT,  // TYPE_VECTOR4
    VK_FORMAT_R8G8B8A8_UINT,        // TYPE_UBYTE4
    VK_FORMAT_R8G8B8A8_UNORM        // TYPE_UBYTE4_NORM
};

const char* ELEMENT_TYPE_STR[] =
{
    "TYPE_INT",
    "TYPE_FLOAT",
    "TYPE_VECTOR2",
    "TYPE_VECTOR3",
    "TYPE_VECTOR4",
    "TYPE_UBYTE4",
    "TYPE_UBYTE4_NORM",
};

static const VkPrimitiveTopology VulkanPrimitiveTopologies[] =
{
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                 // 3  // TRIANGLE_LIST = 0,
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,                     // 1  // LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,                    // 0  // POINT_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,                // 4  // TRIANGLE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,                    // 2  // LINE_STRIP,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,                  // 5  // TRIANGLE_FAN,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                 // 5  // QUAD_LIST
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,      // 6
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,     // 7
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,  // 8
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY, // 9
    VK_PRIMITIVE_TOPOLOGY_PATCH_LIST                     // 10
};

static const VkPolygonMode VulkanPolygonMode[] =
{
    VK_POLYGON_MODE_FILL,               // FILL_SOLID = 0,
    VK_POLYGON_MODE_LINE,               // FILL_WIREFRAME,
    VK_POLYGON_MODE_POINT,              // FILL_POINT
    VK_POLYGON_MODE_FILL_RECTANGLE_NV
};

static const VkCompareOp VulkanCompareMode[] =
{
    VK_COMPARE_OP_ALWAYS,           // 7 // CMP_ALWAYS = 0,
    VK_COMPARE_OP_EQUAL,            // 2 // CMP_EQUAL,
    VK_COMPARE_OP_NOT_EQUAL,        // 5 // CMP_NOTEQUAL,
    VK_COMPARE_OP_LESS,             // 1 // CMP_LESS,
    VK_COMPARE_OP_LESS_OR_EQUAL,    // 3 // CMP_LESSEQUAL,
    VK_COMPARE_OP_GREATER,          // 4 // CMP_GREATER,
    VK_COMPARE_OP_GREATER_OR_EQUAL, // 6 // CMP_GREATEREQUAL
};

static const VkStencilOp VulkanStencilOp[] =
{
    VK_STENCIL_OP_KEEP, // OP_KEEP = 0,
    VK_STENCIL_OP_ZERO, // OP_ZERO,
    VK_STENCIL_OP_REPLACE, // OP_REF,
    VK_STENCIL_OP_INCREMENT_AND_CLAMP, // OP_INCR,
    VK_STENCIL_OP_DECREMENT_AND_CLAMP, // OP_DECR
};

//const unsigned VulkanStencilModeValues[][4] =
//{
//    { VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP },
//    { VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP },
//};

const unsigned PipelineStateMaskBits[PIPELINESTATE_MAX][2] =
{  //  OFFSET, MASK                    28bits
    {  0, 0x0000000F }, // BLENDMODE    4bits
    {  4, 0x0000000F }, // PRIMITIVE    4bits
    {  8, 0x0000000F }, // COLORMASK    4bits
    { 12, 0x00000003 }, // FILLMODE     2bits
    { 14, 0x00000003 }, // CULLMODE     2bits
    { 16, 0x00000007 }, // DEPTHTEST    3bits
    { 19, 0x00000001 }, // DEPTHWRITE   1bit
    { 20, 0x00000001 }, // STENCILTEST  1bit
    { 21, 0x0000000F }, // STENCILMODE  4bits
    { 25, 0x00000007 }, // SAMPLES      3bits
    { 28, 0x00000003 }, // LINEWIDTH    2bits
};

const char* PipelineStateNames_[PIPELINESTATE_MAX] =
{
    "BLEN",
    "PRIM",
    "CMSK",
    "FILL",
    "CULL",
    "ZTEST",
    "ZWRIT",
    "STEST",
    "SMODE",
    "SAMPL",
    "LINEW"
};

static const float LineWidthValues_[] =
{
    1.f,
    2.5f,
    5.f
};

const char* RenderPassTypeStr[] =
{
    "PASS_CLEAR",
    "PASS_VIEW",
    "PASS_PRESENT"
};

const char* RenderSlotTypeStr[] =
{
    "RENDERSLOT_PRESENT",
    "RENDERSLOT_TARGET1",
    "RENDERSLOT_TARGET2",
	"RENDERSLOT_DEPTH",
	0,
    "RENDERSLOT_NONE"
};

template <typename T> T* PhysicalDeviceInfo::GetExtensionFeatures() const
{
    return extensionFeatures_.Find<T>();
}

template <typename T> T& PhysicalDeviceInfo::GetOrCreateExtensionFeatures(VkStructureType featuretype)
{
    // Get the extension features if exists in the map
    T* object = GetExtensionFeatures<T>();

    // If not exists add new extension features in collection
    if (object == nullptr)
    {
        // Get the last features
        void* prevFeatures = extensionFeatures_.Size() ? extensionFeatures_.Back() : nullptr;

        // Create the extension features structure
        T& features     = extensionFeatures_.New<T>();
        features.sType  = featuretype;
        // Link the new features with the previous one in the collection
        features.pNext  = prevFeatures;

        // Get the updated features
        VkPhysicalDeviceFeatures2 physicalfeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        physicalfeatures.pNext = &features;
        vkGetPhysicalDeviceFeatures2(device_, &physicalfeatures);

        object = &features;
    }

    return *object;
}

template <typename T> T* PhysicalDeviceInfo::GetExtensionProperties() const
{
    return extensionProperties_.Find<T>();
}

template <typename T> T& PhysicalDeviceInfo::GetOrCreateExtensionProperties(VkStructureType propertytype)
{
    // Get the extension properties if exists
    T* object = GetExtensionProperties<T>();

    // If not exists add new extension properties in collection
    if (object == nullptr)
    {
        // Get the last properties
		void* prevProperties = extensionProperties_.Size() ? extensionProperties_.Back() : nullptr;

		// Create the extension properties structure
        T& properties        = extensionProperties_.New<T>();
        properties.sType     = propertytype;
        // Link the new properties with the previous one in the collection
        properties.pNext     = prevProperties;

		// Get the updated properties
		VkPhysicalDeviceProperties2 physicalproperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		physicalproperties.pNext = &properties;
		vkGetPhysicalDeviceProperties2(device_, &physicalproperties);

        object = &properties;
    }

    return *object;
}

void PhysicalDeviceInfo::CleanUp()
{
    extensionFeatures_.Clear();
    extensionProperties_.Clear();
}

#ifndef URHO3D_VMA
bool PhysicalDeviceInfo::GetMemoryTypeIndex(uint32_t filter, VkMemoryPropertyFlags properties, uint32_t& memorytype) const
{
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; i++)
    {
        if ((filter & (1 << i)) && (memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties)
        {
            memorytype = i;
            return true;
        }
    }
    return false;
}
#endif

void SetPipelineState(PipelineInfo* info, PipelineState state, unsigned value)
{
    unsigned offset       = PipelineStateMaskBits[state][0];
    unsigned mask         = (PipelineStateMaskBits[state][1] << offset);
    info->pipelineStates_ = ((value << offset) & mask) + (info->pipelineStates_ & ~mask);
}

unsigned GetPipelineStateInternal(PipelineInfo* info, PipelineState state)
{
    unsigned stateValue = (info->pipelineStates_ >> PipelineStateMaskBits[state][0]) & PipelineStateMaskBits[state][1];
    return stateValue;
}


void ExtractStencilMode(int value, CompareMode& mode, StencilOp& pass, StencilOp& fail, StencilOp& zFail)
{
    if (value == 0)
    {
        mode = CMP_ALWAYS;
        pass = OP_REF;
        fail = OP_KEEP;
        zFail = OP_KEEP;
    }
    else if (value == 1)
    {
        mode = CMP_EQUAL;
        pass = OP_KEEP;
        fail = OP_KEEP;
        zFail = OP_KEEP;
    }
}

int StencilMode(CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail)
{
    if (mode == CMP_ALWAYS && pass == OP_REF && fail == OP_KEEP && zFail == OP_KEEP)
        return 0;
    if (mode == CMP_EQUAL && pass == OP_KEEP && fail == OP_KEEP && zFail == OP_KEEP)
        return 1;
    return 0;
}


PipelineBuilder::PipelineBuilder(GraphicsImpl* impl) :
    numShaderStages_(0U),
    numVertexBindings_(0U),
    numVertexAttributes_(0U),
    numDynamicStates_(0U),
    numColorAttachments_(1U),
    impl_(impl),
    viewportSetted_(false),
    pAllocator_(nullptr)
{
    vertexInputState_   = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    inputAssemblyState_ = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    viewportState_      = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    rasterizationState_ = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    depthStencilState_  = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dynamicState_       = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    multiSampleState_   = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    colorBlendState_    = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };

//    vertexInputState_.sType   = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//    vertexInputState_.pNext   = nullptr;
//    vertexInputState_.flags   = 0;
//    inputAssemblyState_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
//    inputAssemblyState_.flags = 0;
//    inputAssemblyState_.pNext = nullptr;
//    viewportState_.sType      = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
//    viewportState_.flags      = 0;
//    viewportState_.pNext      = nullptr;
//    rasterizationState_.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
//    rasterizationState_.flags = 0;
//    rasterizationState_.pNext = nullptr;
//    depthStencilState_.sType  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
//    depthStencilState_.flags  = 0;
//    depthStencilState_.pNext  = nullptr;
//    dynamicState_.sType       = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
//    dynamicState_.flags       = 0;
//    dynamicState_.pNext       = nullptr;
//    multiSampleState_.sType   = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
//    multiSampleState_.flags   = 0;
//    multiSampleState_.pNext   = nullptr;
//    colorBlendState_.sType    = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
//    colorBlendState_.flags    = 0;
//    colorBlendState_.pNext    = nullptr;

    Reset();
}

void PipelineBuilder::Reset()
{
    inputAssemblyState_.topology                = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState_.primitiveRestartEnable  = VK_FALSE;
    viewportState_.viewportCount                = 1;
    viewportState_.pViewports                   = &viewport_;
    viewportState_.scissorCount                 = 1;
    viewportState_.pScissors                    = &scissor_;
    rasterizationState_.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState_.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizationState_.lineWidth               = 1.f;
    rasterizationState_.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizationState_.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState_.depthBiasEnable         = VK_FALSE;
    rasterizationState_.depthBiasClamp          = 0.f;
    rasterizationState_.depthBiasConstantFactor = 0.f;
    rasterizationState_.depthBiasSlopeFactor    = 0.f;
    rasterizationState_.depthClampEnable        = VK_FALSE;
    depthStencilState_.depthTestEnable          = VK_FALSE;
    depthStencilState_.depthWriteEnable         = VK_FALSE;
    depthStencilState_.depthCompareOp           = VK_COMPARE_OP_LESS;
    depthStencilState_.depthBoundsTestEnable    = VK_FALSE;
    depthStencilState_.stencilTestEnable        = VK_FALSE;
    dynamicState_.dynamicStateCount             = 0;
    dynamicState_.pDynamicStates                = nullptr;
    multiSampleState_.alphaToOneEnable          = VK_FALSE;
    multiSampleState_.alphaToCoverageEnable     = VK_FALSE;
    multiSampleState_.minSampleShading          = 0.f;
    multiSampleState_.pSampleMask               = nullptr;
    multiSampleState_.sampleShadingEnable       = VK_FALSE;
    multiSampleState_.rasterizationSamples      = VK_SAMPLE_COUNT_1_BIT;
    colorBlendState_.logicOpEnable              = VK_FALSE;
    colorBlendState_.logicOp                    = VK_LOGIC_OP_COPY;
    colorBlendState_.attachmentCount            = 1;
    colorBlendState_.pAttachments               = colorBlendAttachments_;
    colorBlendState_.blendConstants[0]          = 0.f;
    colorBlendState_.blendConstants[1]          = 0.f;
    colorBlendState_.blendConstants[2]          = 0.f;
    colorBlendState_.blendConstants[3]          = 0.f;

    CleanUp();

//    URHO3D_LOGDEBUGF("PipelineBuilder - Reset");
}

void PipelineBuilder::CleanUp(bool shadermodules, bool vertexinfo, bool dynamicstates, bool colorblending)
{
    viewportSetted_ = false;

    if (shadermodules)
    {
        for (unsigned i = 0; i < shaderModules_.Size(); i++)
        {
            if (shaderModules_[i] != VK_NULL_HANDLE)
                vkDestroyShaderModule(impl_->device_, shaderModules_[i], pAllocator_);
        }

        shaderModules_.Clear();
        numShaderStages_ = 0;
    }
    if (vertexinfo)
    {
        vertexElementsTable_.Clear();
        numVertexBindings_ = 0;
        numVertexAttributes_ = 0;

        vertexInputState_.vertexBindingDescriptionCount   = 0;
        vertexInputState_.pVertexBindingDescriptions      = nullptr;
        vertexInputState_.vertexAttributeDescriptionCount = 0;
        vertexInputState_.pVertexAttributeDescriptions    = nullptr;
    }
    if (dynamicstates)
    {
        numDynamicStates_ = 0;

        dynamicState_.dynamicStateCount = 0;
        dynamicState_.pDynamicStates    = nullptr;
        dynamicState_.flags             = 0;
        dynamicState_.pNext             = nullptr;
    }
    if (colorblending)
    {
        numColorAttachments_ = 0;

        VkPipelineColorBlendAttachmentState& colorBlendAttachment = colorBlendAttachments_[0];
        colorBlendAttachment.blendEnable    = VK_FALSE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    }

//    URHO3D_LOGDEBUGF("PipelineBuilder - CleanUp");
}

void PipelineBuilder::AddShaderStage(ShaderVariation* variation, const String& entry)
{
    if (numShaderStages_ >= VULKAN_MAX_SHADER_STAGES)
    {
        URHO3D_LOGERRORF("Max Shader Stages !");
        return;
    }

    // get the bytecode
    const PODVector<unsigned char>& byteCode = variation->GetByteCode();
    if (!byteCode.Size())
    {
        if (variation->Create())
        {
            URHO3D_LOGERRORF("Can't create shader module %s no bytecode !", variation->GetName().CString());
            return;
        }
    }

    // create shader module
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleInfo.codeSize = byteCode.Size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.Buffer());
    if (vkCreateShaderModule(impl_->GetDevice(), &shaderModuleInfo, pAllocator_, &shaderModule) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create shader module %s !", variation->GetName().CString());
        return;
    }
    shaderModules_.Resize(shaderModules_.Size() + 1);
    shaderModules_.Back() = shaderModule;

    // create the shader stage info
    VkPipelineShaderStageCreateInfo& info = shaderStages_[numShaderStages_];
    info.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage               = variation->GetShaderType() == VS ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    info.module              = shaderModules_.Back();
    info.pName               = "main";
//    info.pName               = entry.CString();
    info.pSpecializationInfo = nullptr;
    info.flags               = 0;
    info.pNext               = nullptr;

    numShaderStages_++;
}

void PipelineBuilder::AddVertexBinding(unsigned binding, bool instance)
{
    if (binding >= VULKAN_MAX_VERTEX_BINDINGS)
    {
        URHO3D_LOGERRORF("Max Vertex Bindings !");
        return;
    }

    if (binding >= numVertexBindings_)
        numVertexBindings_ = binding + 1;

    VkVertexInputBindingDescription& bindingDesc = vertexBindings_[binding];
    bindingDesc.binding   = static_cast<uint32_t>(binding);
    bindingDesc.inputRate = !instance ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
}

void PipelineBuilder::AddVertexElement(unsigned binding, const VertexElement& element)
{
    if (binding >= VULKAN_MAX_VERTEX_BINDINGS)
    {
        URHO3D_LOGERRORF("Max Vertex Bindings !");
        return;
    }

    if (binding >= vertexElementsTable_.Size())
        vertexElementsTable_.Resize(binding + 1);

    PODVector<VertexElement>& elements = vertexElementsTable_[binding];
    elements.Push(element);
}

void PipelineBuilder::AddVertexElements(unsigned binding, const PODVector<VertexElement>& elements)
{
    if (binding >= VULKAN_MAX_VERTEX_BINDINGS)
    {
        URHO3D_LOGERRORF("Max Vertex Bindings !");
        return;
    }

    if (binding >= vertexElementsTable_.Size())
        vertexElementsTable_.Resize(binding + 1);

    vertexElementsTable_[binding] = elements;
}

void PipelineBuilder::AddVertexElements(const Vector<PODVector<VertexElement>>& elementsTable, const bool* instanceTable)
{
    if (elementsTable.Size() >= VULKAN_MAX_VERTEX_BINDINGS)
    {
        URHO3D_LOGERRORF("Max Vertex Bindings !");
        return;
    }

    vertexElementsTable_ = elementsTable;

    if (vertexElementsTable_.Size() != numVertexBindings_)
    {
        numVertexBindings_ = vertexElementsTable_.Size();
        for (unsigned binding = 0; binding < numVertexBindings_; binding++)
        {
            VkVertexInputBindingDescription& bindingDesc = vertexBindings_[binding];
            bindingDesc.binding = static_cast<uint32_t>(binding);
            bindingDesc.inputRate = instanceTable && instanceTable[binding] ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        }
    }
}

void PipelineBuilder::SetTopology(unsigned primitive, bool primitiveRestartEnable, unsigned flags)
{
    inputAssemblyState_.topology               = VulkanPrimitiveTopologies[primitive];
    inputAssemblyState_.primitiveRestartEnable = primitiveRestartEnable ? VK_TRUE : VK_FALSE;
    inputAssemblyState_.flags                  = static_cast<uint32_t>(flags);
    inputAssemblyState_.pNext                  = nullptr;
}

// TODO
void PipelineBuilder::SetViewportStates()
{
    viewport_.x        = 0.f;
    viewport_.y        = 0.f;
    viewport_.width    = (float)impl_->swapChainExtent_.width;
    viewport_.height   = (float)impl_->swapChainExtent_.height;
    viewport_.minDepth = 0.f;
    viewport_.maxDepth = 1.f;

    scissor_.offset    = { 0, 0 };
    scissor_.extent    = impl_->swapChainExtent_;

    viewportState_.viewportCount = 1;
    viewportState_.pViewports    = &viewport_;
    viewportState_.scissorCount  = 1;
    viewportState_.pScissors     = &scissor_;

    viewportSetted_ = true;
}

void PipelineBuilder::SetRasterization(unsigned fillMode, CullMode cullMode, int linewidth)
{
    rasterizationState_.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState_.polygonMode             = VulkanPolygonMode[fillMode];
    rasterizationState_.lineWidth               = LineWidthValues_[Clamp(linewidth, 0, 2)];
//    rasterizationState_.cullMode                = VK_CULL_MODE_NONE;
//    rasterizationState_.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState_.cullMode                = cullMode == CULL_NONE ? VK_CULL_MODE_NONE : cullMode == CULL_CW ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT;
    rasterizationState_.frontFace               = cullMode == CULL_CCW  ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rasterizationState_.depthBiasEnable         = VK_FALSE;
    rasterizationState_.depthBiasClamp          = 0.f;
    rasterizationState_.depthBiasConstantFactor = 0.f;
    rasterizationState_.depthBiasSlopeFactor    = 0.f;
    rasterizationState_.depthClampEnable        = VK_FALSE;
    rasterizationState_.flags                   = 0;
}

void PipelineBuilder::SetDepthStencil(bool enable, int compare, bool write, bool stencil, int stencilmode, unsigned stencilvalue)
{
    depthStencilState_.depthTestEnable       = enable ? VK_TRUE : VK_FALSE;
    depthStencilState_.depthCompareOp        = enable ? VulkanCompareMode[compare] : VK_COMPARE_OP_ALWAYS;
    depthStencilState_.depthWriteEnable      = enable && write ? VK_TRUE : VK_FALSE;

    depthStencilState_.depthBoundsTestEnable = VK_FALSE;
    depthStencilState_.minDepthBounds        = 0.f;       // Optional
    depthStencilState_.maxDepthBounds        = 1.f;       // Optional
    depthStencilState_.stencilTestEnable     = stencil ? VK_TRUE : VK_FALSE;

    if (stencil)
    {
        CompareMode compare;
        StencilOp pass, fail, zfail;
        ExtractStencilMode(stencilmode, compare, pass, fail, zfail);
        depthStencilState_.back.compareOp        = VulkanCompareMode[compare];
        depthStencilState_.back.failOp           = VulkanStencilOp[fail];
        depthStencilState_.back.depthFailOp      = VulkanStencilOp[zfail];
        depthStencilState_.back.passOp           = VulkanStencilOp[pass];
        depthStencilState_.back.compareMask      = 0xff;
        depthStencilState_.back.writeMask        = 0xff;
        depthStencilState_.back.reference        = stencilvalue;
        depthStencilState_.front                 = depthStencilState_.back;
    }
}

void PipelineBuilder::AddDynamicState(VkDynamicState state)
{
    if (numDynamicStates_ + 1 >= VULKAN_MAX_DYNAMIC_STATES)
    {
        URHO3D_LOGERRORF("Max Dynamic State added !");
        return;
    }

    dynamicStates_[numDynamicStates_] = state;
    numDynamicStates_++;

    dynamicState_.dynamicStateCount = numDynamicStates_;
    dynamicState_.pDynamicStates    = numDynamicStates_ ? dynamicStates_ : nullptr;
}

void PipelineBuilder::SetMultiSampleState(int p)
{
    int samples = Min(1 << p, VK_SAMPLE_COUNT_64_BIT);
    URHO3D_LOGDEBUGF("multisample = numSamples=%d (puissance=%d)", samples, p);
    multiSampleState_.sampleShadingEnable  = p > 0 ? VK_TRUE : VK_FALSE;
    multiSampleState_.rasterizationSamples = (VkSampleCountFlagBits)(samples);
    //multiSampleState_.sampleShadingEnable  = VK_FALSE;
    //multiSampleState_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
}

void PipelineBuilder::SetColorBlend(bool enable, VkLogicOp logicOp, float b0, float b1, float b2, float b3)
{
    colorBlendState_.logicOpEnable     = enable ? VK_TRUE : VK_FALSE;
    colorBlendState_.logicOp           = logicOp;
    colorBlendState_.blendConstants[0] = b0;
    colorBlendState_.blendConstants[1] = b1;
    colorBlendState_.blendConstants[2] = b2;
    colorBlendState_.blendConstants[3] = b3;
}

void PipelineBuilder::AddColorBlendAttachment(int index, BlendMode blendMode, unsigned colormask)
{
    if (blendMode > BLEND_SUBTRACTALPHA)
        return;

    if (index + 1 >= VULKAN_MAX_COLOR_ATTACHMENTS)
    {
        URHO3D_LOGERRORF("Max Color Attachments !");
        return;
    }

    if (index >= numColorAttachments_)
        numColorAttachments_ = index + 1;

    VkPipelineColorBlendAttachmentState& colorBlendAttachment = colorBlendAttachments_[index];

    colorBlendAttachment.blendEnable = blendMode == BLEND_REPLACE ? VK_FALSE : VK_TRUE;

    if (blendMode == BLEND_REPLACE)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }
    else if (blendMode == BLEND_ADD)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
//        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
//        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    else if (blendMode == BLEND_MULTIPLY)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }
    else if (blendMode == BLEND_ALPHA)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    else if (blendMode == BLEND_ADDALPHA)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    else if (blendMode == BLEND_PREMULALPHA)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    else if (blendMode == BLEND_INVDESTALPHA)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    }
    else if (blendMode == BLEND_SUBTRACT)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_REVERSE_SUBTRACT;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_REVERSE_SUBTRACT;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }
    else if (blendMode == BLEND_SUBTRACTALPHA)
    {
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_REVERSE_SUBTRACT;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_REVERSE_SUBTRACT;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    }

    colorBlendAttachment.colorWriteMask = colormask;//VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlendState_.attachmentCount = static_cast<uint32_t>(numColorAttachments_);
}

/*
    Create the descriptor sets for the pipeline : a layout set by binding

    a descriptor set layout for camera        => it's not optimal : the camera just need to be bound two time in a frame (for the scene and for the ui)
    a descriptor set layout for model matrix  => should be a dynamic uniform buffer for push all the model matrix in this buffer and only send the offset
    a descriptor set layouts for the samplers => should adjust the number of samplers in the sampler array according to need

    see https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/
    see Graphics::PrepareDraw for the consommation of these descriptors
*/
bool PipelineBuilder::CreateDescriptors(PipelineInfo* info)
{
    unsigned setid = 0;

    unsigned maxToAllocate = info->maxAllocatedDescriptorSets_;

    // Check fort Bindingflag (requires VK version 1.2)
    bool bindingFlagsEnable = VK_VERSION_MAJOR(impl_->vulkanApiVersion_) > 0 && VK_VERSION_MINOR(impl_->vulkanApiVersion_) > 1;
    // Check for DescriptorIndeing
    bool descriptorIndexingEnable = bindingFlagsEnable && impl_->physicalInfo_.GetExtensionFeatures<VkPhysicalDeviceDescriptorIndexingFeatures>() != 0;
    bool uniformBufferAfterBind = descriptorIndexingEnable && impl_->physicalInfo_.GetExtensionFeatures<VkPhysicalDeviceDescriptorIndexingFeatures>()->descriptorBindingUniformBufferUpdateAfterBind;

    for (Vector<DescriptorsGroup>::Iterator it = info->descriptorsGroups_.Begin(); it != info->descriptorsGroups_.End(); ++it)
    {
        DescriptorsGroup& d = *it;

        const Vector<ShaderBind>& bindings = d.bindings_;

        // Create the Descriptor Set Bindings
        Vector<VkDescriptorSetLayoutBinding> layoutBindings;
        layoutBindings.Resize(bindings.Size());
        Vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.Resize(bindings.Size());

        for (unsigned i = 0; i < bindings.Size(); i++)
        {
            const ShaderBind& bind = bindings[i];

            VkDescriptorSetLayoutBinding& binding = layoutBindings[i];
            binding.binding            = (uint32_t)bind.id_;
            binding.descriptorCount    = (uint32_t)bind.unitRange_;
            binding.descriptorType     = (VkDescriptorType)bind.type_;
            binding.pImmutableSamplers = nullptr;
            binding.stageFlags         = bind.stageFlag_;

            VkDescriptorPoolSize& poolsize = poolSizes[i];
            poolsize.descriptorCount = maxToAllocate * bind.unitRange_;
            poolsize.type = (VkDescriptorType)bind.type_;
        }

        // Create Descriptor Set Layout
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = layoutBindings.Size();
        layoutInfo.pBindings    = layoutBindings.Buffer();
        layoutInfo.flags        = 0;
        layoutInfo.pNext        = nullptr;

        // Set Binding Flags
        if (bindingFlagsEnable)
        {
            Vector<VkDescriptorBindingFlags> layoutBindingFlags;
            layoutBindingFlags.Resize(bindings.Size());

            if (descriptorIndexingEnable)
            {
                if (uniformBufferAfterBind)
                {
                    // Enable AfterBind bit only if all Binds in the set are of the allowing types
                    bool afterBindEnable = true;
                    for (unsigned i = 0; i < bindings.Size(); i++)
                    {
                        if (bindings[i].type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                        {
                            afterBindEnable = false;
                            break;
                        }
                    }
                    if (afterBindEnable)
                    {
                        for (unsigned i = 0; i < bindings.Size(); i++)
                            layoutBindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;

                        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                    }
                }
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingflagsInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
            bindingflagsInfo.pNext         = nullptr;
            bindingflagsInfo.bindingCount  = layoutBindingFlags.Size();
            bindingflagsInfo.pBindingFlags = layoutBindingFlags.Buffer();

            layoutInfo.pNext = &bindingflagsInfo;

            if (vkCreateDescriptorSetLayout(impl_->device_, &layoutInfo, pAllocator_, &d.layout_) != VK_SUCCESS)
            {
                URHO3D_LOGERRORF("Can't create descriptorSet layout with binding flags !");
                return false;
            }
        }
        else if (vkCreateDescriptorSetLayout(impl_->device_, &layoutInfo, pAllocator_, &d.layout_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create descriptorSet layout !");
            return false;
        }

        d.setsByFrame_.Resize(impl_->numFrames_);

        for (unsigned frame=0; frame < impl_->numFrames_; frame++)
        {
            DescriptorsGroupAllocation& alloc = d.setsByFrame_[frame];

            // Create Descriptor pools
            {
                VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                poolInfo.maxSets       = maxToAllocate;
                poolInfo.poolSizeCount = poolSizes.Size();
                poolInfo.pPoolSizes    = poolSizes.Buffer();
                poolInfo.flags         = descriptorIndexingEnable ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0;
                if (vkCreateDescriptorPool(impl_->device_, &poolInfo, pAllocator_, &alloc.pool_) != VK_SUCCESS)
                {
                    URHO3D_LOGERRORF("Can't create ubo descriptor pool %d !", d.id_);
                    return false;
                }
            }

            // Allocate DescriptorSets
            // maxToAllocate is the max consumed UBO descriptors during the preparation of a frame
            {
                Vector<VkDescriptorSetLayout> descriptorSetLayouts;
                descriptorSetLayouts.Resize(maxToAllocate);
                for (unsigned l = 0; l < maxToAllocate; l++)
                    descriptorSetLayouts[l] = d.layout_;

                VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                allocInfo.descriptorPool     = alloc.pool_;
                allocInfo.descriptorSetCount = maxToAllocate;
                allocInfo.pSetLayouts        = descriptorSetLayouts.Buffer();

                alloc.sets_.Resize(maxToAllocate);
                if (vkAllocateDescriptorSets(impl_->device_, &allocInfo, alloc.sets_.Buffer()) != VK_SUCCESS)
                {
                    URHO3D_LOGERRORF("Can't allocate descriptor sets !");
                    return false;
                }

                // Initialize the descriptor index
                alloc.index_ = maxToAllocate;
//                alloc.index_ = 0U;

//                URHO3D_LOGDEBUGF("Allocate descriptor sets for pipeline %s UBO at frame=%u binding=%u ...", info->vs_->GetName().CString(), frame, bindingpoint);
            }
        }
    }

    return true;
}

void PipelineBuilder::CreatePipeline(PipelineInfo* info)
{
    const RenderPassInfo* renderPassInfo = impl_->GetRenderPassInfo(info->renderPassKey_);

    if (renderPassInfo->renderPass_ == VK_NULL_HANDLE)
    {
        URHO3D_LOGERRORF("Can't create pipeline : no renderpass renderpasskey=%u !", info->renderPassKey_);
        return;
    }

    // Set Vertex Attributes
    {
        for (unsigned binding = 0; binding < numVertexBindings_; binding++)
        {
            const PODVector<VertexElement>& elements = vertexElementsTable_[binding];

            if (numVertexAttributes_ + elements.Size() >= VULKAN_MAX_VERTEX_ATTRIBUTES)
            {
                URHO3D_LOGERRORF("Max Vertex Attributes at binding=%u !", binding);
                return;
            }

            unsigned int vertexSize = 0;
            unsigned startAttribute = numVertexAttributes_;

            for (unsigned int location = 0; location < elements.Size(); location++)
            {
                VertexElementType elementType = elements[location].type_;

                VkVertexInputAttributeDescription& attribute = vertexAttributes_[startAttribute+location];
                attribute.binding  = binding;
                attribute.location = static_cast<uint32_t>(location);    // one location = 16bytes
                attribute.format   = ELEMENT_TYPE_VKFORMAT[elementType];
                attribute.offset   = static_cast<uint32_t>(vertexSize);

                vertexSize += ELEMENT_TYPESIZES[elementType];

                URHO3D_LOGDEBUGF("  vertex attribute binding=%d location=%u type=%s size=%u location=%u offset=%u", binding, location, ELEMENT_TYPE_STR[elementType], ELEMENT_TYPESIZES[elementType],
                                 attribute.location, attribute.offset);
            }

            // vertexsize must aligned by 16bytes (4floats)
            if (vertexSize % 16 != 0)
                vertexSize = (vertexSize / 16 + 1) * 16;

            // Set Vertex Binding
            VkVertexInputBindingDescription& bindingDesc = vertexBindings_[binding];
            bindingDesc.binding   = static_cast<uint32_t>(binding);
            bindingDesc.stride    = static_cast<uint32_t>(vertexSize);

            URHO3D_LOGDEBUGF("  vertex size=%u", vertexSize);

            numVertexAttributes_ += elements.Size();
        }

        // Update Vertex Input State
        vertexInputState_.vertexBindingDescriptionCount   = static_cast<uint32_t>(numVertexBindings_);
        vertexInputState_.pVertexBindingDescriptions      = numVertexBindings_ ? vertexBindings_ : nullptr;
        vertexInputState_.vertexAttributeDescriptionCount = static_cast<uint32_t>(numVertexAttributes_);
        vertexInputState_.pVertexAttributeDescriptions    = numVertexAttributes_ ? vertexAttributes_ : nullptr;
    }

    if (!viewportSetted_)
    {
        viewport_.x        = 0.f;
        viewport_.y        = 0.f;
        viewport_.width    = (float)impl_->swapChainExtent_.width;
        viewport_.height   = (float)impl_->swapChainExtent_.height;
        viewport_.minDepth = 0.f;
        viewport_.maxDepth = 1.f;

        scissor_.offset    = { 0, 0 };
        scissor_.extent    = impl_->swapChainExtent_;

        viewportState_.viewportCount = 1;
        viewportState_.pViewports    = &viewport_;
        viewportState_.scissorCount  = 1;
        viewportState_.pScissors     = &scissor_;

        viewportSetted_ = true;
    }

    vkDeviceWaitIdle(impl_->device_);

    // Create the descriptor before the pipeline layout
    if (!CreateDescriptors(info))
        return;

    // Pipeline Layout
    if (info->pipelineLayout_ == VK_NULL_HANDLE)
    {
        // Add DescripterLayoutSets
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        Vector<VkDescriptorSetLayout> layouts;
        if (info->descriptorsGroups_.Size())
        {
            for (Vector<DescriptorsGroup>::Iterator set = info->descriptorsGroups_.Begin(); set != info->descriptorsGroups_.End(); ++set)
            {
                URHO3D_LOGDEBUGF("pipeline layout : add descriptorSet set=%u layout = %u !", set->id_, set->layout_);
                layouts.Push(set->layout_);
            }

            pipelineLayoutInfo.setLayoutCount = layouts.Size();
            pipelineLayoutInfo.pSetLayouts    = layouts.Buffer();

        }
        else
        {
            pipelineLayoutInfo.setLayoutCount = 0;
        }
        // Add PushContants
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        if (vkCreatePipelineLayout(impl_->device_, &pipelineLayoutInfo, pAllocator_, &info->pipelineLayout_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create pipeline layout !");
            return;
        }
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount          = static_cast<uint32_t>(numShaderStages_);
    pipelineInfo.pStages             = shaderStages_;
    pipelineInfo.pVertexInputState   = &vertexInputState_;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState_;
    pipelineInfo.pViewportState      = &viewportState_;
    pipelineInfo.pRasterizationState = &rasterizationState_;
    pipelineInfo.pDepthStencilState  = &depthStencilState_;
    pipelineInfo.pDynamicState       = &dynamicState_;
    pipelineInfo.pMultisampleState   = &multiSampleState_;
    pipelineInfo.pColorBlendState    = &colorBlendState_;
    pipelineInfo.layout              = info->pipelineLayout_;
    pipelineInfo.renderPass          = renderPassInfo->renderPass_;
    pipelineInfo.subpass             = renderPassInfo->type_ == PASS_PRESENT ? 0 : 1;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    VkResult result = vkCreateGraphicsPipelines(impl_->device_, impl_->pipelineCache_, 1, &pipelineInfo, pAllocator_, &info->pipeline_);

    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create pipeline !");
    }
    else
    {
        URHO3D_LOGDEBUGF("create pipeline : shaderStages=%u vertexBindings=%u vertexAttributes=%u dynamicStates=%u colorAttachments=%u !",
                                            numShaderStages_, numVertexBindings_, numVertexAttributes_, numDynamicStates_, numColorAttachments_);
        URHO3D_LOGDEBUGF("                  VkPipeline=%u VkPipelineLayout=%u", info->pipeline_, info->pipelineLayout_);
    }

    for (unsigned i = 0; i < shaderModules_.Size(); i++)
    {
        if (shaderModules_[i] != VK_NULL_HANDLE)
            vkDestroyShaderModule(impl_->device_, shaderModules_[i], pAllocator_);
    }

    shaderModules_.Clear();
    numShaderStages_ = 0;
}



PipelineInfo* GraphicsImpl::pipelineInfo_ = nullptr;
PhysicalDeviceInfo GraphicsImpl::physicalInfo_;
VkSurfaceFormatKHR GraphicsImpl::swapChainInfo_;
VkFormat GraphicsImpl::depthStencilFormat_;

GraphicsImpl::GraphicsImpl() :
    validationLayersEnabled_(true),
#ifdef URHO3D_VMA
    allocator_(VK_NULL_HANDLE),
#endif
    instance_(VK_NULL_HANDLE),
    debugMsg_(VK_NULL_HANDLE),
    surface_(VK_NULL_HANDLE),
    oldSurface_(VK_NULL_HANDLE),
    swapChain_(VK_NULL_HANDLE),
    pipelineBuilder_(this),
    pipelineCache_(VK_NULL_HANDLE),
    numFrames_(1),
    currentFrame_(0),
    presentMode_(VK_PRESENT_MODE_IMMEDIATE_KHR),
    frame_(nullptr),
    renderPathData_(nullptr),
    renderPassInfo_(nullptr),
    viewportTexture_(nullptr),
    renderPassIndex_(-1),
    viewportIndex_(0)
{
    defaultPipelineStates_ = 0;

    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_PRIMITIVE, TRIANGLE_LIST);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_COLORMASK, 0xF);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_CULLMODE, CULL_NONE);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_DEPTHTEST, CMP_ALWAYS);//CMP_LESSEQUAL);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_DEPTHWRITE, false);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_FILLMODE, FILL_SOLID);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_STENCILTEST, false);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_STENCILMODE, 0);
    SetPipelineState(defaultPipelineStates_, PIPELINESTATE_SAMPLES, 0);

    pipelineStates_ = defaultPipelineStates_;
    stencilValue_ = 0;

    // Add built-in renderPathInfos
    AddRenderPassInfo("CLEAR_1C");
    AddRenderPassInfo("RENDER_1C_1DS_1");
    AddRenderPassInfo("RENDER_1C_1DS_2");
    AddRenderPassInfo("PRESENT_1C");

    SetRenderPath(0);

	AddInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
	AddInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	AddDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	AddDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	AddDeviceExtension(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

	AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

//	SetDefaultDevice("llvmpipe");
}

void GraphicsImpl::AddInstanceExtension(const char* extension)
{
	requireInstanceExts_.Push(extension);
}

void GraphicsImpl::AddDeviceExtension(const char* extension)
{
	requireDeviceExts_.Push(extension);
}

void GraphicsImpl::SetDefaultDevice(const String& device)
{
	requireDevice_ = device;
}

bool GraphicsImpl::CreateVulkanInstance(Context* context, const String& appname, SDL_Window* window, const Vector<String>& requestedLayers)
{
#ifdef URHO3D_VOLK
    if (volkInitialize() != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't initialize Vulkan !");
        return false;
    }

    URHO3D_LOGINFOF("Initialize Volk for Vulkan !");
#endif
    // get Vulkan API version
    vkEnumerateInstanceVersion(&vulkanApiVersion_);
    URHO3D_LOGINFOF("Version Vulkan : %u.%u.%u (%u)", VK_VERSION_MAJOR(vulkanApiVersion_), VK_VERSION_MINOR(vulkanApiVersion_), VK_VERSION_PATCH(vulkanApiVersion_), vulkanApiVersion_);

    context_ = context;

    // TODO memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    // Get required extensions for the SDL window context
	PODVector<const char*> contextExtensions;
    {
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, 0);
    if (!extensionCount)
    {
        URHO3D_LOGERRORF("Unable to query the number of Vulkan instance extension names !");
        return false;
		}
		contextExtensions.Resize(extensionCount);
		SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, contextExtensions.Buffer());
    }

	// Get available instance extensions
	{
		unsigned int availableInstanceExtsCount;
		vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtsCount, nullptr);
		Vector<VkExtensionProperties> availableInstanceExts(availableInstanceExtsCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtsCount, availableInstanceExts.Buffer());

		// Get Require Instance Extensions
		PODVector<const char*> enableExts;
		for (PODVector<const char*>::ConstIterator it = requireInstanceExts_.Begin(); it != requireInstanceExts_.End(); ++it)
		{
			if (enableExts.Contains(*it))
				continue;

			bool found = false;
			for (Vector<VkExtensionProperties>::ConstIterator extt = availableInstanceExts.Begin(); extt != availableInstanceExts.End(); ++extt)
			{
				if (strcmp(extt->extensionName, *it) == 0)
				{
					found = true;
					break;
				}
			}
			if (found)
			{
				URHO3D_LOGINFOF("found instance extension %s", *it);
				enableExts.Push(*it);
			}
			else
			{
				URHO3D_LOGERRORF("instance extension %s not found !", *it);
			}
		}
		if (requireInstanceExts_.Size() != enableExts.Size())
		{
			URHO3D_LOGERRORF("All required instance extensions not found !");
			return false;
		}

		// Add Extensions for SDL context
		for (PODVector<const char*>::ConstIterator it = contextExtensions.Begin(); it != contextExtensions.End(); ++it)
		{
			if (requireInstanceExts_.Contains(*it))
				continue;
			requireInstanceExts_.Push(*it);
		}
	}

    // Get available vulkan layers
	PODVector<const char*> validatedLayers;
    {
        unsigned int layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, 0);
        if (!layerCount)
            URHO3D_LOGINFOF("no vulkan layer enable !");

        Vector<VkLayerProperties> availableLayers(layerCount);
        if (layerCount)
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.Buffer());

        // validate required layers
        bool validateValidationLayers = false;
        for (Vector<String>::ConstIterator it = requestedLayers.Begin(); it != requestedLayers.End(); ++it)
        {
            const String& layername = *it;
            bool layerFound = false;

            for (Vector<VkLayerProperties>::ConstIterator jt = availableLayers.Begin(); jt != availableLayers.End(); ++jt)
            {
                if (layername == String(jt->layerName))
                {
                    layerFound = true;
                    break;
                }
            }

            // check validation layer support
            if (layerFound)
            {
                validatedLayers.Push(layername.CString());
                if (validationLayersEnabled_ && layername.Contains("validation"))
                    validateValidationLayers = true;
            }
        }

        validationLayersEnabled_ = validateValidationLayers;
	}

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    // add validation layer support
    if (validationLayersEnabled_)
    {
        debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;

        requireInstanceExts_.Push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType                      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName           = appname.CString();
    appInfo.applicationVersion         = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName                = "URHO3D";
    appInfo.engineVersion              = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion                 = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo        = &appInfo;
    instanceInfo.enabledExtensionCount   = requireInstanceExts_.Size();
    instanceInfo.enabledLayerCount       = validatedLayers.Size();
    instanceInfo.ppEnabledExtensionNames = requireInstanceExts_.Size() ? requireInstanceExts_.Buffer() : nullptr;
    instanceInfo.ppEnabledLayerNames     = validatedLayers.Size()  ? validatedLayers.Buffer() : nullptr;
    instanceInfo.pNext                   = debugCreateInfo.sType   ? &debugCreateInfo         : nullptr;

    if (vkCreateInstance(&instanceInfo, pAllocator, &instance_) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Failed to create vulkan instance !");
        return false;
    }

#ifdef URHO3D_VOLK
    volkLoadInstance(instance_);
    URHO3D_LOGDEBUGF("Volk Load Instance !");
#endif

    // add debug messenger
    if (debugCreateInfo.sType)
    {
        auto vkCreateDebug = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");

        VkResult result = vkCreateDebug != nullptr ? vkCreateDebug(instance_, &debugCreateInfo, pAllocator, &debugMsg_) : VK_ERROR_EXTENSION_NOT_PRESENT;
        if (result != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Failed to create debug messenger !");
            return false;
        }
    }

    // create the surface
    if (!CreateWindowSurface(window))
    {
        URHO3D_LOGERRORF("Can't create SDL Surface for Vulkan !");
        return false;
    }

    // get a valid physical devices
    {
        // get the physical devices
        unsigned int physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, nullptr);
        if (physicalDeviceCount == 0)
        {
            URHO3D_LOGERRORF("No physical devices found !");
            return false;
        }
        Vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(instance_, &physicalDeviceCount, physicalDevices.Buffer());

        // validate the physical devices
        Vector<PhysicalDeviceInfo> validDevices;
        Vector<unsigned int> validDeviceScores;
        PODVector<unsigned int> grQueueIndexes;
        PODVector<unsigned int> prQueueIndexes;
        PODVector<unsigned int> cbQueueIndexes;
        PODVector<unsigned int> unQueueIndexes;

        // check the properties for each device
        for (unsigned int deviceindex = 0; deviceindex < physicalDeviceCount; deviceindex++)
        {
            grQueueIndexes.Clear();
            prQueueIndexes.Clear();
            cbQueueIndexes.Clear();
            unQueueIndexes.Clear();

            VkPhysicalDevice device = physicalDevices[deviceindex];

            // get the device properties
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
//            URHO3D_LOGDEBUGF("found physical device [%u] : %s ", deviceindex, deviceProperties.deviceName);

            unsigned int extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            if (extensionCount == 0)
            {
//                URHO3D_LOGDEBUGF("No device extension found for this device ! Skip");
                continue;
            }

			// check for require device extensions
            Vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.Buffer());
			PODVector<const char*> enableDeviceExts;
			for (PODVector<const char*>::ConstIterator it = requireDeviceExts_.Begin(); it != requireDeviceExts_.End(); ++it)
            {
				if (enableDeviceExts.Contains(*it))
					continue;

				bool found = false;
                for (Vector<VkExtensionProperties>::ConstIterator extt = availableDeviceExtensions.Begin(); extt != availableDeviceExtensions.End(); ++extt)
                {
                    if (strcmp(extt->extensionName, *it) == 0)
                    {
						found = true;
						break;
                    }
                }

				if (found)
                {
//					URHO3D_LOGDEBUGF("found device extension %s", *it);
					enableDeviceExts.Push(*it);
				}
				else
				{
					URHO3D_LOGDEBUGF("device extension %s not found !", *it);
				}
			}

            if (requireDeviceExts_.Size() != enableDeviceExts.Size())
            {
                URHO3D_LOGDEBUGF("All required device extensions not found for the device !");
                continue;
            }

            // check for swapchain capabilities
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &surfaceCapabilities);

            Vector<VkSurfaceFormatKHR> surfaceFormats;
            unsigned int formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
            if (formatCount != 0)
            {
                surfaceFormats.Resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, surfaceFormats.Buffer());
            }

            Vector<VkPresentModeKHR> presentModes;
            unsigned int presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
            if (presentModeCount != 0)
            {
                presentModes.Resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, presentModes.Buffer());
            }

            // check for surface format and present mode availability
            if (!surfaceFormats.Size())
            {
                URHO3D_LOGDEBUGF("No surface format found for the device !");
                continue;
            }

            if (!presentModes.Size())
            {
                URHO3D_LOGDEBUGF("No present mode found for the device !");
                continue;
            }

            // get the queue families that this device supports
            unsigned int queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            if (queueFamilyCount == 0)
            {
                URHO3D_LOGDEBUGF("No queues family found for the device !");
                continue;
            }
            Vector<VkQueueFamilyProperties> familyProperties(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, familyProperties.Buffer());

            // get all the graphic and present queues
            for (unsigned int familyindex = 0; familyindex < queueFamilyCount; familyindex++)
            {
                bool graphicOk = (familyProperties[familyindex].queueCount > 0 &&
                                  familyProperties[familyindex].queueFlags & VK_QUEUE_GRAPHICS_BIT);
                if (graphicOk)
                    grQueueIndexes.Push(familyindex);

                VkBool32 presentOk = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, familyindex, surface_, &presentOk);
                if (presentOk)
                    prQueueIndexes.Push(familyindex);

                if (graphicOk && presentOk)
                    cbQueueIndexes.Push(familyindex);

                if (graphicOk || presentOk)
                    unQueueIndexes.Push(familyindex);
            }

            // the device is validated if it has at least a queue with the graphic capability and another with the presentation capability
            if (prQueueIndexes.Size() && grQueueIndexes.Size())
            {
                // preferably, get a queue with a combined graphical and presentation capability (more efficient)

                validDevices.Resize(validDevices.Size() + 1);
                PhysicalDeviceInfo& validDevice = validDevices.Back();

                validDevice.device_              = device;
                validDevice.name_                = deviceProperties.deviceName;
                validDevice.queueIndexes_        = unQueueIndexes;
                validDevice.grQueueIndex_        = cbQueueIndexes.Size() ? cbQueueIndexes[0] : grQueueIndexes[0];
                validDevice.prQueueIndex_        = cbQueueIndexes.Size() ? cbQueueIndexes[0] : prQueueIndexes[0];
                validDevice.surfaceCapabilities_ = surfaceCapabilities;
                validDevice.surfaceFormats_      = surfaceFormats;
                validDevice.presentModes_        = presentModes;

                // set a score
                validDeviceScores.Push(grQueueIndexes.Size() + prQueueIndexes.Size() + 10 * cbQueueIndexes.Size());

                URHO3D_LOGINFOF("physical device [%u] : %s (score %u) !", deviceindex, validDevice.name_.CString(), validDeviceScores.Back());
            }
        }

        if (!validDevices.Size())
        {
            URHO3D_LOGERRORF("No Physical Device found for the display !");
            return false;
        }

		int deviceindex = 0;
		if (!requireDevice_.Empty())
		{
			// get the index for required device
			deviceindex = -1;
			for (unsigned int i = 0; i < validDevices.Size(); i++)
			{
				if (validDevices[i].name_.StartsWith(requireDevice_))
				{
					deviceindex = i;
					break;
				}
			}
			if (deviceindex == -1)
			{
				URHO3D_LOGERRORF("No Physical device %s found or capable !", requireDevice_.CString());
				return false;
			}
		}
		else
        {
            // get the best physical device
            unsigned int bestscore = 0;
            for (unsigned int i = 0; i < validDeviceScores.Size(); i++)
            {
                if (validDeviceScores[i] > bestscore)
                {
                    bestscore = validDeviceScores[i];
                    deviceindex = i;
                }
            }
		}

        // store the physical device info for next uses
        const PhysicalDeviceInfo& device = validDevices[deviceindex];
        physicalInfo_.device_              = device.device_;
        physicalInfo_.name_                = device.name_;
        physicalInfo_.queueIndexes_        = device.queueIndexes_;
        physicalInfo_.grQueueIndex_        = device.grQueueIndex_;
        physicalInfo_.prQueueIndex_        = device.prQueueIndex_;
        physicalInfo_.surfaceCapabilities_ = device.surfaceCapabilities_;
        physicalInfo_.surfaceFormats_      = device.surfaceFormats_;
        physicalInfo_.presentModes_        = device.presentModes_;

		vkGetPhysicalDeviceFeatures(physicalInfo_.device_, &physicalInfo_.features_);
        vkGetPhysicalDeviceProperties(physicalInfo_.device_, &physicalInfo_.properties_);

    #ifndef URHO3D_VMA
        // get the memory properties (necessary for creating buffers)
		vkGetPhysicalDeviceMemoryProperties(physicalInfo_.device_, &physicalInfo_.memoryProperties_);
    #endif
		URHO3D_LOGINFOF("physical device %s selected !", physicalInfo_.name_.CString());
    }

    // get the optimal depth format (must have optimal Tiling features)
    {
		// get the highest quality first
		// TODO : change this for mobile ?
        depthStencilFormat_ = VK_FORMAT_UNDEFINED;
        static const VkFormat preferedformats[5] =
        {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        for (unsigned int i = 0; i < 5; i++)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalInfo_.device_, preferedformats[i], &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                depthStencilFormat_ = preferedformats[i];
                break;
            }
        }

        if (depthStencilFormat_ == VK_FORMAT_UNDEFINED)
            URHO3D_LOGWARNINGF("Can't find an optimal tiling image format for DepthStencil !");
    }

    // create the logical device

    Vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (unsigned int i = 0; i < physicalInfo_.queueIndexes_.Size(); i++)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = physicalInfo_.queueIndexes_[i];
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.Push(queueCreateInfo);
    }

    // require all available features
    physicalInfo_.requireFeatures_ = physicalInfo_.features_;

    // get features for the extension descriptor indexing
    if (requireDeviceExts_.Contains(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
    {
        auto& features   = physicalInfo_.GetOrCreateExtensionFeatures<VkPhysicalDeviceDescriptorIndexingFeatures>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
        auto& properties = physicalInfo_.GetOrCreateExtensionProperties<VkPhysicalDeviceDescriptorIndexingProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES);
    }

    for (unsigned i = 0; i < requireDeviceExts_.Size(); i++)
    {
        URHO3D_LOGINFOF("enable device Extension %s !", requireDeviceExts_[i]);
    }

    for (unsigned i = 0; i < physicalInfo_.extensionFeatures_.Size(); i++)
        URHO3D_LOGDEBUGF("enable feature %s ptr=%u", physicalInfo_.extensionFeatures_.GetTypeAt(i), physicalInfo_.extensionFeatures_.At(i));

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = physicalInfo_.extensionFeatures_.Size() ? physicalInfo_.extensionFeatures_.Back() : nullptr;
    deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.Size());
    deviceInfo.pQueueCreateInfos       = queueCreateInfos.Buffer();
    deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(requireDeviceExts_.Size());
    deviceInfo.ppEnabledExtensionNames = requireDeviceExts_.Size() ? requireDeviceExts_.Buffer() : nullptr;
    deviceInfo.pEnabledFeatures        = &physicalInfo_.requireFeatures_;
    // Deprecated in Vulkan 1.3...
    deviceInfo.enabledLayerCount       = static_cast<uint32_t>(validatedLayers.Size());
    deviceInfo.ppEnabledLayerNames     = validatedLayers.Size() ? validatedLayers.Buffer() : nullptr;

    if (vkCreateDevice(physicalInfo_.device_, &deviceInfo, pAllocator, &device_) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create Create Logical Device !");
        return false;
    }

#ifdef URHO3D_VOLK
    volkLoadDevice(device_);
    URHO3D_LOGDEBUGF("Volk Load Device !");
#endif

    // Get the graphic queue
    vkGetDeviceQueue(device_, physicalInfo_.grQueueIndex_, 0, &graphicQueue_);

    // Get the presentation queue
    vkGetDeviceQueue(device_, physicalInfo_.prQueueIndex_, 0, &presentQueue_);

    // create the command pool
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = physicalInfo_.grQueueIndex_;
    if (vkCreateCommandPool(device_, &poolInfo, pAllocator, &commandPool_) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create command pool !");
        return false;
    }

    // Create the Semaphore Pool
//    unsigned int numsemaphores = MaxFrames * 2;
//    semaphorePool_.Resize(numsemaphores);
//    VkSemaphoreCreateInfo semaphoreInfo{};
//    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
//    for (unsigned int i = 0; i < numsemaphores; i++)
//    {
//        VkResult result = vkCreateSemaphore(device_, &semaphoreInfo, pAllocator, &semaphorePool_[i]);
//        if (result != VK_SUCCESS)
//        {
//            URHO3D_LOGERRORF("Can't create semaphore !");
//            return false;
//        }
//    }
	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue
	VkResult result = vkCreateSemaphore(device_, &semaphoreInfo, pAllocator, &presentComplete_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create semaphore !");
        return false;
    }
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been submitted and executed
	result = vkCreateSemaphore(device_, &semaphoreInfo, pAllocator, &renderComplete_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create semaphore !");
        return false;
    }

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(device_, &pipelineCacheCreateInfo, pAllocator, &pipelineCache_) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create Pipeline Cache !");
        return false;
    }

#ifdef URHO3D_VMA
    // Initialize the memory allocator
    // TODO : can we move the code at the begin of CreateVulkanInstance and use pAllocator = &allocator_ ?
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalInfo_.device_;
    allocatorInfo.device         = device_;
    allocatorInfo.instance       = instance_;
    vmaCreateAllocator(&allocatorInfo, &allocator_);

    URHO3D_LOGDEBUGF("Initialize Vma !");
#endif

    URHO3D_LOGDEBUGF("CreateVulkanInstance !");

    return true;
}

bool GraphicsImpl::CreateWindowSurface(SDL_Window* window)
{
    if (!instance_ || !window)
    {
        URHO3D_LOGERRORF("Can't create SDL Surface for Vulkan : no instance or no window !");
        return false;
    }

    window_ = window;

    // create the surface
    if (SDL_Vulkan_CreateSurface(window_, (SDL_vulkanInstance)instance_, (SDL_vulkanSurface*)&surface_) == SDL_FALSE)
    {
        URHO3D_LOGERRORF("Can't create SDL Surface for Vulkan !");
        return false;
    }

    surfaceDirty_ = false;

    return true;
}

void GraphicsImpl::CleanUpVulkan()
{
    URHO3D_LOGDEBUGF("CleanUpVulkan ... ");

    if (instance_ == VK_NULL_HANDLE)
        return;

    CleanUpSwapChain();
    CleanUpRenderPasses();
    CleanUpPipelines();

#ifdef URHO3D_VMA
    if (allocator_ != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
#endif

    // TODO memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    if (pipelineCache_ != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(device_, pipelineCache_, pAllocator);
        pipelineCache_ = VK_NULL_HANDLE;
    }

    // clean the Semphore Pool
//    for (unsigned int i = 0; i < semaphorePool_.Size(); i++)
//        vkDestroySemaphore(device_, semaphorePool_[i], pAllocator);
//    semaphorePool_.Clear();

	vkDestroySemaphore(device_, presentComplete_, pAllocator);
	vkDestroySemaphore(device_, renderComplete_, pAllocator);

    vkDestroyCommandPool(device_, commandPool_, pAllocator);

    // destroy device, surface and instance
    vkDestroyDevice(device_, pAllocator);

    if (surface_ != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance_, surface_, pAllocator);
        surface_ = VK_NULL_HANDLE;
    }

    if (debugMsg_ != VK_NULL_HANDLE)
    {
        auto vkDestroyDebug = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (vkDestroyDebug != nullptr)
            vkDestroyDebug(instance_, debugMsg_, pAllocator);
        debugMsg_ = VK_NULL_HANDLE;
    }

    physicalInfo_.CleanUp();
    vkDestroyInstance(instance_, pAllocator);
    instance_ = VK_NULL_HANDLE;

    URHO3D_LOGDEBUGF("CleanUpVulkan !");
}

bool GraphicsImpl::CreateSwapChain(int width, int height, bool* srgb, bool* vsync, bool* triplebuffer)
{
    vkDeviceWaitIdle(device_);

    // TODO memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    URHO3D_LOGDEBUGF("Create swapchain w=%d h=%d ...", width, height);

	if (surfaceDirty_)
	{
		if (surface_ != VK_NULL_HANDLE)
        {
        	vkDestroySurfaceKHR(instance_, surface_, pAllocator);
			surface_ = VK_NULL_HANDLE;
        }
	}

	if (surface_ == VK_NULL_HANDLE)
	{
		URHO3D_LOGERRORF("CreateSwapChain ... no windows surface => create it !");
		// recreate the surface
		if (!CreateWindowSurface(window_))
			return false;
	}

    // find the srgb format
    int srgbformat = -1;
    for (unsigned int i = 0; i < physicalInfo_.surfaceFormats_.Size(); i++)
    {
        const VkSurfaceFormatKHR& availableFormat = physicalInfo_.surfaceFormats_[i];
        if ((availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB || availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB)
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            srgbformat = i;
            break;
        }
    }

    // find the unorm format
    int unormformat = -1;
    for (unsigned int i = 0; i < physicalInfo_.surfaceFormats_.Size(); i++)
    {
        const VkSurfaceFormatKHR& availableFormat = physicalInfo_.surfaceFormats_[i];
        if ((availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM))
        {
            unormformat = i;
            break;
        }
    }

    // if srgb specified, try to get the required format
    if (srgb)
        swapChainInfo_ = physicalInfo_.surfaceFormats_[*srgb == true && srgbformat != -1 ? srgbformat : unormformat != -1 ? unormformat : 0];
    // if not specified, in first use the unorm if exists (same behavior than OGL impl)
    else if (unormformat != -1)
        swapChainInfo_ = physicalInfo_.surfaceFormats_[unormformat];
    // else use the srgb if exists
#ifndef DISABLE_SRGB
    else if (srgbformat != -1)
        swapChainInfo_ = physicalInfo_.surfaceFormats_[srgbformat];
#endif
    // else get the first available format
    else
        swapChainInfo_ = physicalInfo_.surfaceFormats_[0];

    // set the present mode.
    if (vsync)
    {
        // default immediate mode : tearing
        presentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
        if (*vsync)
        {
            // pure vsync : high performance (desktop)
            if (physicalInfo_.presentModes_.Contains(VK_PRESENT_MODE_MAILBOX_KHR))
                presentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
            // pure vsync : medium performance (mobile)
            if (presentMode_ == VK_PRESENT_MODE_IMMEDIATE_KHR && physicalInfo_.presentModes_.Contains(VK_PRESENT_MODE_FIFO_KHR))
                presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
            // vsync controlled tearing
            if (presentMode_ == VK_PRESENT_MODE_IMMEDIATE_KHR && physicalInfo_.presentModes_.Contains(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
                presentMode_ = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }

        *vsync = (presentMode_ != VK_PRESENT_MODE_IMMEDIATE_KHR);
    }

    // update the device swapchain capabilities.
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalInfo_.device_, surface_, &physicalInfo_.surfaceCapabilities_);

    // set the swap surface extent if need.
    swapChainExtent_ = physicalInfo_.surfaceCapabilities_.currentExtent;
    if (swapChainExtent_.width != width && swapChainExtent_.height != height)
    {
//        swapChainExtent_.width  = Clamp((uint32_t)width, physicalInfo_.surfaceCapabilities_.minImageExtent.width, physicalInfo_.surfaceCapabilities_.maxImageExtent.width);
//        swapChainExtent_.height = Clamp((uint32_t)height, physicalInfo_.surfaceCapabilities_.minImageExtent.height, physicalInfo_.surfaceCapabilities_.maxImageExtent.height);
        if (width && height)
        {
            swapChainExtent_.width  = (uint32_t)width;
            swapChainExtent_.height = (uint32_t)height;
        }
        else
        {
            swapChainExtent_.width  = Clamp(swapChainExtent_.width, physicalInfo_.surfaceCapabilities_.minImageExtent.width, physicalInfo_.surfaceCapabilities_.maxImageExtent.width);
            swapChainExtent_.height = Clamp(swapChainExtent_.height, physicalInfo_.surfaceCapabilities_.minImageExtent.height, physicalInfo_.surfaceCapabilities_.maxImageExtent.height);
        }
    }

    // set the required images in the swap chain.
    unsigned int numimages = numFrames_;
    if (triplebuffer)
        numimages = *triplebuffer == true ? 3 : vsync && *vsync == true ? 2 : 1;

    numimages = Clamp(numimages, (unsigned int)physicalInfo_.surfaceCapabilities_.minImageCount,
                      (unsigned int)physicalInfo_.surfaceCapabilities_.maxImageCount);

    URHO3D_LOGDEBUGF("Create swapchain numimages=%u (min=%u max=%u) required=%ux%u capabilities=%ux%u => %ux%u srgb=%s surfaceFormat=%u colorSpace=%u ...",
                     numimages, physicalInfo_.surfaceCapabilities_.minImageCount, physicalInfo_.surfaceCapabilities_.maxImageCount,
                     width, height, physicalInfo_.surfaceCapabilities_.maxImageExtent.width, physicalInfo_.surfaceCapabilities_.maxImageExtent.height,
                     swapChainExtent_.width, swapChainExtent_.height, srgb && (*srgb == true) ? "true":"false", swapChainInfo_.format, swapChainInfo_.colorSpace);

    // set the queue sharing mode.
    VkSharingMode sharingmode = VK_SHARING_MODE_EXCLUSIVE;
    unsigned int queuecount = 1;
    const uint32_t* pqueues = nullptr;
    if (physicalInfo_.grQueueIndex_ != physicalInfo_.prQueueIndex_)
    {
        sharingmode = VK_SHARING_MODE_CONCURRENT;
        queuecount  = 2;
        pqueues     = physicalInfo_.queueIndexes_.Buffer();
    }

    VkSurfaceTransformFlagBitsKHR transform    = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;//physicalInfo_.surfaceCapabilities_.currentTransform;
    VkCompositeAlphaFlagBitsKHR compositeAlpha = (VkCompositeAlphaFlagBitsKHR)physicalInfo_.surfaceCapabilities_.supportedCompositeAlpha;

    // create the swap chain.
    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface               = surface_;
    createInfo.minImageCount         = numimages;
    createInfo.imageFormat           = swapChainInfo_.format;
    createInfo.imageColorSpace       = swapChainInfo_.colorSpace;
    createInfo.imageExtent           = swapChainExtent_;
    createInfo.imageArrayLayers      = 1;
    createInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode      = sharingmode;
    createInfo.queueFamilyIndexCount = queuecount;
    createInfo.pQueueFamilyIndices   = pqueues;
    createInfo.preTransform          = transform;
    createInfo.compositeAlpha        = compositeAlpha;//VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;//VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode           = presentMode_;
    createInfo.clipped               = VK_TRUE;
    createInfo.oldSwapchain          = swapChain_;

    if (vkCreateSwapchainKHR(device_, &createInfo, pAllocator, &swapChain_) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create the Swap Chain !");
        return false;
    }

    // get the images in the new swap chain.
    if (vkGetSwapchainImagesKHR(device_, swapChain_, &numimages, 0) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't get swapchain numimages !");
        return false;
    }

    PODVector<VkImage> swapChainImages;
    swapChainImages.Resize(numimages);
    if (vkGetSwapchainImagesKHR(device_, swapChain_, &numimages, swapChainImages.Buffer()) != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't get swapchain images !");
        return false;
    }

    // set the number of frames of the swap chain.
    numFrames_ = numimages;
    if (triplebuffer)
        *triplebuffer = (numFrames_ >= 3);

    // create the synchronization objects for each frame in the swap chain.
    frames_.Resize(numFrames_);
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (unsigned int i = 0; i < numFrames_; i++)
    {
        FrameData& frame = frames_[i];
        frame.id_ = i;
        frame.commandBufferBegun_ = false;
        frame.textureDirty_ = true;
        frame.renderPassIndex_ = -1;

        // create the submit fence
        VkResult result = vkCreateFence(device_, &fenceInfo, pAllocator, &frame.submitSync_);
        if (result != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create submit fence !");
            return false;
        }

        // clean the semaphore handles
//        frame.acquireSync_ = VK_NULL_HANDLE;
//        frame.releaseSync_ = VK_NULL_HANDLE;
    }

    // create the command pools and buffers for each frame in the swap chain.
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
//    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = physicalInfo_.grQueueIndex_;
    VkCommandBufferAllocateInfo bufferInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferInfo.commandBufferCount = 1;
    for (unsigned int i = 0; i < numFrames_; i++)
    {
        FrameData& frame = frames_[i];
        if (vkCreateCommandPool(device_, &poolInfo, pAllocator, &frame.commandPool_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create command pool !");
            return false;
        }
        bufferInfo.commandPool = frame.commandPool_;
        if (vkAllocateCommandBuffers(device_, &bufferInfo, &frame.commandBuffer_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't allocate command buffer !");
            return false;
        }
    }

    // create the frame datas needed for each frame in the swap chain.
    for (unsigned int i = 0; i < numFrames_; i++)
    {
        FrameData& frame = frames_[i];
        frame.image_ = swapChainImages[i];

        // create the swap image view.
        VkImageViewCreateInfo createInfo{};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image                           = frame.image_;
        createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format                          = swapChainInfo_.format;
        createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device_, &createInfo, pAllocator, &frame.imageView_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create the Swap Chain Image Views !");
            return false;
        }
    }

    // set defaults Viewport and Scissor
    viewport_.x        = 0.f;
    viewport_.y        = 0.f;
    viewport_.width    = (float)swapChainExtent_.width;
    viewport_.height   = (float)swapChainExtent_.height;
    viewport_.minDepth = 0.f;
    viewport_.maxDepth = 1.f;
    screenScissor_.offset    = { 0, 0 };
    screenScissor_.extent    = swapChainExtent_;

    swapChainDirty_ = false;

    URHO3D_LOGDEBUGF("Create swapchain ew=%d eh=%d presentmode=%u numframes=%u !", swapChainExtent_.width, swapChainExtent_.height, presentMode_, numFrames_);

    return true;
}

void GraphicsImpl::CleanUpRenderPasses()
{
    // TODO : memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    renderPassInfo_ = nullptr;

    // Destroy RenderPasses
    for (HashMap<unsigned, RenderPassInfo>::Iterator it = renderPassInfos_.Begin(); it != renderPassInfos_.End(); ++it)
    {
        RenderPassInfo& renderPassInfo = it->second_;

        if (renderPassInfo.renderPass_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, renderPassInfo.renderPass_, pAllocator);
            renderPassInfo.renderPass_ = VK_NULL_HANDLE;
        }
    }
}

void GraphicsImpl::CleanUpPipelines()
{
    // TODO : memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    // Destroy Pipelines Datas
    for (HashMap<StringHash, PipelineInfo>::Iterator it = pipelinesInfos_.Begin(); it != pipelinesInfos_.End(); ++it)
    {
        PipelineInfo& info = it->second_;

        if (info.pipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device_, info.pipeline_, pAllocator);
            info.pipeline_ = VK_NULL_HANDLE;
        }
        if (info.pipelineLayout_ != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device_, info.pipelineLayout_, pAllocator);
            info.pipelineLayout_ = VK_NULL_HANDLE;
        }
        for (Vector<DescriptorsGroup>::Iterator group = info.descriptorsGroups_.Begin(); group != info.descriptorsGroups_.End(); ++group)
        {
            if (group->layout_ != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(device_, group->layout_, pAllocator);
                group->layout_ = VK_NULL_HANDLE;
            }

            for (Vector<DescriptorsGroupAllocation>::Iterator alloc = group->setsByFrame_.Begin(); alloc != group->setsByFrame_.End(); ++alloc)
            {
                if (alloc->pool_ != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(device_, alloc->pool_, pAllocator);
                    alloc->pool_ = VK_NULL_HANDLE;
                }
                alloc->sets_.Clear();
            }
        }
    }
}

void GraphicsImpl::CleanUpRenderAttachments()
{
    // TODO : memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    for (Vector<RenderAttachment>::Iterator it = renderAttachments_.Begin(); it != renderAttachments_.End(); ++it)
        DestroyAttachment(*it);

    for (Vector<FrameData>::Iterator it = frames_.Begin(); it != frames_.End(); ++it)
        for (Vector<VkFramebuffer>::Iterator jt = it->framebuffers_.Begin(); jt != it->framebuffers_.End(); ++jt)
        {
            vkDestroyFramebuffer(device_, *jt, pAllocator);
            *jt = VK_NULL_HANDLE;
        }

    viewportInfos_.Clear();
    viewportSizes_.Clear();
    viewportTexture_ = 0;
}

void GraphicsImpl::CleanUpSwapChain()
{
    URHO3D_LOGDEBUGF("CleanUpSwapChain ... ");

    swapChainDirty_ = true;

    vkDeviceWaitIdle(device_);

    // TODO : memoryAllocator
    const VkAllocationCallbacks* pAllocator = nullptr;

    CleanUpRenderAttachments();

    for (unsigned int i = 0; i < frames_.Size(); i++)
    {
        FrameData& frame = frames_[i];

        if (frame.submitSync_ != VK_NULL_HANDLE)
        {
            vkWaitForFences(device_, 1, &frame.submitSync_, true, TIME_OUT);
            vkDestroyFence(device_, frame.submitSync_, pAllocator);
        }

//        // restore to semaphore pool
//        if (frame.acquireSync_  != VK_NULL_HANDLE)
//            semaphorePool_.Push(frame.acquireSync_);
//
//        // restore to semaphore pool
//        if (frame.releaseSync_  != VK_NULL_HANDLE)
//            semaphorePool_.Push(frame.releaseSync_);

        if (frame.commandPool_ != VK_NULL_HANDLE)
        {
            if (frame.commandBuffer_ != VK_NULL_HANDLE)
                vkFreeCommandBuffers(device_, frame.commandPool_, 1, &frame.commandBuffer_);
            vkDestroyCommandPool(device_, frame.commandPool_, pAllocator);
        }

//        if (frame.frameBuffer_ != VK_NULL_HANDLE)
//            vkDestroyFramebuffer(device_, frame.frameBuffer_, pAllocator);

        if (frame.imageView_ != VK_NULL_HANDLE)
            vkDestroyImageView(device_, frame.imageView_, pAllocator);

        frame.submitSync_    = VK_NULL_HANDLE;
//        frame.acquireSync_   = VK_NULL_HANDLE;
//        frame.releaseSync_   = VK_NULL_HANDLE;
        frame.commandPool_   = VK_NULL_HANDLE;
        frame.commandBuffer_ = VK_NULL_HANDLE;
//        frame.frameBuffer_   = VK_NULL_HANDLE;
        frame.imageView_     = VK_NULL_HANDLE;
        frame.image_         = VK_NULL_HANDLE;
        frame.textureDirty_  = true;

        frame.lastPipelineBound_  = VK_NULL_HANDLE;
        frame.lastPipelineInfoBound_ = nullptr;
    }

    if (swapChain_ != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device_, swapChain_, pAllocator);
        swapChain_ = VK_NULL_HANDLE;
    }

    swapChainDirty_     = true;
    scissorDirty_       = true;
    vertexBuffersDirty_ = true;
    pipelineDirty_      = true;

    URHO3D_LOGDEBUGF("CleanUpSwapChain !");
}

void GraphicsImpl::UpdateSwapChain(int width, int height, bool* srgb, bool* vsync, bool* triplebuffer)
{
    if (!width || !height)
        SDL_Vulkan_GetDrawableSize(window_, &width, &height);

    URHO3D_LOGDEBUGF("UpdateSwapChain ... w=%d h=%d", width, height);

    CleanUpPipelines();

    CleanUpRenderPasses();

    CleanUpSwapChain();

    if (CreateSwapChain(width, height, srgb, vsync, triplebuffer))
    {
        if (CreateRenderPaths())
        {
            CreateRenderAttachments();

            CreatePipelines();

            URHO3D_LOGDEBUGF("UpdateSwapChain !");
        }
    }
}

// Render Pass

unsigned GetKey(RenderPath* renderpath)
{
    // TODO : extract RenderPath unique key from RenderPathCommands Contents instead of a memory address.
    String str;
    return str.AppendWithFormat("%u", renderpath).ToHash();
}

// built-in renderpathinfos
const unsigned GraphicsImpl::ClearPass_1C        = StringHash("CLEAR_1C").Value();
const unsigned GraphicsImpl::RenderPass_1C_1DS_1 = StringHash("RENDER_1C_1DS_1").Value();
const unsigned GraphicsImpl::RenderPass_1C_1DS_2 = StringHash("RENDER_1C_1DS_2").Value();
const unsigned GraphicsImpl::PresentPass_1C      = StringHash("PRESENT_1C").Value();

void GraphicsImpl::AddRenderPassInfo(const String& attachmentconfig)
{
    unsigned passkey = StringHash(attachmentconfig).Value();

    if (!renderPassInfos_.Contains(passkey))
    {
        RenderPassInfo& renderPassInfo = renderPassInfos_[passkey];
        renderPassInfo.id_  = renderPassInfos_.Size()-1;
        renderPassInfo.key_ = passkey;
    }
}

// TEST ONLY
void GraphicsImpl::SetRenderPath(RenderPath* renderPath)
{
    URHO3D_LOGDEBUGF("GraphicsImpl() - SetRenderPath ...");

    RenderPathData* renderPathData = 0;

    unsigned key = GetKey(renderPath);

    HashMap<unsigned, RenderPathData>::Iterator it = renderPathDatas_.Find(key);
    if (it == renderPathDatas_.End())
    {
        // TODO : read renderpath configuration
        // TODO : and convert to pass/subpasses descriptions

        // This is an hardcorded vulkan version of "ForwardUrho2D.Xml"
        renderPathData = &renderPathDatas_[key];
        renderPathData->renderPath_ = renderPath;
        renderPathData->passInfos_.Resize(4);

        int pass = 0;
        // Render Pass 0 : clear swap image
        {
            RenderPassInfo& renderPassInfo = renderPassInfos_[ClearPass_1C];
            renderPathData->passInfos_[pass] = &renderPassInfo;

            renderPassInfo.type_ = PASS_CLEAR;
            renderPassInfo.key_  = ClearPass_1C;
            renderPassInfo.attachments_.Resize(1);
            renderPassInfo.attachments_[0].slot_  = RENDERSLOT_PRESENT;
            renderPassInfo.attachments_[0].clear_ = true;
            renderPassInfo.subpasses_.Resize(1);
            renderPassInfo.subpasses_[0].colors_.Resize(1);
            renderPassInfo.subpasses_[0].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[0].colors_[0].layout     = VK_IMAGE_LAYOUT_UNDEFINED;

            renderPathData->renderPathCommandIndexToRenderPassIndexes_[Technique::GetPassIndex("clear")] = Pair<unsigned,unsigned>(pass, 0);
        }
        pass++;
        // Render Pass 1 : Alpha
        {
            RenderPassInfo& renderPassInfo = renderPassInfos_[RenderPass_1C_1DS_1];
            renderPathData->passInfos_[pass] = &renderPassInfo;

            renderPassInfo.type_ = PASS_VIEW;
            renderPassInfo.key_  = RenderPass_1C_1DS_1;
            renderPassInfo.attachments_.Resize(2);
            renderPassInfo.attachments_[0].slot_  = RENDERSLOT_TARGET1;
            renderPassInfo.attachments_[0].clear_ = true;
            renderPassInfo.attachments_[1].slot_  = RENDERSLOT_DEPTH;
            renderPassInfo.attachments_[1].clear_ = true;
            renderPassInfo.subpasses_.Resize(2);
            // clear subpass
            renderPassInfo.subpasses_[0].colors_.Resize(1);
            renderPassInfo.subpasses_[0].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[0].colors_[0].layout     = VK_IMAGE_LAYOUT_UNDEFINED;
            renderPassInfo.subpasses_[0].depths_.Resize(1);
            renderPassInfo.subpasses_[0].depths_[0].attachment = 1;
            renderPassInfo.subpasses_[0].depths_[0].layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            // alpha subpass
            renderPassInfo.subpasses_[1].colors_.Resize(1);
            renderPassInfo.subpasses_[1].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[1].colors_[0].layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            renderPassInfo.subpasses_[1].depths_.Resize(1);
            renderPassInfo.subpasses_[1].depths_[0].attachment = 1;
            renderPassInfo.subpasses_[1].depths_[0].layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            renderPathData->renderPathCommandIndexToRenderPassIndexes_[Technique::GetPassIndex("alpha")] = Pair<unsigned,unsigned>(pass, 1);
        }
        pass++;
        // Render Pass 2 : Refraction to Dialog
        {
            RenderPassInfo& renderPassInfo = renderPassInfos_[RenderPass_1C_1DS_2];
            renderPathData->passInfos_[pass] = &renderPassInfo;

            renderPassInfo.type_ = PASS_VIEW;
            renderPassInfo.key_  = RenderPass_1C_1DS_2;
            renderPassInfo.attachments_.Resize(2);
            renderPassInfo.attachments_[0].slot_  = RENDERSLOT_TARGET2;
            renderPassInfo.attachments_[0].clear_ = true;
            renderPassInfo.attachments_[1].slot_  = RENDERSLOT_DEPTH;
            renderPassInfo.attachments_[1].clear_ = false;
            renderPassInfo.subpasses_.Resize(2);
            // clear subpass
            renderPassInfo.subpasses_[0].colors_.Resize(1);
            renderPassInfo.subpasses_[0].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[0].colors_[0].layout     = VK_IMAGE_LAYOUT_UNDEFINED;
            // refract subpass
            renderPassInfo.subpasses_[1].colors_.Resize(1);
            renderPassInfo.subpasses_[1].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[1].colors_[0].layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            renderPassInfo.subpasses_[1].depths_.Resize(1);
            renderPassInfo.subpasses_[1].depths_[0].attachment = 1;
            renderPassInfo.subpasses_[1].depths_[0].layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            renderPathData->renderPathCommandIndexToRenderPassIndexes_[Technique::GetPassIndex("water")] = Pair<unsigned,unsigned>(pass, 1);
        }
        pass++;
        // Render Pass 3 : Copy each RenderTarget's view on each viewport and draw UI
        {
            RenderPassInfo& renderPassInfo = renderPassInfos_[PresentPass_1C];
            renderPathData->passInfos_[pass] = &renderPassInfo;

            renderPassInfo.type_ = PASS_PRESENT;
            renderPassInfo.key_  = PresentPass_1C;
            renderPassInfo.attachments_.Resize(1);
            renderPassInfo.attachments_[0].slot_  = RENDERSLOT_PRESENT;
            renderPassInfo.attachments_[0].clear_ = false;
            renderPassInfo.subpasses_.Resize(1);
            renderPassInfo.subpasses_[0].colors_.Resize(1);
            renderPassInfo.subpasses_[0].colors_[0].attachment = 0;
            renderPassInfo.subpasses_[0].colors_[0].layout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            renderPathData->renderPathCommandIndexToRenderPassIndexes_[Technique::GetPassIndex("present")] = Pair<unsigned,unsigned>(pass, 0);
        }
    }
    else
    {
        URHO3D_LOGWARNINGF("GraphicsImpl() - SetRenderPath : renderPath=%u use already registered renderpathinfo !");
        renderPathData = &it->second_;
    }

    renderPathData_ = renderPathData;
}

void GraphicsImpl::SetRenderPass(unsigned commandpassindex)
{
    if (renderPathData_)
    {
#ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("GraphicsImpl() - SetRenderPass : commandpassindex=%u ...", commandpassindex);
#endif
        HashMap<unsigned, Pair<unsigned, unsigned > >::ConstIterator it = renderPathData_->renderPathCommandIndexToRenderPassIndexes_.Find(commandpassindex);
        if (it != renderPathData_->renderPathCommandIndexToRenderPassIndexes_.End())
        {
            unsigned renderpassIndex = it->second_.first_;
            unsigned subpassIndex    = it->second_.second_;

#ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("GraphicsImpl() - SetRenderPass : commandpassindex=%u renderpassIndex=%u subpassIndex=%u",
                             commandpassindex, renderpassIndex, subpassIndex);
#endif
            if (renderPassIndex_ != renderpassIndex || subpassIndex_ != subpassIndex)
            {
                viewportTexture_ = 0;

                // TODO : we need to change the viewporttexture between subpasses too.
                // example : subpass "refract" needs texture rendered in the previous subpass "alpha".
                if (frame_ && renderpassIndex > 0)
                {
                    // take the rendertarget texture generated at the previous pass.
                    const int viewSizeIndex = viewportIndex_ != -1 ? viewportInfos_[viewportIndex_].viewSizeIndex_ : 0;
                    const Vector<RenderPassAttachmentInfo>& attachments = renderPathData_->passInfos_[renderPassIndex_]->attachments_;
                    for (unsigned i = 0; i < attachments.Size(); i++)
                    {
                        if (attachments[i].slot_ > RENDERSLOT_PRESENT && attachments[i].slot_ < RENDERSLOT_DEPTH)
                        {
                            viewportTexture_ = renderAttachments_[viewSizeIndex * MAX_RENDERSLOTS + attachments[i].slot_].texture_;
                            break;
                        }
                    }
                }

                renderPassIndex_ = renderpassIndex;
                subpassIndex_    = subpassIndex;
                renderPassInfo_  = renderPathData_->passInfos_[renderPassIndex_];
            }
        }
    }
}

Texture2D* GraphicsImpl::GetCurrentViewportTexture() const
{
    return viewportTexture_;
}

const RenderPassInfo* GraphicsImpl::GetRenderPassInfo(unsigned renderPassKey) const
{
    HashMap<unsigned, RenderPassInfo>::ConstIterator it = renderPassInfos_.Find(renderPassKey);
    return it != renderPassInfos_.End() ? &it->second_ : nullptr;
}

void GraphicsImpl::CreateImageAttachment(int slot, RenderAttachment& attachment, unsigned width, unsigned height)
{
    URHO3D_LOGINFOF("CreateImageAttachment slot=%s(%d) !", RenderSlotTypeStr[slot], slot);

    attachment.slot_ = slot;

    if (slot > RENDERSLOT_PRESENT)
    {
        // TODO : memoryAllocator
        const VkAllocationCallbacks* pAllocator = nullptr;

        // create image
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        if (attachment.slot_ == RENDERSLOT_DEPTH)
            imageInfo.format    = depthStencilFormat_;
        else
            imageInfo.format    = swapChainInfo_.format;
        imageInfo.extent.width  = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (attachment.slot_ == RENDERSLOT_DEPTH)
            imageInfo.usage     = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else
            imageInfo.usage     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags         = 0;

    #ifndef URHO3D_VMA
        if (vkCreateImage(device_, &imageInfo, pAllocator, &attachment.image_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create image !");
            return false;
        }

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(device_, attachment.image_, &memRequirements);
        VkMemoryAllocateInfo memoryInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memoryInfo.allocationSize = memRequirements.size;
        uint32_t memorytypeindex;
        if (!physicalInfo_.GetMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memorytypeindex))
        {
            URHO3D_LOGERRORF("Can't get device memory type !");
            return false;
        }
        memoryInfo.memoryTypeIndex = memorytypeindex;
        if (vkAllocateMemory(device_, &memoryInfo, nullptr, &attachment.memory_) != VK_SUCCESS ||
            vkBindImageMemory(device_, attachment.image_, attachment.memory_, 0) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't allocate/bind device memory !");
            return false;
        }
    #else
        VmaAllocationCreateInfo allocationInfo {};
        allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &attachment.image_, &attachment.memory_, nullptr) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create image !");
            return;
        }
    #endif

        // create image view
        VkImageViewCreateInfo imageViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = attachment.image_;
        if (attachment.slot_ == RENDERSLOT_DEPTH)
            imageViewInfo.format                      = depthStencilFormat_;
        else
            imageViewInfo.format                      = swapChainInfo_.format;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;
        if (attachment.slot_ == RENDERSLOT_DEPTH)
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        else
            imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        if (vkCreateImageView(device_, &imageViewInfo, pAllocator, &attachment.imageView_) != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Can't create image view !");
            return;
        }
    }

    if (attachment.slot_ < RENDERSLOT_DEPTH)
    {
        attachment.texture_ = SharedPtr<Texture2D>(new Texture2D(context_));
        attachment.texture_->SetName("viewport");
        attachment.texture_->SetGPUObject(attachment.image_, attachment.memory_);
        attachment.texture_->SetShaderResourceView(attachment.imageView_);
    }
}

void GraphicsImpl::DestroyAttachment(RenderAttachment& attachment)
{
    if (attachment.slot_ == RENDERSLOT_NONE)
        return;

    URHO3D_LOGINFOF("DestroyAttachment slot=%s(%d) !", RenderSlotTypeStr[attachment.slot_], attachment.slot_);

    if (attachment.slot_ > RENDERSLOT_PRESENT)
    {
        // TODO : memoryAllocator
        const VkAllocationCallbacks* pAllocator = nullptr;

        if (attachment.imageView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_, attachment.imageView_, pAllocator);
            attachment.imageView_ = VK_NULL_HANDLE;
        }
    #ifndef URHO3D_VMA
        if (attachment.image_ != VK_NULL_HANDLE)
        {
            vkDestroyImage(device_, attachment.image_, pAllocator);
            attachment.image_ = VK_NULL_HANDLE;
        }
        if (attachment.memory_ != VK_NULL_HANDLE)
        {
            vkFreeMemory(device_, attachment.memory_, pAllocator);
            attachment.memory_ = VK_NULL_HANDLE;
        }
    #else
        if (attachment.image_ != VK_NULL_HANDLE)
        {
            vmaDestroyImage(allocator_, attachment.image_, attachment.memory_);
            attachment.image_ = VK_NULL_HANDLE;
            attachment.memory_ = VK_NULL_HANDLE;
        }
    #endif
    }

    if (attachment.texture_)
    {
        attachment.texture_->SetGPUObject(0, 0);
        attachment.texture_->SetShaderResourceView(0);
        attachment.texture_.Reset();
    }

    attachment.slot_ = RENDERSLOT_NONE;
}

bool GraphicsImpl::CreateRenderPaths()
{
    for (HashMap<unsigned, RenderPathData>::Iterator it = renderPathDatas_.Begin(); it != renderPathDatas_.End(); ++it)
    {
        if (!CreateRenderPasses(it->second_))
            return false;
    }

    return true;
}

bool GraphicsImpl::CreateRenderPasses(RenderPathData& renderPathData)
{
    const VkAllocationCallbacks* pAllocator = nullptr;

    // Create Render Pass
    for (unsigned passindex = 0; passindex < renderPathData.passInfos_.Size(); passindex++)
    {
        RenderPassInfo& renderPassInfo = *renderPathData.passInfos_[passindex];
        if (renderPassInfo.renderPass_ != VK_NULL_HANDLE)
            continue;

        // Set Attachments Descriptions & References
        PODVector<VkAttachmentDescription> attachmentDescriptions;
        attachmentDescriptions.Resize(renderPassInfo.attachments_.Size());
        renderPassInfo.clearValues_.Resize(renderPassInfo.attachments_.Size());
        for (unsigned i = 0; i < renderPassInfo.attachments_.Size(); i++)
        {
            const RenderPassAttachmentInfo& attachmentInfo = renderPassInfo.attachments_[i];
            VkAttachmentDescription& desc = attachmentDescriptions[i];
            desc.flags          = 0;
            desc.samples        = VK_SAMPLE_COUNT_1_BIT;
            desc.loadOp         = attachmentInfo.clear_ ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            desc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            // Color Attachment
            if (attachmentInfo.slot_ < RENDERSLOT_DEPTH)
            {
                desc.format        = swapChainInfo_.format;
                desc.initialLayout = attachmentInfo.clear_ ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                desc.finalLayout   = attachmentInfo.slot_ == RENDERSLOT_PRESENT && renderPassInfo.type_ == PASS_PRESENT ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                if (attachmentInfo.clear_)
                    renderPassInfo.clearValues_[i].color = {0.f, 0.f, 0.f, 1.f};
                else
                    desc.initialLayout = desc.finalLayout;
            }
            // Depth Attachment
            else
            {
                desc.format        = depthStencilFormat_;
                desc.initialLayout = attachmentInfo.clear_ ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                desc.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                if (attachmentInfo.clear_)
                    renderPassInfo.clearValues_[i].depthStencil = {1.f, 1U};
            }
        }
        // Subpasses Descriptions
        PODVector<VkSubpassDescription> subpassDescriptions;
        subpassDescriptions.Resize(renderPassInfo.subpasses_.Size());
        for (unsigned i = 0; i < subpassDescriptions.Size(); i++)
        {
            VkSubpassDescription& subpassDescription   = subpassDescriptions[i];
            subpassDescription.flags                   = 0;
            subpassDescription.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescription.colorAttachmentCount    = renderPassInfo.subpasses_[i].colors_.Size();
            subpassDescription.pColorAttachments       = renderPassInfo.subpasses_[i].colors_.Size() ? &renderPassInfo.subpasses_[i].colors_[0] : nullptr;
            subpassDescription.pDepthStencilAttachment = renderPassInfo.subpasses_[i].depths_.Size() ? &renderPassInfo.subpasses_[i].depths_[0] : nullptr;
            subpassDescription.inputAttachmentCount    = renderPassInfo.subpasses_[i].inputs_.Size();
            subpassDescription.pInputAttachments       = renderPassInfo.subpasses_[i].inputs_.Size() ? &renderPassInfo.subpasses_[i].inputs_[0] : nullptr;
            subpassDescription.preserveAttachmentCount = 0;
            subpassDescription.pPreserveAttachments    = nullptr;
            subpassDescription.pResolveAttachments     = nullptr;
        }
        // Dependencies for layout transitions
        PODVector<VkSubpassDependency> dependencies;
        dependencies.Resize(2 + subpassDescriptions.Size() - 1);
        // First dependency at the start of the renderpass
        // Does the transition from final to initial layout
        dependencies.Front().srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies.Front().dstSubpass      = 0;
        dependencies.Front().srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies.Front().dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.Front().srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependencies.Front().dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.Front().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        // Dependencies between subpasses
        if (subpassDescriptions.Size() > 1)
            for (unsigned i = 1; i < subpassDescriptions.Size(); i++)
            {
                dependencies[i].srcSubpass      = i - 1;
                dependencies[i].dstSubpass      = i;
                dependencies[i].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dependencies[i].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[i].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
                dependencies[i].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            }
        // Last dependency at the end of the renderpass
        // Does the transition from the initial to the final layout
        dependencies.Back().srcSubpass      = subpassDescriptions.Size() - 1;
        dependencies.Back().dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies.Back().srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies.Back().dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies.Back().srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies.Back().dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependencies.Back().dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        // Pass Creation
        VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpInfo.attachmentCount        = attachmentDescriptions.Size();
        rpInfo.pAttachments           = &attachmentDescriptions[0];
        rpInfo.subpassCount           = subpassDescriptions.Size();
        rpInfo.pSubpasses             = &subpassDescriptions[0];
        rpInfo.dependencyCount        = dependencies.Size();
        rpInfo.pDependencies          = dependencies.Size() ? &dependencies[0] : nullptr;
        bool ok = vkCreateRenderPass(device_, &rpInfo, pAllocator, &renderPassInfo.renderPass_) == VK_SUCCESS;
        if (!ok || renderPassInfo.renderPass_ == VK_NULL_HANDLE)
        {
            URHO3D_LOGERRORF("Can't create renderPathData=%u renderpass passindex=%u !", &renderPathData, passindex);
            continue;
        }

        URHO3D_LOGDEBUGF("GraphicsImpl() - Create Render Passes for renderPathData=%u .. passindex=%u passid=%d passkey=%d passtype=%s(%d) numsubpasses=%u ... OK !",
            &renderPathData, passindex, renderPassInfo.id_, renderPassInfo.key_, RenderPassTypeStr[renderPassInfo.type_], renderPassInfo.type_, renderPassInfo.subpasses_.Size());
    }

    return true;
}

bool GraphicsImpl::CreateRenderAttachments()
{
    SetViewportInfos();

    // Create RenderTarget Buffers
    renderAttachments_.Resize(MAX_RENDERSLOTS * viewportSizes_.Size());
    for (unsigned viewSizeIndex = 0; viewSizeIndex < viewportSizes_.Size(); viewSizeIndex++)
    {
        for (unsigned slot = 0; slot < MAX_RENDERSLOTS; slot++)
        {
            RenderAttachment& attachment = renderAttachments_[viewSizeIndex * MAX_RENDERSLOTS + slot];
            if (slot > RENDERSLOT_PRESENT && slot < MAX_RENDERSLOTS && attachment.slot_ == RENDERSLOT_NONE)
                CreateImageAttachment(slot, attachment, viewportSizes_[viewSizeIndex].x_, viewportSizes_[viewSizeIndex].y_);
            else
                URHO3D_LOGINFOF("attachment slot=%s(%d) viewSizeIndex=%u w=%u h=%u ... already created", RenderSlotTypeStr[slot], slot, viewSizeIndex, viewportSizes_[viewSizeIndex].x_, viewportSizes_[viewSizeIndex].y_);
        }
    }

    // Create FrameBuffers for each frame, renderpass and viewportsize
    const VkAllocationCallbacks* pAllocator = nullptr;
    PODVector<VkImageView> framebufferAttachments;
    for (unsigned frameindex = 0; frameindex < numFrames_; frameindex++)
    {
        FrameData& frame = frames_[frameindex];

        // if viewportSizes_.Size() changes, we just need to add new VkFramebuffers
        // if renderPassInfos_.Size() changes, all VkFramebuffers must be already cleared

        if (frame.framebuffers_.Size() != renderPassInfos_.Size() * viewportSizes_.Size())
        {
            unsigned prevSize = frame.framebuffers_.Size();
            frame.framebuffers_.Resize(renderPassInfos_.Size() * viewportSizes_.Size());
            for (unsigned i = prevSize; i < frame.framebuffers_.Size(); i++)
                frame.framebuffers_[i] = VK_NULL_HANDLE;
        }

        for (unsigned viewSizeIndex = 0; viewSizeIndex < viewportSizes_.Size(); viewSizeIndex++)
        {
            for (HashMap<unsigned, RenderPassInfo>::Iterator it = renderPassInfos_.Begin(); it != renderPassInfos_.End(); ++it)
            {
                RenderPassInfo& renderPassInfo = it->second_;
                const unsigned fbindex = viewSizeIndex * renderPassInfos_.Size() + renderPassInfo.id_;
                VkFramebuffer& framebuffer = frame.framebuffers_[fbindex];

                if (framebuffer == VK_NULL_HANDLE)
                {
                    framebufferAttachments.Resize(renderPassInfo.attachments_.Size());

                    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                    framebufferInfo.renderPass      = renderPassInfo.renderPass_;
                    framebufferInfo.attachmentCount = renderPassInfo.attachments_.Size();
                    framebufferInfo.pAttachments    = &framebufferAttachments[0];
                    framebufferInfo.layers          = 1;
                    framebufferInfo.width           = viewportSizes_[viewSizeIndex].x_;
                    framebufferInfo.height          = viewportSizes_[viewSizeIndex].y_;

                    URHO3D_LOGINFOF("framebuffer frame=%u fbindex=%u viewSizeIndex=%u renderpass=%d w=%u h=%u ... ", frameindex, fbindex, viewSizeIndex, renderPassInfo.id_,
                                    framebufferInfo.width, framebufferInfo.height);

                    for (unsigned k = 0; k < renderPassInfo.attachments_.Size(); k++)
                    {
                        int slot = renderPassInfo.attachments_[k].slot_;
                        if (slot == RENDERSLOT_PRESENT)
                            framebufferAttachments[k] = frame.imageView_;
                        else
                            framebufferAttachments[k] = slot != RENDERSLOT_NONE ? renderAttachments_[viewSizeIndex * MAX_RENDERSLOTS + slot].imageView_ : VK_NULL_HANDLE;

                        URHO3D_LOGINFOF(" ... add attachement %u : slot=%s(%d) imageview=%u", k, RenderSlotTypeStr[slot], slot, framebufferAttachments[k]);
                    }

                    if (vkCreateFramebuffer(device_, &framebufferInfo, pAllocator, &framebuffer) != VK_SUCCESS)
                    {
                        URHO3D_LOGERRORF("Can't create framebuffer !");
                        return false;
                    }
                }
            }
        }
    }

    return true;
}


// Viewports

void GraphicsImpl::SetViewport(int index, const IntRect& rect)
{
    if (index >= (int)MAX_SHADER_VIEWPORTS)
        index = -1;

    viewportChanged_ = viewportIndex_ != index;

    Renderer* renderer = context_->GetSubsystem<Renderer>();
    bool dirty = renderer ? renderer->GetNumViewports() != viewportInfos_.Size() : false;
#ifdef ACTIVE_FRAMELOGDEBUG
    if (dirty)
        URHO3D_LOGINFOF("GraphicsImpl() - SetViewport : index=%d numviewports changed %u -> %u !", index, viewportInfos_.Size(), renderer->GetNumViewports());
#endif
    if (!dirty)
    {
        if (index != -1)
        {
            dirty = (viewportInfos_[index].rect_.offset.x != rect.left_ || viewportInfos_[index].rect_.extent.width  != rect.Width() ||
                     viewportInfos_[index].rect_.offset.y != rect.top_  || viewportInfos_[index].rect_.extent.height != rect.Height());
        #ifdef ACTIVE_FRAMELOGDEBUG
            if (dirty)
                URHO3D_LOGINFOF("GraphicsImpl() - SetViewport : index=%d viewrect changed %d,%d,%u,%u -> %d,%d,%u,%u !", index,
                                viewportInfos_[index].rect_.offset.x, viewportInfos_[index].rect_.offset.y,
                                viewportInfos_[index].rect_.extent.width, viewportInfos_[index].rect_.extent.height,
                                rect.left_, rect.top_, rect.Width(), rect.Height());
        #endif
        }
    }

    if (dirty)
    {
        URHO3D_LOGINFOF("GraphicsImpl() - SetViewport : UpdateRenderAttachments() !");
        vkDeviceWaitIdle(device_);
        CreateRenderAttachments();
    }

    if (index == -1)
    {
        viewport_ = screenViewport_;
        screenScissor_ = { 0, 0, swapChainExtent_.width, swapChainExtent_.height };
    }
    else if (!renderPassInfo_ || (renderPassInfo_ && renderPassInfo_->type_ == PASS_PRESENT))
    {
        screenScissor_ = viewportInfos_[index].rect_;
        viewport_ = { (float)screenScissor_.offset.x, (float)screenScissor_.offset.y, (float)screenScissor_.extent.width, (float)screenScissor_.extent.height };
    }
    else
    {
        viewport_ = { 0.f, 0.f, (float)viewportInfos_[index].rect_.extent.width, (float)viewportInfos_[index].rect_.extent.height };
        screenScissor_ = { 0, 0, (unsigned)viewport_.width, (unsigned)viewport_.height };
    }

    viewportIndex_ = index;

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGINFOF("GraphicsImpl() - SetViewport : index=%d rect=(%F %F %F %F)", viewportIndex_, viewport_.x, viewport_.y, viewport_.width, viewport_.height);
#endif
}

void GraphicsImpl::SetViewportInfos()
{
    Renderer* renderer = context_->GetSubsystem<Renderer>();

    // Take care of screen resizing
    // In this case, the renderer has not already resize the viewports
    Vector2 scale = Vector2::ONE;
//    if (renderer && renderer->GetFrameInfo().viewSize_ != IntVector2::ZERO)
//    {
//        scale.x_ = (float)swapChainExtent_.width / renderer->GetFrameInfo().viewSize_.x_;
//        scale.y_ = (float)swapChainExtent_.height / renderer->GetFrameInfo().viewSize_.y_;
//    }

    // Store the viewport's infos
    viewportInfos_.Resize(renderer ? renderer->GetNumViewports() : 1);
    for (int i = 0; i < viewportInfos_.Size(); i++)
    {
        IntRect rect = renderer && renderer->GetViewport(i) && renderer->GetViewport(i)->GetRect() != IntRect::ZERO ?
                         renderer->GetViewport(i)->GetRect() : IntRect(0, 0, swapChainExtent_.width, swapChainExtent_.height);

        VkRect2D& vkrect = viewportInfos_[i].rect_;
        vkrect = { (int)(rect.left_ * scale.x_), (int)(Min(rect.top_, rect.bottom_) * scale.y_), (unsigned)(rect.Width() * scale.x_), (unsigned)(rect.Height() * scale.y_) };

        URHO3D_LOGINFOF("GraphicsImpl() - SetViewportInfos : viewport=%d rect=(%u %u %u %u) sc=%F", i, vkrect.offset.x, vkrect.offset.y, vkrect.extent.width, vkrect.extent.height, scale.x_);
    }

    // Get the different viewports's sizes
    if (!viewportSizes_.Size())
    {
        viewportSizes_.Push(IntVector2(swapChainExtent_.width, swapChainExtent_.height));
        screenViewport_ = { 0.f, 0.f, (float)swapChainExtent_.width, (float)swapChainExtent_.height };
    }

    for (int i = 0; i < viewportInfos_.Size(); i++)
    {
        IntVector2 size(viewportInfos_[i].rect_.extent.width, viewportInfos_[i].rect_.extent.height);
        Vector<IntVector2>::Iterator it = viewportSizes_.Find(size);
        if (it == viewportSizes_.End())
        {
            viewportInfos_[i].viewSizeIndex_ = viewportSizes_.Size();
            viewportSizes_.Push(size);
        }
        else
            viewportInfos_[i].viewSizeIndex_ = it - viewportSizes_.Begin();

        URHO3D_LOGINFOF("GraphicsImpl() - SetViewportInfos : viewport=%d viewSizeIndex=%u", i, viewportInfos_[i].viewSizeIndex_);
    }
}


// Pipeline

void GraphicsImpl::ResetToDefaultPipelineStates()
{
    pipelineStates_ = defaultPipelineStates_;
}

void GraphicsImpl::SetPipelineState(unsigned& pipelineStates, PipelineState state, unsigned value)
{
    unsigned offset = PipelineStateMaskBits[state][0];
    unsigned mask   = (PipelineStateMaskBits[state][1] << offset);
    unsigned states = ((value << offset) & mask) + (pipelineStates & ~mask);

    if (states != pipelineStates)
    {
        pipelineStates  = states;
        pipelineDirty_  = true;
    }
}

unsigned GraphicsImpl::GetPipelineStateVariation(unsigned entrypipelineStates, PipelineState state, unsigned value)
{
    unsigned offset = PipelineStateMaskBits[state][0];
    unsigned mask   = (PipelineStateMaskBits[state][1] << offset);

    return ((value << offset) & mask) + (entrypipelineStates & ~mask);
}

PipelineInfo* GraphicsImpl::RegisterPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, unsigned numVertexTables, const PODVector<VertexElement>* vertexTables)
{
    // Hash vertex elements
    String elementstr;
    for (unsigned i = 0; i < numVertexTables; i++)
    {
        const PODVector<VertexElement>& elements = vertexTables[i];
        elementstr += String(elements.Size());
        for (PODVector<VertexElement>::ConstIterator it = elements.Begin(); it != elements.End(); ++it)
            elementstr += String((int)it->type_);

        if (i < numVertexTables - 1)
            elementstr += "_";
    }

    // Create PipelineInfo Key
    StringHash elementHash(elementstr);
    String keyname = String(renderPassKey) + "_" + vs->GetName() + "_" + String(elementHash.Value()) + "_" +
                     String(vs->GetVariationHash().Value()) + "_" + String(ps->GetVariationHash().Value()) + "_" +
                     String(states);
    if (stencilValue_)
        keyname += "_" + String(stencilValue_);

    StringHash key(keyname);

    // Register datas in the Pipeline Infos
    PipelineInfo& info      = pipelinesInfos_[key];
    info.key_               = key;
    info.renderPassKey_     = renderPassKey;
    info.vs_                = vs;
    info.ps_                = ps;
    info.pipelineStates_    = states;
    info.stencilValue_      = stencilValue_;

    // Load the bytecodes before getting ubo and samplers
    if (!vs->GetByteCode().Size())
        vs->Create();
    if (!ps->GetByteCode().Size())
        ps->Create();

    // Merge the descriptor structures from the 2 shadervariations.
    HashMap<unsigned, HashMap<unsigned, ShaderBind > > descriptorStruct;
    for (unsigned v = 0; v < 2; v++)
    {
        HashMap<unsigned, HashMap<unsigned, ShaderBind > >& ds = v == 0 ? info.vs_->descriptorStructure_ : info.ps_->descriptorStructure_;
        for (HashMap<unsigned, HashMap<unsigned, ShaderBind > >::ConstIterator it = ds.Begin(); it != ds.End(); ++it)
        {
            unsigned setid = it->first_;
            if (descriptorStruct.Contains(setid))
            {
                HashMap<unsigned, ShaderBind >& mergedBindings = descriptorStruct[setid];
                const HashMap<unsigned, ShaderBind >& bindings = it->second_;
                for (HashMap<unsigned, ShaderBind >::ConstIterator jt = bindings.Begin(); jt != bindings.End(); ++jt)
                {
                    unsigned bind = jt->first_;
                    if (mergedBindings.Contains(bind))
                    {
                        URHO3D_LOGERRORF("RegisterPipelineInfo : DescriptorSet : set=%u.%u already existing !", setid, bind);
                        continue;
                    }

                    const ShaderBind& binding = jt->second_;
                    ShaderBind& mergeBinding = mergedBindings[bind];
                    mergeBinding.id_ = bind;
                    mergeBinding.type_ = binding.type_;
                    mergeBinding.stageFlag_ = v == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
                    mergeBinding.unitStart_ = binding.unitStart_;
                    mergeBinding.unitRange_ = binding.unitRange_;
                }
            }
            else
            {
                descriptorStruct.Insert(it);
            }
        }
    }

    // Create the descriptor Sets Structure
    URHO3D_LOGDEBUG("RegisterPipelineInfo : DescriptorSet - Structure ...");

    info.descriptorsGroups_.Resize(descriptorStruct.Size());
    int i = 0;
    for (HashMap<unsigned, HashMap<unsigned, ShaderBind > >::ConstIterator it = descriptorStruct.Begin(); it != descriptorStruct.End(); ++it, ++i)
    {
        const HashMap<unsigned, ShaderBind >& b = it->second_;

        DescriptorsGroup& descriptorSet = info.descriptorsGroups_[i];
        descriptorSet.id_ = it->first_;
        descriptorSet.bindings_.Resize(b.Size());

        URHO3D_LOGDEBUGF("  . Set=%u ...", descriptorSet.id_);
        int j = 0;
        for (HashMap<unsigned, ShaderBind >::ConstIterator jt = b.Begin(); jt != b.End(); ++jt, ++j)
        {
            ShaderBind& binding = descriptorSet.bindings_[j];
            binding = jt->second_;
            URHO3D_LOGDEBUGF("    -> Bind=%u stage=%u type=%u unit=%u to %u ...", binding.id_, binding.stageFlag_, binding.type_, binding.unitStart_, binding.unitStart_+binding.unitRange_-1);
        }
    }

    // Copy the Vertex Elements
    info.vertexElementsTable_.Resize(numVertexTables);
    for (unsigned i = 0; i < numVertexTables; i++)
        info.vertexElementsTable_[i] = vertexTables[i];

    // Link in hashtables
    Vector<PipelineInfo*>& table = pipelineInfoTable_[renderPassKey][vs->GetVariationHash()][ps->GetVariationHash()][states];
    if (table.Size() <= stencilValue_)
        table.Resize(stencilValue_ + 1);
    table[stencilValue_] = &info;

    URHO3D_LOGERRORF("RegisterPipelineInfo name=%s key=%u keyname=%s ...", vs->GetName().CString(), key.Value(), keyname.CString());
    URHO3D_LOGERRORF("                     renderPassKey=%u ...", renderPassKey);
    URHO3D_LOGERRORF("                     %s vs=%s(%u)", vs->GetCachedFileName().CString(), vs->GetDefines().CString(), vs->GetVariationHash().Value());
    URHO3D_LOGERRORF("                     %s ps=%s(%u)", ps->GetCachedFileName().CString(), ps->GetDefines().CString(), ps->GetVariationHash().Value());
    URHO3D_LOGERRORF("                     states=%u(%s) stencilValue=%u", states, DumpPipelineStates(states).CString(), stencilValue_);

    return &info;
}

PipelineInfo* GraphicsImpl::RegisterPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, VertexBuffer** buffers)
{
    const PODVector<VertexElement>* elements[MAX_VERTEX_STREAMS];

    unsigned numVertexTables = 0;
    while (numVertexTables < MAX_VERTEX_STREAMS && buffers[numVertexTables])
        numVertexTables++;

    for (unsigned i = 0; i < numVertexTables; i++)
        elements[i] = &(buffers[i]->GetElements());

    return RegisterPipelineInfo(renderPassKey, vs, ps, states, numVertexTables, elements[0]);
}

bool GraphicsImpl::SetPipeline(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned pipelineStates, VertexBuffer** vertexBuffers)
{
    if (!vs || !ps)
        return false;

    PipelineInfo* info = GetPipelineInfo(renderPassKey, vs, ps, pipelineStates, stencilValue_);
    if (!info)
    {
        URHO3D_LOGDEBUGF("Can't find pipeline info for shader=%s vs=%s ps=%s pipelineStates=%u => Register new pipeline",
                          vs->GetName().CString(), vs->GetDefines().CString(), ps->GetDefines().CString(), pipelineStates);

        info = RegisterPipelineInfo(renderPassKey, vs, ps, pipelineStates, vertexBuffers);
        if (!info)
        {
            URHO3D_LOGERRORF("Can't create pipeline info for shader=%s vs=%s ps=%s pipelineStates=%u !",
                              vs->GetName().CString(), vs->GetDefines().CString(), ps->GetDefines().CString(), pipelineStates);
            return false;
        }
    }

    if (pipelineInfo_ != info)
        pipelineInfo_ = info;

    if (pipelineInfo_->pipeline_ == VK_NULL_HANDLE)
        pipelineInfo_->pipeline_ = CreatePipeline(pipelineInfo_);

    pipelineDirty_ = false;

    return true;
}

VkPipeline GraphicsImpl::CreatePipeline(PipelineInfo* info)
{
    // check if pipeline exists already
    if (info->pipeline_ != VK_NULL_HANDLE)
    {
//        URHO3D_LOGDEBUGF("GetPipeline %s vs=%s ps=%s alpha=%s", info->shaderName_.CString(), info->vs_->GetDefines().CString(), info->ps_->GetDefines().CString(), info->blendAlpha_ ? "true":"false");
        return info->pipeline_;
    }

    PrimitiveType primitive = (PrimitiveType)GetPipelineStateInternal(info, PIPELINESTATE_PRIMITIVE);
    FillMode fillmode       = (FillMode)GetPipelineStateInternal(info, PIPELINESTATE_FILLMODE);
    CullMode cullmode       = (CullMode)GetPipelineStateInternal(info, PIPELINESTATE_CULLMODE);
    unsigned linewidth      = Clamp(GetPipelineStateInternal(info, PIPELINESTATE_LINEWIDTH), 0U, 2U);
    BlendMode blendmode     = (BlendMode)GetPipelineStateInternal(info, PIPELINESTATE_BLENDMODE);
    unsigned colormask      = GetPipelineStateInternal(info, PIPELINESTATE_COLORMASK);
    int depthtest           = GetPipelineStateInternal(info, PIPELINESTATE_DEPTHTEST);
    bool depthwrite         = GetPipelineStateInternal(info, PIPELINESTATE_DEPTHWRITE);
    bool depthenable        = depthtest != CMP_ALWAYS || depthwrite != 0;
    bool stenciltest        = GetPipelineStateInternal(info, PIPELINESTATE_STENCILTEST);
    int stencilmode         = GetPipelineStateInternal(info, PIPELINESTATE_STENCILMODE);
    int samples             = GetPipelineStateInternal(info, PIPELINESTATE_SAMPLES);

    URHO3D_LOGERRORF("CreatePipeline name=%s key=%u vs=%s ps=%s prim=%d fill=%d cull=%d linew=%F blend=%u colorwrite=%s depthtest=%d depthwrite=%s depthenable=%s stencil=%s stencilvalue=%u samples=%d",
                     info->vs_->GetName().CString(), info->key_.Value(), info->vs_->GetDefines().CString(),
                     info->ps_->GetDefines().CString(), primitive, fillmode, cullmode, LineWidthValues_[linewidth], blendmode, colormask ? "true":"false", depthtest, depthwrite ? "true":"false",
                     depthenable ? "true":"false", stenciltest ? "true":"false", info->stencilValue_, samples);

    pipelineBuilder_.CleanUp();
    pipelineBuilder_.AddShaderStage(info->vs_);
    pipelineBuilder_.AddShaderStage(info->ps_);
    pipelineBuilder_.AddVertexElements(info->vertexElementsTable_);
    pipelineBuilder_.SetTopology(primitive);
    pipelineBuilder_.SetRasterization(fillmode, cullmode, linewidth);
    pipelineBuilder_.SetDepthStencil(depthenable, depthtest, depthwrite, stenciltest, stencilmode, info->stencilValue_);
    pipelineBuilder_.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    pipelineBuilder_.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    pipelineBuilder_.SetMultiSampleState(samples);
//    pipelineBuilder_.SetMultiSampleState(1);
//    pipelineBuilder_.SetColorBlend(true);
    const RenderPassInfo* renderPassInfo = GetRenderPassInfo(info->renderPassKey_);
    if (!renderPassInfo)
    {
        URHO3D_LOGERRORF("CreatePipeline name=%s no RenderPassInfo ! ");
        return (VkPipeline) nullptr;
    }

    unsigned colorAttachmentIndex = 0;
    for (Vector<RenderPassAttachmentInfo>::ConstIterator it = renderPassInfo->attachments_.Begin(); it != renderPassInfo->attachments_.End(); ++it)
        if (it->slot_ < RENDERSLOT_DEPTH)
            pipelineBuilder_.AddColorBlendAttachment(colorAttachmentIndex++, blendmode, colormask);

    pipelineBuilder_.CreatePipeline(info);

    // Update current Pipeline Infos
    pipelineInfo_ = info;

    return pipelineInfo_->pipeline_;
}

void GraphicsImpl::CreatePipelines()
{
    if (!pipelinesInfos_.Size())
        return;

    for (HashMap<StringHash, PipelineInfo>::Iterator it = pipelinesInfos_.Begin(); it != pipelinesInfos_.End(); ++it)
    {
        CreatePipeline(&it->second_);
    }
}

unsigned GraphicsImpl::GetPipelineState(unsigned pipelineStates, PipelineState state) const
{
    unsigned stateValue = (pipelineStates >> PipelineStateMaskBits[state][0]) & PipelineStateMaskBits[state][1];
    return stateValue;
}

unsigned GraphicsImpl::GetDefaultPipelineStates() const
{
    return defaultPipelineStates_;
}

unsigned GraphicsImpl::GetDefaultPipelineStates(PipelineState stateToModify, unsigned value)
{
    unsigned modifiedStates = defaultPipelineStates_;
    SetPipelineState(modifiedStates, stateToModify, value);
    return modifiedStates;
}

PipelineInfo* GraphicsImpl::GetPipelineInfo(unsigned renderPassKey, ShaderVariation* vs, ShaderVariation* ps, unsigned states, unsigned stencilvalue) const
{
    HashMap<unsigned, HashMap<StringHash, HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > > > >::ConstIterator ht = pipelineInfoTable_.Find(renderPassKey);
    if (ht != pipelineInfoTable_.End())
    {
        const HashMap<StringHash, HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > > >& vstable = ht->second_;
        HashMap<StringHash, HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > > >::ConstIterator it = vstable.Find(vs->GetVariationHash());
        if (it != pipelineInfoTable_.End())
        {
            const HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > >& pstable = it->second_;
            HashMap<StringHash, HashMap<unsigned, Vector<PipelineInfo*> > >::ConstIterator jt = pstable.Find(ps->GetVariationHash());
            if (jt != pstable.End())
            {
                const HashMap<unsigned, Vector<PipelineInfo*> >& statestable = jt->second_;
                HashMap<unsigned, Vector<PipelineInfo*> >::ConstIterator kt = statestable.Find(states);
                if (kt != statestable.End())
                {
                    const Vector<PipelineInfo*>& table = kt->second_;
                    if (table.Size() > stencilvalue)
                    {
                        PipelineInfo* info = table[stencilvalue];
                        if (info)
                        {
                            if (!info->vs_)
                                info->vs_ = vs;
                            if (!info->ps_)
                                info->ps_ = ps;
                            return info;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

PipelineInfo* GraphicsImpl::GetPipelineInfo(const StringHash& key) const
{
    HashMap<StringHash, PipelineInfo>::ConstIterator it = pipelinesInfos_.Find(key);
    if (it != pipelinesInfos_.End())
    {
        const PipelineInfo& info = it->second_;
//        URHO3D_LOGDEBUGF("GetPipelineInfo : %s vs=%s ps=%s blendalpha=%s", info.shaderName_.CString(), info.vs_->GetDefines().CString(), info.ps_->GetDefines().CString(), info.blendAlpha_ ? "true":"false");
        return (PipelineInfo*)&info;
    }

    return nullptr;
}

VkPipeline GraphicsImpl::GetPipeline(const StringHash& key) const
{
    HashMap<StringHash, PipelineInfo>::ConstIterator it = pipelinesInfos_.Find(key);
    return it != pipelinesInfos_.End() ? it->second_.pipeline_ : VK_NULL_HANDLE;
}

int GraphicsImpl::GetMaxCompatibleDescriptorSets(PipelineInfo* p1, PipelineInfo* p2) const
{
    return -1;
}

String GraphicsImpl::DumpPipelineStates(unsigned pipelineStates) const
{
    String str;
    for (int state = 0; state < PIPELINESTATE_MAX; state++)
    {
        unsigned value = GetPipelineState(pipelineStates, (PipelineState)state);
        str.AppendWithFormat("%s=%u", PipelineStateNames_[state], value);
        if (state < PIPELINESTATE_MAX - 1)
            str += ",";
    }
    return str;
}

void GraphicsImpl::DumpRegisteredPipelineInfo() const
{
    String s;

    s.AppendWithFormat("DumpRegisteredPipelineInfo : num=%u \n", pipelinesInfos_.Size());

    for (HashMap<StringHash, PipelineInfo >::ConstIterator it = pipelinesInfos_.Begin(); it != pipelinesInfos_.End(); ++it)
    {
        const PipelineInfo& info = it->second_;
        s.AppendWithFormat("key=%u states=%u(%s) stencilValue=%u %s vs=%s ps=%s \n", info.key_.Value(), info.pipelineStates_,
                           DumpPipelineStates(info.pipelineStates_).CString(), info.stencilValue_,
                           info.vs_ ? info.vs_->GetName().CString() : "null", info.vs_ ? info.vs_->GetDefines().CString() : "null",
                           info.ps_ ? info.ps_->GetDefines().CString() : "null");
    }

    URHO3D_LOGERRORF("%s", s.CString());
}

// Presentation

bool GraphicsImpl::AcquireFrame()
{
    if (swapChain_ == VK_NULL_HANDLE)
    {
        URHO3D_LOGERRORF("AcquireFrame : can't get image no swapchain !");
        return false;
    }

//    if (semaphorePool_.Size() == 0)
//    {
//        URHO3D_LOGDEBUGF("AcquireFrame : no more Semaphore in the pool !");
//        return false;
//    }

    // for the acquiring of the frame, get a semaphore in the pool.
//    VkSemaphore acquiresync = semaphorePool_.Back();
//    semaphorePool_.Resize(semaphorePool_.Size()-1);

    // try to get the next image in the swap chain.
//    VkResult result = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, acquiresync, VK_NULL_HANDLE, &currentFrame_);
    VkResult result = vkAcquireNextImageKHR(device_, swapChain_, TIME_OUT, presentComplete_, VK_NULL_HANDLE, &currentFrame_);
    if (result != VK_SUCCESS)
    {
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR)
        {
			URHO3D_LOGERRORF("Graphics() - AcquireFrame ... VK_ERROR=%d ...", result);

			if (result == VK_ERROR_SURFACE_LOST_KHR)
				surfaceDirty_ = true;

            bool sRGB = graphics_->GetSRGB();
            UpdateSwapChain(0, 0, &sRGB);
            result = vkAcquireNextImageKHR(device_, swapChain_, TIME_OUT, presentComplete_, VK_NULL_HANDLE, &currentFrame_);
        }

        // can't get the image, restore semaphore and skip this frame.
        if (result != VK_SUCCESS)
        {
//            semaphorePool_.Push(acquiresync);
            vkQueueWaitIdle(presentQueue_);
            URHO3D_LOGERRORF("AcquireFrame : can't get image !");
//            exit(-1);
            return false;
        }
    }

    FrameData& frame = frames_[currentFrame_];
    frame_ = &frame;

    // to be ready to begin this frame, we must reset the last submitfence and the command pool.
//    if (frame.submitSync_ != VK_NULL_HANDLE)
//    {
//        vkWaitForFences(device_, 1, &frame.submitSync_, true, TIME_OUT);
//        vkResetFences(device_, 1, &frame.submitSync_);
//    }

    if (frame.commandPool_ != VK_NULL_HANDLE)
        vkResetCommandPool(device_, frame.commandPool_, 0);

//    // for the release of the frame, reserve a semaphore in the pool
//    if (frame.releaseSync_ == VK_NULL_HANDLE)
//    {
//        frame.releaseSync_ = semaphorePool_.Back();
//        semaphorePool_.Resize(semaphorePool_.Size()-1);
//    }
//
//    // restore the last semaphore to the pool and replace by the new acquired one.
//    if (frame.acquireSync_ != VK_NULL_HANDLE)
//        semaphorePool_.Push(frame.acquireSync_);
//    frame.acquireSync_ = acquiresync;

    if (!frame.commandBufferBegun_)
    {
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vkBeginCommandBuffer(frame.commandBuffer_, &beginInfo);
    }

    // start with a clear pass on the acquired image
    {
        VkRenderPassBeginInfo renderPassBI{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBI.renderPass         = renderPathData_->passInfos_.Front()->renderPass_;
        renderPassBI.framebuffer        = frame.framebuffers_.Front();
        renderPassBI.renderArea.offset  = { 0, 0 };
        renderPassBI.renderArea.extent  = swapChainExtent_;
        renderPassBI.clearValueCount    = 1;
        renderPassBI.pClearValues       = &renderPathData_->passInfos_.Front()->clearValues_[0];

        vkCmdBeginRenderPass(frame.commandBuffer_, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(frame.commandBuffer_);
    }

    frame.commandBufferBegun_ = true;
    frame.renderPassBegun_ = false;
    frame.renderPassIndex_ = -1;
    renderPassIndex_ = 0;

    return true;
}

bool GraphicsImpl::PresentFrame()
{
    if (swapChain_ == VK_NULL_HANDLE)
        return false;

    VkResult result = VK_NOT_READY;

    FrameData& frame = frames_[currentFrame_];

    if (!frame.commandBufferBegun_)
    {
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkResult result = vkBeginCommandBuffer(frame.commandBuffer_, &beginInfo);
    }
    else if (frame.renderPassBegun_)
    {
        vkCmdEndRenderPass(frame.commandBuffer_);
    }

    // Complete the command buffer.
    result = vkEndCommandBuffer(frame.commandBuffer_);

    frame.renderPassIndex_ = -1;
    frame.commandBufferBegun_ = false;
    frame.renderPassBegun_ = false;
    renderPassIndex_ = 0;

    // Submit command buffer to graphics queue
    if (result == VK_SUCCESS)
    {
        VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitinfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitinfo.commandBufferCount   = 1;
        submitinfo.pCommandBuffers      = &frame.commandBuffer_;
        submitinfo.waitSemaphoreCount   = 1;
//        submitinfo.pWaitSemaphores      = &frame.acquireSync_;
        submitinfo.pWaitSemaphores      = &presentComplete_;
        submitinfo.pWaitDstStageMask    = &waitStage;
        submitinfo.signalSemaphoreCount = 1;
//        submitinfo.pSignalSemaphores    = &frame.releaseSync_;
        submitinfo.pSignalSemaphores    = &renderComplete_;
//        result = vkQueueSubmit(graphicQueue_, 1, &submitinfo, frame.submitSync_);
        result = vkQueueSubmit(graphicQueue_, 1, &submitinfo, VK_NULL_HANDLE);
    }

    if (result == VK_SUCCESS)
    {
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &swapChain_;
        presentInfo.pImageIndices      = &currentFrame_;
        presentInfo.waitSemaphoreCount = 1;
//        presentInfo.pWaitSemaphores    = &frame.releaseSync_;
        presentInfo.pWaitSemaphores    = &renderComplete_;
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);

        vkQueueWaitIdle(presentQueue_);
    }

    frame.lastPipelineBound_ = VK_NULL_HANDLE;
    frame.lastPipelineInfoBound_ = nullptr;

    frame_ = nullptr;

//    if (result != VK_SUCCESS)
//    {
//        URHO3D_LOGERRORF("PresentFrame : can't present !");
//        DumpRegisteredPipelineInfo();
//    }

//    pipelineStates_ = defaultPipelineStates_;

    return result == VK_SUCCESS;
}

unsigned GraphicsImpl::GetUBOPaddedSize(unsigned size)
{
    size_t minalign = physicalInfo_.properties_.limits.minUniformBufferOffsetAlignment;
    if (minalign > 0)
        size = (size + minalign - 1) & ~(minalign - 1);
    return size;
}

int GraphicsImpl::GetLineWidthIndex(float width)
{
    unsigned index = 0;
    float distance, mindistance = 1000.f;

    for (int i = 0; i < 3; i++)
    {
        distance = Abs(LineWidthValues_[i] - width);
        if (distance < mindistance)
        {
            mindistance = distance;
            index = i;
        }
    }

    return index;
}

} // namespace Urho3D
