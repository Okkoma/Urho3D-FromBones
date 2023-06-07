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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"
#include "../Graphics/DebugRenderer.h"
#include "../Urho2D/CollisionShape2D.h"
#include "../Urho2D/Constraint2D.h"
#include "../Urho2D/PhysicsUtils2D.h"
#include "../Urho2D/PhysicsWorld2D.h"
#include "../Urho2D/RigidBody2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
static const BodyType2D DEFAULT_BODYTYPE = BT_STATIC;

static const char* bodyTypeNames[] =
{
    "Static",
    "Kinematic",
    "Dynamic",
    0
};

RigidBody2D::RigidBody2D(Context* context) :
    Component(context),
    useFixtureMass_(true),
    body_(0)
{
    // Make sure the massData members are zero-initialized.
    massData_.mass = 0.0f;
    massData_.I = 0.0f;
    massData_.center.SetZero();
}

RigidBody2D::~RigidBody2D()
{
    if (physicsWorld_)
    {
        ReleaseBody();

        physicsWorld_->RemoveRigidBody(this);
    }
}

void RigidBody2D::RegisterObject(Context* context)
{
    context->RegisterFactory<RigidBody2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Body Type", GetBodyType, SetBodyType, BodyType2D, bodyTypeNames, DEFAULT_BODYTYPE, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Mass", GetMass, SetMass, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Inertia", GetInertia, SetInertia, float, 0.0f, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Mass Center", GetMassCenter, SetMassCenter, Vector2, Vector2::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Use Fixture Mass", GetUseFixtureMass, SetUseFixtureMass, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Linear Damping", GetLinearDamping, SetLinearDamping, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Angular Damping", GetAngularDamping, SetAngularDamping, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Allow Sleep", IsAllowSleep, SetAllowSleep, bool, true, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Fixed Rotation", IsFixedRotation, SetFixedRotation, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Bullet", IsBullet, SetBullet, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Gravity Scale", GetGravityScale, SetGravityScale, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Awake", IsAwake, SetAwake, bool, true, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Linear Velocity", GetLinearVelocity, SetLinearVelocity, Vector2, Vector2::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Angular Velocity", GetAngularVelocity, SetAngularVelocity, float, 0.0f, AM_DEFAULT);
}


void RigidBody2D::OnSetEnabled()
{
    bool enabled = IsEnabledEffective();

    bodyDef_.active = enabled;

    if (body_)
        body_->SetActive(enabled);

    if (enabled && body_)
        OnMarkedDirty(node_);

    MarkNetworkUpdate();
}

void RigidBody2D::SetBodyType(BodyType2D type)
{
    b2BodyType bodyType = (b2BodyType)type;
    if (body_)
    {
        body_->SetType(bodyType);
        // Mass data was reset to keep it legal (e.g. static body should have mass 0.)
        // If not using fixture mass, reassign our mass data now
        if (!useFixtureMass_)
        {
            SanitateInertia();
            body_->SetMassData(&massData_);
        }
    }
    else
    {
        if (bodyDef_.type == bodyType)
            return;

        bodyDef_.type = bodyType;
    }
//    URHO3D_LOGINFOF("GOC_Move2D() - SetBodyType : node=%s(%u) bodyType=%d", node_->GetName().CString(), node_->GetID(), bodyType);

    MarkNetworkUpdate();
}

void RigidBody2D::SetMass(float mass)
{
    mass = Max(mass, 0.0f);
    if (massData_.mass == mass)
        return;

    massData_.mass = mass;

    if (!useFixtureMass_ && body_)
    {
        SanitateInertia();
        body_->SetMassData(&massData_);
    }

    MarkNetworkUpdate();
}

void RigidBody2D::SetInertia(float inertia)
{
    inertia = Max(inertia, 0.0f);
    if (massData_.I == inertia)
        return;

    massData_.I = inertia;

    if (!useFixtureMass_ && body_)
    {
        SanitateInertia();
        body_->SetMassData(&massData_);
    }

    MarkNetworkUpdate();
}

void RigidBody2D::SanitateInertia()
{
    if (bodyDef_.fixedRotation)
        return;

    if (massData_.I > massData_.mass * b2Dot(massData_.center, massData_.center))
        return;

    // Modify Inertia to always satisfy to b2Assert(m_I > 0.0f) in b2Body::SetMassData (m_I = massData->I - m_mass * b2Dot(massData->center, massData->center)
    massData_.I = 10.f * massData_.mass * b2Dot(massData_.center, massData_.center);
}

void RigidBody2D::SetMassCenter(const Vector2& center)
{
    b2Vec2 b2Center = ToB2Vec2(center);
    if (massData_.center == b2Center)
        return;

    massData_.center = b2Center;

    if (!useFixtureMass_ && body_)
    {
        SanitateInertia();
        body_->SetMassData(&massData_);
    }

    MarkNetworkUpdate();
}

void RigidBody2D::SetUseFixtureMass(bool useFixtureMass)
{
    if (useFixtureMass_ == useFixtureMass)
        return;

    useFixtureMass_ = useFixtureMass;

    if (body_)
    {
        if (useFixtureMass_)
        {
            body_->ResetMassData();
        }
        else
        {
            SanitateInertia();
            body_->SetMassData(&massData_);
        }
    }

    MarkNetworkUpdate();
}

void RigidBody2D::SetLinearDamping(float linearDamping)
{
    if (bodyDef_.linearDamping == linearDamping)
        return;

    bodyDef_.linearDamping = linearDamping;

    if (body_)
        body_->SetLinearDamping(linearDamping);

    MarkNetworkUpdate();
}

void RigidBody2D::SetAngularDamping(float angularDamping)
{
    if (bodyDef_.angularDamping == angularDamping)
        return;

    bodyDef_.angularDamping = angularDamping;

    if (body_)
        body_->SetAngularDamping(angularDamping);

    MarkNetworkUpdate();
}

void RigidBody2D::SetAllowSleep(bool allowSleep)
{
    if (bodyDef_.allowSleep == allowSleep)
        return;

    bodyDef_.allowSleep = allowSleep;

    if (body_)
        body_->SetSleepingAllowed(allowSleep);

    MarkNetworkUpdate();
}

void RigidBody2D::SetFixedRotation(bool fixedRotation)
{
    if (bodyDef_.fixedRotation == fixedRotation)
        return;

    bodyDef_.fixedRotation = fixedRotation;

    if (body_)
    {
        body_->SetFixedRotation(fixedRotation);
        // Mass data was reset to keep it legal (e.g. non-rotating body should have inertia 0.)
        // If not using fixture mass, reassign our mass data now
        if (!useFixtureMass_)
        {
            SanitateInertia();
            body_->SetMassData(&massData_);
        }
    }

    MarkNetworkUpdate();
}

void RigidBody2D::SetBullet(bool bullet)
{
    if (bodyDef_.bullet == bullet)
        return;

    bodyDef_.bullet = bullet;

    if (body_)
        body_->SetBullet(bullet);

    MarkNetworkUpdate();
}

void RigidBody2D::SetGravityScale(float gravityScale)
{
    if (bodyDef_.gravityScale == gravityScale)
        return;

    bodyDef_.gravityScale = gravityScale;

    if (body_)
        body_->SetGravityScale(gravityScale);

    MarkNetworkUpdate();
}

//void RigidBody2D::SetAwake(bool awake)
//{
//    if (bodyDef_.awake == awake)
//        return;
//
//    bodyDef_.awake = awake;
//
//    if (body_)
//        body_->SetAwake(awake);
//
//    MarkNetworkUpdate();
//}

//void RigidBody2D::SetLinearDamping(float linearDamping)
//{
//    if (body_)
//        body_->SetLinearDamping(linearDamping);
//    else
//    {
//        if (bodyDef_.linearDamping == linearDamping)
//            return;
//
//        bodyDef_.linearDamping = linearDamping;
//    }
//
//    MarkNetworkUpdate();
//}
//
//void RigidBody2D::SetAngularDamping(float angularDamping)
//{
//    if (body_)
//        body_->SetAngularDamping(angularDamping);
//    else
//    {
//        if (bodyDef_.angularDamping == angularDamping)
//            return;
//
//        bodyDef_.angularDamping = angularDamping;
//    }
//
//    MarkNetworkUpdate();
//}
//
//void RigidBody2D::SetAllowSleep(bool allowSleep)
//{
//    if (body_)
//        body_->SetSleepingAllowed(allowSleep);
//    else
//    {
//        if (bodyDef_.allowSleep == allowSleep)
//            return;
//
//        bodyDef_.allowSleep = allowSleep;
//    }
//
//    MarkNetworkUpdate();
//}
//
//void RigidBody2D::SetFixedRotation(bool fixedRotation)
//{
//    if (body_)
//    {
//        body_->SetFixedRotation(fixedRotation);
//        // Mass data was reset to keep it legal (e.g. non-rotating body should have inertia 0.)
//        // If not using fixture mass, reassign our mass data now
//        if (!useFixtureMass_)
//            body_->SetMassData(&massData_);
//    }
//    else
//    {
//        if (bodyDef_.fixedRotation == fixedRotation)
//            return;
//
//        bodyDef_.fixedRotation = fixedRotation;
//    }
//
//    MarkNetworkUpdate();
//}
//
//void RigidBody2D::SetBullet(bool bullet)
//{
//    if (body_)
//        body_->SetBullet(bullet);
//    else
//    {
//        if (bodyDef_.bullet == bullet)
//            return;
//
//        bodyDef_.bullet = bullet;
//    }
//
//    MarkNetworkUpdate();
//}
//
//void RigidBody2D::SetGravityScale(float gravityScale)
//{
//    if (body_)
//        body_->SetGravityScale(gravityScale);
//    else
//    {
//        if (bodyDef_.gravityScale == gravityScale)
//            return;
//
//        bodyDef_.gravityScale = gravityScale;
//    }
//
//    MarkNetworkUpdate();
//}
//
void RigidBody2D::SetAwake(bool awake)
{
    if (body_)
        body_->SetAwake(awake);
    else
    {
        if (bodyDef_.awake == awake)
            return;

        bodyDef_.awake = awake;
    }

    MarkNetworkUpdate();
}

//void RigidBody2D::SetLinearVelocity(const Vector2& linearVelocity)
//{
//    b2Vec2 b2linearVelocity = ToB2Vec2(linearVelocity);
//    if (bodyDef_.linearVelocity == b2linearVelocity)
//        return;
//
//    bodyDef_.linearVelocity = b2linearVelocity;
//
//    if (body_)
//        body_->SetLinearVelocity(b2linearVelocity);
//
//    MarkNetworkUpdate();
//}

void RigidBody2D::SetLinearVelocity(const Vector2& linearVelocity)
{
    b2Vec2 b2linearVelocity = ToB2Vec2(linearVelocity);
    if (body_)
        body_->SetLinearVelocity(b2linearVelocity);
    else
    {
        if (bodyDef_.linearVelocity == b2linearVelocity)
            return;

        bodyDef_.linearVelocity = b2linearVelocity;
    }

    MarkNetworkUpdate();
}

//void RigidBody2D::SetAngularVelocity(float angularVelocity)
//{
//    if (bodyDef_.angularVelocity == angularVelocity)
//        return;
//
//    bodyDef_.angularVelocity = angularVelocity;
//
//    if (body_)
//        body_->SetAngularVelocity(angularVelocity);
//
//    MarkNetworkUpdate();
//}

void RigidBody2D::SetAngularVelocity(float angularVelocity)
{
    if (body_)
        body_->SetAngularVelocity(angularVelocity);
    else
    {
        if (bodyDef_.angularVelocity == angularVelocity)
            return;

        bodyDef_.angularVelocity = angularVelocity;
    }

    MarkNetworkUpdate();
}

void RigidBody2D::ApplyForce(const Vector2& force, const Vector2& point, bool wake)
{
    if (body_ && force != Vector2::ZERO)
        body_->ApplyForce(ToB2Vec2(force), ToB2Vec2(point), wake);
}

void RigidBody2D::ApplyForceToCenter(const Vector2& force, bool wake)
{
    if (body_ && force != Vector2::ZERO)
        body_->ApplyForceToCenter(ToB2Vec2(force), wake);
}

void RigidBody2D::ApplyTorque(float torque, bool wake)
{
    if (body_ && torque != 0)
        body_->ApplyTorque(torque, wake);
}

void RigidBody2D::ApplyLinearImpulse(const Vector2& impulse, const Vector2& point, bool wake)
{
    if (body_ && impulse != Vector2::ZERO)
        body_->ApplyLinearImpulse(ToB2Vec2(impulse), ToB2Vec2(point), wake);
}

void RigidBody2D::ApplyLinearImpulseToCenter(const Vector2& impulse, bool wake)
{
    if (body_ && impulse != Vector2::ZERO)
        body_->ApplyLinearImpulseToCenter(ToB2Vec2(impulse), wake);
}

void RigidBody2D::ApplyAngularImpulse(float impulse, bool wake)
{
    if (body_)
        body_->ApplyAngularImpulse(impulse, wake);
}

void RigidBody2D::CreateBody()
{
    if (body_)
        return;

    if (!physicsWorld_ || !physicsWorld_->GetWorld())
        return;

    bodyDef_.position = ToB2Vec2(node_->GetWorldPosition2D());
    bodyDef_.angle = node_->GetWorldRotation2D() * M_DEGTORAD;

    body_ = physicsWorld_->GetWorld()->CreateBody(&bodyDef_);
    body_->SetUserData(this);

    for (unsigned i = 0; i < collisionShapes_.Size(); ++i)
    {
        if (collisionShapes_[i])
            collisionShapes_[i]->CreateFixture();
    }

    if (!useFixtureMass_)
    {
        SanitateInertia();
        body_->SetMassData(&massData_);
    }

    for (unsigned i = 0; i < constraints_.Size(); ++i)
    {
        if (constraints_[i])
            constraints_[i]->CreateJoint();
    }
}

void RigidBody2D::ReleaseShapesFixtures()
{
    for (unsigned i = 0; i < collisionShapes_.Size(); ++i)
    {
        if (collisionShapes_[i])
            collisionShapes_[i]->ReleaseFixture();
    }
}

void RigidBody2D::CreateShapesFixtures()
{
    for (unsigned i = 0; i < collisionShapes_.Size(); ++i)
    {
        if (collisionShapes_[i])
            collisionShapes_[i]->CreateFixture();
    }
}


void RigidBody2D::ReleaseBody()
{
    if (!body_)
        return;

    if (!physicsWorld_ || !physicsWorld_->GetWorld())
        return;

    // Make a copy for iteration
    Vector<WeakPtr<Constraint2D> > constraints = constraints_;
    for (unsigned i = 0; i < constraints.Size(); ++i)
    {
        if (constraints[i])
            constraints[i]->ReleaseJoint();
    }

    for (unsigned i = 0; i < collisionShapes_.Size(); ++i)
    {
        if (collisionShapes_[i])
            collisionShapes_[i]->ReleaseFixture();
    }

    physicsWorld_->GetWorld()->DestroyBody(body_);
    body_ = 0;
}

void RigidBody2D::ApplyWorldTransform()
{
    if (!body_ || !node_)
        return;

    if (!body_->IsActive() || !body_->IsAwake())
        return;

    if (body_->GetType() == b2_staticBody)
    {
        body_->SetTransform(ToB2Vec2(node_->GetWorldPosition2D()), node_->GetWorldRotation2D() * M_DEGTORAD);
        return;
    }

    if (!body_->IsAwake())
        return;

    const b2Transform& transform = body_->GetTransform();
    ApplyWorldTransform(Vector2(transform.p.x, transform.p.y), transform.q.GetAngle() * M_RADTODEG);
}

void RigidBody2D::ApplyWorldTransform(const Vector2& newWorldPosition, float newWorldRotation)
{
    if (newWorldPosition != node_->GetWorldPosition2D() || newWorldRotation != node_->GetWorldRotation2D())
    {
//        URHO3D_LOGINFOF("RigidBody2D() - ApplyWorldTransform : node=%s newWorldPosition=%s !", node_->GetName().CString(), newWorldPosition.ToString().CString());

        // Do not feed changed position back to simulation now
        physicsWorld_->SetApplyingTransforms(true);

        node_->SetWorldPosition2D(newWorldPosition);
        node_->SetWorldRotation2D(newWorldRotation);

		physicsWorld_->SetApplyingTransforms(false);
    }
}

void RigidBody2D::SetWorldTransform(const Vector2& newWorldPosition, float newWorldRotation, const Vector2& newWorldscale)
{
//    URHO3D_LOGINFOF("RigidBody2D() - SetWorldTransform : node=%s newWorldPosition=%s !", node_->GetName().CString(), newWorldPosition.ToString().CString());

    if (newWorldRotation)
        node_->SetWorldRotation2D(newWorldRotation);
    if (newWorldscale != Vector2::ZERO)
        node_->SetWorldScale2D(newWorldscale);

    node_->SetWorldPosition2D(newWorldPosition);

    // Apply immediately the world position change
    node_->GetWorldPosition2D();
}

void RigidBody2D::AddCollisionShape2D(CollisionShape2D* collisionShape)
{
    if (!collisionShape)
        return;

    WeakPtr<CollisionShape2D> collisionShapePtr(collisionShape);
    if (collisionShapes_.Contains(collisionShapePtr))
        return;

//    URHO3D_LOGINFOF("RigidBody2D() - AddCollisionShape2D : node=%s(%u) bdID=%u bbody=%u csID=%u ptr=%u !", node_->GetName().CString(), node_->GetID(), GetID(), body_, collisionShape->GetID(), collisionShape);
    collisionShapes_.Push(collisionShapePtr);
}

void RigidBody2D::RemoveCollisionShape2D(CollisionShape2D* collisionShape)
{
    if (!collisionShape)
        return;

    WeakPtr<CollisionShape2D> collisionShapePtr(collisionShape);
//    URHO3D_LOGINFOF("RigidBody2D() - RemoveCollisionShape2D : node=%s(%u) bdID=%u bbody=%u csID=%u ptr=%u !", node_->GetName().CString(), node_->GetID(), GetID(), body_, collisionShape->GetID(), collisionShape);
    collisionShapes_.Remove(collisionShapePtr);
}

void RigidBody2D::AddConstraint2D(Constraint2D* constraint)
{
    if (!constraint)
        return;

    WeakPtr<Constraint2D> constraintPtr(constraint);
    if (constraints_.Contains(constraintPtr))
        return;
    constraints_.Push(constraintPtr);
}

void RigidBody2D::RemoveConstraint2D(Constraint2D* constraint)
{
    if (!constraint)
        return;

    WeakPtr<Constraint2D> constraintPtr(constraint);
    constraints_.Remove(constraintPtr);
}

float RigidBody2D::GetMass() const
{
    if (!useFixtureMass_)
        return massData_.mass;
    else
        return body_ ? body_->GetMass() : 0.0f;
}

float RigidBody2D::GetInertia() const
{
    if (!useFixtureMass_)
        return massData_.I;
    else
        return body_ ? body_->GetInertia() : 0.0f;
}

Vector2 RigidBody2D::GetMassCenter() const
{
    if (!useFixtureMass_)
        return ToVector2(massData_.center);
    else
        return body_ ? ToVector2(body_->GetLocalCenter()) : Vector2::ZERO;
}

Vector2 RigidBody2D::GetWorldMassCenter() const
{
    if (!useFixtureMass_)
        return ToVector2(body_->GetWorldPoint(massData_.center));
    else
        return body_ ? ToVector2(body_->GetWorldCenter()) : Vector2::ZERO;
}

bool RigidBody2D::IsAwake() const
{
    return body_ ? body_->IsAwake() : bodyDef_.awake;
}

Vector2 RigidBody2D::GetLinearVelocity() const
{
    return ToVector2(body_ ? body_->GetLinearVelocity() : bodyDef_.linearVelocity);
}

float RigidBody2D::GetAngularVelocity() const
{
    return body_ ? body_->GetAngularVelocity() : bodyDef_.angularVelocity;
}


void RigidBody2D::OnNodeSet(Node* node)
{
    Component::OnNodeSet(node);

    if (node)
    {
        node->AddListener(this);
        Scene* scene = GetScene();
        physicsWorld_ = scene->GetOrCreateComponent<PhysicsWorld2D>(LOCAL);

        CreateBody();
        physicsWorld_->AddRigidBody(this);

        OnSetEnabled();
    }
}

//void RigidBody2D::OnNodeSet(Node* node)
//{
//    if (node)
//    {
//        node->AddListener(this);
//
//        PODVector<CollisionShape2D*> shapes;
//        node_->GetDerivedComponents<CollisionShape2D>(shapes);
//
//        for (PODVector<CollisionShape2D*>::Iterator i = shapes.Begin(); i != shapes.End(); ++i)
//        {
//            (*i)->CreateFixture();
//            AddCollisionShape2D(*i);
//        }
//    }
//}
//
void RigidBody2D::OnSceneSet(Scene* scene)
{
//    if (scene)
//    {
//        physicsWorld_ = scene->GetOrCreateComponent<PhysicsWorld2D>(LOCAL);
//
//        CreateBody();
//        physicsWorld_->AddRigidBody(this);
//    }
//    else
//    {
//        if (physicsWorld_)
//        {
//            ReleaseBody();
//            physicsWorld_->RemoveRigidBody(this);
//            physicsWorld_.Reset();
//        }
//    }
}

void RigidBody2D::OnMarkedDirty(Node* node)
{
    if (physicsWorld_ && physicsWorld_->IsApplyingTransforms())
        return;

    // Physics operations are not safe from worker threads
    Scene* scene = GetScene();
    if (scene && scene->IsThreadedUpdate())
    {
//        URHO3D_LOGINFOF("RigidBody2D() - OnMarkedDirty : node=%s DelayedMarkedDirty !", node->GetName().CString());
        scene->DelayedMarkedDirty(this);
        return;
    }

    // Check if transform has changed from the last one set in ApplyWorldTransform()
    b2Vec2 newPosition = ToB2Vec2(node_->GetWorldPosition2D());
    float newAngle = node_->GetWorldRotation2D() * M_DEGTORAD;

    if (newPosition != bodyDef_.position || newAngle != bodyDef_.angle)
    {
//        URHO3D_LOGINFOF("RigidBody2D() - OnMarkedDirty : node=%s set Transform !", node->GetName().CString());
        bodyDef_.position = newPosition;
        bodyDef_.angle = newAngle;
        if (body_)
            body_->SetTransform(newPosition, newAngle);
    }
//    else
//    {
//        URHO3D_LOGINFOF("RigidBody2D() - OnMarkedDirty : node=%s same Transform position=%s !", node->GetName().CString(),node_->GetWorldPosition().ToString().CString());
//    }
}

void RigidBody2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug && IsEnabledEffective())
    {
        debug->AddNode(node_, 1.f, false);
        debug->AddLine(node_->GetWorldPosition(), node_->GetWorldPosition()+Vector3(GetLinearVelocity()), Color::GREEN, depthTest);
    }
}

}
