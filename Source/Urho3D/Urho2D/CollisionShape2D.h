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

#pragma once

#include "../Scene/Component.h"

#include <Box2D/Box2D.h>

namespace Urho3D
{

class RigidBody2D;

/// 2D collision shape component.
class URHO3D_API CollisionShape2D : public Component
{
    URHO3D_OBJECT(CollisionShape2D, Component);

public:
    /// Construct.
    CollisionShape2D(Context* context);
    /// Destruct.
    virtual ~CollisionShape2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Handle enabled/disabled state change.
    virtual void OnSetEnabled();

    void SetExtraContactBits(int extraBits);
    /// Set trigger.
    void SetTrigger(bool trigger);
    /// Set filter category && mask bits.
    void SetFilterBits(int categoryBits, int maskBits);
    /// Set filter category bits.
    void SetCategoryBits(int categoryBits);
    /// Set filter mask bits.
    void SetMaskBits(int maskBits);
    /// Set filter group index.
    void SetGroupIndex(int groupIndex);
    /// Set density.
    void SetDensity(float density);
    /// Set friction.
    void SetFriction(float friction);
    /// Set restitution .
    void SetRestitution(float restitution);

    /// Create fixture.
    void CreateFixture();
    /// Release fixture.
    void ReleaseFixture();

//    bool GetColliderStability() const { return colliderStability_; }
    int GetExtraContactBits() const { return extraContactBits_; }

    /// Return trigger.
    bool IsTrigger() const { return fixtureDef_.isSensor; }

    /// Return filter category bits.
    int GetCategoryBits() const { return fixtureDef_.filter.categoryBits; }

    /// Return filter mask bits.
    int GetMaskBits() const { return fixtureDef_.filter.maskBits; }

    /// Return filter group index.
    int GetGroupIndex() const { return fixtureDef_.filter.groupIndex; }

    /// Return density.
    float GetDensity() const { return fixtureDef_.density; }

    /// Return friction.
    float GetFriction() const { return fixtureDef_.friction; }

    /// Return restitution.
    float GetRestitution() const { return fixtureDef_.restitution; }

    /// Return mass.
    float GetMass() const;
    /// Return inertia.
    float GetInertia() const;
    /// Return mass center.
    Vector2 GetMassCenter() const;

    Vector2 GetCachedWorldScale2D() const { return cachedWorldScale_.ToVector2(); }

    /// Return fixture.
    b2Fixture* GetFixture() const { return fixture_; }
    /// Return rigidBody.
    RigidBody2D* GetRigidBody() const { return rigidBody_; }

    /// Handle node transform being dirtied.
    virtual void OnMarkedDirty(Node* node);

    void SetViewZ(int viewz) { viewZ_ = viewz; }
    int GetViewZ() const { return viewZ_; }

    void SetColliderInfo(void* cinfo) { cinfo_ = cinfo; }
    void* GetColliderInfo() const { return cinfo_; }

protected:
    /// Handle node being assigned.
    virtual void OnNodeSet(Node* node);

    /// Apply Node world scale.
    virtual void ApplyNodeWorldScale() = 0;

    /// Rigid body.
    WeakPtr<RigidBody2D> rigidBody_;
    /// Fixture def.
    b2FixtureDef fixtureDef_;
    /// Box2D fixture.
    b2Fixture* fixture_;
    /// Cached world scale.
    Vector3 cachedWorldScale_;

    /// Extra Contact Bits
    int extraContactBits_;

    /// Game Data
    int viewZ_;
    void* cinfo_;
};

}
