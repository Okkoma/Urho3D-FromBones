//
// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2022-2025 - Christophe VILLE.
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
#include "../../Graphics/Shader.h"
#include "../../Graphics/VertexBuffer.h"
#include "../../IO/File.h"
#include "../../IO/FileSystem.h"
#include "../../IO/Log.h"
#include "../../Resource/ResourceCache.h"

#include "../../DebugNew.h"

namespace Urho3D
{

void ShaderVariation::OnDeviceLost()
{
    // No-op on Vulkan
}

bool ShaderVariation::Create()
{
    Release();

    if (!graphics_)
        return false;

    if (!LoadByteCode(String::EMPTY))
    {
        URHO3D_LOGERRORF("ShaderVariation() - Create : this=%u Error Loading ByteCode for shader=%s defines=%s", this, name_.CString(), defines_.CString());
        return false;
    }

    return true;
}

void ShaderVariation::Release()
{
    if (!graphics_)
        return;

    graphics_->CleanupShaderPrograms(this);

    if (type_ == VS)
    {
        if (graphics_->GetVertexShader() == this)
            graphics_->SetShaders(0, 0);
    }
    else
    {
        if (graphics_->GetPixelShader() == this)
            graphics_->SetShaders(0, 0);
    }

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
        useTextureUnit_[i] = false;
    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
        constantBufferSizes_[i] = 0;

    parameters_.Clear();
    byteCode_.Clear();
    elementHash_ = 0;
}

void ShaderVariation::SetDefines(const String& defines)
{
    defines_ = defines;

    // Internal mechanism for appending the CLIPPLANE define, prevents runtime (every frame) string manipulation
    definesClipPlane_ = defines;
    if (!definesClipPlane_.EndsWith(" CLIPPLANE"))
        definesClipPlane_ += " CLIPPLANE";

    variationHash_ = StringHash(name_ + (type_ == VS ? "_VS_" : "_PS_") + defines_);
}

bool ShaderVariation::LoadByteCode(const String& )
{
    if (!owner_)
    {
        URHO3D_LOGERROR("LoadByteCode : Error => no owner !");
        return false;
    }

    ResourceCache* cache = owner_->GetSubsystem<ResourceCache>();

    String folder = "Shaders/Vulkan/";
    String fileName = GetCachedFileName() + (type_ == VS ? ".vs5" : ".ps5");

    // first check if shader exists in distribution folders
    if (!cache->Exists(folder + fileName))
    {
        folder = graphics_->GetShaderCacheDir();
        if (!cache->Exists(folder + fileName))
        {
            URHO3D_LOGERROR(fileName + " not found !");
            return false;
        }
    }

    fileName = folder + fileName;

    URHO3D_LOGDEBUGF("LoadByteCode : %s", fileName.CString());

    SharedPtr<File> file = cache->GetFile(fileName);
    if (file->ReadFileID() != "USHD")
    {
        URHO3D_LOGERROR(fileName + " is not a valid shader bytecode file");
        return false;
    }

    // load metadata
    unsigned short shaderType = file->ReadUShort();
    if (shaderType != (unsigned short)type_)
    {
        URHO3D_LOGERROR(fileName + " is not a shader of type");
        return false;
    }

    unsigned short shaderModel = file->ReadUShort();
    if (shaderModel != 5)
    {
        URHO3D_LOGERROR(fileName + " is not a vulkan shader");
        return false;
    }

    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
        useTextureUnit_[i] = false;

    elementHash_ = file->ReadUInt();
    elementHash_ <<= 32;

    // load DescriptorsSets Structure (used sets and used bindings by set)
    descriptorStructure_.Clear();
    VkShaderStageFlagBits stageflag = type_ == VS ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    unsigned numsets = file->ReadUInt();
    for (unsigned i = 0; i < numsets; i++)
    {
        unsigned setid = file->ReadUByte();
        HashMap<unsigned, ShaderBind >& bindings = descriptorStructure_[setid];

        unsigned char numbinds = file->ReadUByte();
        for (unsigned j = 0; j < numbinds; j++)
        {
            unsigned char bindid = file->ReadUByte();
            ShaderBind& binding = bindings[bindid];
            binding.id_ = bindid;
            binding.stageFlag_ = stageflag;
            binding.type_ = file->ReadUByte();
            binding.unitStart_ = file->ReadUByte();
            binding.unitRange_ = file->ReadUByte();
        }
    }

    // load parameters
    unsigned numParameters = file->ReadUInt();
    for (unsigned i = 0; i < numParameters; ++i)
    {
        String name = file->ReadString();

        ShaderParameter& parameter = parameters_[StringHash(name)];
        parameter.type_ = type_;
        parameter.name_ = name;
        parameter.buffer_ = file->ReadUByte();
        parameter.offset_ = file->ReadUInt();
        parameter.size_ = file->ReadUInt();
    }

    // load texture units
    unsigned numTextureUnits = file->ReadUInt();
    URHO3D_LOGDEBUGF("Num Texture Units Used=%u ", numTextureUnits);
    for (unsigned i = 0; i < numTextureUnits; ++i)
    {
        String unitName = file->ReadString();
        unsigned unit = file->ReadUByte();

        if (unit < MAX_TEXTURE_UNITS)
        {
            URHO3D_LOGDEBUGF("Use Texture Unit=%u ", unit);
            useTextureUnit_[unit] = true;
        }
    }

    // load raw bytecode
    unsigned byteCodeSize = file->ReadUInt();
    if (!byteCodeSize)
    {
        URHO3D_LOGERROR(fileName + " has zero length bytecode");
        return false;
    }

    byteCode_.Resize(byteCodeSize);
    file->Read(byteCode_.Buffer(), byteCodeSize);

    if (type_ == VS)
        URHO3D_LOGDEBUG("Loaded cached vertex shader " + GetFullName() + " variationName=" + name_ + "_VS_" + defines_ + " CachedName=" + GetCachedFileName());
    else
        URHO3D_LOGDEBUG("Loaded cached pixel shader " + GetFullName() + " variationName=" + name_ + "_PS_" + defines_ + " CachedName=" + GetCachedFileName());

    CalculateConstantBufferSizes();

    return true;
}

bool ShaderVariation::Compile()
{
    // No-op on Vulkan
    // the benefit is to precompile shaders in Spirv Bytecode.
    // => Use External Urho3D Tool SpirvShaderPacker for packing the shader with the necessary metadatas (vertex attributes, textures units, shader parameters)
    return false;
}

void ShaderVariation::ParseParameters(unsigned char* bufData, unsigned bufSize)
{
    // No-op on Vulkan
}

void ShaderVariation::SaveByteCode(const String& binaryShaderName)
{
    // No-op on Vulkan
}

void ShaderVariation::CalculateConstantBufferSizes()
{
    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
        constantBufferSizes_[i] = 0;

    for (HashMap<StringHash, ShaderParameter>::ConstIterator i = parameters_.Begin(); i != parameters_.End(); ++i)
    {
        if (i->second_.buffer_ < MAX_SHADER_PARAMETER_GROUPS)
        {
            unsigned oldSize = constantBufferSizes_[i->second_.buffer_];
            // round size to 16bytes
            unsigned size = i->second_.size_ + 15;
            size &= 0xfffffff0;
            unsigned paramEnd = i->second_.offset_ + size;
            if (paramEnd > oldSize)
                constantBufferSizes_[i->second_.buffer_] = paramEnd;
        }
    }
}

}
