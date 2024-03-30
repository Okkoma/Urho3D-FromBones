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

#include "../Core/Context.h"
#include "../IO/Log.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Material.h"
#include "../Graphics/Texture2D.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Urho2D/Renderer2D.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/StaticSprite2D.h"

#include "../DebugNew.h"



namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
extern const char* blendModeNames[];

StaticSprite2D::StaticSprite2D(Context* context) :
    Drawable2D(context),
    blendMode_(BLEND_ALPHA),
    flipX_(false),
    flipY_(false),
    swapXY_(false),
    color_(Color::WHITE),
    color2_(Color::BLACK),
    useHotSpot_(false),
    useDrawRect_(false),
    useTextureRect_(false),
    hotSpot_(0.5f, 0.5f),
    textureRect_(Rect::ZERO)
{
    for (unsigned j=0; j < 2; j++)
    {
        sourceBatches_[j].Resize(1);
        sourceBatches_[j][0].owner_ = this;
    }
}

StaticSprite2D::~StaticSprite2D()
{
}

void StaticSprite2D::RegisterObject(Context* context)
{
    context->RegisterFactory<StaticSprite2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable2D);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Sprite", GetSpriteAttr, SetSpriteAttr, ResourceRef, ResourceRef(Sprite2D::GetTypeStatic(), String::EMPTY),
        AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Blend Mode", GetBlendMode, SetBlendMode, BlendMode, blendModeNames, BLEND_ALPHA, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Flip X", GetFlipX, SetFlipX, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Flip Y", GetFlipY, SetFlipY, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Color", GetColor, SetColor, Color, Color::WHITE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Alpha", GetAlpha, SetAlpha, float, 1.f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("HotSpot", GetHotSpot, SetHotSpotAttr, Vector2, Vector2(0.5f, 0.5f), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Rectangle", GetDrawRect, SetDrawRect, Rect, Rect::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Draw Rectangle", GetUseDrawRect, SetUseDrawRect, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Texture Rectangle", GetTextureRect, SetTextureRect, Rect, Rect::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Texture Rectangle", GetUseTextureRect, SetUseTextureRect, bool, false, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Custom material", GetCustomMaterialAttr, SetCustomMaterialAttr, ResourceRef,
        ResourceRef(Material::GetTypeStatic(), String::EMPTY), AM_DEFAULT);
}

void StaticSprite2D::SetSprite(Sprite2D* sprite)
{
    if (sprite == sprite_)
        return;

    sprite_ = sprite;
    UpdateMaterial();

    sourceBatchesDirty_ = true;
    drawRectDirty_ = true;

    MarkNetworkUpdate();
}

void StaticSprite2D::SetDrawRect(const Rect& rect)
{
    if (rect == Rect::ZERO)
    {
        drawRect_.Clear();
        drawRectDirty_ = true;
    }
    else
    {
        drawRect_ = rect;
    }

    if (useDrawRect_)
    {
        sourceBatchesDirty_ = true;
    }
}

void StaticSprite2D::SetTextureRect(const Rect& rect)
{
    textureRect_ = rect;

    if (useTextureRect_)
    {
        sourceBatchesDirty_ = true;
    }
}

void StaticSprite2D::SetBlendMode(BlendMode blendMode)
{
    if (blendMode == blendMode_)
        return;

    blendMode_ = blendMode;

    UpdateMaterial();
    MarkNetworkUpdate();
}

void StaticSprite2D::SetFlip(bool flipX, bool flipY, bool swapXY)
{
    if (flipX == flipX_ && flipY == flipY_ && swapXY == swapXY_)
        return;

    flipX_ = flipX;
    flipY_ = flipY;
    swapXY_ = swapXY;

    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();

    drawRectDirty_ = true;
}

void StaticSprite2D::SetFlipX(bool flipX)
{
    SetFlip(flipX, flipY_, swapXY_);
}

void StaticSprite2D::SetFlipY(bool flipY)
{
    SetFlip(flipX_, flipY, swapXY_);
}

void StaticSprite2D::SetSwapXY(bool swapXY)
{
    SetFlip(flipX_, flipY_, swapXY);
}

void StaticSprite2D::SetColor(const Color& color)
{
    if (color == color_)
        return;

    color_ = color;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetColors(const Color& color, const Color& color2)
{
    if (color == color_ && color2 == color2_)
        return;

    color_ = color;
    color2_ = color2;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetAlpha(float alpha)
{
    if (alpha == color_.a_)
        return;

    color_.a_ = color2_.a_ = alpha;

    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetUseHotSpot(bool useHotSpot)
{
    if (useHotSpot == useHotSpot_)
        return;

    useHotSpot_ = useHotSpot;

    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();

    drawRectDirty_ = true;
}

void StaticSprite2D::SetUseDrawRect(bool useDrawRect)
{
    if (useDrawRect == useDrawRect_)
        return;

    useDrawRect_ = useDrawRect;
    sourceBatchesDirty_ = true;

    MarkNetworkUpdate();
}

void StaticSprite2D::SetUseTextureRect(bool useTextureRect)
{
    if (useTextureRect == useTextureRect_)
        return;

    useTextureRect_ = useTextureRect;
    sourceBatchesDirty_ = true;
    MarkNetworkUpdate();
}

void StaticSprite2D::SetHotSpot(const Vector2& hotspot)
{
    if (hotspot == hotSpot_)
        return;

    hotSpot_ = hotspot;

    if (useHotSpot_)
    {
        sourceBatchesDirty_ = true;
        MarkNetworkUpdate();

        drawRectDirty_ = true;
    }
}

void StaticSprite2D::SetHotSpotAttr(const Vector2& hotspot)
{
    if (hotspot != hotSpot_)
    {
        hotSpot_ = hotspot;
        SetUseHotSpot(true);
    }
}

Sprite2D* StaticSprite2D::GetSprite() const
{
    return sprite_;
}

Material* StaticSprite2D::GetCustomMaterial() const
{
    return customMaterial_;
}

void StaticSprite2D::SetCustomMaterial(Material* customMaterial)
{
    if (customMaterial == customMaterial_)
        return;

    customMaterial_ = customMaterial;
    sourceBatchesDirty_ = true;

    UpdateMaterial();
    MarkNetworkUpdate();
}

void StaticSprite2D::SetCustomMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    SetCustomMaterial(cache->GetResource<Material>(value.name_));
}

ResourceRef StaticSprite2D::GetCustomMaterialAttr() const
{
    return GetResourceRef(customMaterial_, Material::GetTypeStatic());
//    if (customMaterial_)
//        return GetResourceRef(customMaterial_.Get(), Material::GetTypeStatic());
//    else
//        return ResourceRef(StringHash::ZERO, String::EMPTY);
}

void StaticSprite2D::SetSpriteAttr(const ResourceRef& value)
{
    Sprite2D* sprite = Sprite2D::LoadFromResourceRef(context_, value);

//    URHO3D_LOGINFOF("StaticSprite2D() - SetSpriteAttr : %s(%u) ref=%s sprite=%u", node_->GetName().CString(), node_->GetID(), value.ToString().CString(), sprite);

    if (sprite)
        SetSprite(sprite);
}

ResourceRef StaticSprite2D::GetSpriteAttr() const
{
    return Sprite2D::SaveToResourceRef(sprite_);
}


BoundingBox StaticSprite2D::GetWorldBoundingBox2D()
{
    if (worldBoundingBoxDirty_)
    {
        OnWorldBoundingBoxUpdate();
        worldBoundingBoxDirty_ = false;
    }

    if (!drawRect_.Defined())
    {
        BoundingBox fixed;
        Vector2 position = node_->GetWorldPosition2D();
        fixed.min_.x_ = position.x_ - 1.f;
        fixed.min_.y_ = position.y_ - 1.f;
        fixed.max_.x_ = position.x_ + 1.f;
        fixed.max_.y_ = position.y_ + 1.f;
        fixed.min_.z_ = 0.f;
        fixed.max_.z_ = 0.f;
        return fixed;
    }

    return worldBoundingBox_;
}

//void StaticSprite2D::OnSetEnabled()
//{
//    Drawable2D::OnSetEnabled();
//
//    bool enabled = IsEnabledEffective();
//
//    if (GetScene() && enabled && renderer_)
//        OnWorldBoundingBoxUpdate();
//}

void StaticSprite2D::OnDrawOrderChanged()
{
    sourceBatches_[0][0].drawOrder_ = GetDrawOrder(0);
    if (layer_.y_ != -1)
        sourceBatches_[1][0].drawOrder_ = GetDrawOrder(1);
    sourceBatchesDirty_ = true;
}

void StaticSprite2D::OnWorldBoundingBoxUpdate()
{
    if (!UpdateDrawRectangle())
        return;

    Rect worldDrawRect = drawRect_.Transformed(node_->GetWorldTransform2D());
    worldBoundingBox_.min_.x_ = worldDrawRect.min_.x_;
    worldBoundingBox_.min_.y_ = worldDrawRect.min_.y_;
    worldBoundingBox_.max_.x_ = worldDrawRect.max_.x_;
    worldBoundingBox_.max_.y_ = worldDrawRect.max_.y_;
    worldBoundingBox_.min_.z_ = node_->GetWorldPosition().z_ - 0.5f;
    worldBoundingBox_.max_.z_ = node_->GetWorldPosition().z_ + 0.5f;

    sourceBatchesDirty_ = true;
//    URHO3D_LOGINFOF("StaticSprite2D() - OnWorldBoundingBoxUpdate : node=%s(%u) ... dR=%s OK !", node_->GetName().CString(), node_->GetID(), drawRect_.ToString().CString());
}

bool StaticSprite2D::UpdateDrawRectangle()
{
    if (!drawRectDirty_ || useDrawRect_)
        return true;

	if (!sprite_ && !customMaterial_)
        return false;

    if (!useDrawRect_)
    {
        drawRect_.Clear();

        if (sprite_)
        {
            if (useHotSpot_)
            {
                if (!sprite_->GetDrawRectangle(drawRect_, hotSpot_, flipX_, flipY_))
                // Graphics.TODO 13/03/2024
                //if (!sprite_->GetDrawRectangle(drawRect_, hotSpot_))
                    return false;
            }
            else
            {
                if (!sprite_->GetDrawRectangle(drawRect_, flipX_, flipY_))
                // Graphics.TODO 13/03/2024
                //if (!sprite_->GetDrawRectangle(drawRect_))
                    return false;
            }
        }
        else
        {
            Texture* texture = customMaterial_->GetTexture(TU_DIFFUSE);
            if (texture)
            {
                int w = texture->GetWidth();
                int h = texture->GetHeight();
                drawRect_.min_.x_ = -(float)w * PIXEL_SIZE * 0.5f;
                drawRect_.max_.x_ = (float)w * PIXEL_SIZE * 0.5f;
                drawRect_.min_.y_ = -(float)h * PIXEL_SIZE * 0.5f;
                drawRect_.max_.y_ = (float)h * PIXEL_SIZE * 0.5f;
                useDrawRect_ = true;
            }
            else
            {
                URHO3D_LOGERRORF("StaticSprite2D() - UpdateDrawRectangle : node=%s(%u) ... no sprite && no texture in custommaterial !!!",
                                node_->GetName().CString(), node_->GetID());
            }
        }
    }

    drawRectDirty_ = false;

//    URHO3D_LOGINFOF("StaticSprite2D() - UpdateDrawRectangle : node=%s(%u) ... dR=%s OK !", node_->GetName().CString(), node_->GetID(), drawRect_.ToString().CString());

    return true;
}

static Matrix2x3 sWorldTransfo_;
void StaticSprite2D::UpdateSourceBatches()
{
    if (!sourceBatchesDirty_)
        return;

    if (!StaticSprite2D::UpdateDrawRectangle())
        return;

    Vector<Vertex2D>& vertices1 = sourceBatches_[0][0].vertices_;
    vertices1.Clear();

    if (!useTextureRect_)
    {
        if (sprite_)
        {
            if (!sprite_->GetTextureRectangle(textureRect_, flipX_, flipY_))
                return;
        }
        else
        {
            textureRect_ = Rect(Vector2::ZERO, Vector2::ONE);
            useDrawRect_ = true;
        }
    }

#ifdef URHO3D_VULKAN
    unsigned texmode = 0;
#else
    Vector4 texmode;
#endif
    SetTextureMode(TXM_UNIT, sprite_ ? sourceBatches_[0][0].material_->GetTextureUnit(sprite_->GetTexture()) : TU_DIFFUSE, texmode);
    SetTextureMode(TXM_FX, textureFX_, texmode);

//    URHO3D_LOGINFOF("StaticSprite2D() - UpdateSourceBatches : node=%s(%u) ...", node_->GetName().CString(), node_->GetID());

    /*
    V1---------V2
    |         / |
    |       /   |
    |     /     |
    |   /       |
    | /         |
    V0---------V3
    */
    Vertex2D vertex0;
    Vertex2D vertex1;
    Vertex2D vertex2;
    Vertex2D vertex3;

    sWorldTransfo_ = node_->GetWorldTransform2D();

//    if (sprite_->GetTexture()->GetDpiRatio() != 1.f)
//        sWorldTransfo_.SetScale(sWorldTransfo_.Scale() / sprite_->GetTexture()->GetDpiRatio());

    vertex0.position_ = sWorldTransfo_ * Vector2(drawRect_.min_.x_, drawRect_.min_.y_);
    vertex1.position_ = sWorldTransfo_ * Vector2(drawRect_.min_.x_, drawRect_.max_.y_);
    vertex2.position_ = sWorldTransfo_ * Vector2(drawRect_.max_.x_, drawRect_.max_.y_);
    vertex3.position_ = sWorldTransfo_ * Vector2(drawRect_.max_.x_, drawRect_.min_.y_);
#ifdef URHO3D_VULKAN
    vertex0.z_ = vertex1.z_ = vertex2.z_ = vertex3.z_ = node_->GetWorldPosition().z_;
#else
    vertex0.position_.z_ = vertex1.position_.z_ = vertex2.position_.z_ = vertex3.position_.z_ = node_->GetWorldPosition().z_;
#endif
/*
    Matrix3x4 worldTransfo3D = node_->GetWorldTransform();
    float z = node_->GetWorldPosition().z_;
    vertex0.position_ = worldTransfo3D * Vector3(drawRect_.min_.x_, drawRect_.min_.y_, z);
    vertex1.position_ = worldTransfo3D * Vector3(drawRect_.min_.x_, drawRect_.max_.y_, z);
    vertex2.position_ = worldTransfo3D * Vector3(drawRect_.max_.x_, drawRect_.max_.y_, z);
    vertex3.position_ = worldTransfo3D * Vector3(drawRect_.max_.x_, drawRect_.min_.y_, z);
*/

    vertex0.uv_ = textureRect_.min_;
    (swapXY_ ? vertex3.uv_ : vertex1.uv_) = Vector2(textureRect_.min_.x_, textureRect_.max_.y_);
    vertex2.uv_ = textureRect_.max_;
    (swapXY_ ? vertex1.uv_ : vertex3.uv_) = Vector2(textureRect_.max_.x_, textureRect_.min_.y_);

    vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color_.ToUInt();
	vertex0.texmode_ = vertex1.texmode_ = vertex2.texmode_ = vertex3.texmode_ = texmode;

    vertices1.Push(vertex0);
    vertices1.Push(vertex1);
    vertices1.Push(vertex2);
    vertices1.Push(vertex3);

    if (layer_.y_ != -1)
    {
        Vector<Vertex2D>& vertices2 = sourceBatches_[1][0].vertices_;
        vertices2.Clear();
        vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color2_.ToUInt();
        vertices2.Push(vertex0);
        vertices2.Push(vertex1);
        vertices2.Push(vertex2);
        vertices2.Push(vertex3);
    }

    sourceBatchesDirty_ = false;
//    if (node_->GetID() == 16785126)
//        URHO3D_LOGINFOF("StaticSprite2D() - UpdateSourceBatches : node=%s(%u) ... dR=%s tR=%s wT=%s vertices=%u OK !",
//                    node_->GetName().CString(), node_->GetID(), drawRect_.ToString().CString(), textureRect_.ToString().CString(), sWorldTransfo_.ToString().CString(), vertices1.Size());
}

void StaticSprite2D::UpdateMaterial()
{
    if (customMaterial_)
    {
        sourceBatches_[0][0].material_ = sourceBatches_[1][0].material_ = customMaterial_;
    }
    else
    {
        if (sprite_ && renderer_)
            sourceBatches_[0][0].material_ = sourceBatches_[1][0].material_ = renderer_->GetMaterial(sprite_->GetTexture(), blendMode_);
        else
            sourceBatches_[0][0].material_ = sourceBatches_[1][0].material_ = 0;
    }
}

void StaticSprite2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug && IsEnabledEffective())
    {
        debug->AddNode(node_, 1.f, false);
        debug->AddBoundingBox(worldBoundingBox_, Color::YELLOW, false);
    }
}

}
