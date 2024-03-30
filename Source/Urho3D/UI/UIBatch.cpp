//
// Copyright (c) 2008-2015 the Urho3D project.
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

#include "../Precompiled.h"

#include "../IO/Log.h"

#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsImpl.h"
#include "../Graphics/Texture.h"
#include "../Graphics/VertexBuffer.h"
#include "../UI/UIElement.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* blendModeNames[];

Vector3 UIBatch::posAdjust(0.0f, 0.0f, 0.0f);
bool UIBatch::dumpBatchMerging_ = false;
Graphics* UIBatch::graphics_ = 0;
unsigned UIBatch::VERTEXSTRIDE = 0;
unsigned UIBatch::VERTEXSIZE = 0;
PODVector<VertexElement> UIBatch::vertexElements_;

UIBatch::UIBatch() :
    element_(0),
    blendMode_(BLEND_REPLACE),
    texture_(0),
    invTextureSize_(Vector2::ONE),
    vertexData_(0),
    vertexStart_(0),
    vertexEnd_(0)
{
    SetDefaultColor();
}

UIBatch::UIBatch(UIElement* element, BlendMode blendMode, const IntRect& scissor, Texture* texture, PODVector<float>* vertexData) :
    element_(element),
    blendMode_(blendMode),
    scissor_(scissor),
    texture_(texture),
    invTextureSize_(texture ? Vector2(1.0f / (float)texture->GetWidth(), 1.0f / (float)texture->GetHeight()) : Vector2::ONE),
    vertexData_(vertexData),
    vertexStart_(vertexData->Size()),
    vertexEnd_(vertexData->Size())
{
    SetDefaultColor();
}

void UIBatch::Initialize(Graphics* graphics)
{
    graphics_ = graphics;

    vertexElements_.Clear();

    vertexElements_.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
    vertexElements_.Push(VertexElement(TYPE_UBYTE4_NORM, SEM_COLOR));
    vertexElements_.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
    VertexBuffer::UpdateOffsets(vertexElements_, &VERTEXSIZE);

#ifdef URHO3D_VULKAN
    // Register Pipeline Infos
    ShaderVariation* noTextureVS = graphics_->GetShader(VS, "Basic", "VERTEXCOLOR");
    ShaderVariation* noTexturePS = graphics_->GetShader(PS, "Basic", "VERTEXCOLOR");

    ShaderVariation* diffTextureVS = graphics_->GetShader(VS, "Basic", "DIFFMAP VERTEXCOLOR");
    ShaderVariation* diffTexturePS = graphics_->GetShader(PS, "Basic", "DIFFMAP VERTEXCOLOR");
    ShaderVariation* diffMaskTexturePS = graphics_->GetShader(PS, "Basic", "ALPHAMASK DIFFMAP VERTEXCOLOR");
    ShaderVariation* alphaTexturePS = graphics_->GetShader(PS, "Basic", "ALPHAMAP VERTEXCOLOR");

    unsigned states[MAX_BLENDMODES];
    for (unsigned blendmode=0; blendmode < MAX_BLENDMODES; blendmode++)
        states[blendmode] = graphics_->GetImpl()->GetDefaultPipelineStates(PIPELINESTATE_BLENDMODE, blendmode);

    if (noTextureVS && noTexturePS)
    {
        graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, noTextureVS, noTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);
        graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, noTextureVS, noTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);

        graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, noTextureVS, noTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
        graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, noTextureVS, noTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
    }

    if (diffTextureVS)
    {
        if (diffTexturePS)
        {
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
        }
        if (diffMaskTexturePS)
        {
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ADD], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ADDALPHA], 1, &UIBatch::vertexElements_);

            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_REPLACE], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ADD], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, diffMaskTexturePS, states[BLEND_ADDALPHA], 1, &UIBatch::vertexElements_);
        }
        if (alphaTexturePS)
        {
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_1C_1DS, diffTextureVS, alphaTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
            graphics_->GetImpl()->RegisterPipelineInfo(GraphicsImpl::RenderPass_2C_1DS, diffTextureVS, alphaTexturePS, states[BLEND_ALPHA], 1, &UIBatch::vertexElements_);
        }
    }
#endif

    VERTEXSTRIDE = VERTEXSIZE / sizeof(float);
}

void UIBatch::SetColor(const Color& color, bool overrideAlpha)
{
    if (!element_)
        overrideAlpha = true;

    useGradient_ = false;
    color_ =
        overrideAlpha ? color.ToUInt() : Color(color.r_, color.g_, color.b_, color.a_ * element_->GetDerivedOpacity()).ToUInt();
}

void UIBatch::SetDefaultColor()
{
    if (element_)
    {
        color_ = element_->GetDerivedColor().ToUInt();
        useGradient_ = element_->HasColorGradient();
    }
    else
    {
        color_ = 0xffffffff;
        useGradient_ = false;
    }
}

void UIBatch::AddQuad(int x, int y, int width, int height, int texOffsetX, int texOffsetY, int texWidth, int texHeight)
{
    unsigned topLeftColor, topRightColor, bottomLeftColor, bottomRightColor;

    if (!useGradient_)
    {
        // If alpha is 0, nothing will be rendered, so do not add the quad
        if (!(color_ & 0xff000000))
            return;

        topLeftColor = color_;
        topRightColor = color_;
        bottomLeftColor = color_;
        bottomRightColor = color_;
    }
    else
    {
        topLeftColor = GetInterpolatedColor(x, y);
        topRightColor = GetInterpolatedColor(x + width, y);
        bottomLeftColor = GetInterpolatedColor(x, y + height);
        bottomRightColor = GetInterpolatedColor(x + width, y + height);
    }

    const IntVector2& screenPos = element_ ? element_->GetScreenPosition() : IntVector2::ZERO;

    float left = (float)(x + screenPos.x_) - posAdjust.x_;
    float right = left + (float)width;
    float top = (float)(y + screenPos.y_) - posAdjust.x_;
    float bottom = top + (float)height;

    float leftUV = texOffsetX * invTextureSize_.x_;
    float topUV = texOffsetY * invTextureSize_.y_;
    float rightUV = (texOffsetX + (texWidth ? texWidth : width)) * invTextureSize_.x_;
    float bottomUV = (texOffsetY + (texHeight ? texHeight : height)) * invTextureSize_.y_;

    unsigned begin = vertexData_->Size();

    vertexData_->Resize(begin + 6 * VERTEXSTRIDE);
    float* dest = &(vertexData_->At(begin));
    vertexEnd_ = vertexData_->Size();

    unsigned i = 0;
    dest[i] = left;
    dest[i+1] = top;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = right;
    dest[i+1] = top;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = left;
    dest[i+1] = bottom;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = bottomUV;

    i += VERTEXSTRIDE;
    dest[i] = right;
    dest[i+1] = top;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = right;
    dest[i+1] = bottom;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = bottomUV;

    i += VERTEXSTRIDE;
    dest[i] = left;
    dest[i+1] = bottom;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = bottomUV;
}

void UIBatch::AddQuad(const Matrix3x4& transform, int x, int y, int width, int height, int texOffsetX, int texOffsetY,
    int texWidth, int texHeight)
{
    unsigned topLeftColor, topRightColor, bottomLeftColor, bottomRightColor;

    if (!useGradient_)
    {
        // If alpha is 0, nothing will be rendered, so do not add the quad
        if (!(color_ & 0xff000000))
            return;

        topLeftColor = color_;
        topRightColor = color_;
        bottomLeftColor = color_;
        bottomRightColor = color_;
    }
    else
    {
        topLeftColor = GetInterpolatedColor(x, y);
        topRightColor = GetInterpolatedColor(x + width, y);
        bottomLeftColor = GetInterpolatedColor(x, y + height);
        bottomRightColor = GetInterpolatedColor(x + width, y + height);
    }

    Vector3 v1 = (transform * Vector3((float)x, (float)y, 0.0f)) - posAdjust;
    Vector3 v2 = (transform * Vector3((float)x + (float)width, (float)y, 0.0f)) - posAdjust;
    Vector3 v3 = (transform * Vector3((float)x, (float)y + (float)height, 0.0f)) - posAdjust;
    Vector3 v4 = (transform * Vector3((float)x + (float)width, (float)y + (float)height, 0.0f)) - posAdjust;

    float leftUV = ((float)texOffsetX) * invTextureSize_.x_;
    float topUV = ((float)texOffsetY) * invTextureSize_.y_;
    float rightUV = ((float)(texOffsetX + (texWidth ? texWidth : width))) * invTextureSize_.x_;
    float bottomUV = ((float)(texOffsetY + (texHeight ? texHeight : height))) * invTextureSize_.y_;

    unsigned begin = vertexData_->Size();
    vertexData_->Resize(begin + 6 * VERTEXSTRIDE);
    float* dest = &(vertexData_->At(begin));
    vertexEnd_ = vertexData_->Size();

    unsigned i = 0;
    dest[i] = v1.x_;
    dest[i+1] = v1.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = v2.x_;
    dest[i+1] = v2.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = v3.x_;
    dest[i+1] = v3.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = bottomUV;

    i += VERTEXSTRIDE;
    dest[i] = v2.x_;
    dest[i+1] = v2.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = topRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = topUV;

    i += VERTEXSTRIDE;
    dest[i] = v4.x_;
    dest[i+1] = v4.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomRightColor;
    dest[i+4] = rightUV;
    dest[i+5] = bottomUV;

    i += VERTEXSTRIDE;
    dest[i] = v3.x_;
    dest[i+1] = v3.y_;
    dest[i+2] = 0.0f;
    ((unsigned&)dest[i+3]) = bottomLeftColor;
    dest[i+4] = leftUV;
    dest[i+5] = bottomUV;
}

void UIBatch::AddQuad(int x, int y, int width, int height, int texOffsetX, int texOffsetY, int texWidth, int texHeight, bool tiled)
{
    if (!(element_->HasColorGradient() || element_->GetDerivedColor().ToUInt() & 0xff000000))
        return; // No gradient and alpha is 0, so do not add the quad

    if (!tiled)
    {
        AddQuad(x, y, width, height, texOffsetX, texOffsetY, texWidth, texHeight);
        return;
    }

    int tileX = 0;
    int tileY = 0;
    int tileW = 0;
    int tileH = 0;

    while (tileY < height)
    {
        tileX = 0;
        tileH = Min(height - tileY, texHeight);

        while (tileX < width)
        {
            tileW = Min(width - tileX, texWidth);

            AddQuad(x + tileX, y + tileY, tileW, tileH, texOffsetX, texOffsetY, tileW, tileH);

            tileX += tileW;
        }

        tileY += tileH;
    }
}

bool UIBatch::Merge(const UIBatch& batch, unsigned counter)
{
    if (batch.blendMode_ != blendMode_ ||
        batch.scissor_ != scissor_ ||
        batch.texture_ != texture_ ||
        batch.vertexData_ != vertexData_ ||
        batch.vertexStart_ != vertexEnd_)
    {
        if (dumpBatchMerging_)
        {
            String blendstr("OK");
            String scissstr("OK");
            String textstr;

            if (batch.blendMode_ != blendMode_)
                blendstr.AppendWithFormat("(%s!=%s)", blendModeNames[blendMode_], blendModeNames[batch.blendMode_]);
            else
                blendstr.AppendWithFormat("(%s)", blendModeNames[blendMode_]);

            if (batch.scissor_ != scissor_)
            {
                scissstr.Clear();
                scissstr.AppendWithFormat("(%s!=%s)", scissor_.ToString().CString(), batch.scissor_.ToString().CString());
            }

            if (batch.texture_ != texture_)
                textstr.AppendWithFormat("(%s!=%s)", String((void*)texture_).CString(), String((void*)batch.texture_).CString());
            else
                textstr.AppendWithFormat("(%s)", String((void*)texture_).CString());

            URHO3D_LOGWARNINGF("UIBatch() - Merge : count=%u can not be merged blend=%s scis=%s tex=%s vdata=%s vstart=%s elt1=%s elt2=%s!",
                               counter, blendstr.CString(), scissstr.CString(), textstr.CString(),
                               batch.vertexData_ != vertexData_ ? "Diff":"OK", batch.vertexStart_ != vertexEnd_ ? "Diff":"OK", element_->GetName().CString(), batch.element_->GetName().CString());
        }

        return false;
    }

    vertexEnd_ = batch.vertexEnd_;
    return true;
}

unsigned UIBatch::GetInterpolatedColor(int x, int y)
{
    const IntVector2& size = element_->GetSize();

    if (size.x_ && size.y_)
    {
        float cLerpX = Clamp((float)x / (float)size.x_, 0.0f, 1.0f);
        float cLerpY = Clamp((float)y / (float)size.y_, 0.0f, 1.0f);

        Color topColor = element_->GetColor(C_TOPLEFT).Lerp(element_->GetColor(C_TOPRIGHT), cLerpX);
        Color bottomColor = element_->GetColor(C_BOTTOMLEFT).Lerp(element_->GetColor(C_BOTTOMRIGHT), cLerpX);
        Color color = topColor.Lerp(bottomColor, cLerpY);
        color.a_ *= element_->GetDerivedOpacity();
        return color.ToUInt();
    }
    else
    {
        Color color = element_->GetColor(C_TOPLEFT);
        color.a_ *= element_->GetDerivedOpacity();
        return color.ToUInt();
    }
}

void UIBatch::AddOrMerge(const UIBatch& batch, PODVector<UIBatch>& batches)
{
    if (batch.vertexEnd_ == batch.vertexStart_)
        return;

    if (!batches.Empty() && batches.Back().Merge(batch, batches.Size()))
        return;

    batches.Push(batch);
}

}
