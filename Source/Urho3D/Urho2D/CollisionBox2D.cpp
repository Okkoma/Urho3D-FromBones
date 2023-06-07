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
#include "../Urho2D/CollisionBox2D.h"
#include "../Urho2D/PhysicsUtils2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
static const Vector2 DEFAULT_BOX_SIZE(0.01f, 0.01f);

CollisionBox2D::CollisionBox2D(Context* context) :
    CollisionShape2D(context),
    size_(DEFAULT_BOX_SIZE),
    center_(Vector2::ZERO),
    angle_(0.0f)
{
    float halfWidth = size_.x_ * 0.5f * cachedWorldScale_.x_;
    float halfHeight = size_.y_ * 0.5f * cachedWorldScale_.y_;
    boxShape_.SetAsBox(halfWidth, halfHeight);
    fixtureDef_.shape = &boxShape_;
}

CollisionBox2D::~CollisionBox2D()
{
}

void CollisionBox2D::RegisterObject(Context* context)
{
    context->RegisterFactory<CollisionBox2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Size", GetSize, SetSize, Vector2, DEFAULT_BOX_SIZE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Center", GetCenter, SetCenter, Vector2, Vector2::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Angle", GetAngle, SetAngle, float, 0.0f, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(CollisionShape2D);
}

void CollisionBox2D::SetSize(const Vector2& size)
{
    if (size == size_)
        return;

    size_ = size;

    MarkNetworkUpdate();
    RecreateFixture();
}

void CollisionBox2D::SetSize(float width, float height)
{
    SetSize(Vector2(width, height));
}

void CollisionBox2D::SetCenter(const Vector2& center)
{
    if (center == center_)
        return;

    center_ = pivot_ = center;

    MarkNetworkUpdate();
    RecreateFixture();
}

void CollisionBox2D::SetCenter(float x, float y)
{
    SetCenter(Vector2(x, y));
}

void CollisionBox2D::SetPivot(const Vector2& pivot)
{
    if (pivot == pivot_)
        return;

    pivot_ = pivot;

    MarkNetworkUpdate();
    RecreateFixture();
}

void CollisionBox2D::SetPivot(float x, float y)
{
    SetPivot(Vector2(x, y));
}

void CollisionBox2D::SetAngle(float angle)
{
    if (angle == angle_)
        return;

    angle_ = angle;
	tangent_ = Tan(angle_);

    MarkNetworkUpdate();
    RecreateFixture();
}

void CollisionBox2D::SetBox(const Vector2& center, const Vector2& size, const Vector2& pivot, float angle)
{
    center_ = center;
    pivot_ = pivot;
    size_ = size;
    angle_ = angle;
	tangent_ = Tan(angle_);

    MarkNetworkUpdate();
    RecreateFixture();
}

// Test 01/11/2020 in AnimatedSprite2D::UpdateTriggers
// for preventing the recreation of fixtures (which destroyes contact in box2D without any warning of the End of the Contact : that is problematic for Fall Cases).
// the following method doesn't destroy fixture, just modifies the shape ..
// but it's not good because FromBones doesn't temporize and it's a pingpong between FALL-TOUCHGROUND Animations... so keep SetBox Method.
void CollisionBox2D::UpdateBox(const Vector2& center, const Vector2& size, const Vector2& pivot, float angle)
{
    center_ = center;
    pivot_ = pivot;
    size_ = size;
    angle_ = angle;
    tangent_ = Tan(angle_);

    float worldScaleX = cachedWorldScale_.x_;
    float worldScaleY = cachedWorldScale_.y_;
    float halfWidth = size_.x_ * 0.5f * worldScaleX;
    float halfHeight = size_.y_ * 0.5f * worldScaleY;

    if (!fixture_)
        return;

    b2PolygonShape* shape = (b2PolygonShape*)fixture_->GetShape();

    if (center_ == Vector2::ZERO && angle_ == 0.0f)
    {
        boxShape_.SetAsBox(halfWidth, halfHeight);
        shape->SetAsBox(halfWidth, halfHeight);
    }
    else
    {
        Vector2 scaledCenter = center_ * Vector2(worldScaleX, worldScaleY);

        if (pivot_ != center_)
        {
            Vector2 scaledPivot = center_ * Vector2(worldScaleX, worldScaleY);
            boxShape_.SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), ToB2Vec2(scaledPivot), angle_ * M_DEGTORAD);
            shape->SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), ToB2Vec2(scaledPivot), angle_ * M_DEGTORAD);
        }
        else
        {
            boxShape_.SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), angle_ * M_DEGTORAD);
            shape->SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), angle_ * M_DEGTORAD);
        }
    }
}

void CollisionBox2D::UpdateBox(const Vector2& center, const Vector2& size, float cos, float sin)
{
	// FromBones : keep old center to track the displacement for waterlayer
	pivot_ = center_;
    center_ = center;

    size_ = size;
	tangent_ = cos ? sin / cos : 0.f;

    if (!fixture_)
        return;

    float halfWidth = size_.x_ * 0.5f * cachedWorldScale_.x_;
    float halfHeight = size_.y_ * 0.5f * cachedWorldScale_.y_;

    b2Vec2 centerScaled;
    centerScaled.x = center_.x_ * cachedWorldScale_.x_;
    centerScaled.y = center_.y_ * cachedWorldScale_.y_;

    boxShape_.SetAsBox(halfWidth, halfHeight, centerScaled, cos, sin);

    b2PolygonShape* shape = (b2PolygonShape*)fixture_->GetShape();
    shape->SetAsBox(halfWidth, halfHeight, centerScaled, cos, sin);
}

void CollisionBox2D::ApplyNodeWorldScale()
{
    RecreateFixture();
}

void CollisionBox2D::RecreateFixture()
{
    ReleaseFixture();

    float worldScaleX = cachedWorldScale_.x_;
    float worldScaleY = cachedWorldScale_.y_;
    float halfWidth = size_.x_ * 0.5f * worldScaleX;
    float halfHeight = size_.y_ * 0.5f * worldScaleY;

    if (center_ == Vector2::ZERO && angle_ == 0.0f)
        boxShape_.SetAsBox(halfWidth, halfHeight);
    else
    {
        Vector2 scaledCenter = center_ * Vector2(worldScaleX, worldScaleY);

        if (pivot_ != center_)
        {
            Vector2 scaledPivot = center_ * Vector2(worldScaleX, worldScaleY);
            boxShape_.SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), ToB2Vec2(scaledPivot), angle_ * M_DEGTORAD);
        }
        else
            boxShape_.SetAsBox(halfWidth, halfHeight, ToB2Vec2(scaledCenter), angle_ * M_DEGTORAD);
    }

    CreateFixture();
}

}
