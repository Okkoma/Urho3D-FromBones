//
// Copyright (c) 2008-2017 the Urho3D project.
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
#include "../../Core/Profiler.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsEvents.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../IO/FileSystem.h"
#include "../../IO/Log.h"
#include "../../Resource/ResourceCache.h"
#include "../../Resource/XMLFile.h"

#include "../../DebugNew.h"

namespace Urho3D
{

static const VkImageUsageFlagBits VulkanTextureUsage[] =
{
    (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), // TEXTURE_STATIC = 0,
    (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), // TEXTURE_DYNAMIC,
    (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), // TEXTURE_RENDERTARGET,
    (VkImageUsageFlagBits)(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT), // TEXTURE_DEPTHSTENCIL
};

static const VkMemoryPropertyFlagBits VulkanMemoryProperties[] =
{
    (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // TEXTURE_STATIC = 0,
    (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // TEXTURE_DYNAMIC,
    //(VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), // TEXTURE_DYNAMIC,
    (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // TEXTURE_RENDERTARGET,
    (VkMemoryPropertyFlagBits)(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), // TEXTURE_DEPTHSTENCIL
};

static const VmaMemoryUsage VmaMemoryUsages[] =
{
    (VmaMemoryUsage)(VMA_MEMORY_USAGE_GPU_ONLY),    // TEXTURE_STATIC = 0,
    (VmaMemoryUsage)(VMA_MEMORY_USAGE_GPU_ONLY),    // TEXTURE_DYNAMIC,
//    (VmaMemoryUsage)(VMA_MEMORY_USAGE_CPU_TO_GPU),  // TEXTURE_DYNAMIC,
    (VmaMemoryUsage)(VMA_MEMORY_USAGE_GPU_ONLY),    // TEXTURE_RENDERTARGET,
    (VmaMemoryUsage)(VMA_MEMORY_USAGE_GPU_ONLY),    // TEXTURE_DEPTHSTENCIL
};

const char* TextureUsageNames_[] =
{
    "TEXTURE_STATIC",
    "TEXTURE_DYNAMIC",
    "TEXTURE_RENDERTARGET",
    "TEXTURE_DEPTHSTENCIL"
};


void Texture2D::OnDeviceLost()
{
    // No-op on VULKAN
}

void Texture2D::OnDeviceReset()
{
    // No-op on VULKAN
}

void Texture2D::Release()
{
    if (graphics_ && object_.buffer_)
    {
        for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
        {
            if (graphics_->GetTexture(i) == this)
                graphics_->SetTexture(i, 0);
        }
    }

    if (renderSurface_)
        renderSurface_->Release();

    if (graphics_)
    {
        if (sampler_)
            vkDestroySampler(graphics_->GetImpl()->GetDevice(), (VkSampler)sampler_, nullptr);

        if (imageView_)
            vkDestroyImageView(graphics_->GetImpl()->GetDevice(), (VkImageView)imageView_, nullptr);

        if (object_.buffer_)
        {
        #ifdef URHO3D_VMA
            VkResult result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
            vmaDestroyImage(graphics_->GetImpl()->GetAllocator(), (VkImage)object_.buffer_, (VmaAllocation)object_.vmaState_);
            object_.vmaState_ = VK_NULL_HANDLE;
        #else
            vkFreeMemory(graphics_->GetImpl()->GetDevice(), object_.memory_, nullptr);
            vkDestroyImage(graphics_->GetImpl()->GetDevice(), (VkImage)object_.buffer_, nullptr);
            object_.memory_ = VK_NULL_HANDLE;
        #endif
        }

        URHO3D_LOGDEBUGF("Release image !");
    }

    object_.buffer_ = 0;
    imageView_ = 0;
    sampler_ = 0;
}

bool Texture2D::SetData(unsigned levels, int x, int y, int width, int height, const void* data)
{
    VkFormat format = (VkFormat) (sRGB_ ? GetSRGBFormat(format_) : format_);

    URHO3D_LOGDEBUGF("SetData ... name=%s levels=%u usage=%s(%u) format=%u ...",
                     GetName().CString(), levels, TextureUsageNames_[usage_], usage_, format);

    if (usage_ <= TEXTURE_DYNAMIC)
    {
        VkResult result;
        unsigned components = format_ == Graphics::GetAlphaFormat() ? 1 : 4;
        VkDeviceSize imageSize = width * height * components;

        // Create a Stagging Buffer
        VkBuffer stagingBuffer;
    #ifdef URHO3D_VMA
        VmaAllocation stagingBufferMemory;
    #else
        VkDeviceMemory stagingBufferMemory;
    #endif
        {
            VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size        = imageSize;
            bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        #ifdef URHO3D_VMA
            // let the VMA library know that this data should be writeable by CPU, but also readable by GPU
            VmaAllocationCreateInfo allocationInfo{};
            allocationInfo.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocationInfo.requiredFlags  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            allocationInfo.flags          = VMA_ALLOCATION_CREATE_MAPPED_BIT;
            // allocate the buffer
            result = vmaCreateBuffer(graphics_->GetImpl()->GetAllocator(), &bufferInfo, &allocationInfo, &stagingBuffer, &stagingBufferMemory, nullptr);
        #else
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            result = vkCreateBuffer(graphics_->GetImpl()->GetDevice(), &bufferInfo, nullptr, &stagingBuffer);
            if (result == VK_SUCCESS)
            {
                VkMemoryRequirements memRequirements;
                vkGetBufferMemoryRequirements(graphics_->GetImpl()->GetDevice(), stagingBuffer, &memRequirements);
                uint32_t memorytypeindex;
                result = graphics_->GetImpl()->GetPhysicalDeviceInfo().GetMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memorytypeindex) ? VK_SUCCESS : VK_NOT_READY;

                if (result == VK_SUCCESS)
                {
                    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                    allocInfo.allocationSize = memRequirements.size;
                    allocInfo.memoryTypeIndex = memorytypeindex;
                    result = vkAllocateMemory(graphics_->GetImpl()->GetDevice(), &allocInfo, nullptr, &stagingBufferMemory);
                    if (result == VK_SUCCESS)
                        result = vkBindBufferMemory(graphics_->GetImpl()->GetDevice(), stagingBuffer, stagingBufferMemory, 0);
                }
            }
        #endif
            if (result != VK_SUCCESS)
            {
                URHO3D_LOGERRORF("Can't to create stagging buffer!");
                return false;
            }
        }

        // Copy Data to Stagging Buffer
        {
            void* hwData = nullptr;
        #ifdef URHO3D_VMA
            result = vmaMapMemory(graphics_->GetImpl()->GetAllocator(), stagingBufferMemory, &hwData);
        #else
            result = vkMapMemory(graphics_->GetImpl()->GetDevice(), stagingBufferMemory, 0, imageSize, 0, &hwData);
        #endif
            if (result == VK_SUCCESS)
            {
                memcpy(hwData, data, static_cast<size_t>(imageSize));
            #ifdef URHO3D_VMA
    //            result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), stagingBufferMemory, 0, VK_WHOLE_SIZE);
                vmaUnmapMemory(graphics_->GetImpl()->GetAllocator(), stagingBufferMemory);
            #else
                VkMappedMemoryRange mappedRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
//                mappedRange.memory = stagingBufferMemory;
//                mappedRange.offset = 0;
//                mappedRange.size = VK_WHOLE_SIZE;
//                result = vkInvalidateMappedMemoryRanges(graphics_->GetImpl()->GetDevice(), 1, &mappedRange);
                vkUnmapMemory(graphics_->GetImpl()->GetDevice(), stagingBufferMemory);
            #endif
            }
            if (result != VK_SUCCESS)
            {
                URHO3D_LOGERRORF("Failed to map texture !");
                return false;
            }
        }

        // Allocate CommandBuffer
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = graphics_->GetImpl()->GetCommandPool();
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(graphics_->GetImpl()->GetDevice(), &allocInfo, &commandBuffer);

        // Start CommandBuffer
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.image = (VkImage)object_.buffer_;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        // Copy Stagging Buffer to GPU
        {
            // Image Barrier : To Transfer Dst
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = levels ? levels : 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            // Copy Buffer to Image
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { x, y, 0 };
            region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, (VkImage)object_.buffer_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        // Check for image format supports linear blitting
        if (levels > 0)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(graphics_->GetImpl()->GetPhysicalDeviceInfo().device_, format, &formatProperties);
            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            {
                URHO3D_LOGERRORF("texture image format does not support linear blitting!");
                levels = 0;
            }
        }

        // Generate Mimap Levels
        if (levels > 0)
        {
            int32_t mipWidth = width;
            int32_t mipHeight = height;

            barrier.subresourceRange.levelCount = 1;

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            for (uint32_t level = 1; level < levels; level++)
            {
                // Image Barrier : To Transfer Src
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.subresourceRange.baseMipLevel = level-1;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                // Generate the mipimage by linear blitting
                blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
                blit.srcSubresource.mipLevel = level-1;
                blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
                blit.dstSubresource.mipLevel = level;

                vkCmdBlitImage(commandBuffer,
                        (VkImage)object_.buffer_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        (VkImage)object_.buffer_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit,
                        VK_FILTER_LINEAR);

                // Image Barrier : To Shader Read
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                if (mipWidth > 1)
                    mipWidth /= 2;
                if (mipHeight > 1)
                    mipHeight /= 2;
            }
        }

        // Image Barrier : To Shader Read
        barrier.subresourceRange.baseMipLevel = levels ? levels-1 : 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // End Command Buffer
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(graphics_->GetImpl()->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphics_->GetImpl()->GetGraphicsQueue());

        vkFreeCommandBuffers(graphics_->GetImpl()->GetDevice(), graphics_->GetImpl()->GetCommandPool(), 1, &commandBuffer);

        // Release Stagging Buffer
        vkDestroyBuffer(graphics_->GetImpl()->GetDevice(), stagingBuffer, nullptr);
    #ifdef URHO3D_VMA
        vmaFreeMemory(graphics_->GetImpl()->GetAllocator(), stagingBufferMemory);
    #else
        vkFreeMemory(graphics_->GetImpl()->GetDevice(), stagingBufferMemory, nullptr);
    #endif

        URHO3D_LOGDEBUGF("SetData ... OK !");
        return true;
    }

    return false;
}

bool Texture2D::SetData(Image* image, bool useAlpha)
{
    if (!image)
    {
        URHO3D_LOGERROR("Null image, can not load texture");
        return false;
    }

    // Use a shared ptr for managing the temporary mip images created during this function
    SharedPtr<Image> mipImage;
    unsigned memoryUse = sizeof(Texture2D);
    int quality = QUALITY_HIGH;
    Renderer* renderer = GetSubsystem<Renderer>();
    if (renderer)
        quality = renderer->GetTextureQuality();

    if (!image->IsCompressed())
    {
        // Convert unsuitable formats to RGBA
        unsigned components = image->GetComponents();
        if ((components == 1 && !useAlpha) || components == 2 || components == 3)
        {
            mipImage = image->ConvertToRGBA(); image = mipImage;
            if (!image)
                return false;
            components = image->GetComponents();
        }

        unsigned char* levelData = image->GetData();
        int levelWidth = image->GetWidth();
        int levelHeight = image->GetHeight();
        unsigned format = 0;

        // Discard unnecessary mip levels
        for (unsigned i = 0; i < mipsToSkip_[quality]; ++i)
        {
            mipImage = image->GetNextLevel(); image = mipImage;
            levelData = image->GetData();
            levelWidth = image->GetWidth();
            levelHeight = image->GetHeight();
        }

        switch (components)
        {
        case 1:
            format = Graphics::GetAlphaFormat();
            break;

        case 4:
            format = Graphics::GetRGBAFormat();
            break;

        default: break;
        }

        // If image was previously compressed, reset number of requested levels to avoid error if level count is too high for new size
        if (IsCompressed() && requestedLevels_ > 1)
            requestedLevels_ = 0;

        SetSize(levelWidth, levelHeight, format);
        levels_ = 1;
        URHO3D_LOGDEBUGF("SetData ... UnCompressed levels=%u components=%u image=%s", levels_, components, GetName().CString());

        SetData(0, 0, 0, levelWidth, levelHeight, levelData);
        memoryUse += levelWidth * levelHeight * components;
    }
    else
    {
        int width = image->GetWidth();
        int height = image->GetHeight();
        unsigned levels = image->GetNumCompressedLevels();
        unsigned format = graphics_->GetFormat(image->GetCompressedFormat());
        bool needDecompress = false;

        if (!format)
        {
            format = Graphics::GetRGBAFormat();
            needDecompress = true;
        }

        unsigned mipsToSkip = mipsToSkip_[quality];
        if (mipsToSkip >= levels)
            mipsToSkip = levels - 1;
        while (mipsToSkip && (width / (1 << mipsToSkip) < 4 || height / (1 << mipsToSkip) < 4))
            --mipsToSkip;
        width /= (1 << mipsToSkip);
        height /= (1 << mipsToSkip);

        SetNumLevels(Max((levels - mipsToSkip), 1U));
        SetSize(width, height, format);

        CompressedLevel level = image->GetCompressedLevel(0 + mipsToSkip);

//        URHO3D_LOGDEBUGF("SetData ... needDecompress=%s levels=%u image=%s",
//                         needDecompress ? "true":"false", levels_, GetName().CString());

        if (!needDecompress)
        {
            SetData(levels_, 0, 0, level.width_, level.height_, level.data_);
            memoryUse += level.rows_ * level.rowSize_;
        }
        else
        {
            unsigned char* rgbaData = new unsigned char[level.width_ * level.height_ * 4];
            level.Decompress(rgbaData);

            SetData(levels_, 0, 0, level.width_, level.height_, rgbaData);
            memoryUse += level.width_ * level.height_ * 4;
            delete[] rgbaData;
        }
    }

    SetMemoryUse(memoryUse);
    return true;
}

bool Texture2D::GetData(unsigned level, void* dest) const
{
//    if (!object_.ptr_)
//    {
//        URHO3D_LOGERROR("No texture created, can not get data");
//        return false;
//    }
//
//    if (!dest)
//    {
//        URHO3D_LOGERROR("Null destination for getting data");
//        return false;
//    }
//
//    if (level >= levels_)
//    {
//        URHO3D_LOGERROR("Illegal mip level for getting data");
//        return false;
//    }
//
//    if (multiSample_ > 1 && !autoResolve_)
//    {
//        URHO3D_LOGERROR("Can not get data from multisampled texture without autoresolve");
//        return false;
//    }
//
//    if (resolveDirty_)
//        graphics_->ResolveToTexture(const_cast<Texture2D*>(this));
//
//    int levelWidth = GetLevelWidth(level);
//    int levelHeight = GetLevelHeight(level);
//
//    D3D11_TEXTURE2D_DESC textureDesc;
//    memset(&textureDesc, 0, sizeof textureDesc);
//    textureDesc.Width = (UINT)levelWidth;
//    textureDesc.Height = (UINT)levelHeight;
//    textureDesc.MipLevels = 1;
//    textureDesc.ArraySize = 1;
//    textureDesc.Format = (DXGI_FORMAT)format_;
//    textureDesc.SampleDesc.Count = 1;
//    textureDesc.SampleDesc.Quality = 0;
//    textureDesc.Usage = D3D11_USAGE_STAGING;
//    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
//
//    ID3D11Texture2D* stagingTexture = 0;
//    HRESULT hr = graphics_->GetImpl()->GetDevice()->CreateTexture2D(&textureDesc, 0, &stagingTexture);
//    if (FAILED(hr))
//    {
//        URHO3D_LOGD3DERROR("Failed to create staging texture for GetData", hr);
//        URHO3D_SAFE_RELEASE(stagingTexture);
//        return false;
//    }
//
//    ID3D11Resource* srcResource = (ID3D11Resource*)(resolveTexture_ ? resolveTexture_ : object_.ptr_);
//    unsigned srcSubResource = D3D11CalcSubresource(level, 0, levels_);
//
//    D3D11_BOX srcBox;
//    srcBox.left = 0;
//    srcBox.right = (UINT)levelWidth;
//    srcBox.top = 0;
//    srcBox.bottom = (UINT)levelHeight;
//    srcBox.front = 0;
//    srcBox.back = 1;
//    graphics_->GetImpl()->GetDeviceContext()->CopySubresourceRegion(stagingTexture, 0, 0, 0, 0, srcResource,
//        srcSubResource, &srcBox);
//
//    D3D11_MAPPED_SUBRESOURCE mappedData;
//    mappedData.pData = 0;
//    unsigned rowSize = GetRowDataSize(levelWidth);
//    unsigned numRows = (unsigned)(IsCompressed() ? (levelHeight + 3) >> 2 : levelHeight);
//
//    hr = graphics_->GetImpl()->GetDeviceContext()->Map((ID3D11Resource*)stagingTexture, 0, D3D11_MAP_READ, 0, &mappedData);
//    if (FAILED(hr) || !mappedData.pData)
//    {
//        URHO3D_LOGD3DERROR("Failed to map staging texture for GetData", hr);
//        URHO3D_SAFE_RELEASE(stagingTexture);
//        return false;
//    }
//    else
//    {
//        for (unsigned row = 0; row < numRows; ++row)
//            memcpy((unsigned char*)dest + row * rowSize, (unsigned char*)mappedData.pData + row * mappedData.RowPitch, rowSize);
//        graphics_->GetImpl()->GetDeviceContext()->Unmap((ID3D11Resource*)stagingTexture, 0);
//        URHO3D_SAFE_RELEASE(stagingTexture);
//        return true;
//    }

    return true;
}

bool Texture2D::Create()
{
    // TODO Test Static texture
//    if (usage_ != TEXTURE_STATIC)
//        return false;

    Release();

    if (!graphics_ || !width_ || !height_)
        return false;

#ifdef URHO3D_VMA
    if (!graphics_->GetImpl()->GetAllocator())
        return false;
#endif

    levels_ = CheckMaxLevels(width_, height_, requestedLevels_);

    // Create GPU Texture
    VkResult result;

    VkFormat format;
    VkImageTiling tiling;

    if (usage_ <= TEXTURE_DYNAMIC)
    {
        format = (VkFormat) (sRGB_ ? GetSRGBFormat(format_) : format_);
        tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else if (usage_ == TEXTURE_RENDERTARGET)
    {
        format = (VkFormat) GraphicsImpl::GetSwapChainFormat();
        tiling = VK_IMAGE_TILING_OPTIMAL;
    }
    else if (usage_ == TEXTURE_DEPTHSTENCIL)
    {
        format = GraphicsImpl::GetDepthStencilFormat();
        tiling = VK_IMAGE_TILING_OPTIMAL;
    }

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width_;
    imageInfo.extent.height = height_;
    imageInfo.extent.depth  = depth_;
    imageInfo.mipLevels     = levels_;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VulkanTextureUsage[usage_];
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

#ifdef URHO3D_VMA
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage          = VmaMemoryUsages[usage_];
    allocationInfo.requiredFlags  = VulkanMemoryProperties[usage_];
    allocationInfo.flags          = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    result = vmaCreateImage(graphics_->GetImpl()->GetAllocator(), &imageInfo, &allocationInfo, (VkImage*)&object_.buffer_, (VmaAllocation*)&object_.vmaState_, nullptr);
#else
    result = vkCreateImage(graphics_->GetImpl()->GetDevice(), &imageInfo, nullptr, (VkImage*)&object_.buffer_);
    if (result == VK_SUCCESS)
    {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(graphics_->GetImpl()->GetDevice(), (VkImage)object_.buffer_, &memRequirements);
        uint32_t memorytypeindex;
        if (!graphics_->GetImpl()->GetPhysicalDeviceInfo().GetMemoryTypeIndex(memRequirements.memoryTypeBits, VulkanMemoryProperties[usage_], memorytypeindex))
        {
            URHO3D_LOGERRORF("Can't get device memory type for texture !");
            return false;
        }

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memorytypeindex;
        result = vkAllocateMemory(graphics_->GetImpl()->GetDevice(), &allocInfo, nullptr, &object_.memory_);
        if (result == VK_SUCCESS)
            result = vkBindImageMemory(graphics_->GetImpl()->GetDevice(), (VkImage)object_.buffer_, object_.memory_, 0);
    }
#endif

    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create texture");
        return false;
    }

    parametersDirty_ = true;

    // Create Image View
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image                           = (VkImage)object_.buffer_;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = levels_;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    result = vkCreateImageView(graphics_->GetImpl()->GetDevice(), &viewInfo, nullptr, (VkImageView*)&imageView_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create texture image view for shader use");
        return false;
    }

    URHO3D_LOGDEBUGF("Create Texture levels=%u imageview=%u sampler=%u !", levels_, GetShaderResourceView(), GetSampler());

    return true;

//    D3D11_TEXTURE2D_DESC textureDesc;
//    memset(&textureDesc, 0, sizeof textureDesc);
//    textureDesc.Format = (DXGI_FORMAT)(sRGB_ ? GetSRGBFormat(format_) : format_);
//
//    // Disable multisampling if not supported
//    if (multiSample_ > 1 && !graphics_->GetImpl()->CheckMultiSampleSupport(textureDesc.Format, multiSample_))
//    {
//        multiSample_ = 1;
//        autoResolve_ = false;
//    }
//
//    // Set mipmapping
//    if (usage_ == TEXTURE_DEPTHSTENCIL)
//        levels_ = 1;
//    else if (usage_ == TEXTURE_RENDERTARGET && levels_ != 1 && multiSample_ == 1)
//        textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
//
//    textureDesc.Width = (UINT)width_;
//    textureDesc.Height = (UINT)height_;
//    // Disable mip levels from the multisample texture. Rather create them to the resolve texture
//    textureDesc.MipLevels = multiSample_ == 1 ? levels_ : 1;
//    textureDesc.ArraySize = 1;
//    textureDesc.SampleDesc.Count = (UINT)multiSample_;
//    textureDesc.SampleDesc.Quality = graphics_->GetImpl()->GetMultiSampleQuality(textureDesc.Format, multiSample_);
//
//    textureDesc.Usage = usage_ == TEXTURE_DYNAMIC ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
//    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//    if (usage_ == TEXTURE_RENDERTARGET)
//        textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
//    else if (usage_ == TEXTURE_DEPTHSTENCIL)
//        textureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
//    textureDesc.CPUAccessFlags = usage_ == TEXTURE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;
//
//    // D3D feature level 10.0 or below does not support readable depth when multisampled
//    if (usage_ == TEXTURE_DEPTHSTENCIL && multiSample_ > 1 && graphics_->GetImpl()->GetDevice()->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_1)
//        textureDesc.BindFlags &= ~D3D11_BIND_SHADER_RESOURCE;
//
//    HRESULT hr = graphics_->GetImpl()->GetDevice()->CreateTexture2D(&textureDesc, 0, (ID3D11Texture2D**)&object_);
//    if (FAILED(hr))
//    {
//        URHO3D_LOGD3DERROR("Failed to create texture", hr);
//        URHO3D_SAFE_RELEASE(object_.ptr_);
//        return false;
//    }
//
//    // Create resolve texture for multisampling if necessary
//    if (multiSample_ > 1 && autoResolve_)
//    {
//        textureDesc.MipLevels = levels_;
//        textureDesc.SampleDesc.Count = 1;
//        textureDesc.SampleDesc.Quality = 0;
//        if (levels_ != 1)
//            textureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
//
//        HRESULT hr = graphics_->GetImpl()->GetDevice()->CreateTexture2D(&textureDesc, 0, (ID3D11Texture2D**)&resolveTexture_);
//        if (FAILED(hr))
//        {
//            URHO3D_LOGD3DERROR("Failed to create resolve texture", hr);
//            URHO3D_SAFE_RELEASE(resolveTexture_);
//            return false;
//        }
//    }
//
//    if (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
//    {
//        D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
//        memset(&resourceViewDesc, 0, sizeof resourceViewDesc);
//        resourceViewDesc.Format = (DXGI_FORMAT)GetSRVFormat(textureDesc.Format);
//        resourceViewDesc.ViewDimension = (multiSample_ > 1 && !autoResolve_) ? D3D11_SRV_DIMENSION_TEXTURE2DMS :
//            D3D11_SRV_DIMENSION_TEXTURE2D;
//        resourceViewDesc.Texture2D.MipLevels = (UINT)levels_;
//
//        // Sample the resolve texture if created, otherwise the original
//        ID3D11Resource* viewObject = resolveTexture_ ? (ID3D11Resource*)resolveTexture_ : (ID3D11Resource*)object_.ptr_;
//        hr = graphics_->GetImpl()->GetDevice()->CreateShaderResourceView(viewObject, &resourceViewDesc,
//            (ID3D11ShaderResourceView**)&shaderResourceView_);
//        if (FAILED(hr))
//        {
//            URHO3D_LOGD3DERROR("Failed to create shader resource view for texture", hr);
//            URHO3D_SAFE_RELEASE(shaderResourceView_);
//            return false;
//        }
//    }
//
//    if (usage_ == TEXTURE_RENDERTARGET)
//    {
//        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
//        memset(&renderTargetViewDesc, 0, sizeof renderTargetViewDesc);
//        renderTargetViewDesc.Format = textureDesc.Format;
//        renderTargetViewDesc.ViewDimension = multiSample_ > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
//
//        hr = graphics_->GetImpl()->GetDevice()->CreateRenderTargetView((ID3D11Resource*)object_.ptr_, &renderTargetViewDesc,
//            (ID3D11RenderTargetView**)&renderSurface_->renderTargetView_);
//        if (FAILED(hr))
//        {
//            URHO3D_LOGD3DERROR("Failed to create rendertarget view for texture", hr);
//            URHO3D_SAFE_RELEASE(renderSurface_->renderTargetView_);
//            return false;
//        }
//    }
//    else if (usage_ == TEXTURE_DEPTHSTENCIL)
//    {
//        D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
//        memset(&depthStencilViewDesc, 0, sizeof depthStencilViewDesc);
//        depthStencilViewDesc.Format = (DXGI_FORMAT)GetDSVFormat(textureDesc.Format);
//        depthStencilViewDesc.ViewDimension = multiSample_ > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
//
//        hr = graphics_->GetImpl()->GetDevice()->CreateDepthStencilView((ID3D11Resource*)object_.ptr_, &depthStencilViewDesc,
//            (ID3D11DepthStencilView**)&renderSurface_->renderTargetView_);
//        if (FAILED(hr))
//        {
//            URHO3D_LOGD3DERROR("Failed to create depth-stencil view for texture", hr);
//            URHO3D_SAFE_RELEASE(renderSurface_->renderTargetView_);
//            return false;
//        }
//
//        // Create also a read-only version of the view for simultaneous depth testing and sampling in shader
//        // Requires feature level 11
//        if (graphics_->GetImpl()->GetDevice()->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
//        {
//            depthStencilViewDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH;
//            hr = graphics_->GetImpl()->GetDevice()->CreateDepthStencilView((ID3D11Resource*)object_.ptr_, &depthStencilViewDesc,
//                (ID3D11DepthStencilView**)&renderSurface_->readOnlyView_);
//            if (FAILED(hr))
//            {
//                URHO3D_LOGD3DERROR("Failed to create read-only depth-stencil view for texture", hr);
//                URHO3D_SAFE_RELEASE(renderSurface_->readOnlyView_);
//            }
//        }
//    }
}

}
