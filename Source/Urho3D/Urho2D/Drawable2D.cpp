//
// Copyright (c) 2008-2016 the Urho3D project.
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
#include "../Core/Context.h"

#include "../Graphics/Camera.h"
#include "../Graphics/Material.h"
#include "../Graphics/Texture2D.h"
#include "../Scene/Scene.h"
#include "../Urho2D/Drawable2D.h"
#include "../Urho2D/Renderer2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

const float PIXEL_SIZE = 0.01f;

/// TextureMode HardCoding For VULKAN
const unsigned TEXTUREMODEMASK[] =
{
    0x0000000F, // TXM_UNIT          : 0000001111
    0xFFFFFFF0, // TXM_FX            : 1111110000
    0x00000010, // TXM_FX_LIT        : 0000010000
    0x00000020, // TXM_FX_CROPALPHA  : 0000100000
    0x00000040, // TXM_FX_BLUR       : 0001000000
    0x00000080, // TXM_FX_FXAA       : 0010000000
    0x00000300, // TXM_FX_TILEINDEX  : 1100000000
};

const unsigned TEXTUREMODEOFFSET[] =
{
    0, // TXM_UNIT
    4, // TXM_FX
    4, // TXM_FX_LIT
    5, // TXM_FX_CROPALPHA
    6, // TXM_FX_BLUR
    7, // TXM_FX_FXAA
    8, // TXM_FX_TILEINDEX
};

SourceBatch2D::SourceBatch2D() :
    distance_(0.0f),
    drawOrder_(0),
    quadvertices_(true)
{
}

Drawable2D::Drawable2D(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY2D),
    layer_(IntVector2(0,-1)),
    layerModifier_(0),
    orderInLayer_(0),
    textureFX_(0),
    sourceBatchesDirty_(false),
    drawRect_(Rect::ZERO),
    drawRectDirty_(true),
    visibility_(true),
    isSourceBatchedAtEnd_(false)
{
    worldBoundingBox_.min_.z_ = 0.f;
    worldBoundingBox_.max_.z_ = 1.f;

    enableDebugLog_ = false;
}

Drawable2D::~Drawable2D()
{
    if (renderer_)
        renderer_->RemoveDrawable(this);
}

void Drawable2D::RegisterObject(Context* context)
{
    URHO3D_ACCESSOR_ATTRIBUTE("Layer", GetLayer2, SetLayer2, IntVector2, IntVector2(0,-1), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Layer Modifier", GetLayerModifier, SetLayerModifier, int, 0, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Order in Layer", GetOrderInLayer, SetOrderInLayer, int, 0, AM_DEFAULT);
    URHO3D_ATTRIBUTE("View Mask", int, viewMask_, DEFAULT_VIEWMASK, AM_DEFAULT);
    URHO3D_ATTRIBUTE("TextureFx", int, textureFX_, 0, AM_DEFAULT);
}

void Drawable2D::OnSetEnabled()
{
    bool enabled = IsEnabledEffective();

    if (enabled)
    {
        worldBoundingBoxDirty_ = visibility_ = true;
        if (renderer_)
            renderer_->AddDrawable(this);
    }
    else
    {
        if (renderer_)
            renderer_->RemoveDrawable(this);

        sourceBatchesDirty_ = worldBoundingBoxDirty_ = visibility_ = false;
//        worldBoundingBoxDirty_ = visibility_ = false;
        ClearSourceBatches();
    }
}

void Drawable2D::SetLayer(int layer)
{
    if (layer == layer_.x_)
        return;

    layer_.x_ = layer;

    OnDrawOrderChanged();
    MarkNetworkUpdate();
}

void Drawable2D::SetLayer2(const IntVector2& layer)
{
    if (layer == layer_)
        return;

    layer_ = layer;

    OnDrawOrderChanged();
    MarkNetworkUpdate();
}

void Drawable2D::SetLayerModifier(int layermodifier)
{
    if (layermodifier == layerModifier_)
        return;

    layerModifier_ = layermodifier;

    OnDrawOrderChanged();
    MarkNetworkUpdate();
}

void Drawable2D::SetOrderInLayer(int orderInLayer)
{
    if (orderInLayer == orderInLayer_)
        return;

    orderInLayer_ = orderInLayer;

    OnDrawOrderChanged();
    MarkNetworkUpdate();
}

/// For VULKAN
void Drawable2D::SetTextureMode(TextureModeFlag flag, unsigned value, unsigned& texmode)
{
    texmode &= ~TEXTUREMODEMASK[flag];
    texmode |= (value << TEXTUREMODEOFFSET[flag]);
}

unsigned Drawable2D::GetTextureMode(TextureModeFlag flag, unsigned texmode)
{
    return ((texmode & TEXTUREMODEMASK[flag]) >> TEXTUREMODEOFFSET[flag]);
}

/// For OPENGL
void Drawable2D::SetTextureMode(TextureModeFlag flag, unsigned value, Vector4& texmode)
{
    if (flag == TXM_UNIT)
    {
        texmode.x_ = value & 0xF;
    }
    else if (flag == TXM_FX)
    {
        texmode.y_ = value & 0x1; // bit 0
        texmode.z_ = (value & 0xE) >> 1; // bit 1-2-3
        texmode.w_ = value >> 4; // bit 4-5
    }
    else if (flag == TXM_FX_LIT)
    {
        texmode.y_ = value & 0x1; // bit 0
    }
}

unsigned Drawable2D::GetTextureMode(TextureModeFlag flag, const Vector4& texmode)
{
    if (flag == TXM_UNIT)
    {
        return texmode.x_;
    }
    else if (flag == TXM_FX)
    {
        return texmode.y_;
    }
    else if (flag == TXM_FX_LIT)
    {
        return unsigned(texmode.y_) & 0x1;
    }

    return 0;
}

const Rect& Drawable2D::GetDrawRectangle()
{
    bool ok = UpdateDrawRectangle();
    return drawRect_;
}

BoundingBox Drawable2D::GetWorldBoundingBox2D()
{
    return Drawable::GetWorldBoundingBox();
}

bool Drawable2D::UpdateDrawRectangle()
{
    drawRectDirty_ = false;
    return true;
}

void Drawable2D::ForceUpdateBatches()
{
    sourceBatchesDirty_ = drawRectDirty_ = true;

//    URHO3D_LOGINFOF("Drawable2D() - ForceUpdateBatches : node=%s(%u) ... !", node_->GetName().CString(), node_->GetID());

    UpdateSourceBatchesToRender(0);
    if (layer_.y_ != -1)
        UpdateSourceBatchesToRender(1);

    if (drawRect_.Defined() && worldBoundingBoxDirty_)
    {
        Rect worldDrawRect = drawRect_.Transformed(node_->GetWorldTransform2D());
        worldBoundingBox_.min_.x_ = worldDrawRect.min_.x_;
        worldBoundingBox_.min_.y_ = worldDrawRect.min_.y_;
        worldBoundingBox_.max_.x_ = worldDrawRect.max_.x_;
        worldBoundingBox_.max_.y_ = worldDrawRect.max_.y_;
        worldBoundingBoxDirty_ = false;
    }
}

void Drawable2D::ClearSourceBatches()
{
//    URHO3D_LOGINFOF("Drawable2D() - ClearSourceBatches : node=%s(%u) enabled=%s visibility_=%s ",
//             node_->GetName().CString(), node_->GetID(), IsEnabledEffective() ? "true" : "false", visibility_ ? "true" : "false");

    for (unsigned j=0; j < 2; j++)
    {
        sourceBatchesToRender_[j].Clear();
        for (unsigned i=0; i < sourceBatches_[j].Size(); i++)
            sourceBatches_[j][i].vertices_.Clear();
    }
}

void Drawable2D::UpdateSourceBatchesToRender(int id)
{
    UpdateSourceBatches();

    sourceBatchesToRender_[id].Clear();
    for (unsigned i=0; i < sourceBatches_[id].Size(); i++)
        sourceBatchesToRender_[id].Push(&(sourceBatches_[id][i]));
}

const Vector<SourceBatch2D*>& Drawable2D::GetSourceBatchesToRender(Camera* camera)
{
    // batchsetid = 0 => INNERVIEW/FRONTVIEW
    // batchsetid = 1 => BACKACTORVIEW

    // Beware : HardCoded ! come from FromBones DefsViews.h and ViewManager SwithViewZ Masks
    static const unsigned BACKVIEW_MASK   = (Urho3D::DRAWABLE_ANY+1) << 1;
    static const unsigned INNERVIEW_MASK  = (Urho3D::DRAWABLE_ANY+1) << 2;
    static const unsigned FRONTVIEW_MASK = (Urho3D::DRAWABLE_ANY+1) << 5;
    // if the camera is setted for INNERVIEW and the drawable is in FRONTVIEW but not in THRESHOLDVIEW => use BACKVIEWACTOR
//    int batchsetid = (camera->GetViewMask() & BACKVIEW_MASK) && (viewMask_ & FRONTVIEW_MASK) && !(viewMask_ & INNERVIEW_MASK) ? 1 : 0;
    // if the camera has no common mask with the drawable => use BACKVIEWACTOR
    int batchsetid = 0;

    if (camera->GetViewMask() != DRAWABLE_ANY)
    {
        // if Camera is not in INNERVIEW (BACKVIEW_MASK is deactive) and drawable is in InnerView => use BACKVIEWACTOR
        if (!(camera->GetViewMask() & BACKVIEW_MASK) && (viewMask_ & INNERVIEW_MASK))
            batchsetid = 1;
//        // if Camera is in INNERVIEW (BACKVIEW_MASK is active) and drawable is in FRONTVIEW but not in THRESHOLDVIEW  => use BACKVIEWACTOR
        else if ((camera->GetViewMask() & BACKVIEW_MASK) && (viewMask_ & FRONTVIEW_MASK) && !(viewMask_ & INNERVIEW_MASK))
            batchsetid = 1;
    }

    // skip render if in BACKACTORVIEW and the layer is not defined. (case for particuleemitter)
    if (layer_.y_ == -1 && batchsetid == 1)
    {
        sourceBatchesToRender_[0].Clear();
        return sourceBatchesToRender_[0];
    }

    if (sourceBatchesDirty_)
    {
//        if (enableDebugLog_)
//            URHO3D_LOGERRORF("Drawable2D() - GetSourceBatchesToRender : node=%s(%u) layer=%s batchsetid=%d ",
//                        node_->GetName().CString(), node_->GetID(), layer_.ToString().CString(), batchsetid);
        UpdateSourceBatchesToRender(batchsetid);
    }

    return sourceBatchesToRender_[batchsetid];
}

void Drawable2D::OnSceneSet(Scene* scene)
{
    // Do not call Drawable::OnSceneSet(node), as 2D drawable components should not be added to the octree
    // but are instead rendered through Renderer2D
    if (scene)
    {
        renderer_ = scene->GetOrCreateComponent<Renderer2D>(LOCAL);

        if (IsEnabledEffective())
            renderer_->AddDrawable(this);
    }
    else
    {
        if (renderer_)
            renderer_->RemoveDrawable(this);
    }
}

void Drawable2D::MarkDirty()
{
    OnMarkedDirty(node_);
}

void Drawable2D::OnMarkedDirty(Node* node)
{
    sourceBatchesDirty_ = worldBoundingBoxDirty_ = true;
}

}
