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

#pragma once

#include "../Container/Ptr.h"

#ifdef URHO3D_VULKAN

#ifndef URHO3D_VOLK
#include <vulkan/vulkan.h>
#endif

#ifdef URHO3D_VMA
    #ifdef URHO3D_VOLK
        #define VK_NO_PROTOTYPES
        #define VMA_STATIC_VULKAN_FUNCTIONS 0
    #endif
#include "vma/vk_mem_alloc.h"
#endif

#endif

namespace Urho3D
{

class Graphics;

/// API-specific GPU object representation.
union URHO3D_API GPUObjectHandle
{
    /// Object pointer (Direct3D.)
    void* ptr_;
    /// Object name (OpenGL.)
    unsigned name_;
//#if defined(URHO3D_VULKAN)
//    /// Object struct (Vulkan.)
//    struct
//    {
//        VkImage buffer_;
//    #if defined(URHO3D_VMA)
//        VmaAllocation vmaState_;
//    #else
//        VkDeviceMemory memory_;
//    #endif
//    };
//#endif
#if defined(URHO3D_VULKAN)
    /// Object struct (Vulkan.)
    struct
    {
        void* buffer_;
    #if defined(URHO3D_VMA)
        void* vmaState_;
    #else
        void* memory_;
    #endif
    };
#endif
};

/// Base class for GPU resources.
class URHO3D_API GPUObject
{
public:
    /// Construct with graphics subsystem pointer.
    GPUObject(Graphics* graphics);
    /// Destruct. Remove from the Graphics.
    virtual ~GPUObject();

    /// Mark the GPU resource destroyed on graphics context destruction.
    virtual void OnDeviceLost();
    /// Recreate the GPU resource and restore data if applicable.
    virtual void OnDeviceReset();
    /// Unconditionally release the GPU resource.
    virtual void Release();

    /// Clear the data lost flag.
    void ClearDataLost();

    /// Return the graphics subsystem associated with this GPU object.
    Graphics* GetGraphics() const;
    /// Return the object pointer. Applicable only on Direct3D.
#ifdef URHO3D_VULKAN
//    VkImage GetGPUObject() const { return object_.buffer_; }
//	#if defined(URHO3D_VMA)
//    void SetGPUObject(VkImage buffer, VmaAllocation memory)
//    {
//        object_.buffer_ = buffer;
//        object_.vmaState_ = (VmaAllocation)memory;
//    }
//    #else
//    void SetGPUObject(VkImage buffer, VkDeviceMemory memory)
//    {
//        object_.buffer_ = buffer;
//		object_.memory_ = memory;
//    }
//    #endif
    void* GetGPUObject() const { return object_.buffer_; }
	#if defined(URHO3D_VMA)
    void SetGPUObject(void* buffer, void* memory)
    {
        object_.buffer_ = buffer;
        object_.vmaState_ = memory;
    }
    #else
    void SetGPUObject(void* buffer, void* memory)
    {
        object_.buffer_ = buffer;
		object_.memory_ = memory;
    }
    #endif
#else
    void* GetGPUObject() const { return object_.ptr_; }
#endif
    /// Return the object name. Applicable only on OpenGL.
    unsigned GetGPUObjectName() const { return object_.name_; }
    /// Return whether data is lost due to context loss.
    bool IsDataLost() const { return dataLost_; }
    /// Return whether has pending data assigned while graphics context was lost.
    bool HasPendingData() const { return dataPending_; }

protected:
    /// Graphics subsystem.
    WeakPtr<Graphics> graphics_;
    /// Object pointer or name.
    GPUObjectHandle object_;
    /// Data lost flag.
    bool dataLost_;
    /// Data pending flag.
    bool dataPending_;
};

}

