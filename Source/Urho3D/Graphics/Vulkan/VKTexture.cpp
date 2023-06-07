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

#include "../../Core/Profiler.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/Material.h"
#include "../../IO/FileSystem.h"
#include "../../IO/Log.h"
#include "../../Resource/ResourceCache.h"
#include "../../Resource/XMLFile.h"

#include "../../DebugNew.h"

namespace Urho3D
{

const VkFilter VulkanFilterMode[] =
{
    VK_FILTER_NEAREST,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_LINEAR,
    VK_FILTER_NEAREST,
};

const VkSamplerAddressMode VulkanAddressMode[] =
{
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
};

void Texture::SetSRGB(bool enable)
{
#ifndef DISABLE_SRGB
    if (graphics_)
        enable &= graphics_->GetSRGBSupport();

    if (enable != sRGB_)
    {
        sRGB_ = enable;
        // If texture had already been created, must recreate it to set the sRGB texture format
        if (object_.buffer_)
            Create();
    }
#else
    enable = false;
#endif
}

bool Texture::GetParametersDirty() const
{
    return parametersDirty_ || !sampler_;
}

bool Texture::IsCompressed() const
{
//    return format_ == DXGI_FORMAT_BC1_UNORM || format_ == DXGI_FORMAT_BC2_UNORM || format_ == DXGI_FORMAT_BC3_UNORM;
    return false;
}

unsigned Texture::GetRowDataSize(int width) const
{
//    switch (format_)
//    {
//    case DXGI_FORMAT_R8_UNORM:
//    case DXGI_FORMAT_A8_UNORM:
//        return (unsigned)width;
//
//    case DXGI_FORMAT_R8G8_UNORM:
//    case DXGI_FORMAT_R16_UNORM:
//    case DXGI_FORMAT_R16_FLOAT:
//    case DXGI_FORMAT_R16_TYPELESS:
//        return (unsigned)(width * 2);
//
//    case DXGI_FORMAT_R8G8B8A8_UNORM:
//    case DXGI_FORMAT_R16G16_UNORM:
//    case DXGI_FORMAT_R16G16_FLOAT:
//    case DXGI_FORMAT_R32_FLOAT:
//    case DXGI_FORMAT_R24G8_TYPELESS:
//    case DXGI_FORMAT_R32_TYPELESS:
//        return (unsigned)(width * 4);
//
//    case DXGI_FORMAT_R16G16B16A16_UNORM:
//    case DXGI_FORMAT_R16G16B16A16_FLOAT:
//        return (unsigned)(width * 8);
//
//    case DXGI_FORMAT_R32G32B32A32_FLOAT:
//        return (unsigned)(width * 16);
//
//    case DXGI_FORMAT_BC1_UNORM:
//        return (unsigned)(((width + 3) >> 2) * 8);
//
//    case DXGI_FORMAT_BC2_UNORM:
//    case DXGI_FORMAT_BC3_UNORM:
//        return (unsigned)(((width + 3) >> 2) * 16);
//
//    default:
//        return 0;
//    }

    return 0;
}

void Texture::UpdateParameters()
{
    if ((!parametersDirty_ && sampler_) || !object_.buffer_)
        return;

    vkDestroySampler(graphics_->GetImpl()->GetDevice(), (VkSampler)sampler_, nullptr);

    // Create Sampler
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
//    samplerInfo.magFilter               = VK_FILTER_NEAREST;
//    samplerInfo.minFilter               = VK_FILTER_NEAREST;
//    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
//    samplerInfo.magFilter               = VK_FILTER_LINEAR;
//    samplerInfo.minFilter               = VK_FILTER_LINEAR;
//    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.magFilter               = VulkanFilterMode[filterMode_ != FILTER_DEFAULT ? filterMode_ : graphics_->GetDefaultTextureFilterMode()];
    samplerInfo.minFilter               = samplerInfo.magFilter;
    samplerInfo.mipmapMode              = samplerInfo.minFilter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.f;
    samplerInfo.minLod                  = 0.f;
    samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE; // no lod clamping for use with immuablesampler : use always a maximal mip levels //static_cast<float>(levels_);
    samplerInfo.addressModeU            = VulkanAddressMode[addressMode_[0]];
    samplerInfo.addressModeV            = VulkanAddressMode[addressMode_[1]];
    samplerInfo.addressModeW            = VulkanAddressMode[addressMode_[2]];
    samplerInfo.anisotropyEnable        = anisotropy_ ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy           = Min(anisotropy_ ? anisotropy_ : graphics_->GetDefaultTextureAnisotropy(), graphics_->GetImpl()->GetPhysicalDeviceInfo().properties_.limits.maxSamplerAnisotropy);
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;//VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_LESS_OR_EQUAL;//VK_COMPARE_OP_ALWAYS;

    VkResult result = vkCreateSampler(graphics_->GetImpl()->GetDevice(), &samplerInfo, nullptr, (VkSampler*)&sampler_);
    if (result != VK_SUCCESS)
    {
        URHO3D_LOGERRORF("Can't create texture sampler for shader use");
        return;
    }

    URHO3D_LOGDEBUGF("Texture - UpdateParameters : name=%s imageview=%u sampler=%u addressMode=(%u,%u,%u) anisotropy=%u mag=%u min=%u mipmap=%u",
                     GetName().CString(), GetShaderResourceView(), GetSampler(),
                     addressMode_[0], addressMode_[1], addressMode_[2], anisotropy_,
                     samplerInfo.magFilter, samplerInfo.minFilter, samplerInfo.mipmapMode);

    parametersDirty_ = false;
}

unsigned Texture::GetSRVFormat(unsigned format)
{
//    if (format == DXGI_FORMAT_R24G8_TYPELESS)
//        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
//    else if (format == DXGI_FORMAT_R16_TYPELESS)
//        return DXGI_FORMAT_R16_UNORM;
//    else if (format == DXGI_FORMAT_R32_TYPELESS)
//        return DXGI_FORMAT_R32_FLOAT;
//    else
        return format;
}

unsigned Texture::GetDSVFormat(unsigned format)
{
//    if (format == DXGI_FORMAT_R24G8_TYPELESS)
//        return DXGI_FORMAT_D24_UNORM_S8_UINT;
//    else if (format == DXGI_FORMAT_R16_TYPELESS)
//        return DXGI_FORMAT_D16_UNORM;
//    else if (format == DXGI_FORMAT_R32_TYPELESS)
//        return DXGI_FORMAT_D32_FLOAT;
//    else
        return format;
}

unsigned Texture::GetSRGBFormat(unsigned format)
{
    if (!graphics_ || !graphics_->GetSRGBSupport())
        return format;

    switch (format)
    {
    case VK_FORMAT_R8_UNORM:
        return VK_FORMAT_R8_SRGB;
    case VK_FORMAT_R8G8_UNORM:
        return VK_FORMAT_R8G8_SRGB;
    case VK_FORMAT_R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_SRGB;

    default:
        return format;
    }
}

void Texture::RegenerateLevels()
{
//    if (!shaderResourceView_)
//        return;
//
//    graphics_->GetImpl()->GetDeviceContext()->GenerateMips((ID3D11ShaderResourceView*)shaderResourceView_);
//    levelsDirty_ = false;
}

}
