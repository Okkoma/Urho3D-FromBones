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

// Define pour activer l'implémentation complète de RenderSurface pour Vulkan
#define URHO3D_VULKAN_RENDERSURFACE

#include "../../Graphics/Camera.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/RenderSurface.h"
#include "../../Graphics/Texture.h"

#include "../../DebugNew.h"


namespace Urho3D
{

RenderSurface::RenderSurface(Texture* parentTexture) :
    parentTexture_(parentTexture),
    renderTargetView_(0),
    readOnlyView_(0),
    updateMode_(SURFACE_UPDATEVISIBLE),
    updateQueued_(false)
{
#ifdef URHO3D_VULKAN_RENDERSURFACE
    // Initialiser les structures Vulkan
    framebuffer_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;
    colorImageView_ = VK_NULL_HANDLE;
    depthImageView_ = VK_NULL_HANDLE;
    resolveImageView_ = VK_NULL_HANDLE;
    renderBufferImage_ = VK_NULL_HANDLE;
    renderBufferMemory_ = VK_NULL_HANDLE;
#endif
}

void RenderSurface::Release()
{
#ifdef URHO3D_VULKAN_RENDERSURFACE
    Graphics* graphics = parentTexture_->GetGraphics();
    if (graphics && graphics->GetImpl())
    {
        VkDevice device = graphics->GetImpl()->GetDevice();
        const VkAllocationCallbacks* pAllocator = nullptr;
        
        // Nettoyer les ressources Vulkan
        if (framebuffer_ != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer_, pAllocator);
            framebuffer_ = VK_NULL_HANDLE;
        }
        
        if (colorImageView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, colorImageView_, pAllocator);
            colorImageView_ = VK_NULL_HANDLE;
        }
        
        if (depthImageView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, depthImageView_, pAllocator);
            depthImageView_ = VK_NULL_HANDLE;
        }
        
        if (resolveImageView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, resolveImageView_, pAllocator);
            resolveImageView_ = VK_NULL_HANDLE;
        }
        
        // Nettoyer les images et mémoires de renderbuffer
        if (renderBufferImage_ != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, renderBufferImage_, pAllocator);
            renderBufferImage_ = VK_NULL_HANDLE;
        }
        
        if (renderBufferMemory_ != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, renderBufferMemory_, pAllocator);
            renderBufferMemory_ = VK_NULL_HANDLE;
        }
        
        // Note: renderPass_ est géré par GraphicsImpl, pas ici
        renderPass_ = VK_NULL_HANDLE;
    }
    
    // Nettoyer les rendertargets liés
    if (graphics)
    {
        for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        {
            if (graphics->GetRenderTarget(i) == this)
                graphics->ResetRenderTarget(i);
        }
        
        if (graphics->GetDepthStencil() == this)
            graphics->ResetDepthStencil();
    }
#else
    // Ancienne implémentation commentée
//    Graphics* graphics = parentTexture_->GetGraphics();
//    if (graphics && renderTargetView_)
//    {
//        for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
//        {
//            if (graphics->GetRenderTarget(i) == this)
//                graphics->ResetRenderTarget(i);
//        }
//
//        if (graphics->GetDepthStencil() == this)
//            graphics->ResetDepthStencil();
//    }
//
//    URHO3D_SAFE_RELEASE(renderTargetView_);
//    URHO3D_SAFE_RELEASE(readOnlyView_);
#endif
}

bool RenderSurface::CreateRenderBuffer(unsigned width, unsigned height, unsigned format, int multiSample)
{
#ifdef URHO3D_VULKAN_RENDERSURFACE
    // Pour Vulkan, on crée une image et une vue au lieu d'un renderbuffer
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics || !graphics->GetImpl())
        return false;
    
    VkDevice device = graphics->GetImpl()->GetDevice();
    const VkAllocationCallbacks* pAllocator = nullptr;
    
    // Créer l'image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = (VkFormat)format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = multiSample > 1 ? VK_SAMPLE_COUNT_2_BIT : VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vkCreateImage(device, &imageInfo, pAllocator, &renderBufferImage_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create render buffer image");
        return false;
    }
    
    // Allouer la mémoire
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, renderBufferImage_, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = graphics->GetImpl()->FindMemoryType(memRequirements.memoryTypeBits, 
                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    result = vkAllocateMemory(device, &allocInfo, pAllocator, &renderBufferMemory_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to allocate render buffer memory");
        vkDestroyImage(device, renderBufferImage_, pAllocator);
        return false;
    }
    
    vkBindImageMemory(device, renderBufferImage_, renderBufferMemory_, 0);
    
    // Créer la vue
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = renderBufferImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = (VkFormat)format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    result = vkCreateImageView(device, &viewInfo, pAllocator, &colorImageView_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create render buffer image view");
        vkDestroyImage(device, renderBufferImage_, pAllocator);
        vkFreeMemory(device, renderBufferMemory_, pAllocator);
        return false;
    }
    
    // Si multisampling avec auto-resolve, créer la vue de résolution
    if (multiSample > 1 && parentTexture_->GetAutoResolve())
    {
        CreateResolveImageView();
    }
    
    return true;
#else
    // Not used on Direct3D
    return false;
#endif
}

void RenderSurface::OnDeviceLost()
{
#ifdef URHO3D_VULKAN_RENDERSURFACE
    // Pour Vulkan, on nettoie les ressources
    Release();
#else
    // No-op on Direct3D
#endif
}

#ifdef URHO3D_VULKAN_RENDERSURFACE
VkFramebuffer RenderSurface::GetFramebuffer() const
{
    return framebuffer_;
}

VkRenderPass RenderSurface::GetRenderPass() const
{
    return renderPass_;
}

VkImageView RenderSurface::GetColorImageView() const
{
    // Si on a une image view spécifique pour la couleur, l'utiliser
    if (colorImageView_ != VK_NULL_HANDLE)
        return colorImageView_;
    
    // Sinon, essayer de récupérer l'image view de la texture parente
    if (parentTexture_)
    {
        // Pour Vulkan, l'imageView_ est stocké dans shaderResourceView_
        void* imageView = parentTexture_->GetShaderResourceView();
        if (imageView)
            return (VkImageView)imageView;
    }
    
    return VK_NULL_HANDLE;
}

VkImageView RenderSurface::GetDepthImageView() const
{
    // Si on a une image view spécifique pour la profondeur, l'utiliser
    if (depthImageView_ != VK_NULL_HANDLE)
        return depthImageView_;
    
    // Pour les textures de profondeur, essayer de récupérer l'image view de la texture parente
    if (parentTexture_ && parentTexture_->GetUsage() == TEXTURE_DEPTHSTENCIL)
    {
        void* imageView = parentTexture_->GetShaderResourceView();
        if (imageView)
            return (VkImageView)imageView;
    }
    
    return VK_NULL_HANDLE;
}

VkImageView RenderSurface::GetResolveImageView() const
{
    return resolveImageView_;
}

bool RenderSurface::CreateFramebuffer(VkRenderPass renderPass, const Vector<VkImageView>& attachments)
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics || !graphics->GetImpl())
        return false;
    
    VkDevice device = graphics->GetImpl()->GetDevice();
    const VkAllocationCallbacks* pAllocator = nullptr;
    
    // Créer le framebuffer
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = attachments.Size();
    framebufferInfo.pAttachments = attachments.Buffer();
    framebufferInfo.width = GetWidth();
    framebufferInfo.height = GetHeight();
    framebufferInfo.layers = 1;
    
    VkResult result = vkCreateFramebuffer(device, &framebufferInfo, pAllocator, &framebuffer_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create framebuffer");
        return false;
    }
    
    renderPass_ = renderPass;
    return true;
}

bool RenderSurface::CreateResolveImageView()
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics || !graphics->GetImpl())
        return false;
    
    VkDevice device = graphics->GetImpl()->GetDevice();
    const VkAllocationCallbacks* pAllocator = nullptr;
    
    // Créer une image de résolution (non-multisamplée)
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = GetWidth();
    imageInfo.extent.height = GetHeight();
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = (VkFormat)parentTexture_->GetFormat();
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkImage resolveImage;
    VkResult result = vkCreateImage(device, &imageInfo, pAllocator, &resolveImage);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create resolve image");
        return false;
    }
    
    // Allouer la mémoire
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, resolveImage, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = graphics->GetImpl()->FindMemoryType(memRequirements.memoryTypeBits, 
                                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VkDeviceMemory resolveMemory;
    result = vkAllocateMemory(device, &allocInfo, pAllocator, &resolveMemory);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to allocate resolve memory");
        vkDestroyImage(device, resolveImage, pAllocator);
        return false;
    }
    
    vkBindImageMemory(device, resolveImage, resolveMemory, 0);
    
    // Créer la vue de résolution
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = resolveImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = (VkFormat)parentTexture_->GetFormat();
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    result = vkCreateImageView(device, &viewInfo, pAllocator, &resolveImageView_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create resolve image view");
        vkDestroyImage(device, resolveImage, pAllocator);
        vkFreeMemory(device, resolveMemory, pAllocator);
        return false;
    }
    
    return true;
}

bool RenderSurface::CreateDepthImageView()
{
    Graphics* graphics = parentTexture_->GetGraphics();
    if (!graphics || !graphics->GetImpl())
        return false;
    
    VkDevice device = graphics->GetImpl()->GetDevice();
    const VkAllocationCallbacks* pAllocator = nullptr;
    
    // Créer la vue de profondeur pour la texture parent
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = (VkImage)parentTexture_->GetGPUObject(); // Supposant que la texture a une méthode GetGPUObject()
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = (VkFormat)parentTexture_->GetFormat();
    
    // Déterminer l'aspect mask selon le format
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (parentTexture_->GetFormat() == graphics->GetDepthStencilFormat())
        aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    
    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkResult result = vkCreateImageView(device, &viewInfo, pAllocator, &depthImageView_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERROR("Failed to create depth image view");
        return false;
    }
    
    return true;
}
#endif

}
