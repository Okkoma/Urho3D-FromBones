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
#include "../Core/Profiler.h"
#include "../Graphics/DebugRenderer.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Renderer.h"
#include "../IO/Log.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Urho2D/CollisionShape2D.h"
#include "../Urho2D/CollisionChain2D.h"
#include "../Urho2D/CollisionBox2D.h"
#include "../Urho2D/CollisionCircle2D.h"
#include "../Urho2D/PhysicsEvents2D.h"
#include "../Urho2D/PhysicsUtils2D.h"
#include "../Urho2D/PhysicsWorld2D.h"
#include "../Urho2D/RigidBody2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

const int CONTACT_BOTTOM = 4;

extern const char* SUBSYSTEM_CATEGORY;
static const Vector2 DEFAULT_GRAVITY(0.0f, -9.81f);
static const int DEFAULT_VELOCITY_ITERATIONS = 8;
static const int DEFAULT_POSITION_ITERATIONS = 3;

PhysicsWorld2D::PhysicsWorld2D(Context* context) :
    Component(context),
    gravity_(DEFAULT_GRAVITY),
    velocityIterations_(DEFAULT_VELOCITY_ITERATIONS),
    positionIterations_(DEFAULT_POSITION_ITERATIONS),
    debugRenderer_(0),
    physicsStepping_(false),
    applyingTransforms_(false),
    updateEnabled_(true)
{
    // Set default debug draw flags
    m_drawFlags = e_shapeBit;

    // Create Box2D world
    world_ = new b2World(ToB2Vec2(gravity_));
    // Set contact listener
    world_->SetContactListener(this);
    // Set debug draw
    world_->SetDebugDraw(this);

    world_->SetContinuousPhysics(true);
    world_->SetSubStepping(true);

    beginContactInfos_.Reserve(1000);
    endContactInfos_.Reserve(1000);
}

PhysicsWorld2D::~PhysicsWorld2D()
{
    for (unsigned i = 0; i < rigidBodies_.Size(); ++i)
        if (rigidBodies_[i])
            rigidBodies_[i]->ReleaseBody();
}

void PhysicsWorld2D::RegisterObject(Context* context)
{
    context->RegisterFactory<PhysicsWorld2D>(SUBSYSTEM_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Draw Shape", GetDrawShape, SetDrawShape, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Joint", GetDrawJoint, SetDrawJoint, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Aabb", GetDrawAabb, SetDrawAabb, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Pair", GetDrawPair, SetDrawPair, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw CenterOfMass", GetDrawCenterOfMass, SetDrawCenterOfMass, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Allow Sleeping", GetAllowSleeping, SetAllowSleeping, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Warm Starting", GetWarmStarting, SetWarmStarting, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Continuous Physics", GetContinuousPhysics, SetContinuousPhysics, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Sub Stepping", GetSubStepping, SetSubStepping, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Gravity", GetGravity, SetGravity, Vector2, DEFAULT_GRAVITY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Auto Clear Forces", GetAutoClearForces, SetAutoClearForces, bool, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Velocity Iterations", GetVelocityIterations, SetVelocityIterations, int, DEFAULT_VELOCITY_ITERATIONS,
        AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Position Iterations", GetPositionIterations, SetPositionIterations, int, DEFAULT_POSITION_ITERATIONS,
        AM_DEFAULT);
}

void PhysicsWorld2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (debug)
    {
        URHO3D_PROFILE(Physics2DDrawDebug);

        debugRenderer_ = debug;
        debugDepthTest_ = depthTest;
        world_->DrawDebugData();
        debugRenderer_ = 0;
    }
}

void PhysicsWorld2D::DrawDebug(CollisionShape2D* shape, DebugRenderer* debug, bool depthTest, const Color& color)
{
    if (debug)
    {
        const Matrix2x3& transform = shape->GetNode()->GetWorldTransform2D();
        if (shape->IsInstanceOf<CollisionChain2D>())
        {
            const PODVector<Vector2>& vertices = static_cast<CollisionChain2D*>(shape)->GetVertices();
            for (int i = 0; i < vertices.Size() - 1; ++i)
                debug->AddLine(Vector3(transform * vertices[i]), Vector3(transform * vertices[i + 1]), color, depthTest);
            debug->AddLine(Vector3(transform * vertices[vertices.Size() - 1]), Vector3(transform * vertices[0]), color, depthTest);
        }
        else if (shape->IsInstanceOf<CollisionBox2D>())
        {

        }
        else if (shape->IsInstanceOf<CollisionCircle2D>())
        {
            CollisionCircle2D* circle = static_cast<CollisionCircle2D*>(shape);
            debug->AddCircle(Vector3(transform * circle->GetCenter()), Vector3::FORWARD, circle->GetRadius() * shape->GetNode()->GetWorldScale2D().x_, color, 8, false, depthTest);
        }
    }
}
//void PhysicsWorld2D::BeginContact(b2Contact* contact)
//{
//    // Only handle contact event while stepping the physics simulation
//    if (!physicsStepping_)
//        return;
//
//    b2Fixture* fixtureA = contact->GetFixtureA();
//    b2Fixture* fixtureB = contact->GetFixtureB();
//    if (!fixtureA || !fixtureB)
//        return;
//
//    beginContactInfos_.Push(ContactInfo(contact));
//}
//
//void PhysicsWorld2D::EndContact(b2Contact* contact)
//{
//    if (!physicsStepping_)
//        return;
//
//    b2Fixture* fixtureA = contact->GetFixtureA();
//    b2Fixture* fixtureB = contact->GetFixtureB();
//    if (!fixtureA || !fixtureB)
//        return;
//
//    endContactInfos_.Push(ContactInfo(contact));
//}

static b2Fixture* mapFixture;
static b2Fixture* otherFixture;
static CollisionShape2D* mapShape;
static CollisionShape2D* otherShape;
static int ishapeA;
static int ishapeB;
static int ishapeNext;
static int zPlatform;
static int zBody;
static float normaly;
static float normalx;
static float shapenormaly;
static int pointid;
static bool isFluid;

static void* WALLCOLLIDER = (void*)1;
static void* PLATEFORMCOLLIDER = (void*)2;
static void* WATERCOLLIDER = (void*)3;

void PhysicsWorld2D::BeginContact(b2Contact* contact)
{
    // Only handle contact event while stepping the physics simulation
    if (!physicsStepping_)
        return;

    mapFixture = contact->GetFixtureA();
    otherFixture = contact->GetFixtureB();

    if (!mapFixture || !otherFixture)
        return;

    mapShape = (CollisionShape2D*)(mapFixture->GetUserData());
    otherShape     = (CollisionShape2D*)(otherFixture->GetUserData());

    if (!mapShape || !otherShape)
		return;

//    URHO3D_LOGINFOF("Contact bodyA=%s(%u) bodyB=%s(%u) ...",
//                    mapShape ? mapShape->GetNode()->GetName().CString() : "", mapShape ? mapShape->GetNode()->GetID() : 0,
//                    otherShape ? otherShape->GetNode()->GetName().CString() : "", otherShape ? otherShape->GetNode()->GetID() : 0);

    /// check if a fixture is a platform
    bool swapBodies = false;
    {
        bool fixtureAIsMapCollider = (mapShape->GetColliderInfo());
        bool fixtureBIsMapCollider = (otherShape->GetColliderInfo());

        /// no map colliders : send contact
        if (!fixtureAIsMapCollider && !fixtureBIsMapCollider)
        {
//            URHO3D_LOGINFOF("... no platform => send contact directly !");
            beginContactInfos_.Push(ContactInfo(contact));
            return;
        }
        /// 2 map colliders : desactive contact
        else if (fixtureAIsMapCollider && fixtureBIsMapCollider)
        {
            if (mapShape->GetColliderInfo() != PLATEFORMCOLLIDER && otherShape->GetColliderInfo() != PLATEFORMCOLLIDER)
            {
//                URHO3D_LOGINFOF("... 2 platforms => allow contact !");
                beginContactInfos_.Push(ContactInfo(contact));

//                URHO3D_LOGINFOF("... 2 platforms => desactive contact !");
//                contact->SetEnabled(false);

                return;
            }
            else
            {
                swapBodies = fixtureBIsMapCollider && otherShape->GetColliderInfo() != PLATEFORMCOLLIDER;
            }
        }
        /// switch case
        else
        {
            swapBodies = fixtureBIsMapCollider;
        }
    }

    if (swapBodies)
    {
//        URHO3D_LOGINFOF("... swapBodies ...");

        Swap(otherFixture, mapFixture);
        Swap(otherShape, mapShape);

        ishapeA = contact->GetChildIndexB();
        ishapeB = contact->GetChildIndexA();
    }
    else
    {
        ishapeA = contact->GetChildIndexA();
        ishapeB = contact->GetChildIndexB();
    }

    isFluid = mapShape->GetColliderInfo() == WATERCOLLIDER;

    if (otherShape->IsTrigger() && !isFluid)
    {
//        URHO3D_LOGINFOF("... otherShape body=%s(%u) is a trigger => add contact !",
//                        otherShape ? otherShape->GetNode()->GetName().CString() : "", otherShape ? otherShape->GetNode()->GetID() : 0);
        beginContactInfos_.Push(ContactInfo(contact));
        return;
    }

    zPlatform = mapShape->GetViewZ() + (mapShape->GetColliderInfo() == PLATEFORMCOLLIDER ? -1 : 0);
    zBody = otherShape->GetViewZ();

    /// platform above otherbody : desactive contact
    if (zPlatform > zBody)
    {
        /// fluid case
        if (isFluid)
        {
			beginContactInfos_.Resize(beginContactInfos_.Size()+1);
			ContactInfo& contactinfo = beginContactInfos_.Back();
			contactinfo.bodyA_ = (RigidBody2D*)(otherFixture->GetBody()->GetUserData());
			contactinfo.bodyB_ = (RigidBody2D*)(mapFixture->GetBody()->GetUserData());
			contactinfo.shapeA_ = otherShape;
			contactinfo.shapeB_ = mapShape;

        	if (!otherShape->IsTrigger())
			{
				b2WorldManifold wManifold;
				contact->GetWorldManifold(&wManifold);

				if (swapBodies)
				{
					normalx = -wManifold.normal.x;
					normaly = -wManifold.normal.y;
				}
				else
				{
					normalx = wManifold.normal.x;
					normaly = wManifold.normal.y;
				}

				contactinfo.contactPoint_ = ToVector2(wManifold.points[0]);
				contactinfo.normal_.x_ = normalx;
				contactinfo.normal_.y_ = normaly;

//				URHO3D_LOGINFOF("... body=%s(%u) touches the fluidsurfaces shape=%u contact=%F %F normal=%F %F!",
//								contactinfo.bodyA_->GetNode()->GetName().CString(), contactinfo.bodyA_->GetNode()->GetID(), mapShape,
//								contactinfo.contactPoint_.x_, contactinfo.contactPoint_.y_, normalx, normaly);
			}
        }

        contact->SetEnabled(false);

//        URHO3D_LOGINFOF("... Zplateform=%d > Zbody=%d => desactive contact !", zPlatform, zBody);

        return;
    }

    /// platform behind otherbody : check "one way wall"
    b2WorldManifold wManifold;
    contact->GetWorldManifold(&wManifold);

    /// get the world normal
    if (swapBodies)
    {
        normalx = -wManifold.normal.x;
        normaly = -wManifold.normal.y;
    }
    else
    {
        normalx = wManifold.normal.x;
        normaly = wManifold.normal.y;
    }

    pointid = 0;

    if (zPlatform < zBody)
    {
//        URHO3D_LOGINFOF("... Zplateform=%d < Zbody=%d => check normal=%f %f ... ", zPlatform, zBody, normalx, normaly);

        /// is always solid if category bits is 1 (basically at initialization, in Frombones it's the trigger category)
        bool solid = otherShape->GetCategoryBits() == 1;

        /// check contact with the top of the plateform
        /// othershape must have a bottom contact
        if (!solid && normaly > 0.1f && otherShape->GetExtraContactBits() & CONTACT_BOTTOM)
        {
            b2Body* platformb2Body = mapFixture->GetBody();
            b2Body* otherb2Body = otherFixture->GetBody();
			shapenormaly = 1.f;

            /// check if outside the shape
            if (mapFixture->GetShape()->GetType() == b2Shape::e_chain)
            {
                b2ChainShape* shape = (b2ChainShape*) mapFixture->GetShape();
                shapenormaly = platformb2Body->GetWorldVector(shape->m_vertices[(ishapeA+1) % shape->m_count]-shape->m_vertices[ishapeA]).x;
            }

//            URHO3D_LOGINFOF("... Zplateform=%d < Zbody=%d => check normal=%f %f shapenormaly=%f ... ", zPlatform, zBody, normalx, normaly, shapenormaly);

            /// the contact is located outside the shape
            if (shapenormaly > 0.f)
            {
                /// check all contact points
                int numPoints = contact->GetManifold()->pointCount;

                for (int i = 0; i < numPoints; i++)
                {
                    b2Vec2& contactpoint = wManifold.points[i];
                    //float pointy = otherb2Body->GetLocalPoint(wManifold.points[i]).y;

					/// 20220921 : Remove this check that don't work when contact occures between Bougie+TableGarnie
//                    float fixturecentery = otherb2Body->GetWorldPoint(otherFixture->GetShape()->GetCenter()).y;
//                    URHO3D_LOGINFOF("... Zplateform=%d < Zbody=%d => check normal=%f %f shapenormaly=%f point=%d (contactpointy=%f < fixturecentery=%f ?)... ", zPlatform, zBody, normalx, normaly, shapenormaly, i, contactpoint.y, fixturecentery);
//					/// the contact point must be under the center of the shape
//                    if (contactpoint.y < fixturecentery)
                    {
                        float relativeVely = otherb2Body->GetLinearVelocityFromWorldPoint(contactpoint).y - platformb2Body->GetLinearVelocityFromWorldPoint(contactpoint).y;
//						URHO3D_LOGINFOF("... Zplateform=%d < Zbody=%d => check normal=%f %f shapenormaly=%f point=%d refvely=%f ... ", zPlatform, zBody, normalx, normaly, shapenormaly, i, relativeVely);

                        if (relativeVely < -1.f)
                        {
                            /// contactpoint is moving onto platform, keep contact solid
//                            URHO3D_LOGINFOF("... => othershape=%u extrabits=%d refvely=%f pointy=%f otherCenterY=%f contactpoint[%d] is moving just over the plateform solid=true => active contact",
//                                            otherShape->GetID(), otherShape->GetExtraContactBits(), relativeVely, contactpoint.y, fixturecentery, i);
                            solid = true;
                            pointid = i;
                            break;
                        }

                        else if (relativeVely < 1.f)
                        {
                            /// borderline case, moving only slightly out of platform
                            float contactPointRelativeToPlatformY = platformb2Body->GetLocalPoint(contactpoint).y;
                            if (contactPointRelativeToPlatformY > 0.05f)
                            {
//                                URHO3D_LOGINFOF("... => othershape=%u extrabits=%d refvely=%f pointy=%f otherCenterY=%f contactpoint[%d] is moving out the platform solid=true => active contact",
//                                                otherShape->GetID(), otherShape->GetExtraContactBits(), relativeVely, contactpoint.y, fixturecentery, i);
                                solid = true;
                                pointid = i;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!solid)
        {
//            URHO3D_LOGINFOF("... plateformZ=%d bodyZ=%d not solid => desactive contact", zPlatform, zBody);
            contact->SetEnabled(false);
            return;
        }

//        URHO3D_LOGINFOF("... plateformZ=%d bodyZ=%d solid => active contact", zPlatform, zBody);
    }

	beginContactInfos_.Resize(beginContactInfos_.Size()+1);
	ContactInfo& contactinfo = beginContactInfos_.Back();

    contactinfo.bodyA_ = (RigidBody2D*)(mapFixture->GetBody()->GetUserData());
    contactinfo.bodyB_ = (RigidBody2D*)(otherFixture->GetBody()->GetUserData());
    contactinfo.shapeA_ = mapShape;
    contactinfo.shapeB_ = otherShape;
    contactinfo.iShapeA_ = ishapeA;
    contactinfo.iShapeB_ = ishapeB;
    contactinfo.contactPoint_ = ToVector2(wManifold.points[pointid]);
    contactinfo.normal_.x_ = normalx;
    contactinfo.normal_.y_ = normaly;

//    URHO3D_LOGINFOF("... Zplateform=%d Zbody=%d add contact ...", zPlatform, zBody);
}

void PhysicsWorld2D::EndContact(b2Contact* contact)
{
    if (!physicsStepping_)
        return;

    if (!contact->GetFixtureA() || !contact->GetFixtureB())
        return;

//    URHO3D_LOGINFOF("EndContact - bodyA=%s bodyB=%s !",
//                    ((RigidBody2D*)(contact->GetFixtureA()->GetBody()->GetUserData()))->GetNode()->GetName().CString(),
//                    ((RigidBody2D*)(contact->GetFixtureB()->GetBody()->GetUserData()))->GetNode()->GetName().CString());

    contact->SetEnabled(true);

    endContactInfos_.Resize(endContactInfos_.Size()+1);
    ContactInfo& contactinfo = endContactInfos_.Back();

    b2Fixture* fixture = contact->GetFixtureA();
    if (fixture)
    {
        contactinfo.bodyA_   = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        contactinfo.shapeA_  = (CollisionShape2D*)(fixture->GetUserData());
        contactinfo.iShapeA_ = contact->GetChildIndexA();
    }
    else
	{
		contactinfo.bodyA_   = 0;
		contactinfo.shapeA_  = 0;
		contactinfo.iShapeA_ = 0;
	}

    fixture = contact->GetFixtureB();
    if (fixture)
    {
        contactinfo.bodyB_   = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        contactinfo.shapeB_  = (CollisionShape2D*)(fixture->GetUserData());
        contactinfo.iShapeB_ = contact->GetChildIndexB();
    }
    else
	{
		contactinfo.bodyB_   = 0;
		contactinfo.shapeB_  = 0;
		contactinfo.iShapeB_ = 0;
	}
}

void PhysicsWorld2D::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Color c = ToColor(color);
    for (int i = 0; i < vertexCount - 1; ++i)
        debugRenderer_->AddLine(ToVector3(vertices[i]), ToVector3(vertices[i + 1]), c, debugDepthTest_);

    debugRenderer_->AddLine(ToVector3(vertices[vertexCount - 1]), ToVector3(vertices[0]), c, debugDepthTest_);
}

void PhysicsWorld2D::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    Vector3 v = ToVector3(vertices[0]);
    Color c(color.r, color.g, color.b, 0.5f);
    for (int i = 1; i < vertexCount - 1; ++i)
        debugRenderer_->AddTriangle(v, ToVector3(vertices[i]), ToVector3(vertices[i + 1]), c, debugDepthTest_);
}

extern URHO3D_API const float PIXEL_SIZE;

void PhysicsWorld2D::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    debugRenderer_->AddCircle(Vector3(center.x, center.y, 0.f), Vector3::FORWARD, radius, ToColor(color), 8, false, debugDepthTest_);
}

void PhysicsWorld2D::DrawPoint(const b2Vec2& center, float32 size, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    debugRenderer_->AddCircle(Vector3(center.x, center.y, 0.f), Vector3::FORWARD, size * 0.5f * PIXEL_SIZE, ToColor(color), 6, false, debugDepthTest_);
}

void PhysicsWorld2D::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
    if (!debugRenderer_)
        return;

    debugRenderer_->AddCircle(Vector3(center.x, center.y, 0.f), Vector3::FORWARD, radius, Color(color.r, color.g, color.b, 0.5f), 8, true, debugDepthTest_);
}

void PhysicsWorld2D::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    if (debugRenderer_)
        debugRenderer_->AddLine(ToVector3(p1), ToVector3(p2), ToColor(color), debugDepthTest_);
}

void PhysicsWorld2D::DrawTransform(const b2Transform& xf)
{
    if (!debugRenderer_)
        return;

    const float32 axisScale = 0.4f;

    b2Vec2 p1 = xf.p, p2;
    p2 = p1 + axisScale * xf.q.GetXAxis();
    debugRenderer_->AddLine(Vector3(p1.x, p1.y, 0.0f), Vector3(p2.x, p2.y, 0.0f), Color::RED, debugDepthTest_);

    p2 = p1 + axisScale * xf.q.GetYAxis();
    debugRenderer_->AddLine(Vector3(p1.x, p1.y, 0.0f), Vector3(p2.x, p2.y, 0.0f), Color::GREEN, debugDepthTest_);
}

void PhysicsWorld2D::Update(float timeStep)
{
    URHO3D_PROFILE(UpdatePhysics2D);

    using namespace PhysicsPreStep2D;

    beginContactInfos_.Clear();
    endContactInfos_.Clear();

    VariantMap& eventData = GetEventDataMap();
    eventData[P_WORLD] = this;
    eventData[P_TIMESTEP] = timeStep;
    SendEvent(E_PHYSICSPRESTEP2D, eventData);

    physicsStepping_ = true;
    world_->Step(timeStep, velocityIterations_, positionIterations_);
    physicsStepping_ = false;

    // Apply world transforms. Unparented transforms first
    for (unsigned i = 0; i < rigidBodies_.Size();)
    {
        if (rigidBodies_[i])
        {
            rigidBodies_[i]->ApplyWorldTransform();
            ++i;
        }
        else
        {
            // Erase possible stale weak pointer
            rigidBodies_.Erase(i);
        }
    }

    // Apply delayed (parented) world transforms now, if any
    while (!delayedWorldTransforms_.Empty())
    {
        for (HashMap<RigidBody2D*, DelayedWorldTransform2D>::Iterator i = delayedWorldTransforms_.Begin();
            i != delayedWorldTransforms_.End();)
        {
            const DelayedWorldTransform2D& transform = i->second_;

            // If parent's transform has already been assigned, can proceed
            if (!delayedWorldTransforms_.Contains(transform.parentRigidBody_))
            {
                transform.rigidBody_->ApplyWorldTransform(transform.worldPosition_, transform.worldRotation_);
                i = delayedWorldTransforms_.Erase(i);
            }
            else
                ++i;
        }
    }

    SendBeginContactEvents();
    SendEndContactEvents();

    using namespace PhysicsPostStep2D;
    SendEvent(E_PHYSICSPOSTSTEP2D, eventData);
}

void PhysicsWorld2D::DrawDebugGeometry()
{
    DebugRenderer* debug = GetComponent<DebugRenderer>();
    if (debug)
        DrawDebugGeometry(debug, false);
}

void PhysicsWorld2D::SetUpdateEnabled(bool enable)
{
    updateEnabled_ = enable;
}

void PhysicsWorld2D::SetDrawShape(bool drawShape)
{
    if (drawShape)
        m_drawFlags |= e_shapeBit;
    else
        m_drawFlags &= ~e_shapeBit;
}

void PhysicsWorld2D::SetDrawJoint(bool drawJoint)
{
    if (drawJoint)
        m_drawFlags |= e_jointBit;
    else
        m_drawFlags &= ~e_jointBit;
}

void PhysicsWorld2D::SetDrawAabb(bool drawAabb)
{
    if (drawAabb)
        m_drawFlags |= e_aabbBit;
    else
        m_drawFlags &= ~e_aabbBit;
}

void PhysicsWorld2D::SetDrawPair(bool drawPair)
{
    if (drawPair)
        m_drawFlags |= e_pairBit;
    else
        m_drawFlags &= ~e_pairBit;
}

void PhysicsWorld2D::SetDrawCenterOfMass(bool drawCenterOfMass)
{
    if (drawCenterOfMass)
        m_drawFlags |= e_centerOfMassBit;
    else
        m_drawFlags &= ~e_centerOfMassBit;
}

void PhysicsWorld2D::SetAllowSleeping(bool enable)
{
    world_->SetAllowSleeping(enable);
}

void PhysicsWorld2D::SetWarmStarting(bool enable)
{
    world_->SetWarmStarting(enable);
}

void PhysicsWorld2D::SetContinuousPhysics(bool enable)
{
    world_->SetContinuousPhysics(enable);
}

void PhysicsWorld2D::SetSubStepping(bool enable)
{
    world_->SetSubStepping(enable);
}

void PhysicsWorld2D::SetGravity(const Vector2& gravity)
{
    gravity_ = gravity;

    world_->SetGravity(ToB2Vec2(gravity_));
}

void PhysicsWorld2D::SetAutoClearForces(bool enable)
{
    world_->SetAutoClearForces(enable);
}

void PhysicsWorld2D::SetVelocityIterations(int velocityIterations)
{
    velocityIterations_ = velocityIterations;
}

void PhysicsWorld2D::SetPositionIterations(int positionIterations)
{
    positionIterations_ = positionIterations;
}

void PhysicsWorld2D::AddRigidBody(RigidBody2D* rigidBody)
{
    if (!rigidBody)
        return;

    WeakPtr<RigidBody2D> rigidBodyPtr(rigidBody);
    if (rigidBodies_.Contains(rigidBodyPtr))
        return;

    rigidBodies_.Push(rigidBodyPtr);
}

void PhysicsWorld2D::RemoveRigidBody(RigidBody2D* rigidBody)
{
    if (!rigidBody)
        return;

    WeakPtr<RigidBody2D> rigidBodyPtr(rigidBody);
    rigidBodies_.Remove(rigidBodyPtr);
}

void PhysicsWorld2D::AddDelayedWorldTransform(const DelayedWorldTransform2D& transform)
{
    delayedWorldTransforms_[transform.rigidBody_] = transform;
}

// Ray cast call back class.
class RayCastCallback : public b2RayCastCallback
{
public:
    // Construct.
    RayCastCallback(PODVector<PhysicsRaycastResult2D>& results, const Vector2& startPoint, unsigned collisionMask) :
        results_(results),
        startPoint_(startPoint),
        collisionMask_(collisionMask)
    {
    }

    // Called for each fixture found in the query.
    virtual float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

        if ((fixture->GetFilterData().categoryBits & collisionMask_) == 0)
            return true;

        PhysicsRaycastResult2D result;
        result.position_ = ToVector2(point);
        result.normal_ = ToVector2(normal);
        result.distance_ = (result.position_ - startPoint_).Length();
        result.body_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());

        results_.Push(result);
        return true;
    }

protected:
    // Physics raycast results.
    PODVector<PhysicsRaycastResult2D>& results_;
    // Start point.
    Vector2 startPoint_;
    // Collision mask.
    unsigned collisionMask_;
};

void PhysicsWorld2D::Raycast(PODVector<PhysicsRaycastResult2D>& results, const Vector2& startPoint, const Vector2& endPoint,
    unsigned collisionMask)
{
    results.Clear();

    RayCastCallback callback(results, startPoint, collisionMask);
    world_->RayCast(&callback, ToB2Vec2(startPoint), ToB2Vec2(endPoint));
}

// Single ray cast call back class.
class SingleRayCastCallback : public b2RayCastCallback
{
public:
    // Construct.
    SingleRayCastCallback(PhysicsRaycastResult2D& result, const Vector2& startPoint, unsigned collisionMask) :
        result_(result),
        startPoint_(startPoint),
        collisionMask_(collisionMask),
        minDistance_(M_INFINITY)
    {
    }

    // Called for each fixture found in the query.
    virtual float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

//        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
        if ((fixture->GetFilterData().categoryBits & collisionMask_) == 0)
            return true;

        float distance = (ToVector2(point) - startPoint_).Length();
        if (distance < minDistance_)
        {
            minDistance_ = distance;

            result_.position_ = ToVector2(point);
            result_.normal_ = ToVector2(normal);
            result_.distance_ = distance;
            result_.body_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        }

        return true;
    }

private:
    // Physics raycast result.
    PhysicsRaycastResult2D& result_;
    // Start point.
    Vector2 startPoint_;
    // Collision mask.
    unsigned collisionMask_;
    // Minimum distance.
    float minDistance_;
};

void PhysicsWorld2D::RaycastSingle(PhysicsRaycastResult2D& result, const Vector2& startPoint, const Vector2& endPoint,
    unsigned collisionMask)
{
    result.body_ = 0;

    SingleRayCastCallback callback(result, startPoint, collisionMask);
    world_->RayCast(&callback, ToB2Vec2(startPoint), ToB2Vec2(endPoint));
}

// Point query callback class.
class PointQueryCallback : public b2QueryCallback
{
public:
    // Construct.
    PointQueryCallback(const b2Vec2& point, unsigned collisionMask) :
        point_(point),
        collisionMask_(collisionMask),
        rigidBody_(0),
        shape_(0)
    {
    }

    // Called for each fixture found in the query AABB.
    virtual bool ReportFixture(b2Fixture* fixture)
    {
        // Ignore sensor
//        if (fixture->IsSensor())
//            return true;

        if ((fixture->GetFilterData().categoryBits & collisionMask_) == 0)
            return true;

        if (fixture->TestPoint(point_))
        {
            rigidBody_ = (RigidBody2D*)(fixture->GetBody()->GetUserData());
            shape_ = (CollisionShape2D*)(fixture->GetUserData());
            return false;
        }

        return true;
    }

    // Return rigid body.
    RigidBody2D* GetRigidBody() const { return rigidBody_; }
    CollisionShape2D* GetShape() const { return shape_; }

private:
    // Point.
    b2Vec2 point_;
    // Collision mask.
    unsigned collisionMask_;
    // Rigid body.
    RigidBody2D* rigidBody_;
    // Collision shape.
    CollisionShape2D* shape_;
};


void PhysicsWorld2D::GetPhysicElements(const Vector2& point, RigidBody2D*& body, CollisionShape2D*& shape, unsigned collisionMask)
{
    PointQueryCallback callback(ToB2Vec2(point), collisionMask);

    b2AABB b2Aabb;
    Vector2 delta(M_EPSILON, M_EPSILON);
    b2Aabb.lowerBound = ToB2Vec2(point - delta);
    b2Aabb.upperBound = ToB2Vec2(point + delta);

    world_->QueryAABB(&callback, b2Aabb);

    body = callback.GetRigidBody();
    shape = callback.GetShape();
}

RigidBody2D* PhysicsWorld2D::GetRigidBody(const Vector2& point, unsigned collisionMask)
{
    PointQueryCallback callback(ToB2Vec2(point), collisionMask);

    b2AABB b2Aabb;
    Vector2 delta(M_EPSILON, M_EPSILON);
    b2Aabb.lowerBound = ToB2Vec2(point - delta);
    b2Aabb.upperBound = ToB2Vec2(point + delta);

    world_->QueryAABB(&callback, b2Aabb);
    return callback.GetRigidBody();
}

RigidBody2D* PhysicsWorld2D::GetRigidBody(int screenX, int screenY, unsigned collisionMask)
{
    Renderer* renderer = GetSubsystem<Renderer>();
    for (unsigned i = 0; i < renderer->GetNumViewports(); ++i)
    {
        Viewport* viewport = renderer->GetViewport(i);
        // Find a viewport with same scene
        if (viewport && viewport->GetScene() == GetScene())
        {
            Vector3 worldPoint = viewport->ScreenToWorldPoint(screenX, screenY, 0.0f);
            return GetRigidBody(Vector2(worldPoint.x_, worldPoint.y_), collisionMask);
        }
    }

    return 0;
}

// Aabb query callback class.
class AabbQueryCallback : public b2QueryCallback
{
public:
    // Construct.
    AabbQueryCallback(PODVector<RigidBody2D*>& results, unsigned collisionMask) :
        results_(results),
        collisionMask_(collisionMask)
    {
    }

    // Called for each fixture found in the query AABB.
    virtual bool ReportFixture(b2Fixture* fixture)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

//        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
        if ((fixture->GetFilterData().categoryBits & collisionMask_) == 0)
            return true;

        results_.Push((RigidBody2D*)(fixture->GetBody()->GetUserData()));

        return true;
    }

private:
    // Results.
    PODVector<RigidBody2D*>& results_;
    // Collision mask.
    unsigned collisionMask_;
};

// Aabb query callback class without duplicates
class AabbQueryPruneCallback : public b2QueryCallback
{
public:
    // Construct.
    AabbQueryPruneCallback(PODVector<RigidBody2D*>& results, unsigned collisionMask) :
        results_(results),
        collisionMask_(collisionMask)
    {
    }

    // Called for each fixture found in the query AABB.
    virtual bool ReportFixture(b2Fixture* fixture)
    {
        // Ignore sensor
        if (fixture->IsSensor())
            return true;

//        if ((fixture->GetFilterData().maskBits & collisionMask_) == 0)
        if ((fixture->GetFilterData().categoryBits & collisionMask_) == 0)
            return true;

        RigidBody2D* body = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        if (!results_.Size() || results_.Find(body) == results_.End())
            results_.Push(body);

        return true;
    }

private:
    // Results.
    PODVector<RigidBody2D*>& results_;
    // Collision mask.
    unsigned collisionMask_;
};

void PhysicsWorld2D::GetRigidBodies(PODVector<RigidBody2D*>& results, const Rect& aabb, unsigned collisionMask, bool prune)
{
    b2AABB b2Aabb;
    Vector2 delta(M_EPSILON, M_EPSILON);
    b2Aabb.lowerBound = ToB2Vec2(aabb.min_ - delta);
    b2Aabb.upperBound = ToB2Vec2(aabb.max_ + delta);

    if (prune)
    {
        AabbQueryPruneCallback callback(results, collisionMask);
        world_->QueryAABB(&callback, b2Aabb);
    }
    else
    {
        AabbQueryCallback callback(results, collisionMask);
        world_->QueryAABB(&callback, b2Aabb);
    }
}

bool PhysicsWorld2D::GetAllowSleeping() const
{
    return world_->GetAllowSleeping();
}

bool PhysicsWorld2D::GetWarmStarting() const
{
    return world_->GetWarmStarting();
}

bool PhysicsWorld2D::GetContinuousPhysics() const
{
    return world_->GetContinuousPhysics();
}

bool PhysicsWorld2D::GetSubStepping() const
{
    return world_->GetSubStepping();
}

bool PhysicsWorld2D::GetAutoClearForces() const
{
    return world_->GetAutoClearForces();
}

void PhysicsWorld2D::OnSceneSet(Scene* scene)
{
    // Subscribe to the scene subsystem update, which will trigger the physics simulation step
    if (scene)
        SubscribeToEvent(scene, E_SCENESUBSYSTEMUPDATE, URHO3D_HANDLER(PhysicsWorld2D, HandleSceneSubsystemUpdate));
    else
        UnsubscribeFromEvent(E_SCENESUBSYSTEMUPDATE);
}

void PhysicsWorld2D::HandleSceneSubsystemUpdate(StringHash eventType, VariantMap& eventData)
{
    if (!updateEnabled_)
        return;

    using namespace SceneSubsystemUpdate;
    Update(eventData[P_TIMESTEP].GetFloat());
}

void PhysicsWorld2D::SendBeginContactEvents()
{
    if (beginContactInfos_.Empty())
        return;

    using namespace PhysicsBeginContact2D;
    VariantMap& eventData = GetEventDataMap();

    for (unsigned i = 0; i < beginContactInfos_.Size(); ++i)
    {
        const ContactInfo& contactInfo = beginContactInfos_[i];

        eventData[P_CONTACTINFO] = i;

		if (contactInfo.bodyA_->GetNode())
			contactInfo.bodyA_->GetNode()->SendEvent(E_PHYSICSBEGINCONTACT2D, eventData);
		if (contactInfo.bodyB_->GetNode())
			contactInfo.bodyB_->GetNode()->SendEvent(E_PHYSICSBEGINCONTACT2D, eventData);
    }
}

void PhysicsWorld2D::SendEndContactEvents()
{
    if (endContactInfos_.Empty())
        return;

    using namespace PhysicsEndContact2D;
    VariantMap& eventData = GetEventDataMap();

    for (unsigned i = 0; i < endContactInfos_.Size(); ++i)
    {
        const ContactInfo& contactInfo = endContactInfos_[i];

        eventData[P_CONTACTINFO] = i;

		if (contactInfo.bodyA_->GetNode())
			contactInfo.bodyA_->GetNode()->SendEvent(E_PHYSICSENDCONTACT2D, eventData);
		if (contactInfo.bodyB_->GetNode())
			contactInfo.bodyB_->GetNode()->SendEvent(E_PHYSICSENDCONTACT2D, eventData);
    }
}

ContactInfo::ContactInfo() { }

ContactInfo::ContactInfo(b2Contact* contact)
{
	b2Fixture* fixture = contact->GetFixtureA();
    if (fixture)
    {
        bodyA_   = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        shapeA_  = (CollisionShape2D*)(fixture->GetUserData());
        iShapeA_ = contact->GetChildIndexA();
    }
    else
	{
		bodyA_   = 0;
		shapeA_  = 0;
		iShapeA_ = 0;
	}

    fixture = contact->GetFixtureB();
    if (fixture)
    {
        bodyB_   = (RigidBody2D*)(fixture->GetBody()->GetUserData());
        shapeB_  = (CollisionShape2D*)(fixture->GetUserData());
        iShapeB_ = contact->GetChildIndexB();
    }
    else
	{
		bodyB_   = 0;
		shapeB_  = 0;
		iShapeB_ = 0;
	}

    b2WorldManifold wManifold;
    contact->GetWorldManifold(&wManifold);
    contactPoint_ = ToVector2(wManifold.points[0]);
    normal_ = ToVector2(wManifold.normal);
}

ContactInfo::ContactInfo(const ContactInfo& other) :
    bodyA_(other.bodyA_),
    bodyB_(other.bodyB_),
    shapeA_(other.shapeA_),
    shapeB_(other.shapeB_),
    iShapeA_(other.iShapeA_),
    iShapeB_(other.iShapeB_),
    contactPoint_(other.contactPoint_),
    normal_(other.normal_) { }

}
