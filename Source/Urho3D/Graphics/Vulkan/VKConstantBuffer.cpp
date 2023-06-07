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

#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/ConstantBuffer.h"
#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

void ConstantBuffer::OnDeviceReset()
{
    // No-op on VULKAN
}

void ConstantBuffer::Release()
{
    if (graphics_ && object_.buffer_)
    {
    #ifdef URHO3D_VMA
        VkResult result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
        vmaDestroyBuffer(graphics_->GetImpl()->GetAllocator(), (VkBuffer)object_.buffer_, (VmaAllocation)object_.vmaState_);
    #else
        vkFreeMemory(graphics_->GetImpl()->GetDevice(), object_.memory_, 0);
        vkDestroyBuffer(graphics_->GetImpl()->GetDevice(), (VkBuffer)object_.buffer_, 0);
        object_.memory_ = VK_NULL_HANDLE;
    #endif
        URHO3D_LOGERRORF("Release constant buffer size=%u !", size_);
    }

    object_.buffer_ = 0;
    shadowData_.Reset();
    size_ = 0;
}

bool ConstantBuffer::SetSize(unsigned size)
{
    Release();

    if (!size)
    {
        URHO3D_LOGERROR("Can not create zero-sized constant buffer");
        return false;
    }

    // Round up to next 16 bytes
    size += 15;
    size &= 0xfffffff0;

    size_ = size;
    offsetToUpdate_ = size_;
    rangeToUpdate_ = 0;
    dirty_ = false;
    shadowData_ = new unsigned char[size_];
    memset(shadowData_.Get(), 0, size_);

    if (graphics_)
	{
        VkResult result;

        // allocate uniform buffer
        VkBufferCreateInfo bufferInfo;
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = 0;
        bufferInfo.flags  = 0;
        bufferInfo.size  = size_;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    #ifdef URHO3D_VMA
        // let the VMA library know that this data should be writeable by CPU, but also readable by GPU
        VmaAllocationCreateInfo allocationInfo;
        allocationInfo.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocationInfo.requiredFlags  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        allocationInfo.flags          = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocationInfo.pool           = (VmaPool)0;
        allocationInfo.memoryTypeBits = 0;
        allocationInfo.pUserData      = 0;

        // allocate the buffer
        result = vmaCreateBuffer(graphics_->GetImpl()->GetAllocator(), &bufferInfo, &allocationInfo, (VkBuffer*)&object_.buffer_, (VmaAllocation*)&object_.vmaState_, 0);
    #else
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        result = vkCreateBuffer(graphics_->GetImpl()->GetDevice(), &bufferInfo, 0, (VkBuffer*)&object_.buffer_);
        if (result == VK_SUCCESS)
        {
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(graphics_->GetImpl()->GetDevice(), (VkBuffer)object_.buffer_, &memRequirements);
            uint32_t memorytypeindex;
            if (!graphics_->GetImpl()->GetPhysicalDeviceInfo().GetMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT, memorytypeindex))
            {
                URHO3D_LOGERRORF("Can't get device memory type for buffer !");
                return false;
            }

            VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = memorytypeindex;
            result = vkAllocateMemory(graphics_->GetImpl()->GetDevice(), &allocInfo, 0, &object_.memory_);
            if (result == VK_SUCCESS)
                result = vkBindBufferMemory(graphics_->GetImpl()->GetDevice(), (VkBuffer)object_.buffer_, object_.memory_, 0);
        }
    #endif

        if (result != VK_SUCCESS)
        {
            URHO3D_LOGERRORF("Failed to create constant buffer");
            return false;
        }

        URHO3D_LOGDEBUGF("Create constant this=%u buffer=%u size=%u", this, object_.buffer_, size_);
    }

    return true;
}

void ConstantBuffer::SetParameter(unsigned offset, unsigned size, const void* data)
{
    if (dynamic_)
    {
        if (offsetToUpdate_ == size_)
        {
            objectindex_++;
            if (objectindex_ >= MAX_OBJECTS)
                objectindex_ = 0;
        }

        offset += objectindex_ * GraphicsImpl::GetUBOPaddedSize(size_ / MAX_OBJECTS);
    }

    if (offset + size > size_)
        return; // Would overflow the buffer

    memcpy(&shadowData_[offset], data, size);

    if (dynamic_)
    {
        if (offsetToUpdate_ == size_)
        {
            offsetToUpdate_ = offset;
            rangeToUpdate_ = size;
        }
        // accumulate the size to update
        else
        {
            if (offset < offsetToUpdate_)
            {
                rangeToUpdate_  = offsetToUpdate_ - offset + size;
                offsetToUpdate_ = offset;
            }
            else
            {
                rangeToUpdate_  = offset - offsetToUpdate_ + size;
            }
        }

//        URHO3D_LOGDEBUGF("SetParameter constant buffer=%u objindex=%u offset=%u size=%u offsetToUpdate=%u rangeToUpdate=%u", this, objectindex_, offset, size, offsetToUpdate_, rangeToUpdate_);
    }

    dirty_ = true;
}

void ConstantBuffer::Apply()
{
    if (!object_.buffer_ || !shadowData_)
        return;

    void* hwData = 0;

    // Map buffer
#ifdef URHO3D_VMA
    if (vmaMapMemory(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, &hwData) != VK_SUCCESS)
#else
    if (vkMapMemory(graphics_->GetImpl()->GetDevice(), object_.memory_, 0, VK_WHOLE_SIZE, 0, &hwData) != VK_SUCCESS)
#endif
    {
        URHO3D_LOGERRORF("Failed to map constant buffer !");
        return;
    }

    // Copy to buffer
    if (dynamic_)
    {
        memcpy((unsigned char*)hwData+offsetToUpdate_, &shadowData_[offsetToUpdate_], rangeToUpdate_);
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("Apply constant buffer offset=%u range=%u to gpu", offsetToUpdate_, rangeToUpdate_);
    #endif
        offsetToUpdate_ = size_;
        rangeToUpdate_ = 0;
    }
    else
    {
        memcpy(hwData, shadowData_.Get(), size_);
//        URHO3D_LOGDEBUGF("Apply constant buffer size=%u to gpu", size_);
    }

    // Flush gpu memory after the data transfer (not necessary for Coherent Memory, To Remove ?)
    VkResult result;

#ifdef URHO3D_VMA
    result = vmaFlushAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
#else
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = object_.memory_;
    mappedRange.offset = 0;
    mappedRange.size = VK_WHOLE_SIZE;
    result = vkFlushMappedMemoryRanges(graphics_->GetImpl()->GetDevice(), 1, &mappedRange);
#endif

    // Invalidate gpu memory before unmap (not necessary for Coherent Memory, To Remove ?)
#ifdef URHO3D_VMA
    result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
#else
    VkMappedMemoryRange mappedRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    mappedRange.memory = object_.memory_;
    mappedRange.offset = 0;
    mappedRange.size = VK_WHOLE_SIZE;
    result = vkInvalidateMappedMemoryRanges(graphics_->GetImpl()->GetDevice(), 1, &mappedRange);
#endif

    // Unmap buffer
#ifdef URHO3D_VMA
    vmaUnmapMemory(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_);
#else
    vkUnmapMemory(graphics_->GetImpl()->GetDevice(), object_.memory_);
#endif

    dirty_ = false;
}

}
