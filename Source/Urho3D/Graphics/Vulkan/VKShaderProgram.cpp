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

#include "../../Precompiled.h"

#include "../../Core/Context.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/Vulkan/VKShaderProgram.h"
#include "../../IO/Log.h"

#include "../../DebugNew.h"

namespace Urho3D
{

ShaderProgram::ShaderProgram(Graphics* graphics, ShaderVariation* vertexShader, ShaderVariation* pixelShader)
{
    URHO3D_LOGDEBUGF("ShaderProgram : %s vs=%u ps=%u ", vertexShader->GetName().CString(), vertexShader->GetVariationHash().Value(), pixelShader->GetVariationHash().Value());

    // Create needed constant buffers
    const unsigned* vsBufferSizes = vertexShader->GetConstantBufferSizes();
    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
    {
        unsigned maxObjects = ConstantBufferMaxObjects[VS][i];
        unsigned size = maxObjects ? graphics->GetImpl()->GetUBOPaddedSize(vsBufferSizes[i]) * maxObjects : vsBufferSizes[i];

        if (vsBufferSizes[i])
        {
            URHO3D_LOGDEBUGF("ShaderProgram : VS get or create constantbuffer group=%u size=%u", i, size);
            vsConstantBuffers_[i] = graphics->GetOrCreateConstantBuffer(VS, (i << 27) | (vertexShader->GetVariationHash().Value() & 0x7ffffff), size);
            vsConstantBuffers_[i]->SetNumObjects(maxObjects);
        }
    }

    const unsigned* psBufferSizes = pixelShader->GetConstantBufferSizes();
    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
    {
        unsigned maxObjects = ConstantBufferMaxObjects[PS][i];
        unsigned size = maxObjects ? graphics->GetImpl()->GetUBOPaddedSize(psBufferSizes[i]) * maxObjects : psBufferSizes[i];

        if (psBufferSizes[i])
        {
            URHO3D_LOGDEBUGF("ShaderProgram : PS get or create constantbuffer group=%u size=%u", i, size);

            psConstantBuffers_[i] = graphics->GetOrCreateConstantBuffer(PS, (i << 27) | (pixelShader->GetVariationHash().Value() & 0x7ffffff), size);
            psConstantBuffers_[i]->SetNumObjects(maxObjects);
        }
    }

    // Copy parameters, add direct links to constant buffers
    const HashMap<StringHash, ShaderParameter>& vsParams = vertexShader->GetParameters();
    for (HashMap<StringHash, ShaderParameter>::ConstIterator i = vsParams.Begin(); i != vsParams.End(); ++i)
    {
        parameters_[i->first_] = i->second_;
        parameters_[i->first_].bufferPtr_ = vsConstantBuffers_[i->second_.buffer_].Get();
    }

    const HashMap<StringHash, ShaderParameter>& psParams = pixelShader->GetParameters();
    for (HashMap<StringHash, ShaderParameter>::ConstIterator i = psParams.Begin(); i != psParams.End(); ++i)
    {
        parameters_[i->first_] = i->second_;
        parameters_[i->first_].bufferPtr_ = psConstantBuffers_[i->second_.buffer_].Get();
    }

    // Optimize shader parameter lookup by rehashing to next power of two
    parameters_.Rehash(NextPowerOfTwo(parameters_.Size()));
}

}
