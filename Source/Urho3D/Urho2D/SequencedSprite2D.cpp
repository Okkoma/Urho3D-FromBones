//
// Copyright (c) 2008-2023 OkkomaSutdio
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
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Graphics/Material.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/SequencedSprite2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;

SequencedSprite2D::SequencedSprite2D(Context* context) :
    StaticSprite2D(context),
    followOwner_(false),
    shrink_(false),
    shrinkSpeed_(15.f)
{
}

void SequencedSprite2D::RegisterObject(Context* context)
{
    context->RegisterFactory<SequencedSprite2D>(URHO2D_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(StaticSprite2D);
    URHO3D_ACCESSOR_ATTRIBUTE("Follow Owner", GetFollowOwner, SetFollowOwner, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Shrink", GetShrink, SetShrink, bool, false, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Shrink Speed", float, shrinkSpeed_, 15.f, AM_DEFAULT);

//    URHO3D_ACCESSOR_ATTRIBUTE("Spritesheet", GetEmptyString, SetSpriteSheetAttr, String, String::EMPTY, AM_DEFAULT);
/*
		<attribute name="Spritesheet" value="SpriteSheet2D;2D/milk.xml" />
		<attribute name="Sequence" value="1" />
		<attribute name="Rate" value="1" />
		<attribute name="Stretch To Target" value="1" />
*/
}

void SequencedSprite2D::SetFollowOwner(bool enable)
{
    if (followOwner_ != enable)
    {
        followOwner_ = enable;

        if (owner_)
        {
            if (enable)
                owner_->AddListener(this);
            else
                owner_->RemoveListener(this);
        }
    }
}

bool SequencedSprite2D::GetFollowOwner() const
{
    return followOwner_;
}

void SequencedSprite2D::SetShrink(bool enable)
{
    if (shrink_ != enable)
    {
        shrink_ = enable;
        shrinkLength_ = 0.f;
    }
}

bool SequencedSprite2D::GetShrink() const
{
    return shrink_;
}

void SequencedSprite2D::SetOwner(Node* node)
{
    if (node != owner_)
    {
        if (owner_)
            owner_->RemoveListener(this);

        if (node && followOwner_)
            node->AddListener(this);

        owner_ = node;
        if (owner_)
        {
            StaticSprite2D* ownerDrawable = owner_->GetDerivedComponent<StaticSprite2D>();
            if (ownerDrawable)
                SetFlipX(ownerDrawable->GetFlipX());
        }

        followInitialOffset_ = owner_ ? node_->GetWorldPosition2D() - owner_->GetWorldPosition2D() : Vector2::ZERO;
        initialFlipX_ = GetFlipX();
        initialAlpha_ = GetAlpha();
    }
}

void SequencedSprite2D::OnMarkedDirty(Node* node)
{
    sourceBatchesDirty_ = true;
}

void SequencedSprite2D::OnSetEnabled()
{
    Drawable2D::OnSetEnabled();

    bool enabled = IsEnabledEffective();

    Scene* scene = GetScene();
    if (scene)
    {
        if (enabled)
        {
            shrinkLength_ = 0.f;
            initialFlipX_ = GetFlipX();
            initialAlpha_ = GetAlpha();
            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, URHO3D_HANDLER(SequencedSprite2D, HandleScenePostUpdate));
        }
        else
        {
            UnsubscribeFromEvent(scene, E_SCENEPOSTUPDATE);
            SetOwner(0);
        }
    }
}

void SequencedSprite2D::HandleScenePostUpdate(StringHash eventType, VariantMap& eventData)
{
    Update();
}

void SequencedSprite2D::Update()
{
    if (owner_)
    {
        if (shrink_)
        {
            if (initialFlipX_ != GetFlipX())
            {
                initialFlipX_ = GetFlipX();
                shrinkLength_ = M_LARGE_VALUE;
                SetAlpha(initialAlpha_);
            }

            shrinkLength_ += shrinkSpeed_ * PIXEL_SIZE;
            SetAlpha(Max(GetAlpha() - 0.025f, 0.f));

            sourceBatchesDirty_ = true;
        }
    }
}

void SequencedSprite2D::UpdateSourceBatches()
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
    SetTextureMode(TXM_UNIT, sprite_ ? sourceBatches_[0][0].material_->GetTextureUnit((Texture*)sprite_->GetTexture()) : TU_DIFFUSE, texmode);
    SetTextureMode(TXM_FX, textureFX_, texmode);

//    URHO3D_LOGINFOF("SequencedSprite2D() - UpdateSourceBatches : node=%s(%u) ...", node_->GetName().CString(), node_->GetID());

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

    vertex0.position_ = node_->GetWorldTransform2D() * drawRect_.min_;
    vertex1.position_ = node_->GetWorldTransform2D() * Vector2(drawRect_.min_.x_, drawRect_.max_.y_);
    vertex2.position_ = node_->GetWorldTransform2D() * drawRect_.max_;
    vertex3.position_ = node_->GetWorldTransform2D() * Vector2(drawRect_.max_.x_, drawRect_.min_.y_);

    int align = 0;

    if (owner_)
    {
        if (owner_->GetWorldPosition2D().x_ < node_->GetWorldPosition2D().x_)
            align = 1;
        else
            align = 2;

        if (followOwner_)
        {
            float halfheight = (vertex1.position_.y_ - vertex0.position_.y_) * 0.5f;
            vertex1.position_.y_ = vertex2.position_.y_ = owner_->GetWorldPosition2D().y_ + followInitialOffset_.y_ + halfheight;
            vertex0.position_.y_ = vertex3.position_.y_ = owner_->GetWorldPosition2D().y_ + followInitialOffset_.y_ - halfheight;

            if (align == 1)
                vertex0.position_.x_ = vertex1.position_.x_ = owner_->GetWorldPosition2D().x_ + followInitialOffset_.x_;
            else
                vertex2.position_.x_ = vertex3.position_.x_ = owner_->GetWorldPosition2D().x_ + followInitialOffset_.x_;
        }
    }

    if (shrink_)
    {
        // shrink to owner (who is on the left)
        if (align == 1)
        {
            if (shrinkLength_ >= M_LARGE_VALUE || vertex2.position_.x_ - shrinkLength_ <= vertex0.position_.x_)
            {
                vertex2.position_.x_ = vertex3.position_.x_ = vertex0.position_.x_;
                shrinkLength_ = M_LARGE_VALUE;
            }
            else
            {
                vertex2.position_.x_ = vertex3.position_.x_ = vertex2.position_.x_ - shrinkLength_;
            }
        }
        // shrink to owner (who is on the right)
        else if (align == 2)
        {
            if (shrinkLength_ >= M_LARGE_VALUE || vertex0.position_.x_ + shrinkLength_ >= vertex2.position_.x_)
            {
                vertex0.position_.x_ = vertex1.position_.x_ = vertex2.position_.x_;
                shrinkLength_ = M_LARGE_VALUE;
            }
            else
            {
                vertex0.position_.x_ = vertex1.position_.x_ = vertex0.position_.x_ + shrinkLength_;
            }
        }
        // shrink centered
        else
        {
            if (shrinkLength_ >= M_LARGE_VALUE || vertex0.position_.x_ >= vertex2.position_.x_)
            {
                vertex0.position_.x_ = vertex1.position_.x_ = vertex3.position_.x_ = vertex2.position_.x_;
                shrinkLength_ = M_LARGE_VALUE;
            }
            else
            {
                vertex0.position_.x_ = vertex1.position_.x_ = vertex0.position_.x_ + shrinkLength_/2.f;
                vertex2.position_.x_ = vertex3.position_.x_ = vertex2.position_.x_ - shrinkLength_/2.f;
            }
        }
    }

#ifdef URHO3D_VULKAN
    vertex0.z_ = vertex1.z_ = vertex2.z_ = vertex3.z_ = node_->GetWorldPosition().z_;
#else
    vertex0.position_.z_ = vertex1.position_.z_ = vertex2.position_.z_ = vertex3.position_.z_ = node_->GetWorldPosition().z_;
#endif

    vertex0.uv_ = textureRect_.min_;
    vertex1.uv_ = Vector2(textureRect_.min_.x_, textureRect_.max_.y_);
    vertex2.uv_ = textureRect_.max_;
    vertex3.uv_ = Vector2(textureRect_.max_.x_, textureRect_.min_.y_);

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
}

}
