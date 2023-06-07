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
#include "../../Graphics/VertexBuffer.h"
#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

void VertexBuffer::OnDeviceLost()
{
    Release();
}

void VertexBuffer::OnDeviceReset()
{
    // No-op on VULKAN
}

void VertexBuffer::Release()
{
    Unlock();

    if (graphics_ && object_.buffer_)
    {
        for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (graphics_->GetVertexBuffer(i) == this)
                graphics_->SetVertexBuffer(0);
        }
    #ifdef URHO3D_VMA
        VkResult result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
        vmaDestroyBuffer(graphics_->GetImpl()->GetAllocator(), (VkBuffer)object_.buffer_, (VmaAllocation)object_.vmaState_);
    #else
        vkFreeMemory(graphics_->GetImpl()->GetDevice(), object_.memory_, nullptr);
        vkDestroyBuffer(graphics_->GetImpl()->GetDevice(), (VkBuffer)object_.buffer_, nullptr);
        object_.memory_ = VK_NULL_HANDLE;
    #endif
//        URHO3D_LOGDEBUGF("Release vertex buffer vertexcount=%u size=%u !", vertexCount_, vertexCount_ * vertexSize_);
    }

    object_.buffer_ = 0;
}

bool VertexBuffer::SetData(const void* data)
{
    if (!data)
    {
        URHO3D_LOGERROR("Null pointer for vertex buffer data");
        return false;
    }

    if (!vertexSize_)
    {
        URHO3D_LOGERROR("Vertex elements not defined, can not set vertex buffer data");
        return false;
    }

//    if (shadowData_ && data != shadowData_.Get())
//        memcpy(shadowData_.Get(), data, vertexCount_ * vertexSize_);

    if (object_.buffer_)
    {
        if (!dynamic_)
        {
            URHO3D_LOGWARNINGF("SetData vertex buffer in static not implemented force dynamic !");
            dynamic_ = true;
        }
        if (dynamic_)
        {
            void* hwData = MapBuffer(0, vertexCount_, true);
            if (hwData)
            {
                memcpy(hwData, data, vertexCount_ * vertexSize_);
            #ifdef URHO3D_VMA
                VkResult result = vmaFlushAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
            #else
                VkMappedMemoryRange mappedRange = {};
                mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                mappedRange.memory = object_.memory_;
                mappedRange.offset = 0;
                mappedRange.size = VK_WHOLE_SIZE;
                VkResult result = vkFlushMappedMemoryRanges(graphics_->GetImpl()->GetDevice(), 1, &mappedRange);
            #endif

                UnmapBuffer();

//                URHO3D_LOGDEBUGF("SetData vertex buffer vertexcount=%u size=%u", vertexCount_, vertexCount_ * vertexSize_);
            }
            else
            {
                URHO3D_LOGDEBUGF("SetData vertex buffer vertexcount=%u size=%u no data copied !", vertexCount_, vertexCount_ * vertexSize_);
                return false;
            }
        }
    }

    return true;
}

bool VertexBuffer::SetDataRange(const void* data, unsigned start, unsigned count, bool discard)
{
    if (start == 0 && count == vertexCount_)
        return SetData(data);

    if (!data)
    {
        URHO3D_LOGERROR("Null pointer for vertex buffer data");
        return false;
    }

    if (!vertexSize_)
    {
        URHO3D_LOGERROR("Vertex elements not defined, can not set vertex buffer data");
        return false;
    }

    if (start + count > vertexCount_)
    {
        URHO3D_LOGERROR("Illegal range for setting new vertex buffer data");
        return false;
    }

    if (!count)
        return true;

    if (shadowData_ && shadowData_.Get() + start * vertexSize_ != data)
        memcpy(shadowData_.Get() + start * vertexSize_, data, count * vertexSize_);

    if (object_.buffer_)
    {
        if (!dynamic_)
        {
            URHO3D_LOGWARNINGF("SetDataRange vertex buffer in static not implemented force dynamic !");
            dynamic_ = true;
        }
        if (dynamic_)
        {
            void* hwData = MapBuffer(start, count, discard);
            if (hwData)
            {
                memcpy(hwData, data, count * vertexSize_);
                UnmapBuffer();
            }
            else
                return false;
        }
	    else
        {
            URHO3D_LOGWARNINGF("SetDataRange Vertex buffer in static not implemented !");
//            D3D11_BOX destBox;
//            destBox.left = start * vertexSize_;
//            destBox.right = destBox.left + count * vertexSize_;
//            destBox.top = 0;
//            destBox.bottom = 1;
//            destBox.front = 0;
//            destBox.back = 1;
//
//            graphics_->GetImpl()->GetDeviceContext()->UpdateSubresource((ID3D11Buffer*)object_.ptr_, 0, &destBox, data, 0, 0);
        }
    }

    return true;
}

void* VertexBuffer::Lock(unsigned start, unsigned count, bool discard)
{
    if (lockState_ != LOCK_NONE)
    {
        URHO3D_LOGERROR("Vertex buffer already locked");
        return 0;
    }

    if (!vertexSize_)
    {
        URHO3D_LOGERROR("Vertex elements not defined, can not lock vertex buffer");
        return 0;
    }

    if (start + count > vertexCount_)
    {
        URHO3D_LOGERROR("Illegal range for locking vertex buffer");
        return 0;
    }

    if (!count)
        return 0;

    lockStart_ = start;
    lockCount_ = count;

    // Because shadow data must be kept in sync, can only lock hardware buffer if not shadowed
    if (object_.ptr_ && !shadowData_ && dynamic_)
        return MapBuffer(start, count, discard);
    else if (shadowData_)
    {
        lockState_ = LOCK_SHADOW;
        return shadowData_.Get() + start * vertexSize_;
    }
    else if (graphics_)
    {
        lockState_ = LOCK_SCRATCH;
        lockScratchData_ = graphics_->ReserveScratchBuffer(count * vertexSize_);
        return lockScratchData_;
    }
    else
        return 0;
}

void VertexBuffer::Unlock()
{
    switch (lockState_)
    {
    case LOCK_HARDWARE:
        UnmapBuffer();
        break;

    case LOCK_SHADOW:
        SetDataRange(shadowData_.Get() + lockStart_ * vertexSize_, lockStart_, lockCount_);
        lockState_ = LOCK_NONE;
        break;

    case LOCK_SCRATCH:
        SetDataRange(lockScratchData_, lockStart_, lockCount_);
        if (graphics_)
            graphics_->FreeScratchBuffer(lockScratchData_);
        lockScratchData_ = 0;
        lockState_ = LOCK_NONE;
        break;

    default: break;
    }
}

bool VertexBuffer::Create()
{
    Release();

    if (!vertexCount_ || !elements_.Size())
        return true;

    if (graphics_)
	{
        if (!dynamic_)
        {
//            URHO3D_LOGWARNINGF("Create vertex buffer in static not implemented force dynamic !");
            dynamic_ = true;
        }
	    if (dynamic_)
	    {
	        VkResult result;

	        // allocate vertex buffer
            VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size  = vertexCount_ * vertexSize_;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        #ifdef URHO3D_VMA
            // let the VMA library know that this data should be writeable by CPU, but also readable by GPU
            VmaAllocationCreateInfo allocationInfo{};
            allocationInfo.usage          = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocationInfo.requiredFlags  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            allocationInfo.flags          = VMA_ALLOCATION_CREATE_MAPPED_BIT;

            // allocate the buffer
            result = vmaCreateBuffer(graphics_->GetImpl()->GetAllocator(), &bufferInfo, &allocationInfo, (VkBuffer*)&object_.buffer_, (VmaAllocation*)&object_.vmaState_, nullptr);
        #else
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            result = vkCreateBuffer(graphics_->GetImpl()->GetDevice(), &bufferInfo, nullptr, (VkBuffer*)&object_.buffer_);
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
                result = vkAllocateMemory(graphics_->GetImpl()->GetDevice(), &allocInfo, nullptr, &object_.memory_);
                if (result == VK_SUCCESS)
                    result = vkBindBufferMemory(graphics_->GetImpl()->GetDevice(), (VkBuffer)object_.buffer_, object_.memory_, 0);
            }
        #endif

            if (result != VK_SUCCESS)
            {
                URHO3D_LOGERRORF("Failed to create vertex buffer");
                return false;
            }

//            URHO3D_LOGDEBUGF("Create dynamic vertex buffer=%u vertexcount=%u size=%u", object_.buffer_, vertexCount_, vertexCount_ * vertexSize_);
	    }
	    // TODO use staging buffer
	    else
        {
            URHO3D_LOGWARNINGF("Vertex buffer in static not implemented !");
        }
	}

    return true;
}

bool VertexBuffer::UpdateToGPU()
{
    if (object_.buffer_ && shadowData_)
        return SetData(shadowData_.Get());
    else
        return false;
}

void* VertexBuffer::MapBuffer(unsigned start, unsigned count, bool discard)
{
    void* hwData = nullptr;

    if (object_.buffer_)
    {
        // TODO map at start and for count
        void* data;
    #ifdef URHO3D_VMA
        if (vmaMapMemory(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, &data) != VK_SUCCESS)
    #else
        if (vkMapMemory(graphics_->GetImpl()->GetDevice(), object_.memory_, start * vertexSize_, VK_WHOLE_SIZE, 0, &data) != VK_SUCCESS)
    #endif
        {
            URHO3D_LOGERRORF("Failed to map vertex buffer !");
        }
        else
        {
            hwData = data;
            lockState_ = LOCK_HARDWARE;
        }
    }

    return hwData;
}

void VertexBuffer::UnmapBuffer()
{
    if (object_.buffer_ && lockState_ == LOCK_HARDWARE)
    {
    #ifdef URHO3D_VMA
        VkResult result = vmaInvalidateAllocation(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(graphics_->GetImpl()->GetAllocator(), (VmaAllocation)object_.vmaState_);
    #else
        VkMappedMemoryRange mappedRange = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        mappedRange.memory = object_.memory_;
        mappedRange.offset = 0;
        mappedRange.size = VK_WHOLE_SIZE;
        VkResult result = vkInvalidateMappedMemoryRanges(graphics_->GetImpl()->GetDevice(), 1, &mappedRange);
        vkUnmapMemory(graphics_->GetImpl()->GetDevice(), object_.memory_);
    #endif
//        URHO3D_LOGDEBUGF("UnmapBuffer vertex buffer vertexcount=%u size=%u mem=%u", vertexCount_, vertexCount_ * vertexSize_, object_.buffer_);
        lockState_ = LOCK_NONE;
    }
}

}
