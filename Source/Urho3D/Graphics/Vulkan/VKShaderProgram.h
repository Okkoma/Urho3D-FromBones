//
// Copyright (c) 2008-2017 the Urho3D project.
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

#pragma once

#include "../../IO/Log.h"

#include "VKGraphicsImpl.h"

#include "../../Container/HashMap.h"
#include "../../Graphics/ConstantBuffer.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/ShaderVariation.h"

namespace Urho3D
{

const unsigned ConstantBufferMaxObjects[MAX_SHADERTYPES][MAX_SHADER_PARAMETER_GROUPS] =
{
    {         // VS
        0,    // SP_FRAME
        32,   // SP_CAMERA
        0,    // SP_ZONE
        20,   // SP_LIGHT
        0,    // SP_MATERIAL
        400,  // SP_OBJECT
        0,    // SP_CUSTOM
    },
    {         // PS
        0,    // SP_FRAME
        0,    // SP_CAMERA
        0,    // SP_ZONE
        200,  // SP_LIGHT
        0,    // SP_MATERIAL
        32,   // SP_OBJECT
        0,    // SP_CUSTOM
    },
};
/// Combined information for specific vertex and pixel shaders.
class URHO3D_API ShaderProgram : public RefCounted
{
public:
    /// Construct.
    ShaderProgram(Graphics* graphics, ShaderVariation* vertexShader, ShaderVariation* pixelShader);

    /// Destruct.
    ~ShaderProgram()
    {
    }

    /// Combined parameters from the vertex and pixel shader.
    HashMap<StringHash, ShaderParameter> parameters_;
    /// Vertex shader constant buffers.
    SharedPtr<ConstantBuffer> vsConstantBuffers_[MAX_SHADER_PARAMETER_GROUPS];
    /// Pixel shader constant buffers.
    SharedPtr<ConstantBuffer> psConstantBuffers_[MAX_SHADER_PARAMETER_GROUPS];
};

}
