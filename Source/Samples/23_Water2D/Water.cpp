//
// Copyright (c) 2008-2022 the Urho3D project.
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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/RenderSurface.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Graphics/DebugRenderer.h>

#include <Urho3D/Urho2D/Renderer2D.h>
#include <Urho3D/Urho2D/AnimatedSprite2D.h>
#include <Urho3D/Urho2D/AnimationSet2D.h>
#include <Urho3D/Urho2D/TileMap2D.h>
#include <Urho3D/Urho2D/TileMapLayer2D.h>
#include <Urho3D/Urho2D/TmxFile2D.h>

#include <Urho3D/Input/Input.h>
#include <Urho3D/IO/File.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "Water.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(Water)

Water::Water(Context* context) :
    Sample(context)
{
}

void Water::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update event
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
//    Sample::InitMouseMode(MM_RELATIVE);
    auto* input = GetSubsystem<Input>();
    input->SetMouseVisible(true, true);
}

static Sprite2D* sGroundtile = 0;

void AddGroundTile(Node* rootnode, const Vector3& position, const Color& color, int layer)
{
    auto* tilenode = rootnode->CreateChild("tile");
    tilenode->SetPosition(position);
    auto* staticSprite2D = tilenode->CreateComponent<StaticSprite2D>();
    staticSprite2D->SetSprite(sGroundtile);
    staticSprite2D->SetColor(color);
    staticSprite2D->SetLayer(layer);
    staticSprite2D->SetOccluder(true);
}

const float backgroundz = 0.f;
const float waterz = -0.05f;
const float foregroundz = -0.1f;

void Water::CreateScene()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* graphics = GetSubsystem<Graphics>();

    scene_ = new Scene(context_);
    scene_->CreateComponent<Octree>();

    // Create camera node
    cameraNode_ = scene_->CreateChild("Camera");
    // Set camera's position
    cameraNode_->SetPosition(Vector3(0.0f, 0.0f, -10.0f));

    auto* camera = cameraNode_->CreateComponent<Camera>();
//    camera->SetOrthographic(true);
//    camera->SetOrthoSize((float)graphics->GetHeight() * PIXEL_SIZE);
//    camera->SetZoom(1.5f * Min((float)graphics->GetWidth() / 1280.0f, (float)graphics->GetHeight() / 800.0f)); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.5) is set for full visibility at 1280x800 resolution)

    Node* groundnode = scene_->CreateChild("Ground");
    groundnode->SetPosition(Vector3(0.f, 0.f, backgroundz));
    Node* waterNode  = scene_->CreateChild("Water");
    waterNode->SetPosition(Vector3(0.f, 0.f, waterz));

    // add tilemap
/*
    auto* tmxFile = cache->GetResource<TmxFile2D>("Urho2D/Tilesets/Ortho.tmx");
    if (tmxFile)
    {
        auto* tilemapnode = scene_->CreateChild("tilemap");
        auto* tileMap = tilemapnode->CreateComponent<TileMap2D>();
        tileMap->SetTmxFile(tmxFile);
        tilemapnode->SetScale(Vector3(0.5f, 0.5f, 1.f));
        tilemapnode->SetPosition(Vector3(-6.f, -4.f, 0.f));
    }
*/
    // create the sprites of the tiles
    Texture2D* texture = cache->GetResource<Texture2D>("2D/Textures/groundtiles.png");
    sGroundtile = new Sprite2D(context_);
    sGroundtile->SetTexture(texture);
    sGroundtile->SetRectangle(IntRect(0, 0, 128, 128));

    // add a background tile
    AddGroundTile(groundnode, Vector3(0.f, 0.f, backgroundz), Color::WHITE, 0);

    // add a foreground tile
    AddGroundTile(groundnode, Vector3(-1.f, 0.f, foregroundz), Color::YELLOW, 100);

    // add a water tile
    AddGroundTile(waterNode, Vector3(0.f, 0.f, 0.f), Color(0.6f, 0.9f, 0.8f, 1.f), 1);
    auto* waterSprite2D = waterNode->GetChild("tile")->GetComponent<StaticSprite2D>();
    waterSprite2D->SetSprite(0);
    waterSprite2D->SetDrawRect(Rect(-0.64f, -0.64f, 0.64f, 0.64f));
    waterSprite2D->SetUseDrawRect(true);
    waterSprite2D->SetCustomMaterial(cache->GetResource<Material>("Materials/Water2D.xml"));
}

void Water::SetupViewport()
{
    auto* graphics = GetSubsystem<Graphics>();
    auto* renderer = GetSubsystem<Renderer>();
    auto* cache = GetSubsystem<ResourceCache>();

    SharedPtr<RenderPath> renderpath(new RenderPath());
    renderpath->Load(GetSubsystem<ResourceCache>()->GetResource<XMLFile>("RenderPaths/ForwardUrho2D.xml"));

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>(), renderpath));
    renderer->SetViewport(0, viewport);
}

void Water::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(Water, HandleUpdate));
}

void Water::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

    // In case resolution has changed, adjust the reflection camera aspect ratio
    if (reflectionCameraNode_)
    {
        auto* graphics = GetSubsystem<Graphics>();
        auto* reflectionCamera = reflectionCameraNode_->GetComponent<Camera>();
        reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());
    }
}

bool drawDebug_ = false;

void Water::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);

	if (GetSubsystem<Input>()->GetScancodePress(SCANCODE_G))
    {
        drawDebug_ = drawDebug_ ? false : true;
        if (drawDebug_)
            SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(Water, OnPostRenderUpdate));
        else
            UnsubscribeFromEvent(E_POSTRENDERUPDATE);
    }
}

void Water::OnPostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    DebugRenderer* debugRenderer = scene_->GetOrCreateComponent<DebugRenderer>();

    if (scene_->GetChild("Ground"))
    {
        PODVector<StaticSprite2D*> drawables;
        scene_->GetChild("Ground")->GetDerivedComponents<StaticSprite2D>(drawables, true);
        for (int i=0; i < drawables.Size(); i++)
            drawables[i]->DrawDebugGeometry(debugRenderer, false);
    }

    if (scene_->GetChild("Water"))
        scene_->GetChild("Water")->GetComponent<StaticSprite2D>()->DrawDebugGeometry(debugRenderer, false);
}
