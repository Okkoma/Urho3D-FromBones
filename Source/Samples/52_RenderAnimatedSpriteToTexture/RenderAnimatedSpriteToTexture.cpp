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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/RenderSurface.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Graphics/DebugRenderer.h>

#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>

#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/ObjectAnimation.h>
#include <Urho3D/Scene/ValueAnimation.h>

#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/Text3D.h>
#include <Urho3D/UI/Font.h>

#include <Urho3D/Urho2D/Renderer2D.h>
#include <Urho3D/Urho2D/AnimatedSprite2D.h>
#include <Urho3D/Urho2D/AnimationSet2D.h>
#include <Urho3D/Urho2D/SpriterInstance2D.h>
#include <Urho3D/Urho2D/SpriterData2D.h>
#include <Urho3D/Urho2D/Sprite2D.h>

#include "RenderAnimatedSpriteToTexture.h"


#include <Urho3D/DebugNew.h>


#define ANIMATEDSPRITE_INTEGRATION
//#define ACTIVE_RENDERTEST

/// Render Target Camera Nodes & Scenes

SharedPtr<Scene> rttScene_;
WeakPtr<Node> rttRootNode_;
int rttunit_;

SharedPtr<Texture2D> renderTexture_;
SharedPtr<Material> renderMaterial_;
SharedPtr<Viewport> rttViewport_;

Node *fantomette1_, *fantomette2_, *fantomette3_;
Node *rttfantomette1_, *rttfantomette2_;

Graphics* graphics_;

/// Camera scene node.
WeakPtr<Node> cameraNode_;
const float cameraDepth_ = 5.f;

const StringHash CMAP_HEAD1 = StringHash("Head1");
const StringHash CMAP_HEAD2 = StringHash("Head2");
const StringHash CMAP_HEAD3 = StringHash("Head3");
const StringHash CMAP_NAKED = StringHash("Naked");
const StringHash CMAP_ARMOR = StringHash("Armor");
const StringHash CMAP_HELMET = StringHash("Helmet");
const StringHash CMAP_WEAPON1 = StringHash("Weapon1");
const StringHash CMAP_WEAPON2 = StringHash("Weapon2");
const StringHash CMAP_BELT = StringHash("Belt");
const StringHash CMAP_CAPE = StringHash("Cape");
const StringHash CMAP_BLINDFOLD = StringHash("BlindFold");
const StringHash CMAP_TAIL = StringHash("Tail");
const StringHash CMAP_NOARMOR = StringHash("No_Armor");
const StringHash CMAP_NOHELMET = StringHash("No_Helmet");
const StringHash CMAP_NOWEAPON1 = StringHash("No_Weapon1");
const StringHash CMAP_NOWEAPON2 = StringHash("No_Weapon2");
const StringHash CMAP_NOBELT = StringHash("No_Belt");
const StringHash CMAP_NOCAPE = StringHash("No_Cape");
const StringHash CMAP_NOBLINDFOLD = StringHash("No_BlindFold");
const StringHash CMAP_NOTAIL = StringHash("No_Tail");

static float rttyaw_ = 0.f;
static float rttpitch_ = 0.f;
static bool drawDebug_ = false;
static bool spriteDirty_ = false;


void TextTest(Context* context, Node* rootnode, const Vector3& position, const String& message, const String& fontname, float duration, float fadescale, int fontsize)
{
    Font* font = context->GetSubsystem<ResourceCache>()->GetResource<Font>(fontname);
    Vector3 scale = Vector3::ONE / rootnode->GetWorldScale();

    Node* node = rootnode->CreateChild("Text3D");
    node->SetEnabled(false);
    Text3D* text3D = node->CreateComponent<Text3D>();
    text3D->SetEnabled(false);
    text3D->SetText(message);
    text3D->SetFont(font, fontsize);
    text3D->SetAlignment(HA_CENTER, VA_CENTER);

    SharedPtr<ObjectAnimation> textAnimation(new ObjectAnimation(context));
    SharedPtr<ValueAnimation> alphaAnimation(new ValueAnimation(context));
    alphaAnimation->SetKeyFrame(0.f, 0.f);
    alphaAnimation->SetKeyFrame(0.05f*duration, 1.f);
    alphaAnimation->SetKeyFrame(0.85f*duration, 1.f);
    alphaAnimation->SetKeyFrame(duration, 0.f);
    textAnimation->AddAttributeAnimation("Opacity", alphaAnimation, WM_ONCE);
    text3D->SetObjectAnimation(textAnimation);

    if (fadescale != 1.f)
    {
        SharedPtr<ObjectAnimation> nodeAnimation(new ObjectAnimation(context));
        SharedPtr<ValueAnimation> scaleAnimation(new ValueAnimation(context));
        scaleAnimation->SetKeyFrame(0.f, scale);
        scaleAnimation->SetKeyFrame(0.05f*duration, scale);
        scaleAnimation->SetKeyFrame(0.85f*duration, scale);
        scaleAnimation->SetKeyFrame(duration, fadescale * scale);
        nodeAnimation->AddAttributeAnimation("Scale", scaleAnimation, WM_ONCE);
        node->SetObjectAnimation(nodeAnimation);
    }

    node->SetEnabled(true);
    text3D->SetEnabled(true);
    node->SetScale(scale);
    node->SetPosition(position);
}


URHO3D_DEFINE_APPLICATION_MAIN(RenderAnimatedSpriteToTexture)

RenderAnimatedSpriteToTexture::RenderAnimatedSpriteToTexture(Context* context) :
    Sample(context)
{

}

void RenderAnimatedSpriteToTexture::Start()
{
    // Execute base class startup
    Sample::Start();

    graphics_ = GetSubsystem<Graphics>();

    fantomette1_ = fantomette2_ = rttfantomette1_ = rttfantomette2_ = 0;

    // Create the scene content
    CreateScene();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
//    Sample::InitMouseMode(MM_RELATIVE);
    Sample::InitMouseMode(MM_FREE);
}

void RenderAnimatedSpriteToTexture::Stop()
{
	UnsubscribeFromAllEvents();

    cameraNode_.Reset();

#ifdef ACTIVE_RENDERTEST
    AnimatedSprite2D::SetRenderTargetContext();

    renderTexture_.Reset();
    renderMaterial_.Reset();
    rttViewport_.Reset();
	rttScene_.Reset();
#endif

    // Execute base class startup
    Sample::Stop();
}

void RenderAnimatedSpriteToTexture::CreateRenderTargetSprite(Node*& node, Node*& rttnode, const String& scmlset, const String& customssheet, const Vector2& position,
                                                             const Vector2& scale, Material* material, const Color& color, int layer, int textureEffect)
{
    if (!rttnode)
    {
        // Create a drawable in the rtt sene.
        if (!rttRootNode_)
            rttRootNode_ = rttScene_->CreateChild("RttRootNode");
        rttnode = rttRootNode_->CreateChild();
        // set the drawable in the rtt scene
        AnimatedSprite2D* animatedSprite = rttnode->CreateComponent<AnimatedSprite2D>();
        if (!customssheet.Empty())
            animatedSprite->SetCustomSpriteSheetAttr(customssheet);
        animatedSprite->SetCustomMaterial(material);
        if (textureEffect)
            animatedSprite->SetTextureFX(textureEffect);
        AnimationSet2D* animationSet = GetSubsystem<ResourceCache>()->GetResource<AnimationSet2D>(scmlset);
        animatedSprite->SetAnimationSet(animationSet);
        animatedSprite->SetAnimation("idle");
        animatedSprite->SetDynamicBoundingBox(true);
        animatedSprite->SetCustomSpriteSheetAttr(String::EMPTY);
    }

    // create and set the drawable in the displayed scene
    node = scene_->CreateChild();
    node->SetPosition2D(position);
    node->SetScale2D(scale);

    Sprite2D* sprite = new Sprite2D(context_);
    sprite->SetTexture(renderTexture_);
    sprite->SetRectangle(IntRect(0, 0, renderTexture_->GetWidth(), renderTexture_->GetHeight()));
    StaticSprite2D* staticSprite = node->CreateComponent<StaticSprite2D>();
    staticSprite->SetCustomMaterial(material);
    // use the effects CROPALPHA + UNLIT
    if (textureEffect)
        staticSprite->SetTextureFX(textureEffect);
    staticSprite->SetSprite(sprite);
    // set the alpha
    staticSprite->SetColor(color);
    staticSprite->SetLayer(layer);

    spriteDirty_ = true;
}

void RenderAnimatedSpriteToTexture::UpdateRenderTargetSprite(StaticSprite2D* ssprite, AnimatedSprite2D* rttAnimatedSprite)
{
    const int enlarge = 8;

    BoundingBox bbox = rttAnimatedSprite->GetWorldBoundingBox2D();

    float hscreenx = (float)renderTexture_->GetWidth() * 0.5f;
    float hscreeny = (float)renderTexture_->GetHeight() * 0.5f;

    IntRect rect((int)(hscreenx + bbox.min_.x_ / PIXEL_SIZE) - enlarge, (int)(hscreeny - bbox.max_.y_ / PIXEL_SIZE) - enlarge,
                 (int)(hscreenx + bbox.max_.x_ / PIXEL_SIZE) + enlarge, (int)(hscreeny - bbox.min_.y_ / PIXEL_SIZE) + enlarge);

    Vector2 hotspot((rttAnimatedSprite->GetNode()->GetPosition().x_ - bbox.min_.x_) / (bbox.max_.x_ - bbox.min_.x_),
                    (rttAnimatedSprite->GetNode()->GetPosition().y_ - bbox.min_.y_) / (bbox.max_.y_ - bbox.min_.y_));

    ssprite->GetSprite()->SetRectangle(rect);
    ssprite->GetSprite()->SetSourceSize(rect.right_ - rect.left_, rect.bottom_ - rect.top_);
    ssprite->GetSprite()->SetHotSpot(hotspot);

    ssprite->SetDrawRect(Rect::ZERO);
    ssprite->MarkDirty();

//    URHO3D_LOGINFOF("sprite rttbox=%s rectangle=%s sourcesize=%s hotspot=%s", bbox.ToString().CString(), rect.ToString().CString(), rect.Size().ToString().CString(), hotspot.ToString().CString());
}

void RenderAnimatedSpriteToTexture::UpdateRenderTargetNodePositions()
{
    float hw = (float)renderTexture_->GetWidth() * 0.5f * PIXEL_SIZE;
//    float hh = (float)renderTexture_->GetHeight() * 0.5f * PIXEL_SIZE;
//    Rect rscreen(-hw, -hh, hw, hh);

    const Vector<SharedPtr<Node> >& children = rttRootNode_->GetChildren();

    /// TODO : just distribute nodes on a row for the moment
    int numparts = children.Size() + 1;
    float pw = 2.f * hw / numparts;
    for (int i=0; i < children.Size(); i++)
        children[i]->SetPosition(Vector3(-hw + (i+1)*pw, 0.f, 0.f));
}


const char* SpriterObjInfoStr[] =
{
    "bone",
    "sprite",
    "point",
    "box",
    0
};

const char* SpriterCurveTypeStr[] =
{
    "instant",
    "linear",
    "quadratic",
    "cubic",
    "quartic",
    "quintic",
    "bezier",
    0
};

void SpriterSaveData(Context* context, AnimationSet2D* animationset, const String& filename)
{
    if (!animationset->GetSpriterData())
        return;

    Spriter::SpriterData& spriterdata = *animationset->GetSpriterData();

    SharedPtr<XMLFile> xml(new XMLFile(context));
    XMLElement rootElem = xml->CreateRoot("spriter_data");
    rootElem.SetInt("scml_version", spriterdata.scmlVersion_);
    rootElem.SetAttribute("generator", spriterdata.generator_);
    rootElem.SetAttribute("generator_version", spriterdata.generatorVersion_);

    for (unsigned i=0; i < spriterdata.folders_.Size(); i++)
    {
        Spriter::Folder* folder = spriterdata.folders_[i];
        XMLElement folderElem = rootElem.CreateChild("folder");
        folderElem.SetUInt("id", folder->id_);

        for (unsigned j=0; j < folder->files_.Size(); j++)
        {
            Spriter::File* file = folder->files_[j];
            XMLElement fileElem = folderElem.CreateChild("file");
            fileElem.SetUInt("id", file->id_);
            fileElem.SetAttribute("name", file->name_);
            fileElem.SetUInt("width", file->width_);
            fileElem.SetUInt("height", file->height_);
            fileElem.SetFloat("pivot_x", file->pivotX_);
            fileElem.SetFloat("pivot_y", file->pivotY_);
        }
    }

    for (unsigned i=0; i < spriterdata.entities_.Size(); i++)
    {
        Spriter::Entity* entity = spriterdata.entities_[i];
        XMLElement entityElem = rootElem.CreateChild("entity");
        entityElem.SetUInt("id", entity->id_);
        entityElem.SetAttribute("name", entity->name_);

        // Color

        // ObjInfo
        for (HashMap<String, Spriter::ObjInfo >::ConstIterator it=entity->objInfos_.Begin(); it != entity->objInfos_.End(); ++it)
        {
            const String& name = it->first_;
            const Spriter::ObjInfo& objinfo = it->second_;
            int type = (int)objinfo.type_;
            if (type < 0 || type > 3)
                continue;

            XMLElement objinfoElem = entityElem.CreateChild("obj_info");
            objinfoElem.SetAttribute("name", it->first_);
            objinfoElem.SetAttribute("type", SpriterObjInfoStr[type]);
            objinfoElem.SetFloat("w", objinfo.width_);
            objinfoElem.SetFloat("h", objinfo.height_);
            if (objinfo.pivotX_ != 0.f)
                objinfoElem.SetFloat("pivot_x", objinfo.pivotX_);
            if (objinfo.pivotY_ != 1.f)
                objinfoElem.SetFloat("pivot_y", objinfo.pivotY_);
        }

        // CharacterMaps
        for (PODVector<Spriter::CharacterMap*>::ConstIterator it=entity->characterMaps_.Begin(); it != entity->characterMaps_.End(); ++it)
        {
            const Spriter::CharacterMap& cmap = **it;

            XMLElement cmapElem = entityElem.CreateChild("character_map");
            cmapElem.SetInt("id", cmap.id_);
            cmapElem.SetAttribute("name", cmap.name_);

            for (PODVector<Spriter::MapInstruction*>::ConstIterator jt=cmap.maps_.Begin(); jt != cmap.maps_.End(); ++jt)
            {
                const Spriter::MapInstruction& cinst = **jt;
                XMLElement cinstElem = cmapElem.CreateChild("map");
                cinstElem.SetInt("folder", cinst.folder_);
                cinstElem.SetInt("file", cinst.file_);
                if (cinst.targetFolder_ != -1 && cinst.targetFile_ != -1)
                {
                    cinstElem.SetInt("target_folder", cinst.targetFolder_);
                    cinstElem.SetInt("target_file", cinst.targetFile_);
                }
            }
        }

        // Animations
        for (PODVector<Spriter::Animation*>::ConstIterator it=entity->animations_.Begin(); it != entity->animations_.End(); ++it)
        {
            Spriter::Animation* animation = *it;

            XMLElement animationElem = entityElem.CreateChild("animation");
            animationElem.SetUInt("id", animation->id_);
            animationElem.SetAttribute("name", animation->name_);
            animationElem.SetUInt("length", animation->length_ * 1000);
            // interval ? new spec in scml ?
            if (animation->looping_)
//                animationElem.SetBool("looping", animation->looping_);
                animationElem.SetInt("interval", 100);

            XMLElement mainlineElem = animationElem.CreateChild("mainline");
            for (PODVector<Spriter::MainlineKey*>::ConstIterator it=animation->mainlineKeys_.Begin(); it != animation->mainlineKeys_.End(); ++it)
            {
                Spriter::MainlineKey* mainlinekey = *it;

                XMLElement mainlinekeyElem = mainlineElem.CreateChild("key");
                mainlinekeyElem.SetUInt("id", mainlinekey->id_);
                mainlinekeyElem.SetUInt("time", mainlinekey->time_ * 1000);

                for (PODVector<Spriter::Ref*>::ConstIterator jt=mainlinekey->boneRefs_.Begin(); jt != mainlinekey->boneRefs_.End(); ++jt)
                {
                    Spriter::Ref* ref = *jt;

                    XMLElement bonerefElem = mainlinekeyElem.CreateChild("bone_ref");
                    bonerefElem.SetUInt("id", ref->id_);
                    if (ref->parent_ > -1)
                        bonerefElem.SetInt("parent", ref->parent_);
                    bonerefElem.SetInt("timeline", ref->timeline_);
                    bonerefElem.SetInt("key", ref->key_);
                }

                for (PODVector<Spriter::Ref*>::ConstIterator jt=mainlinekey->objectRefs_.Begin(); jt != mainlinekey->objectRefs_.End(); ++jt)
                {
                    Spriter::Ref* ref = *jt;
                    XMLElement objrefElem = mainlinekeyElem.CreateChild("object_ref");
                    objrefElem.SetUInt("id", ref->id_);
                    if (ref->parent_ > -1)
                        objrefElem.SetInt("parent", ref->parent_);
                    objrefElem.SetInt("timeline", ref->timeline_);
                    objrefElem.SetInt("key", ref->key_);
                    if (ref->zIndex_ > -1)
                        objrefElem.SetInt("zIndex", ref->zIndex_);
                }
            }

            unsigned bonecounter = 0;
            for (PODVector<Spriter::Timeline*>::ConstIterator it=animation->timelines_.Begin(); it != animation->timelines_.End(); ++it)
            {
                Spriter::Timeline* timeline = *it;

                int type = (int)timeline->objectType_;
                if (type < 0 || type > 3)
                    continue;

                XMLElement timelineElem = animationElem.CreateChild("timeline");
                timelineElem.SetUInt("id", (unsigned)(it-animation->timelines_.Begin()));
                if (type == Spriter::BONE)
                    timelineElem.SetUInt("obj", bonecounter++);
                timelineElem.SetAttribute("name", timeline->name_);
                if (type != Spriter::SPRITE)
                    timelineElem.SetAttribute("object_type", SpriterObjInfoStr[type]);

                if (type == Spriter::BONE)
                {
                    for (PODVector<Spriter::SpatialTimelineKey*>::ConstIterator jt = timeline->keys_.Begin(); jt != timeline->keys_.End(); ++jt)
                    {
                        Spriter::BoneTimelineKey* bonetimekey = (Spriter::BoneTimelineKey*)(*jt);
                        XMLElement timekeyElem = timelineElem.CreateChild("key");
                        timekeyElem.SetUInt("id", bonetimekey->id_);
                        if (bonetimekey->time_ > 0.f)
                            timekeyElem.SetUInt("time", bonetimekey->time_*1000);
                        int curvetype = (int)bonetimekey->curveType_;
                        if (curvetype != Spriter::LINEAR)
                        {
                            timekeyElem.SetAttribute("curve_type", SpriterCurveTypeStr[curvetype]);
                            if (curvetype > Spriter::INSTANT)
                            {
                                timekeyElem.SetFloat("c1", bonetimekey->c1_);
                                if (curvetype > Spriter::QUADRATIC)
                                {
                                    timekeyElem.SetFloat("c2", bonetimekey->c2_);
                                    if (curvetype > Spriter::CUBIC)
                                    {
                                        timekeyElem.SetFloat("c3", bonetimekey->c3_);
                                        if (curvetype > Spriter::QUARTIC)
                                            timekeyElem.SetFloat("c4", bonetimekey->c4_);
                                    }
                                }
                            }
                        }
                        if (bonetimekey->info_.spin < 1)
                            timekeyElem.SetInt("spin", bonetimekey->info_.spin);

                        XMLElement bonekeyElem = timekeyElem.CreateChild("bone");
                        bonekeyElem.SetFloat("x", bonetimekey->info_.x_);
                        bonekeyElem.SetFloat("y", bonetimekey->info_.y_);
                        bonekeyElem.SetFloat("angle", bonetimekey->info_.angle_);
                        if (bonetimekey->info_.scaleX_ != 1.f)
                            bonekeyElem.SetFloat("scale_x", bonetimekey->info_.scaleX_);
                        if (bonetimekey->info_.scaleY_ != 1.f)
                            bonekeyElem.SetFloat("scale_y", bonetimekey->info_.scaleY_);
                    }
                }
                else if (type == Spriter::SPRITE || type == Spriter::POINT)
                {
                    for (PODVector<Spriter::SpatialTimelineKey*>::ConstIterator jt = timeline->keys_.Begin(); jt != timeline->keys_.End(); ++jt)
                    {
                        Spriter::SpriteTimelineKey* spritetimekey = (Spriter::SpriteTimelineKey*)(*jt);
                        XMLElement timekeyElem = timelineElem.CreateChild("key");
                        timekeyElem.SetUInt("id", spritetimekey->id_);
                        if (spritetimekey->time_ > 0.f)
                            timekeyElem.SetUInt("time", spritetimekey->time_*1000);
                        int curvetype = (int)spritetimekey->curveType_;
                        if (curvetype != Spriter::LINEAR)
                        {
                            timekeyElem.SetAttribute("curve_type", SpriterCurveTypeStr[curvetype]);
                            if (curvetype > Spriter::INSTANT)
                            {
                                timekeyElem.SetFloat("c1", spritetimekey->c1_);
                                if (curvetype > Spriter::QUADRATIC)
                                {
                                    timekeyElem.SetFloat("c2", spritetimekey->c2_);
                                    if (curvetype > Spriter::CUBIC)
                                    {
                                        timekeyElem.SetFloat("c3", spritetimekey->c3_);
                                        if (curvetype > Spriter::QUARTIC)
                                            timekeyElem.SetFloat("c4", spritetimekey->c4_);
                                    }
                                }
                            }
                        }
                        if (spritetimekey->info_.spin < 1)
                            timekeyElem.SetInt("spin", spritetimekey->info_.spin);

                        XMLElement spritekeyElem = timekeyElem.CreateChild("object");
                        if (type == Spriter::SPRITE)
                        {
                            spritekeyElem.SetUInt("folder", spritetimekey->folderId_);
                            spritekeyElem.SetUInt("file", spritetimekey->fileId_);
                        }

                        spritekeyElem.SetFloat("x", spritetimekey->info_.x_);
                        spritekeyElem.SetFloat("y", spritetimekey->info_.y_);
                        if (spritetimekey->pivotX_ != 0.f)
                            spritekeyElem.SetFloat("pivot_x", spritetimekey->pivotX_);
                        if (spritetimekey->pivotY_ != 1.f)
                            spritekeyElem.SetFloat("pivot_y", spritetimekey->pivotY_);

                        spritekeyElem.SetFloat("angle", spritetimekey->info_.angle_);

                        if (spritetimekey->info_.scaleX_ != 1.f)
                            spritekeyElem.SetFloat("scale_x", spritetimekey->info_.scaleX_);
                        if (spritetimekey->info_.scaleY_ != 1.f)
                            spritekeyElem.SetFloat("scale_y", spritetimekey->info_.scaleY_);
                        if (spritetimekey->info_.alpha_ != 1.f)
                            spritekeyElem.SetFloat("a", spritetimekey->info_.alpha_);
                    }
                }
                else if (type == Spriter::BOX)
                {
                    for (PODVector<Spriter::SpatialTimelineKey*>::ConstIterator jt = timeline->keys_.Begin(); jt != timeline->keys_.End(); ++jt)
                    {
                        Spriter::BoxTimelineKey* boxtimekey = (Spriter::BoxTimelineKey*)(*jt);

                        XMLElement timekeyElem = timelineElem.CreateChild("key");
                        timekeyElem.SetUInt("id", boxtimekey->id_);
                        if (boxtimekey->time_ > 0.f)
                            timekeyElem.SetUInt("time", boxtimekey->time_*1000);

                        if (boxtimekey->info_.spin < 1)
                            timekeyElem.SetInt("spin", boxtimekey->info_.spin);

                        XMLElement boxkeyElem = timekeyElem.CreateChild("object");

                        boxkeyElem.SetFloat("x", boxtimekey->info_.x_);
                        boxkeyElem.SetFloat("y", boxtimekey->info_.y_);
                        if (boxtimekey->pivotX_ != 0.f)
                            boxkeyElem.SetFloat("pivot_x", boxtimekey->pivotX_);
                        if (boxtimekey->pivotY_ != 1.f)
                            boxkeyElem.SetFloat("pivot_y", boxtimekey->pivotY_);

                        boxkeyElem.SetFloat("angle", boxtimekey->info_.angle_);
                    }
                }
            }
        }
    }

    FileSystem* fs = context->GetSubsystem<FileSystem>();
    File f(context, fs->GetProgramDir() + "/Data/" + filename, FILE_WRITE);
    if (f.IsOpen())
    {
        xml->Save(f, "\t");
        f.Close();
    }
}

void SpriterRescaleBonesAndSprites(AnimationSet2D* animationset, float scalefactor)
{
    Spriter::SpriterData* spriterdata = animationset->GetSpriterData();

    unsigned numentities = spriterdata->entities_.Size();
    for (int e=0; e < numentities; e++)
    {
        Spriter::Entity* entity = spriterdata->entities_[e];

//        URHO3D_LOGINFOF(" => entity=%s", entity->name_.CString());

        PODVector<Spriter::Animation*>& animations = entity->animations_;
        for (PODVector<Spriter::Animation*>::Iterator anim = animations.Begin(); anim != animations.End(); ++anim)
        {
            Spriter::Animation* animation = *anim;

//            URHO3D_LOGINFOF(" --> animation=%s", animation->name_.CString());

            const PODVector<Spriter::MainlineKey*>& mainlineKeys = animation->mainlineKeys_;
            unsigned nummainlinekeys = mainlineKeys.Size();

            Vector<Spriter::TimelineKey*> visitedTimeKeys;

            for (unsigned m = 0; m < nummainlinekeys; m++)
            {
                Spriter::MainlineKey* mainlineKey = mainlineKeys[m];
//                URHO3D_LOGINFOF(" ---> mainlinekey=%d time=%F", mainlineKey->id_, mainlineKey->time_);

//                URHO3D_LOGINFOF(" ----> bones ...");
                for (unsigned i = 0; i < mainlineKey->boneRefs_.Size(); ++i)
                {
                    Spriter::Ref* ref = mainlineKey->boneRefs_[i];
                    Spriter::Timeline* timeline = animation->timelines_[ref->timeline_];
                    Spriter::TimelineKey* timekey = timeline->keys_[ref->key_];

                    if (!visitedTimeKeys.Contains(timekey))
                    {
                        Spriter::SpatialTimelineKey* spatialtimekey = (Spriter::SpatialTimelineKey*)timekey;
                        Spriter::SpatialInfo& info = spatialtimekey->info_;
                        info.x_ *= scalefactor;
                        info.y_ *= scalefactor;
                        visitedTimeKeys.Push(timekey);

//                        URHO3D_LOGINFOF(" -----> timeline=%s type=Bone x=%F y=%F scalex=%F scaley=%F", timeline->name_.CString(),
//                                        info.x_, info.y_, info.scaleX_, info.scaleY_);
                    }

                }

//                URHO3D_LOGINFOF(" ----> objects ...");

                for (unsigned i = 0; i < mainlineKey->objectRefs_.Size(); ++i)
                {
                    Spriter::Ref* ref = mainlineKey->objectRefs_[i];
                    Spriter::Timeline* timeline   = animation->timelines_[ref->timeline_];
                    Spriter::TimelineKey* timekey = timeline->keys_[ref->key_];

                    if (!visitedTimeKeys.Contains(timekey))
                    {
                        Spriter::SpatialTimelineKey* spatialtimekey = (Spriter::SpatialTimelineKey*)timekey;
                        Spriter::SpatialInfo& info = spatialtimekey->info_;
                        info.x_ *= scalefactor;
                        info.y_ *= scalefactor;
                        info.scaleX_ *= scalefactor;
                        info.scaleY_ *= scalefactor;
                        visitedTimeKeys.Push(timekey);

//                        URHO3D_LOGINFOF(" -----> timeline=%s type=Sprite x=%F y=%F scalex=%F scaley=%F",
//                                        timeline->name_.CString(), timeline->objectType_ == Spriter::SPRITE ? "Sprite" : timeline->objectType_ == Spriter::BOX ? "Box" : "Point",
//                                        info.x_, info.y_, info.scaleX_, info.scaleY_);
                    }
                }
            }
        }
    }
}


void RenderAnimatedSpriteToTexture::CreateScene()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();

    // Load Materials
    Material* materialActors = cache->GetResource<Material>("Materials/LayerActors.xml");
//    Material* materialTiles = cache->GetResource<Material>("Materials/LayerGrounds.xml");

	// Create the scene in which we move around
	scene_ = new Scene(context_);
    scene_->CreateComponent<Octree>();

#ifdef ACTIVE_RENDERTEST
    // Create the scene which will be rendered to a texture
    rttScene_ = new Scene(context_);
    rttScene_->CreateComponent<Octree>();

    // create the render target scene for rendering alpha animatesdsprites
    {
        /*
            Note : if you already have created a named rendertarget texture in code and have stored it
            into the resource cache by using AddManualResource() you can use it directly as an output (by referring to its name)
            in the renderpath without requiring a rendertarget definition for it.
        */

        // Create the render Texture
        renderTexture_ = new Texture2D(context_);
        renderTexture_->SetSize(2048, 2048, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
//        renderTexture_->SetSize(graphics_->GetWidth(), graphics_->GetHeight(), Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
        renderTexture_->SetFilterMode(FILTER_BILINEAR);
        renderTexture_->SetName("RenderTarget2D");
        renderTexture_->SetNumLevels(1);
        cache->AddManualResource(renderTexture_);

        // Load the material and assign render Texture to a textureunit
        rttunit_ = 0;
        materialActors->SetTexture((TextureUnit)rttunit_, renderTexture_.Get());

        // Create a camera for the render-to-texture scene. Simply leave it at the world origin and let it observe the scene
        Node* rttCameraNode = rttScene_->CreateChild("Camera");
        Camera* camera = rttCameraNode->CreateComponent<Camera>();
        camera->SetOrthographic(true);
        camera->SetOrthoSize((float)renderTexture_->GetHeight() * PIXEL_SIZE);
        camera->SetFarClip(cameraDepth_*10.f+1.f);
        rttCameraNode->SetPosition(Vector3(0.0f, 0.0f, -cameraDepth_*10.f));

        // Get the texture's RenderSurface object (exists when the texture has been created in rendertarget mode)
        // and define the viewport for rendering the second scene, similarly as how backbuffer viewports are defined
        // to the Renderer subsystem. By default the texture viewport will be updated when the texture is visible
        // in the main view
        SharedPtr<RenderPath> renderpath(new RenderPath());
        renderpath->Load(GetSubsystem<ResourceCache>()->GetResource<XMLFile>("RenderPaths/Urho2DRenderTarget.xml"));
        rttViewport_ = new Viewport(context_, rttScene_, rttCameraNode->GetComponent<Camera>(), renderpath);
//        renderTexture_->GetRenderSurface()->SetUpdateMode(SURFACE_UPDATEALWAYS);
        renderTexture_->GetRenderSurface()->SetViewport(0, rttViewport_);
    }
#endif
    // Dump Material Texture Units
	{
	    for (int i = 0; i < 16; i++)
        {
            Texture* texture = materialActors->GetTexture((TextureUnit)i);
            if (texture)
                URHO3D_LOGINFOF("Texture Unit=%d Name=%s", i, texture->GetName().CString());
        }
//        for (int i = 0; i < 16; i++)
//        {
//            Texture* texture = materialTiles->GetTexture((TextureUnit)i);
//            if (texture)
//                URHO3D_LOGINFOF("Texture Unit=%d Name=%s", i, texture->GetName().CString());
//        }
	}

	// Create the master scene
    {
        // Create Some other entities without alpharenderer

        Node* petitenode = scene_->CreateChild("petite");
        petitenode->SetPosition(Vector3(0.f, 0.f, 0.f));
        AnimatedSprite2D* petite = petitenode->CreateComponent<AnimatedSprite2D>();
        petite->SetCustomMaterial(materialActors);
        petite->SetCustomSpriteSheetAttr("2D/spritesheet2.xml");
        AnimationSet2D* petiteanimset = cache->GetResource<AnimationSet2D>("2D/petite.scml");
        petite->SetAnimationSet(petiteanimset);
        petite->SetEntity("darkpetite");
        petite->SetAnimation("idle");

        Node* petitnode = scene_->CreateChild("petit");
        petitnode->SetPosition(Vector3(1.f, 0.f, 0.f));
        AnimatedSprite2D* petit = petitnode->CreateComponent<AnimatedSprite2D>();
        petit->SetCustomMaterial(materialActors);
        petit->SetCustomSpriteSheetAttr("2D/spritesheet1.xml");
        AnimationSet2D* petitanimset = cache->GetResource<AnimationSet2D>("2D/petit.scml");
        petit->SetAnimationSet(petitanimset);
        petit->SetEntity("petit");
        petit->SetAnimation("idle");
        petit->ApplyCharacterMap(CMAP_HEAD2);
        petit->ApplyCharacterMap(CMAP_NOWEAPON1);
        petit->ApplyCharacterMap(CMAP_NOWEAPON2);
        petit->ApplyCharacterMap(CMAP_NOARMOR);
        petit->ApplyCharacterMap(CMAP_NOHELMET);
        petit->ApplyCharacterMap(CMAP_NOBELT);
        petit->ApplyCharacterMap(CMAP_NOCAPE);
        petit->ApplyCharacterMap(CMAP_NOBLINDFOLD);

        Node* sorceressnode = scene_->CreateChild("sorceress");
        sorceressnode->SetPosition(Vector3(2.f, 0.f, 0.f));
        AnimatedSprite2D* sorceress = sorceressnode->CreateComponent<AnimatedSprite2D>();
        sorceress->SetCustomMaterial(materialActors);
        sorceress->SetCustomSpriteSheetAttr("2D/spritesheet2.xml");
        AnimationSet2D* sorceressanimset = cache->GetResource<AnimationSet2D>("2D/sorceress.scml");
        sorceress->SetAnimationSet(sorceressanimset);
        sorceress->SetEntity("sorceress");
        sorceress->SetAnimation("idle");
		sorceress->ApplyCharacterMap(String("Fire"));

//        AnimationSet2D* chapanzeanimset = cache->GetResource<AnimationSet2D>("2D/chapanze.scml");
//        SpriterRescaleBonesAndSprites(chapanzeanimset, 2.f);
//        SpriterSaveData(context_, chapanzeanimset, "2D/chapanzenew.scml");
//
//        Node* chapanzenode = scene_->CreateChild("chapanze");
//        chapanzenode->SetPosition(Vector3(-2.f, 0.f, 0.f));
//        AnimatedSprite2D* chapanze = chapanzenode->CreateComponent<AnimatedSprite2D>();
//        chapanze->SetCustomSpriteSheetAttr("2D/chapanze.xml");
//        chapanze->SetCustomMaterial(materialActors);
//        chapanze->SetAnimationSet(cache->GetResource<AnimationSet2D>("2D/chapanzenew.scml"));
//        chapanze->SetAnimation("idle");

//        Node* fantomettenode = scene_->CreateChild("fantomette");
//        fantomettenode->SetPosition(Vector3(-1.f, 0.f, 0.f));
//        AnimatedSprite2D* fantomette = fantomettenode->CreateComponent<AnimatedSprite2D>();
//        fantomette->SetCustomMaterial(materialActors);
//        fantomette->SetCustomSpriteSheetAttr("2D/petite.xml");
//        fantomette->SetAnimationSet(cache->GetResource<AnimationSet2D>("2D/fantomette.scml"));
//        fantomette->SetEntity("fantomette");
//        fantomette->SetAnimation("idle");

#ifdef ACTIVE_RENDERTEST
#ifndef ANIMATEDSPRITE_INTEGRATION
        // create 2 translucide drawables with the texture Effects CROPALPHA + UNLIT
        CreateRenderTargetSprite(fantomette1_, rttfantomette1_, "2D/fantomette.scml", "2D/spritesheet2.xml", Vector2(-2.f, 0.f), Vector2::ONE, materialActors, Color(0.f, 1.f, 0.f, 0.5f), 3, 3);
        CreateRenderTargetSprite(fantomette2_, rttfantomette2_, "2D/fantomette.scml", "2D/spritesheet2.xml", Vector2(-1.f, 0.f), Vector2::ONE, materialActors, Color(0.f, 0.2f, 1.f, 0.5f), 2, 3);
        UpdateRenderTargetNodePositions();
#else
        AnimatedSprite2D::SetRenderTargetContext(renderTexture_, rttViewport_, materialActors);

        fantomette1_ = scene_->CreateChild("fantomette1");
        fantomette1_->SetPosition2D(Vector2(1.f, 0.f));
        fantomette1_->SetScale2D(Vector2::ONE);
        AnimatedSprite2D* asprite1 = fantomette1_->CreateComponent<AnimatedSprite2D>();
        asprite1->SetRenderTargetFrom("2D/fantomette.scml|2D/spritesheet2.xml|11");
        asprite1->SetColor(Color(0.f, 1.f, 0.f, 0.5f));
        asprite1->SetLayer(4);

        fantomette2_ = scene_->CreateChild("fantomette2");
        fantomette2_->SetPosition2D(Vector2(-2.f, 0.f));
        fantomette2_->SetScale2D(Vector2::ONE);
        AnimatedSprite2D* asprite2 = fantomette2_->CreateComponent<AnimatedSprite2D>();
        asprite2->SetRenderTargetFrom(asprite1);
        asprite2->SetColor(Color(1.f, 0.f, 0.f, 0.5f));
        asprite2->SetLayer(3);

        fantomette3_ = scene_->CreateChild("fantomette3");
        fantomette3_->SetPosition2D(Vector2(-1.f, 0.f));
        fantomette3_->SetScale2D(Vector2::ONE);
        AnimatedSprite2D* asprite3 = fantomette3_->CreateComponent<AnimatedSprite2D>();
        asprite3->SetRenderTargetFrom("2D/fantomette.scml|2D/spritesheet2.xml|11");
        asprite3->SetColor(Color(0.f, 0.2f, 0.85f, 0.5f));
        asprite3->SetLayer(2);
#endif
#endif

        // Create the camera which we will move around. Limit far clip distance to match the fog
        cameraNode_ = scene_->CreateChild("Camera");
        Camera* camera = cameraNode_->CreateComponent<Camera>();
        // Set an initial position for the camera scene node above the plane
//        camera->SetOrthographic(true);
//        camera->SetOrthoSize((float)graphics_->GetHeight() * PIXEL_SIZE);
        camera->SetFarClip(300.f);
        cameraNode_->SetPosition(Vector3(0.0f, 0.0f, -cameraDepth_));
    }

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    GetSubsystem<Renderer>()->SetViewport(0, viewport);

//    TextTest(context_, scene_->GetChild("petit"), Vector3(0.f, 1.f, 0.1f), "Help !", "Fonts/Lycanthrope.ttf", 5.f, 5.f, 50);
}

void RenderAnimatedSpriteToTexture::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(RenderAnimatedSpriteToTexture, HandleUpdate));
}

static int animindex_ = 0;

void RenderAnimatedSpriteToTexture::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    Input* input = GetSubsystem<Input>();

    bool rttmodecontrol = input->GetScancodeDown(SCANCODE_SHIFT);

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();

    Node* cameraNode = cameraNode_;
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);
    cameraNode = cameraNode_;
    cameraNode->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    if (input->GetScancodeDown(SCANCODE_PAGEUP))
        cameraNode->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetScancodeDown(SCANCODE_PAGEDOWN))
        cameraNode->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetScancodeDown(SCANCODE_UP))
        cameraNode->Translate(Vector3::UP * MOVE_SPEED * timeStep);
    if (input->GetScancodeDown(SCANCODE_DOWN))
        cameraNode->Translate(Vector3::DOWN * MOVE_SPEED * timeStep);
    if (input->GetScancodeDown(SCANCODE_LEFT))
        cameraNode->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetScancodeDown(SCANCODE_RIGHT))
        cameraNode->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

	if (input->GetScancodeDown(SCANCODE_A) || input->GetScancodeDown(SCANCODE_D) ||
        input->GetScancodeDown(SCANCODE_W) || input->GetScancodeDown(SCANCODE_S))
	{
	    Vector3 direction;
	    if (input->GetScancodeDown(SCANCODE_D))
            direction.x_ = 1.f;
        else if (input->GetScancodeDown(SCANCODE_A))
            direction.x_ = -1.f;

        if (input->GetScancodeDown(SCANCODE_W))
            direction.y_ = 1.f;
        else if (input->GetScancodeDown(SCANCODE_S))
            direction.y_ = -1.f;
#ifdef ACTIVE_RENDERTEST
#ifndef ANIMATEDSPRITE_INTEGRATION
        AnimatedSprite2D* asprite1 = rttfantomette1_ ? rttfantomette1_->GetComponent<AnimatedSprite2D>() : 0;
        AnimatedSprite2D* asprite3 = asprite1;
#else
        AnimatedSprite2D* asprite1 = fantomette1_ ? fantomette1_->GetComponent<AnimatedSprite2D>()->GetRenderTarget() : 0;
        AnimatedSprite2D* asprite3 = fantomette3_ ? fantomette3_->GetComponent<AnimatedSprite2D>()->GetRenderTarget() : 0;
#endif
        if (asprite1)
            asprite1->SetAnimation("fly_up");
        if (asprite3)
            asprite3->SetAnimation("fly_up");
		if (direction.x_)
        {
            if (asprite1)
                asprite1->SetFlipX(direction.x_ < 0.f);
            if (asprite3)
                asprite3->SetFlipX(direction.x_ < 0.f);
        }

        if (fantomette1_)
            fantomette1_->SetNetPositionAttr(fantomette1_->GetPosition() + 0.007f * direction);
        if (fantomette2_)
            fantomette2_->SetNetPositionAttr(fantomette2_->GetPosition() + 0.0035f * direction);
        if (fantomette3_)
            fantomette3_->SetNetPositionAttr(fantomette3_->GetPosition() + 0.00233f * direction);
        spriteDirty_ = true;
#endif
	}
#ifdef ACTIVE_RENDERTEST
	else
	{
#ifndef ANIMATEDSPRITE_INTEGRATION
	    AnimatedSprite2D* asprite1 = rttfantomette1_ ? rttfantomette1_->GetComponent<AnimatedSprite2D>() : 0;
	    AnimatedSprite2D* asprite3 = asprite1;
#else
        AnimatedSprite2D* asprite1 = fantomette1_ ? fantomette1_->GetComponent<AnimatedSprite2D>()->GetRenderTarget() : 0;
        AnimatedSprite2D* asprite3 = fantomette3_ ? fantomette3_->GetComponent<AnimatedSprite2D>()->GetRenderTarget() : 0;
#endif
		if (asprite1 && asprite1->GetAnimation() != "idle")
		{
		    asprite1->SetAnimation("idle");
		    if (asprite3)
                asprite3->SetAnimation("idle");
            spriteDirty_ = true;
		}
	}

	if (spriteDirty_)
    {
#ifndef ANIMATEDSPRITE_INTEGRATION
        if (fantomette1_)
            UpdateRenderTargetSprite(fantomette1_->GetComponent<StaticSprite2D>(), rttfantomette1_->GetComponent<AnimatedSprite2D>());
        if (fantomette2_)
            UpdateRenderTargetSprite(fantomette2_->GetComponent<StaticSprite2D>(), rttfantomette2_->GetComponent<AnimatedSprite2D>());
#endif
        spriteDirty_ = false;
    }
#endif
	if (input->GetScancodePress(SCANCODE_G))
    {
        drawDebug_ = drawDebug_ ? false : true;
        if (drawDebug_)
            SubscribeToEvent(E_POSTRENDERUPDATE, URHO3D_HANDLER(RenderAnimatedSpriteToTexture, OnPostRenderUpdate));
        else
            UnsubscribeFromEvent(E_POSTRENDERUPDATE);
    }

	if (input->GetScancodePress(SCANCODE_SPACE))
    {
    	#ifdef ACTIVE_RENDERTEST
        if (fantomette2_)
            fantomette2_->GetComponent<AnimatedSprite2D>()->GetRenderTarget()->SetEnabled(!fantomette2_->GetComponent<AnimatedSprite2D>()->GetRenderTarget()->IsEnabled());
		#endif

		Node* sorceressnode = scene_->GetChild("sorceress");
		if (sorceressnode)
		{
			AnimatedSprite2D* anim = sorceressnode->GetComponent<AnimatedSprite2D>();
			if (anim->GetSpriterAnimation(animindex_+1))
				animindex_++;
			else
				animindex_ = 0;

			anim->SetSpriterAnimation(animindex_);
		}
    }
}

void RenderAnimatedSpriteToTexture::OnPostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
    {
        DebugRenderer* debugRenderer = scene_->GetOrCreateComponent<DebugRenderer>();

        PODVector<AnimatedSprite2D*> drawables;
        scene_->GetDerivedComponents<AnimatedSprite2D>(drawables, true);

        for (unsigned i = 0; i < drawables.Size(); ++i)
            drawables[i]->DrawDebugGeometry(debugRenderer, false);
    }
#ifdef ACTIVE_RENDERTEST
    {
        DebugRenderer* debugRenderer = rttScene_->GetOrCreateComponent<DebugRenderer>();

        PODVector<StaticSprite2D*> drawables;
        rttScene_->GetDerivedComponents<StaticSprite2D>(drawables, true);

        for (unsigned i = 0; i < drawables.Size(); ++i)
            drawables[i]->DrawDebugGeometry(debugRenderer, false);
    }
#endif
}


