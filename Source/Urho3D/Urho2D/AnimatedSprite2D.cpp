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

#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "../Scene/SceneEvents.h"
#include "../Urho2D/Renderer2D.h"
#include "../Urho2D/CollisionCircle2D.h"
#include "../Urho2D/CollisionBox2D.h"
#include "../Urho2D/AnimatedSprite2D.h"
#include "../Urho2D/AnimationSet2D.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/SpriterInstance2D.h"
#include "../Graphics/Material.h"
#include "../Graphics/Texture2D.h"
#include "../DebugNew.h"

#include "../Graphics/Graphics.h"
#include "../Graphics/Octree.h"
#include "../Graphics/Camera.h"
#include "../Graphics/RenderPath.h"
#include "../Resource/XMLFile.h"


#ifdef URHO3D_SPINE
#include <spine/spine.h>
#endif

static const Urho3D::StringHash SPRITER_SOUND           = Urho3D::StringHash("SPRITER_Sound");
static const Urho3D::StringHash SPRITER_ANIMATION       = Urho3D::StringHash("SPRITER_Animation");
static const Urho3D::StringHash SPRITER_ENTITY          = Urho3D::StringHash("SPRITER_Entity");
static const Urho3D::StringHash SPRITER_PARTICULE       = Urho3D::StringHash("SPRITER_Particule");

namespace Urho3D
{

extern const char* URHO2D_CATEGORY;
extern const char* blendModeNames[];

const char* loopModeNames[] =
{
    "Default",
    "ForceLooped",
    "ForceClamped",
    0
};

static Matrix2x3 sWorldTransform_, sLocalTransform_;
// Rotation 90°
static Matrix2x3 sRotatedMatrix_(-4.37114e-08f, -1.f, 0.f, 1.f, -4.37114e-08f, 0.f);

SpriteMapInfo::SpriteMapInfo()
{ }

void SpriteMapInfo::Clear()
{
    sprite_.Reset();
    map_ = 0;
    instruction_ = 0;
}

void SpriteMapInfo::Set(unsigned key, Sprite2D* sprite, Spriter::CharacterMap* map, Spriter::MapInstruction* instruction)
{
    key_ = key;
    sprite_ = sprite;
    map_ = map;
    instruction_ = instruction;
}

AnimatedSprite2D::AnimatedSprite2D(Context* context) :
    StaticSprite2D(context),
    speed_(1.0f),
    localRotation_(0.f),
    loopMode_(LM_DEFAULT),
    useCharacterMap_(false),
    characterMapDirty_(true),
    renderEnabled_(true),
    dynamicBBox_(false),
    colorsDirty_(false),
    mappingScaleRatio_(1.f),
    customSourceBatches_(0),
    animationIndex_(0)
#ifdef URHO3D_SPINE
    ,skeleton_(0),
    animationStateData_(0),
    animationState_(0)
#endif
{
    for (unsigned i=0; i<2; i++)
    {
        sourceBatches_[i].Reserve(10);
        sourceBatches_[i].Resize(1);
    }

    triggerNodes_.Reserve(5);
    spriteInfoMapping_.Clear();
    worldBoundingBoxDirty_ = true;
}

AnimatedSprite2D::~AnimatedSprite2D()
{
    Dispose();
}

void AnimatedSprite2D::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimatedSprite2D>(URHO2D_CATEGORY);

    URHO3D_COPY_BASE_ATTRIBUTES(StaticSprite2D);
//    URHO3D_REMOVE_ATTRIBUTE("Sprite");
    URHO3D_ACCESSOR_ATTRIBUTE("Speed", GetSpeed, SetSpeed, float, 1.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Rotation", GetLocalRotation, SetLocalRotation, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Position", GetLocalPosition, SetLocalPosition, Vector2, Vector2::ZERO, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Custom Spritesheet", GetEmptyString, SetCustomSpriteSheetAttr, String, String::EMPTY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Rendered Target", GetRenderTargetAttr, SetRenderTargetAttr, String, String::EMPTY, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Animation Set", GetAnimationSetAttr, SetAnimationSetAttr, ResourceRef, ResourceRef(AnimatedSprite2D::GetTypeStatic(), String::EMPTY), AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Entity", GetEntityName, SetEntity, String, String::EMPTY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Animation", GetAnimation, SetAnimationAttr, String, String::EMPTY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Applied Character Maps", GetAppliedCharacterMapsAttr, SetAppliedCharacterMapsAttr, VariantVector, Variant::emptyVariantVector, AM_DEFAULT);
    //URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Apply Character Map", GetCharacterMapAttr, SetCharacterMapAttr, String, String::EMPTY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Apply Character Map", GetEmptyString, SetCharacterMapAttr, String, String::EMPTY, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("MappingScaleRatio", GetMappingScaleRatio, SetMappingScaleRatio, float, 1.f, AM_DEFAULT);
    URHO3D_ENUM_ACCESSOR_ATTRIBUTE("Loop Mode", GetLoopMode, SetLoopMode, LoopMode2D, loopModeNames, LM_DEFAULT, AM_DEFAULT);
//    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Sprite", GetSpriteAttr, SetSpriteAttr, ResourceRef, ResourceRef(Sprite2D::GetTypeStatic(), String::EMPTY), AM_DEFAULT);
}


/// ENTITY/ANIMATION SETTERS

void AnimatedSprite2D::SetAnimationSet(AnimationSet2D* animationSet)
{
    if (animationSet == animationSet_)
        return;

    Dispose(true);

    animationSet_ = animationSet;
    if (!animationSet_)
        return;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetAnimationSet : entity = %s ...", entityName_.CString());

#ifdef URHO3D_SPINE
    if (animationSet_->GetSkeletonData())
    {
        spSkeletonData* skeletonData = animationSet->GetSkeletonData();

        // Create skeleton
        skeleton_ = spSkeleton_create(skeletonData);
        skeleton_->scaleX = flipX_ ? -1.f : 1.f;
        skeleton_->scaleY = flipY_ ? -1.f : 1.f;

        if (skeleton_->data->skinsCount > 0)
        {
            // If entity is empty use first skin in spine
            if (entityName_.Empty())
                entityName_ = skeleton_->data->skins[0]->name;
            spSkeleton_setSkinByName(skeleton_, entityName_.CString());
        }

        spSkeleton_updateWorldTransform(skeleton_);
    }
#endif
    if (animationSet_->GetSpriterData())
    {
        spriterInstance_ = new Spriter::SpriterInstance(this, animationSet_->GetSpriterData());

        const PODVector<Spriter::Entity* > entities = animationSet_->GetSpriterData()->entities_;

        if (!entities.Empty())
        {
            bool entityNameToSet = true;

            if (!entityName_.Empty())
            {
                for (unsigned i = 0; i < entities.Size(); i++)
                {
                    if (entities[i]->name_ == entityName_)
                    {
                        entityNameToSet = false;
                        break;
                    }
                }
            }

            if (entityNameToSet)
                entityName_ = animationSet_->GetSpriterData()->entities_[0]->name_;

            spriterInstance_->SetEntity(entityName_);
        }
    }

    if (!StaticSprite2D::GetSprite())
    {
        if (animationSet_->GetSprite())
            StaticSprite2D::SetSprite(animationSet_->GetSprite());
        else
            StaticSprite2D::SetSprite(GetSprite(0));
    }

//    if (enableDebugLog_)
//        URHO3D_LOGERRORF("AnimatedSprite2D() - SetAnimationSet : node=%s(%u) entity=%s",
//             node_->GetName().CString(), node_->GetID(), entityName_.CString());

    // Clear animation name
    animationName_.Clear();
    loopMode_ = LM_DEFAULT;
}

void AnimatedSprite2D::SetEntity(const String& entity)
{
    if (entity == entityName_)
        return;

    drawRectDirty_ = true;

    entityName_ = entity;

    if (enableDebugLog_)
        URHO3D_LOGERRORF("AnimatedSprite2D() - SetEntity : node=%s(%u) entity=%s",
             node_->GetName().CString(), node_->GetID(), entityName_.CString());

#ifdef URHO3D_SPINE
    if (skeleton_)
        spSkeleton_setSkinByName(skeleton_, entityName_.CString());
#endif
    if (spriterInstance_)
        spriterInstance_->SetEntity(entityName_);
}

void AnimatedSprite2D::SetSpriterEntity(int index)
{
    if (!animationSet_ || !spriterInstance_)
        return;

    index %= GetNumSpriterEntities();

    if (!animationSet_->GetSpriterData()->entities_[index])
        return;

    const String& entityname = animationSet_->GetSpriterData()->entities_[index]->name_;

    if (entityname == entityName_)
        return;

    worldBoundingBoxDirty_ = drawRectDirty_ = true;

    entityName_ = entityname;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetSpriterEntity : entity = %s", entityName_.CString());

    spriterInstance_->SetEntity(index);
    SetAnimation(GetAnimation());
}

void AnimatedSprite2D::SetAnimation(const String& name, LoopMode2D loopMode)
{
    if (!animationSet_)
        return;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetAnimation : %s(%u) current:%s to new:%s ... ",
//                     node_->GetName().CString(), node_->GetID(), animationName_.CString(), name.CString());
    if (!name.Empty())
    {
        if (animationSet_->HasAnimation(name))
            animationName_ = name;
    }

    if (animationName_.Empty() || !animationSet_->HasAnimation(animationName_))
        animationName_ = GetDefaultAnimation();

    if (animationName_.Empty())
    {
        URHO3D_LOGWARNINGF("AnimatedSprite2D() - SetAnimation : No Animation Name !");
        return;
    }

    loopMode_ = loopMode;

    if (enableDebugLog_)
        URHO3D_LOGERRORF("AnimatedSprite2D() - SetAnimation : node=%s(%u) animation=%s",
             node_->GetName().CString(), node_->GetID(), animationName_.CString());
#ifdef URHO3D_SPINE
    if (skeleton_)
        SetSpineAnimation();
#endif
    if (spriterInstance_)
        SetSpriterAnimation();

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetAnimation : node=%s(%u) name = %s ... OK !", node_->GetName().CString(), node_->GetID(), animationName_.CString());
}

void AnimatedSprite2D::SetRenderEnable(bool enable, int zindex)
{
    if (!enable)
    {
        sourceBatches_[0].Resize(1);
        sourceBatches_[1].Resize(1);
        ClearSourceBatches();
        renderZIndex_ = zindex;
    }

    renderEnabled_ = enable;
}

void AnimatedSprite2D::SetLoopMode(LoopMode2D loopMode)
{
    loopMode_ = loopMode;
}

void AnimatedSprite2D::SetSpeed(float speed)
{
    speed_ = speed;
    MarkNetworkUpdate();
}

void AnimatedSprite2D::SetCustomSpriteSheetAttr(const String& value)
{
    //if (!value.Empty())
    {
//        URHO3D_LOGERRORF("AnimatedSprite2D() - SetCustomSpriteSheetAttr : %s(%u) spritesheet=%s", node_->GetName().CString(), node_->GetID(), value.CString());
        AnimationSet2D::customSpritesheetFile_ = value;
    }
}

void AnimatedSprite2D::SetAnimationSetAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

//    if (!AnimationSet2D::customSpritesheetFile_.Empty())
//        URHO3D_LOGERRORF("AnimatedSprite2D() - SetAnimationSetAttr : %s(%u) use custom spritesheet=%s", node_->GetName().CString(), node_->GetID(), AnimationSet2D::customSpritesheetFile_.CString());

    SetAnimationSet(cache->GetResource<AnimationSet2D>(value.name_));
    AnimationSet2D::customSpritesheetFile_.Clear();
}

//void AnimatedSprite2D::SetSpriteAttr(const ResourceRef& value)
//{
//    if (!value.name_.Empty())
//        sprite_ = Sprite2D::LoadFromResourceRef(context_, value);
//}

void AnimatedSprite2D::SetAnimationAttr(const String& name)
{
    animationName_ = name;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetAnimationAttr : %s(%u) name=%s !",
//                     node_->GetName().CString(), node_->GetID(), animationName_.CString());

    SetAnimation(animationName_, loopMode_);
}

void AnimatedSprite2D::SetLocalRotation(float angle)
{
    localRotation_ = angle;
}

void AnimatedSprite2D::SetLocalPosition(const Vector2& position)
{
    localPosition_ = position;
}

void AnimatedSprite2D::CleanDependences()
{
//    URHO3D_LOGINFOF("AnimatedSprite2D() - CleanDependences : node=%s(%u) enabled=%s", node_->GetName().CString(), node_->GetID(), IsEnabledEffective() ? "true" : "false");

    ClearTriggers(true);
    ClearRenderedAnimations();
}

void AnimatedSprite2D::ResetAnimation()
{
    if (GetSpriterInstance())
    {
        spriterInstance_->ResetCurrentTime();
        spriterInstance_->Update(0.f);
    }
}


/// ENTITY/ANIMATION GETTERS

unsigned AnimatedSprite2D::GetNumSpriterEntities() const
{
    return animationSet_ && GetSpriterInstance() ? animationSet_->GetSpriterData()->entities_.Size() : 0;
}

const String& AnimatedSprite2D::GetSpriterEntity(int index) const
{
    return GetSpriterInstance() && spriterInstance_->GetEntity(index) ? spriterInstance_->GetEntity(index)->name_ : String::EMPTY;
}

unsigned AnimatedSprite2D::GetSpriterEntityIndex() const
{
    return GetSpriterInstance() && spriterInstance_->GetEntity() ? spriterInstance_->GetEntity()->id_ : 0;
}

const String& AnimatedSprite2D::GetDefaultAnimation() const
{
	if (!GetSpriterInstance())
		return String::EMPTY;

	if (spriterInstance_->GetAnimation())
		return spriterInstance_->GetAnimation()->name_;

    if (animationSet_->GetNumAnimations())
        return animationSet_->GetAnimation(0);

	return String::EMPTY;
}

bool AnimatedSprite2D::HasAnimation(const String& name) const
{
    return GetSpriterInstance() && spriterInstance_->GetAnimation(name);
}

AnimationSet2D* AnimatedSprite2D::GetAnimationSet() const
{
    return animationSet_;
}

float AnimatedSprite2D::GetCurrentAnimationTime() const
{
    return spriterInstance_->GetCurrentTime();
}

bool AnimatedSprite2D::HasFinishedAnimation() const
{
    return spriterInstance_->HasFinishedAnimation();
}

Spriter::SpriterInstance* AnimatedSprite2D::GetSpriterInstance() const
{
    return spriterInstance_.Get();
}

Spriter::Animation* AnimatedSprite2D::GetSpriterAnimation(int index) const
{
    return index == -1 ? spriterInstance_->GetAnimation() : spriterInstance_->GetAnimation(index);
}

Spriter::Animation* AnimatedSprite2D::GetSpriterAnimation(const String& animationName) const
{
    return animationName != String::EMPTY ? GetSpriterInstance() ? GetSpriterInstance()->GetAnimation(animationName) : 0 : 0;
}

ResourceRef AnimatedSprite2D::GetAnimationSetAttr() const
{
    return GetResourceRef(animationSet_, AnimationSet2D::GetTypeStatic());
}

//ResourceRef AnimatedSprite2D::GetSpriteAttr() const
//{
//    return Sprite2D::SaveToResourceRef(sprite_);
//}

float AnimatedSprite2D::GetLocalRotation() const
{
    return localRotation_;
}

const Vector2& AnimatedSprite2D::GetLocalPosition() const
{
    return localPosition_;
}


/// NODE ADDER

void AnimatedSprite2D::AddPhysicalNode(Node* node)
{
    if (triggerNodes_.Size())
    {
        // check if already in triggerNodes
        for (Vector<WeakPtr<Node> >::Iterator it=triggerNodes_.Begin(); it!=triggerNodes_.End(); ++it)
        {
            if (it->Get() == node)
                return;
        }
    }

    triggerNodes_.Push(WeakPtr<Node>(node));
}

/// CHARACTER MAPPING SETTERS

void AnimatedSprite2D::SetAppliedCharacterMapsAttr(const VariantVector& characterMapApplied)
{
    ResetCharacterMapping(false);

    if (characterMapApplied.Empty())
        return;

    for (VariantVector::ConstIterator it=characterMapApplied.Begin(); it != characterMapApplied.End(); ++it)
    {
        Spriter::CharacterMap* characterMap = GetCharacterMap(it->GetStringHash());
        if (characterMap)
        {
        //    URHO3D_LOGINFOF("AnimatedSprite2D() - SetAppliedCharacterMapsAttr : %s(%u) applies characterMap=%s(%u) ...",
        //                node_->GetName().CString(), node_->GetID(), characterMap->name_.CString(), characterMap->hashname_.Value());
            ApplyCharacterMap(it->GetStringHash());
        }
    }

    MarkNetworkUpdate();
}

void AnimatedSprite2D::SetCharacterMapAttr(const String& characterMapNames)
{
//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetCharacterMapAttr : %s(%u) names=%s !",
//                     node_->GetName().CString(), node_->GetID(), characterMapNames.CString());

    characterMapApplied_.Clear();

    if (characterMapNames.Empty())
        return;

    bool state = false;
    Vector<String> names = characterMapNames.Split('|', false);

    for (Vector<String>::ConstIterator it=names.Begin(); it != names.End(); ++it)
        state |= ApplyCharacterMap(StringHash(*it));

    MarkNetworkUpdate();

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SetCharacterMapAttr : %s(%u) names=%s !",
//                    node_->GetName().CString(), node_->GetID(), characterMapNames.CString());
}

bool AnimatedSprite2D::ApplyCharacterMap(const StringHash& characterMap)
{
    return ApplyCharacterMap(GetCharacterMap(characterMap));
}

bool AnimatedSprite2D::ApplyCharacterMap(const String& characterMap)
{
    return ApplyCharacterMap(StringHash(characterMap));
}

bool AnimatedSprite2D::ApplyCharacterMap(Spriter::CharacterMap* characterMap)
{
    if (!characterMap)
        return false;

    unsigned key;
    const PODVector<Spriter::MapInstruction*>& mapInstructions = characterMap->maps_;
    for (PODVector<Spriter::MapInstruction*>::ConstIterator it = mapInstructions.Begin(); it != mapInstructions.End(); ++it)
    {
        Spriter::MapInstruction* instruct = *it;

        key = Spriter::GetKey(instruct->folder_, instruct->file_);
//        key = (instruct->folder_ << 16) + instruct->file_;

        if (instruct->targetFolder_ == -1)
            spriteMapping_[key].Clear();
        else
            spriteMapping_[key].Set(key, animationSet_->GetSpriterFileSprite(instruct->targetFolder_, instruct->targetFile_), characterMap, instruct);
    }

    if (!IsCharacterMapApplied(characterMap->hashname_))
        characterMapApplied_.Push(characterMap->hashname_);

    characterMaps_.Push(characterMap);

    useCharacterMap_ = true;

    sourceBatchesDirty_ = true;

    return true;
}

bool AnimatedSprite2D::ApplyColorMap(const StringHash& colorMap)
{
    return ApplyColorMap(GetColorMap(colorMap));
}

bool AnimatedSprite2D::ApplyColorMap(const String& colorMap)
{
    return ApplyColorMap(StringHash(colorMap));
}

bool AnimatedSprite2D::ApplyColorMap(Spriter::ColorMap* colorMap)
{
    if (!colorMap)
        return false;

    unsigned key;
    const PODVector<Spriter::ColorMapInstruction*>& mapInstructions = colorMap->maps_;
    for (PODVector<Spriter::ColorMapInstruction*>::ConstIterator it = mapInstructions.Begin(); it != mapInstructions.End(); ++it)
    {
        Spriter::ColorMapInstruction* map = *it;
        colorMapping_[ (map->folder_ << 16) + map->file_] = map->color_;
    }

//    if (!IsCharacterMapApplied(characterMap->hashname_))
//        characterMapApplied_.Push(characterMap->hashname_);

    colorMaps_.Push(colorMap);

    sourceBatchesDirty_ = colorsDirty_ = true;

    return true;
}

void AnimatedSprite2D::SwapSprite(const StringHash& characterMap, Sprite2D* replacement, unsigned index, bool keepProportion)
{
    Sprite2D* original = GetCharacterMapSprite(characterMap, index);

    SwapSprite(original, replacement, keepProportion);

    sourceBatchesDirty_ = true;
}

void AnimatedSprite2D::SwapSprites(const StringHash& characterMap, const PODVector<Sprite2D*>& replacements, bool keepProportion)
{
//    URHO3D_LOGINFOF("AnimatedSprite2D() - SwapSprites : ...");

    Spriter::CharacterMap* characterMapOrigin = GetCharacterMap(characterMap);

    if (!characterMapOrigin)
    {
        URHO3D_LOGWARNINGF("AnimatedSprite2D() - SwapSprites : no characterMap origin !");
        return;
    }

    PODVector<Sprite2D*> originalSprites;
    GetMappedSprites(characterMapOrigin, originalSprites);

    if (!originalSprites.Size() || !replacements.Size())
    {
        URHO3D_LOGWARNINGF("AnimatedSprite2D() - SwapSprites : no spriteslist !");
        return;
    }

    if (replacements.Size() == 1)
    {
        // weapon case : only first sprite is changed (allow the mapping for the other sprites of original)
        SwapSprite(originalSprites[0], replacements[0], keepProportion);
    }
    else
    {
        // armor case : all sprites are changed (if no more sprite in replacements for a original one, remove the original)
        SwapSprites(originalSprites, replacements, keepProportion);
    }

    ApplyCharacterMap(characterMap);

    sourceBatchesDirty_ = true;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SwapSprites : ... OK !");
}

void AnimatedSprite2D::SwapSprite(const String& characterMap, Sprite2D* replacement, unsigned index, bool keepProportion)
{
    SwapSprite(StringHash(characterMap), replacement, index, keepProportion);
}

void AnimatedSprite2D::SwapSprites(const String& characterMap, const PODVector<Sprite2D*>& replacements, bool keepProportion)
{
    SwapSprites(StringHash(characterMap), replacements, keepProportion);
}

void AnimatedSprite2D::UnSwapAllSprites()
{
    swappedSprites_.Clear();
    spriteInfoMapping_.Clear();
}

void AnimatedSprite2D::SwapSprite(Sprite2D* original, Sprite2D* replacement, bool keepRatio)
{
    if (!original)
    {
        URHO3D_LOGWARNINGF("AnimatedSprite2D() - SwapSprite : node=%s(%u) original=NONE replacement=%s => verify original in CharacterMap in SCML !",
                            node_->GetName().CString(), node_->GetID(), replacement ? replacement->GetName().CString() : "NONE");
        return;
    }

    swappedSprites_[original] = SharedPtr<Sprite2D>(replacement);

    if (original == replacement)
        return;

    if (replacement)
    {
        const IntRect& orect = original->GetRectangle();
        const IntRect& rrect = replacement->GetRectangle();

        SpriteInfo& info = spriteInfoMapping_[replacement][original];
        info.sprite_ = replacement;
        info.dPivot_.x_ = replacement->GetHotSpot().x_ - original->GetHotSpot().x_;
        info.dPivot_.y_ = replacement->GetHotSpot().y_ - original->GetHotSpot().y_;

        info.scale_.x_ = (float)(orect.right_ - orect.left_) / (rrect.right_ - rrect.left_);
        info.scale_.y_ = (float)(orect.bottom_ - orect.top_) / (rrect.bottom_ - rrect.top_);

        if (keepRatio)
        {
            info.scale_.x_ = info.scale_.y_ = mappingScaleRatio_;
            //info.scalex_ = info.scaley_ = (info.scalex_ + info.scaley_) * 0.5f * mappingScaleRatio_;
        }

//        URHO3D_LOGINFOF("AnimatedSprite2D() - SwapSprite : node=%s(%u) original=%s osize=(%d,%d) ohot=(%F,%F) replacement=%s rsize=(%d,%d) rhot=(%F,%F) keepRatio=%s scale=(%f,%f) deltaHotspot=(%f,%f)",
//                            node_->GetName().CString(), node_->GetID(),
//                            original->GetName().CString(), orect.right_ - orect.left_, orect.bottom_ - orect.top_,
//                            original->GetHotSpot().x_, original->GetHotSpot().y_,
//                            replacement->GetName().CString(), rrect.right_ - rrect.left_, rrect.bottom_ - rrect.top_,
//                            replacement->GetHotSpot().x_, replacement->GetHotSpot().y_,
//                            keepRatio ? "true": "false", info.scalex_, info.scaley_, info.deltaHotspotx_, info.deltaHotspoty_);
    }
}

void AnimatedSprite2D::SwapSprites(const PODVector<Sprite2D*>& originals, const PODVector<Sprite2D*>& replacements, bool keepRatio)
{
//    int size = Min((int)originals.Size(), (int)replacements.Size());
    int size = originals.Size();

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SwapSprites : node=%s(%u) originalListSize=%u replacementListSize=%u keepRatio=%s ...",
//                    node_->GetName().CString(), node_->GetID(), originals.Size(), replacements.Size(), keepRatio ? "true":"false");

    if (!size)
        return;

    for (int i = 0; i < size; ++i)
    {
//        URHO3D_LOGINFOF(" [%d/%d] => original=%s(ptr=%u) replacement=%s(ptr=%u)",
//                        i, size-1, originals[i] ? originals[i]->GetName().CString() : String::EMPTY, originals[i],
//                        i >= replacements.Size() || !replacements[i] ? "" : replacements[i]->GetName().CString(), i >= replacements.Size() ? 0 : replacements[i]);

        SwapSprite(originals[i], i >= replacements.Size() ? 0 : replacements[i], keepRatio);
    }

//    URHO3D_LOGINFOF("AnimatedSprite2D() - SwapSprites : node=%s(%u) ... OK !", node_->GetName().CString(), node_->GetID());
}

void AnimatedSprite2D::UnSwapSprite(Sprite2D* original)
{
    if (!original)
        return;

    swappedSprites_.Erase(original);
}

void AnimatedSprite2D::SetColorDirty()
{
    colorsDirty_ = sourceBatchesDirty_ = true;
}

void AnimatedSprite2D::SetSpriteColor(unsigned key, const Color& color)
{
    colorMapping_[key] = color;
    SetColorDirty();
}

void AnimatedSprite2D::ResetCharacterMapping(bool resetSwappedSprites)
{
    ClearRenderedAnimations();

    characterMaps_.Clear();
    characterMapApplied_.Clear();

    spriteMapping_.Clear();
    spritesInfos_.Clear();
    colorMapping_.Clear();

    if (resetSwappedSprites)
        UnSwapAllSprites();

    characterMapDirty_ = false;
    sourceBatchesDirty_ = colorsDirty_ = true;
    useCharacterMap_ = false;
}

void AnimatedSprite2D::SetMappingScaleRatio(float ratio)
{
    mappingScaleRatio_ = ratio;
}


/// CHARACTER MAPPING GETTERS

/*
String AnimatedSprite2D::GetCharacterMapAttr() const
{
    String names;
    for (PODVector<Spriter::CharacterMap* >::ConstIterator it=characterMaps_.End()-1; it==characterMaps_.Begin(); --it)
    {
        const String& name = (*it)->name_;
        if (!names.Contains(name))
        {
            if (names.Empty())
                names.Append('|');
            names.Append(name);
        }
    }
    return names;
}
*/

const VariantVector& AnimatedSprite2D::GetAppliedCharacterMapsAttr() const
{
    return characterMapApplied_;
}

bool AnimatedSprite2D::HasCharacterMapping() const
{
    if (!GetSpriterInstance())
        return false;

    if (!spriterInstance_->GetEntity())
        return false;

    return spriterInstance_->GetEntity()->characterMaps_.Size() != 0;
}

bool AnimatedSprite2D::HasCharacterMap(const StringHash& hashname) const
{
    return GetCharacterMap(hashname) != 0;
}

bool AnimatedSprite2D::HasCharacterMap(const String& name) const
{
    return GetCharacterMap(name) != 0;
}

Spriter::CharacterMap* AnimatedSprite2D::GetCharacterMap(const StringHash& characterMap) const
{
    if (!GetSpriterInstance())
        return 0;

    Spriter::Entity* entity = spriterInstance_->GetEntity();
    if (!entity)
        return 0;

    const PODVector<Spriter::CharacterMap*>& characterMaps = entity->characterMaps_;
    for (PODVector<Spriter::CharacterMap*>::ConstIterator it=characterMaps.Begin(); it != characterMaps.End(); ++it)
    {
        if ((*it)->hashname_ == characterMap)
            return *it;
    }
//    URHO3D_LOGWARNINGF("AnimatedSprite2D() - GetCharacterMap : no characterMap hashname=%u", characterMap.Value());
    return 0;
}

Spriter::CharacterMap* AnimatedSprite2D::GetCharacterMap(const String& characterMap) const
{
    return GetCharacterMap(StringHash(characterMap));
}

Spriter::ColorMap* AnimatedSprite2D::GetColorMap(const StringHash& hashname) const
{
    if (!GetSpriterInstance())
        return 0;

    Spriter::Entity* entity = spriterInstance_->GetEntity();
    if (!entity)
        return 0;

    const PODVector<Spriter::ColorMap*>& colorMaps = entity->colorMaps_;
    for (PODVector<Spriter::ColorMap*>::ConstIterator it=colorMaps.Begin(); it != colorMaps.End(); ++it)
    {
        if ((*it)->hashname_ == hashname)
            return *it;
    }

    return 0;
}

Spriter::ColorMap* AnimatedSprite2D::GetColorMap(const String& name) const
{
    return GetColorMap(StringHash(name));
}

bool AnimatedSprite2D::IsCharacterMapApplied(const StringHash& characterMap) const
{
    return characterMapApplied_.Contains(characterMap);
}

bool AnimatedSprite2D::IsCharacterMapApplied(const String& characterMap) const
{
    return IsCharacterMapApplied(StringHash(characterMap));
}

unsigned AnimatedSprite2D::GetNumSpriteKeys() const
{
    return spritesInfos_.Size() ? spritesKeys_.Size() : spriterInstance_->GetNumSpriteKeys();
}

const PODVector<Spriter::SpriteTimelineKey* >& AnimatedSprite2D::GetSpriteKeys() const
{
    return spritesInfos_.Size() ? spritesKeys_ : spriterInstance_->GetSpriteKeys();
}

const SpriteMapInfo* AnimatedSprite2D::GetSpriteMapInfo(unsigned key) const
{
    HashMap<unsigned, SpriteMapInfo >::ConstIterator it = spriteMapping_.Find(key);
    return it != spriteMapping_.End() ? &it->second_ : 0;
}

SpriteInfo* AnimatedSprite2D::GetSpriteInfo(unsigned key, const SpriteMapInfo* mapinfo, Sprite2D* sprite, Sprite2D* origin)
{
    SpriteInfo& info = spriteInfoMapping_[sprite][origin];
    if (info.sprite_ != sprite)
        info.Set(sprite);
    if (info.mapinfo_ != mapinfo)
        info.mapinfo_ = mapinfo;
    if (colorsDirty_)
        info.pcolor_ = &GetSpriteColor(key);

    return &info;
}

const PODVector<SpriteInfo*>& AnimatedSprite2D::GetSpriteInfos()
{
    spritesKeys_.Clear();
    spritesInfos_.Clear();

    if (!spriterInstance_->GetSpriteKeys().Size())
        UpdateSpriterAnimation(0.f);

    unsigned numSpriteKeys = spriterInstance_->GetNumSpriteKeys();
    if (numSpriteKeys)
    {
        const PODVector<Spriter::SpriteTimelineKey* >& spriteKeys = spriterInstance_->GetSpriteKeys();
        Sprite2D *sprite, *origin;
        Spriter::SpriteTimelineKey* spriteKey;
        unsigned key;

        // Get Sprite Keys only
        for (unsigned i = 0; i < numSpriteKeys; ++i)
        {
            spriteKey = spriteKeys[i];

            key = Spriter::GetKey(spriteKey->folderId_, spriteKey->fileId_);
            const SpriteMapInfo* mapinfo = GetSpriteMapInfo(key);
            origin = mapinfo ? mapinfo->sprite_ : animationSet_->GetSpriterFileSprite(key);
            if (!origin)
                continue;

            sprite = GetSwappedSprite(origin);
            if (!sprite)
                continue;

            spritesKeys_.Push(spriteKey);

            spritesInfos_.Push(GetSpriteInfo(key, mapinfo, sprite, origin));
        }
        if (colorsDirty_)
            colorsDirty_ = false;

//        if (!spritesInfos_.Size())
//            URHO3D_LOGWARNINGF("AnimatedSprite2D() - CreateSpriteInfos : node=%s ... No Spriter Keys !", node_->GetName().CString());
    }

    return spritesInfos_;
}

/// Sprites Getters

Sprite2D* AnimatedSprite2D::GetCharacterMapSprite(const StringHash& characterMap, unsigned index) const
{
    return animationSet_->GetCharacterMapSprite(GetCharacterMap(characterMap), index);
}

Sprite2D* AnimatedSprite2D::GetCharacterMapSprite(const String& characterMap, unsigned index) const
{
    return GetCharacterMapSprite(StringHash(characterMap), index);
}

void AnimatedSprite2D::GetMappedSprites(Spriter::CharacterMap* characterMap, PODVector<Sprite2D*>& sprites) const
{
    if (!characterMap)
        return;

    sprites.Clear();

    Spriter::MapInstruction* map;
    const PODVector<Spriter::MapInstruction*>& mapInstructions = characterMap->maps_;
    for (PODVector<Spriter::MapInstruction*>::ConstIterator it = mapInstructions.Begin(); it != mapInstructions.End(); ++it)
    {
        map = *it;
//        if (map->targetFolder_ == -1)
//            continue;
//        sprites.Push(animationSet_->GetSpriterFileSprite(map->targetFolder_, map->targetFile_));

        sprites.Push(map->targetFolder_ == -1 ? 0 : animationSet_->GetSpriterFileSprite(map->targetFolder_, map->targetFile_));
    }
}

Sprite2D* AnimatedSprite2D::GetMappedSprite(unsigned key) const
{
    HashMap<unsigned, SpriteMapInfo >::ConstIterator it = spriteMapping_.Find(key);
    return it != spriteMapping_.End() ? it->second_.sprite_.Get() : animationSet_->GetSpriterFileSprite(key);
}

Sprite2D* AnimatedSprite2D::GetMappedSprite(int folderid, int fileid) const
{
    return GetMappedSprite((folderid << 16) + fileid);
}

Sprite2D* AnimatedSprite2D::GetSwappedSprite(Sprite2D* original) const
{
    if (!original)
        return 0;

    HashMap<Sprite2D*, SharedPtr<Sprite2D> >::ConstIterator it = swappedSprites_.Find(original);
    return it != swappedSprites_.End() ? it->second_.Get() : original;
}

const Color& AnimatedSprite2D::GetSpriteColor(unsigned key) const
{
    HashMap<unsigned, Color >::ConstIterator it = colorMapping_.Find(key);
    return it != colorMapping_.End() ? it->second_ : Color::WHITE;
}

void AnimatedSprite2D::GetSpriteLocalPositions(unsigned spriteindex, Vector2& position, float& angle, Vector2& scale)
{
    const Spriter::SpriteTimelineKey& spritekey = *spritesKeys_[spriteindex];
    const Spriter::SpatialInfo& spatialinfo = spritekey.info_;
    const SpriteInfo* spriteinfo  = spritesInfos_[spriteindex];

    position.x_ = spatialinfo.x_ * PIXEL_SIZE;
    position.y_ = spatialinfo.y_ * PIXEL_SIZE;

    if (flipX_)
        position.x_ = -position.x_;
    if (flipY_)
        position.y_ = -position.y_;

    angle = spatialinfo.angle_;
    if (flipX_ != flipY_)
        angle = -angle;

//    if (spriteinfo->sprite_->GetRotated())
//        angle -= 90;

//    if (spriteinfo->rotate_)
//        angle = (angle+180);

    scale.x_ = spatialinfo.scaleX_ * spriteinfo->scale_.x_;
    scale.y_ = spatialinfo.scaleY_ * spriteinfo->scale_.y_;

//    Rect drawRect;
//    spriteinfo->sprite_->GetDrawRectangle(drawRect, Vector2(spritekey.pivotX_, spritekey.pivotY_));
//    position = drawRect.Transformed(Matrix2x3(position, angle, scale)).Center();
}

bool AnimatedSprite2D::GetSpriteAt(const Vector2& wposition, bool findbottomsprite, float minalpha, SpriteDebugInfo& info)
{
    if (useCharacterMap_ && !spritesInfos_.Size())
        GetSpriteInfos();

    unsigned numspritekeys = GetNumSpriteKeys();
    const PODVector<Spriter::SpriteTimelineKey* >& spriteKeys = GetSpriteKeys();

    unsigned spriteindex = findbottomsprite ? 0 : numspritekeys-1;
    const int inc = findbottomsprite ? 1 : -1;

    for (;numspritekeys > 0;numspritekeys--,spriteindex += inc)
    {
        const Spriter::SpriteTimelineKey& spriteKey = *spriteKeys[spriteindex];

        SpriteInfo* spriteinfo = spritesInfos_.Size() ? spritesInfos_[spriteindex] : 0;
        unsigned key = (spriteKey.folderId_ << 16) + spriteKey.fileId_;
        Sprite2D* msprite = GetMappedSprite(key);
        Sprite2D* sprite = spriteinfo ? spriteinfo->sprite_ : msprite;
        if (!sprite)
            continue;

        // 1. check if inside the drawrect
        Vector2 position, pivot, scale;
        Rect drawRect;
        float angle;

        const Spriter::SpatialInfo& spatialinfo = spriteKey.info_;
        if (spriteinfo)
        {
            if (spriteinfo->mapinfo_)
            {
                if (!flipX_)
                {
                    position.x_ = spatialinfo.x_ + spriteinfo->mapinfo_->instruction_->targetdx_;
                    pivot.x_ = spriteKey.pivotX_ + spriteinfo->dPivot_.x_;
                }
                else
                {
                    position.x_ = -spatialinfo.x_ - spriteinfo->mapinfo_->instruction_->targetdx_;
                    pivot.x_ = 1.0f - spriteKey.pivotX_ - spriteinfo->dPivot_.x_;
                }
                if (!flipY_)
                {
                    position.y_ = spatialinfo.y_ + spriteinfo->mapinfo_->instruction_->targetdy_;
                    pivot.y_ = spriteKey.pivotY_ + spriteinfo->dPivot_.y_;
                }
                else
                {
                    position.y_ = -spatialinfo.y_ - spriteinfo->mapinfo_->instruction_->targetdy_;
                    pivot.y_ = 1.0f - spriteKey.pivotY_ - spriteinfo->dPivot_.y_;
                }
                angle = spatialinfo.angle_ + spriteinfo->mapinfo_->instruction_->targetdangle_;
            }
            else
            {
                if (!flipX_)
                {
                    position.x_ = spatialinfo.x_;
                    pivot.x_ = spriteKey.pivotX_ + spriteinfo->dPivot_.x_;
                }
                else
                {
                    position.x_ = -spatialinfo.x_;
                    pivot.x_ = 1.0f - spriteKey.pivotX_ - spriteinfo->dPivot_.x_;
                }
                if (!flipY_)
                {
                    position.y_ = spatialinfo.y_;
                    pivot.y_ = spriteKey.pivotY_ + spriteinfo->dPivot_.y_;
                }
                else
                {
                    position.y_ = -spatialinfo.y_;
                    pivot.y_ = 1.0f - spriteKey.pivotY_ - spriteinfo->dPivot_.y_;
                }
                angle = spatialinfo.angle_;
            }
        }
        else
        {
            if (!flipX_)
            {
                position.x_ = spatialinfo.x_;
                pivot.x_ = spriteKey.pivotX_;
            }
            else
            {
                position.x_ = -spatialinfo.x_;
                pivot.x_ = 1.0f - spriteKey.pivotX_;
            }
            if (!flipY_)
            {
                position.y_ = spatialinfo.y_;
                pivot.y_ = spriteKey.pivotY_;
            }
            else
            {
                position.y_ = -spatialinfo.y_;
                pivot.y_ = 1.0f - spriteKey.pivotY_;
            }
            angle = spatialinfo.angle_;
        }
        if (flipX_ != flipY_)
            angle = -angle;

        scale.x_ = spatialinfo.scaleX_;
        scale.y_ = spatialinfo.scaleY_;
        if (spriteinfo)
        {
            scale.x_ *= spriteinfo->scale_.x_;
            scale.y_ *= spriteinfo->scale_.y_;
            if (spriteinfo->mapinfo_)
            {
                scale.x_ *= spriteinfo->mapinfo_->instruction_->targetscalex_;
                scale.y_ *= spriteinfo->mapinfo_->instruction_->targetscaley_;
            }
        }

        sLocalTransform_.Set(position * PIXEL_SIZE, angle, scale);

        if (sprite->GetRotated())
        {
            // set the translation part
            sRotatedMatrix_.m02_ = -pivot.x_ * (float)sprite->GetSourceSize().x_ * PIXEL_SIZE;
            sRotatedMatrix_.m12_ = (1.f-pivot.y_) * (float)sprite->GetSourceSize().y_ * PIXEL_SIZE;
            sLocalTransform_ = sLocalTransform_ * sRotatedMatrix_;
        }

        position = sLocalTransform_.Inverse() * wposition;
        sprite->GetDrawRectangle(drawRect, pivot);
        if (drawRect.IsInside(position) == OUTSIDE)
            continue;

        // 2. check if inside the texture
        // normalize the position inside drawrect and use it in texture rectangle to retrieve the pixel color
        position -= drawRect.min_;
        position /= drawRect.Size();

        // Get the pixel in the sprite rectangle
        const IntRect& rect = sprite->GetRectangle();
        IntVector2 pixelcoord(flipX_ ? rect.right_ - position.x_ * (rect.right_ - rect.left_) : rect.left_ + position.x_ * (rect.right_ - rect.left_),
                              flipY_ ? rect.top_ - position.y_ * (rect.top_ - rect.bottom_) : rect.bottom_ + position.y_ * (rect.top_ - rect.bottom_));
        if (rect.IsInside(pixelcoord) == OUTSIDE)
            continue;
        // Is the Pixel inside the image ?
        if (sprite->GetTexture()->GetLoadImageStored())
            if (sprite->GetTexture()->GetLoadImage()->GetPixel(pixelcoord.x_, pixelcoord.y_).a_ < minalpha)
                continue;

        // 3. set the debug info
        info.key_ = key;
        info.spriteindex_ = spriteindex;
        info.sprite_ = msprite;
        info.spriteinfo_ = spriteinfo;
        info.localposition_ = position;
        info.localscale_ = scale;
        info.localrotation_ = angle;

        Matrix2x3 nodeWorldTransform;
        if (localRotation_ != 0.f || localPosition_ != Vector2::ZERO)
            nodeWorldTransform = GetNode()->GetWorldTransform2D() * Matrix2x3(localPosition_, localRotation_, Vector2::ONE);
        else
            nodeWorldTransform = GetNode()->GetWorldTransform2D();

        sWorldTransform_ = nodeWorldTransform * sLocalTransform_;
        info.vertices_.Clear();
        info.vertices_.Push(sWorldTransform_ * drawRect.min_);
        info.vertices_.Push(sWorldTransform_ * Vector2(drawRect.min_.x_, drawRect.max_.y_));
        info.vertices_.Push(sWorldTransform_ * drawRect.max_);
        info.vertices_.Push(sWorldTransform_ * Vector2(drawRect.max_.x_, drawRect.min_.y_));

        return true;
    }
    return false;
}

Sprite2D* AnimatedSprite2D::GetSprite(unsigned zorder) const
{
    if (zorder >= GetNumSpriteKeys())
        return 0;

    if (zorder < spritesInfos_.Size())
        return spritesInfos_[zorder]->sprite_;

    const Spriter::SpriteTimelineKey& spriteKey = *spriterInstance_->GetSpriteKeys()[zorder];
    return GetMappedSprite((spriteKey.folderId_ << 16) + spriteKey.fileId_);
}

/// RENDERTARGET

static SharedPtr<Texture2D> sRttTexture_;
static SharedPtr<Viewport> sRttViewport_;
static SharedPtr<Material> sRttMaterial_;
static SharedPtr<Scene> sRttScene_;
static WeakPtr<Node> sRttRootNode_;
//static int sNumRttNodesEnabled_;
static bool sRttRootNodeDirty_;

void AnimatedSprite2D::SetRenderTargetContext(Texture2D* texture, Viewport* viewport, Material* material)
{
    sRttTexture_ = texture;
    sRttViewport_ = viewport;
    sRttMaterial_ = material;
//    sNumRttNodesEnabled_ = 0;

    if (sRttViewport_)
        sRttScene_ = sRttViewport_->GetScene();
    else
        sRttScene_.Reset();
}

void AnimatedSprite2D::SetRenderTargetAttr(const String& rttNodeParams)
{
    if (renderTargetParams_ != rttNodeParams)
    {
        renderTargetParams_ = rttNodeParams;

        if (!rttNodeParams.Empty())
        {
            // Remove existing RenderedNode
            if (GetRenderTarget())
            {
                Node* rootnode = renderTarget_->GetNode()->GetParent();
                renderTarget_->GetNode()->Remove();
                renderTarget_.Reset();

                URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderTargetAttr : this=%u ... remove renderAnimation numRenderedNodes=%u",
                                this, rootnode->GetNumChildren());

                if (!sRttRootNode_->GetNumChildren())
                {
                    // Desactive the view
                    sRttViewport_->SetScene(0);

                    // Send this to alert for desactiving the bind of the rendertarget
                    VariantMap& eventData = GetEventDataMap();
                    eventData[ComponentChanged::P_COMPONENT] = this;
                    eventData[ComponentChanged::P_NEWCOMPONENT] = renderTarget_.Get();
                    this->SendEvent(E_COMPONENTCHANGED, eventData);
                }
            }

            // If no Render Target Texture, use classic rendering
            if (!sRttTexture_)
            {
                URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderTargetAttr : node=%s(%u) ... no renderTargetTexture ... use classic rendering !", node_->GetName().CString(), node_->GetID());
                renderTarget_.Reset();
                SetRenderTargetFrom(renderTargetParams_);
            }
        }

        URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderTargetAttr : this=%u ... params=%s",
                         this, renderTargetParams_.CString());
    }
}

void AnimatedSprite2D::SetRenderTargetFrom(const String& rttNodeParams, bool sendevent)
{
//    URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderTargetFrom : this=%u params=%s ...", this, rttNodeParams.CString());

    Vector<String> params = rttNodeParams.Split('|');
    String scmlset = params[0];
    String customssheet = params[1];
    int textureEffects = params.Size() > 2 ? ToInt(params[2]) : 0;

    if (sRttTexture_)
    {
        SetRenderTarget(scmlset, customssheet, textureEffects, sendevent);
        SetRenderSprite();
    }
    // No Render Target Texture : use Only Custom Material without RenderTargeting
    else
    {
        // Create Classic SpriterInstance
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        AnimationSet2D::customSpritesheetFile_ = customssheet;
        SetAnimationSet(cache->GetResource<AnimationSet2D>(scmlset));
        AnimationSet2D::customSpritesheetFile_.Clear();
        SetTextureFX(textureEffects);
    }
}

void AnimatedSprite2D::SetRenderTargetFrom(AnimatedSprite2D* otherAnimation)
{
    renderTarget_ = otherAnimation->GetRenderTarget();

    SetRenderSprite(otherAnimation->GetRenderSprite());
    SetCustomMaterial(otherAnimation->GetCustomMaterial());
    SetTextureFX(otherAnimation->GetTextureFX());
}

void AnimatedSprite2D::SetRenderTarget(const String& scmlset, const String& customssheet, int textureEffects, bool sendevent)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    if (!sRttRootNode_)
        sRttRootNode_ = sRttScene_->CreateTemporaryChild("RttRootNode", LOCAL);

    // Be sure to active the view
    sRttViewport_->SetScene(sRttScene_);

    // create the drawable in the rtt scene
    Node* renderNode = sRttRootNode_->CreateTemporaryChild("RttNode", LOCAL);
    renderTarget_ = renderNode->CreateComponent<AnimatedSprite2D>(LOCAL);

    if (sRttMaterial_)
    {
        renderTarget_->SetCustomMaterial(sRttMaterial_);
    }

    if (textureEffects)
        renderTarget_->SetTextureFX(textureEffects & (~14)); // Never Apply CROP+BLUR+FXAA Effects = 2+4+8

    AnimationSet2D::customSpritesheetFile_ = customssheet;
    renderTarget_->SetAnimationSet(cache->GetResource<AnimationSet2D>(scmlset));
    AnimationSet2D::customSpritesheetFile_.Clear();
    renderTarget_->SetSpriterAnimation(0);
    renderTarget_->SetDynamicBoundingBox(true);

    // mark dirty for the node positioning
    sRttRootNodeDirty_ = true;

    if (sRttMaterial_)
        SetCustomMaterial(sRttMaterial_);
    SetTextureFX(textureEffects);

    URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderTarget : %s(%u) create a rendertarget %s(%u) material=%s !",
                     node_->GetName().CString(), node_->GetID(), renderNode->GetName().CString(), renderNode->GetID(),
                     sRttMaterial_? sRttMaterial_->GetName().CString() : "none");

    if (sendevent)
    {
        VariantMap& eventData = GetEventDataMap();
        eventData[ComponentChanged::P_COMPONENT] = this;
        eventData[ComponentChanged::P_NEWCOMPONENT] = renderTarget_.Get();
        this->SendEvent(E_COMPONENTCHANGED, eventData);
    }
}

void AnimatedSprite2D::SetRenderSprite(Sprite2D* sprite)
{
    if (renderSprite_)
    {
        renderSprite_->SetTexture(sRttTexture_);
    }
    else if (!sprite)
    {
        sprite = new Sprite2D(context_);
        sprite->SetTexture(sRttTexture_);
//        URHO3D_LOGERRORF("AnimatedSprite2D() - SetRenderSprite : %s(%u) create a rendersprite !", node_->GetName().CString(), node_->GetID());
    }

    if (sprite && renderSprite_ != sprite)
        renderSprite_ = sprite;
}

void AnimatedSprite2D::UpdateRenderTarget()
{
    // Update the nodes positionniong in rttscene
    if (sRttRootNodeDirty_)
    {
        const Vector<SharedPtr<Node> >& children = sRttRootNode_->GetChildren();
        if (children.Size())
        {
            float hw = (float)sRttTexture_->GetWidth() * 0.5f * PIXEL_SIZE;

            /// TODO : just distribute nodes on a row for the moment
            int numparts = children.Size() + 1;
            float pw = 2.f * hw / numparts;
            for (int i=0; i < children.Size(); i++)
                children[i]->SetPosition(Vector3(-hw + (i+1)*pw, 0.f, 0.f));

            sRttRootNodeDirty_ = false;
        }
    }

    const int enlarge = 8;
    BoundingBox bbox = renderTarget_->GetWorldBoundingBox2D();

    float hscreenx = (float)sRttTexture_->GetWidth() * 0.5f;
    float hscreeny = (float)sRttTexture_->GetHeight() * 0.5f;

    IntRect rect((int)(hscreenx + bbox.min_.x_ / PIXEL_SIZE) - enlarge, (int)(hscreeny - bbox.max_.y_ / PIXEL_SIZE) - enlarge,
                 (int)(hscreenx + bbox.max_.x_ / PIXEL_SIZE) + enlarge, (int)(hscreeny - bbox.min_.y_ / PIXEL_SIZE) + enlarge);

    Vector2 hotspot((renderTarget_->GetNode()->GetPosition().x_ - bbox.min_.x_) / (bbox.max_.x_ - bbox.min_.x_),
                    (renderTarget_->GetNode()->GetPosition().y_ - bbox.min_.y_) / (bbox.max_.y_ - bbox.min_.y_));

    renderSprite_->SetRectangle(rect);
    renderSprite_->SetSourceSize(rect.right_ - rect.left_, rect.bottom_ - rect.top_);
    renderSprite_->SetHotSpot(hotspot);

    drawRectDirty_ = true;

//    URHO3D_LOGDEBUGF("AnimatedSprite2D() - UpdateRenderTarget : %s(%u) rttbox=%s rectangle=%s sourcesize=%s hotspot=%s",
//                    node_->GetName().CString(), node_->GetID(), bbox.ToString().CString(), rect.ToString().CString(),
//                    rect.Size().ToString().CString(), hotspot.ToString().CString());
}

Texture* AnimatedSprite2D::GetRenderTexture() const
{
    return static_cast<Texture*>(sRttTexture_.Get());
}


/// RENDERED ANIMATIONS

void AnimatedSprite2D::ClearRenderedAnimations()
{
    if (node_ && GetSpriterInstance())
    {
        HashMap<String, Spriter::NodeUpdater >& nodeupdaters = GetSpriterInstance()->GetNodeUpdaters();
        for (HashMap<String, Spriter::NodeUpdater >::Iterator it = nodeupdaters.Begin(); it != nodeupdaters.End(); ++it)
        {
            if (it->first_.StartsWith("MT"))
                continue;

            Spriter::NodeUpdater& nodeupdater = it->second_;
            AnimatedSprite2D* animation = static_cast<AnimatedSprite2D*>(nodeupdater.ucomponent_);
            if (animation)
            {
                URHO3D_LOGINFOF("AnimatedSprite2D() - ClearRenderedAnimations : node=%s(%u) ... Clear Animation=%u for slot=%s", node_->GetName().CString(), node_->GetID(), animation, it->first_.CString());
                animation->ClearRenderedAnimations();
            }

            nodeupdater.ucomponent_ = 0;
        }
    }

    for (PODVector<AnimatedSprite2D*>::Iterator it=renderedAnimations_.Begin(); it != renderedAnimations_.End(); ++it)
    {
        (*it)->Remove();
    }

    renderedAnimations_.Clear();
}

AnimatedSprite2D* AnimatedSprite2D::AddRenderedAnimation(const String& characterMapName, AnimationSet2D* animationSet, int textureFX)
{
    if (!GetSpriterInstance())
        return 0;

    // to be sure to have spriterInstance_ updated
    UpdateSpriterAnimation(0.f);

    HashMap<String, Spriter::NodeUpdater >::Iterator it = spriterInstance_->GetNodeUpdaters().Find(characterMapName);
    if (it == spriterInstance_->GetNodeUpdaters().End())
    {
        URHO3D_LOGERRORF("AnimatedSprite2D() - AddRenderedAnimation : node=%s(%u) no nodeupdater for slot=%s ...", node_->GetName().CString(), node_->GetID(), characterMapName.CString());
        return 0;
    }

    Spriter::NodeUpdater& nodeupdater = it->second_;

    Node* node = node_->GetChild(characterMapName);
    if (!node)
    {
        node = node_->CreateChild(characterMapName, LOCAL);
        node->SetTemporary(true);
        node->isPoolNode_ = node_->isPoolNode_;
        node->SetChangeModeEnable(false);
    }

    // find the mapped sprite size
    Spriter::CharacterMap* characterMapOrigin = GetCharacterMap(characterMapName);
    PODVector<Sprite2D*> originalSprites;
    if (characterMapOrigin)
        GetMappedSprites(characterMapOrigin, originalSprites);
    const IntVector2& animationsize   = animationSet->GetSprite() ? animationSet->GetSprite()->GetSourceSize() : IntVector2::ONE;
    const IntVector2& spritesize      = originalSprites.Size()    ? originalSprites[0]->GetSourceSize() : IntVector2::ZERO;

    const Spriter::SpatialInfo& sinfo = nodeupdater.timekey_->info_;
    // scale the animation to the bone node child and apply mappingScaleRatio_ too
    Vector2 scale(sinfo.scaleX_, sinfo.scaleY_);

    // scale the animation to the mapped sprite if exists
    if (spritesize.x_ * spritesize.y_ != 0)
    {
        float scaleratio = spritesize.x_ > spritesize.y_ ? (float)spritesize.x_ / animationsize.x_ : (float)spritesize.y_ / animationsize.y_;
        //float scaleratio = ((float)spritesize.x_ / animationsize.x_ + (float)spritesize.y_ / animationsize.y_) * 0.5f;
        scale.x_ *= scaleratio;
        scale.y_ *= scaleratio;
    }

    // apply scale
    node->SetScale2D(scale);

    int zindex = nodeupdater.timekey_->zIndex_;

    AnimatedSprite2D* animation = (AnimatedSprite2D*)nodeupdater.ucomponent_;
    if (!animation)
    {
        animation = node->GetOrCreateComponent<AnimatedSprite2D>(LOCAL);

        animation->SetAnimationSet(animationSet);
        animation->SetAnimation(animationName_);
        nodeupdater.ucomponent_ = animation;

        // Check for apply the CustomMaterial for the renderedAnimation
        // use this material if the texture exists in it
        if (customMaterial_ && customMaterial_->GetTextureUnit(animationSet->GetSprite()->GetTexture()) != (TextureUnit)(-1))
        {
            animation->SetCustomMaterial(customMaterial_);
            URHO3D_LOGINFOF("AnimatedSprite2D() - AddRenderedAnimation : node=%s(%u) animation=%u added for slot=%s at zindex=%u with customMaterial=%s !",
                            node_->GetName().CString(), node_->GetID(), animation, characterMapName.CString(), animation->renderZIndex_, customMaterial_->GetName().CString());
        }

        animation->SetTextureFX(textureFX);
    }

    animation->SetRenderEnable(false, zindex);

    if (!renderedAnimations_.Contains(animation))
    {
        // always ordered by ascending zindex
        unsigned i = 0;
        while (i < renderedAnimations_.Size() && zindex > renderedAnimations_[i]->renderZIndex_) { i++; }
        renderedAnimations_.Insert(i, animation);
    }

    URHO3D_LOGINFOF("AnimatedSprite2D() - AddRenderedAnimation : node=%s(%u) for slot=%s at zindex=%u scale=%s (sprsize=(%d,%d)(%s) anisize=(%d,%d)(%s))!",
                    node_->GetName().CString(), node_->GetID(), characterMapName.CString(), animation->renderZIndex_, node->GetScale2D().ToString().CString(),
                    spritesize.x_, spritesize.y_, originalSprites.Size() ? originalSprites[0]->GetName().CString() : "",
                    animationsize.x_, animationsize.y_, animationSet->GetSprite() ? animationSet->GetSprite()->GetName().CString() : "");

    return animation;
}

bool AnimatedSprite2D::RemoveRenderedAnimation(const String& characterMapName)
{
    if (!GetSpriterInstance())
        return false;

    bool updated = false;

    HashMap<String, Spriter::NodeUpdater >::Iterator it = spriterInstance_->GetNodeUpdaters().Find(characterMapName);
    if (it != spriterInstance_->GetNodeUpdaters().End())
    {
        Spriter::NodeUpdater& nodeupdater = it->second_;

        AnimatedSprite2D* animation = (AnimatedSprite2D*)nodeupdater.ucomponent_;
        if (animation)
        {
            URHO3D_LOGINFOF("AnimatedSprite2D() - RemoveRenderedAnimation : node=%s(%u) animation=%u removed for slot=%s !", node_->GetName().CString(), node_->GetID(), animation, characterMapName.CString());
            Node* node = animation->GetNode();
            node->SetEnabledRecursive(false);

            animation->ClearRenderedAnimations();
            animation->Remove();

            if (renderedAnimations_.Contains(animation))
            {
                renderedAnimations_.Remove(animation);
                updated = true;
            }
        }

        nodeupdater.ucomponent_ = 0;

//        spriterInstance_->GetNodeUpdaters().Erase(it);
    }

    return updated;
}




/// HELPERS

void AnimatedSprite2D::DumpSpritesInfos() const
{
    URHO3D_LOGINFOF("AnimatedSprite2D() - DumpSpritesInfos : node=%s(%u), numSprites=%u",
                    node_->GetName().CString(), node_->GetID(), spritesInfos_.Size());

    String name;

    for (unsigned i=0;i<spritesInfos_.Size();i++)
    {
        name = spritesInfos_[i]->sprite_ ? spritesInfos_[i]->sprite_->GetName() : String::EMPTY;
        URHO3D_LOGINFOF("sprite %u/%u = %s", i+1, spritesInfos_.Size(), name.CString());
    }
}




/// HANDLERS

void AnimatedSprite2D::OnSetEnabled()
{
    if (enableDebugLog_)
        URHO3D_LOGINFOF("AnimatedSprite2D() - OnSetEnabled : node=%s(%u) enabled=%s ",
             node_->GetName().CString(), node_->GetID(), IsEnabledEffective() ? "true" : "false");

    Drawable2D::OnSetEnabled();

    bool enabled = IsEnabledEffective();

    Scene* scene = GetScene();
    if (scene)
    {
        if (GetSpriterInstance())
        {
            spriterInstance_->ResetCurrentTime();
        }

        if (enabled)
        {
            if (!renderTargetParams_.Empty())
            {
                // create render target animation now if not exists.
                if (!GetRenderTarget())
                {
                    SetRenderTargetFrom(renderTargetParams_, true);
                }
                visibility_ = true;
            }
            else
            {
//                drawRectDirty_ = true;
//                UpdateDrawRectangle();

                UpdateAnimation(0.f);
            }

            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, URHO3D_HANDLER(AnimatedSprite2D, HandleScenePostUpdate));
        }
        else
        {
            UnsubscribeFromEvent(scene, E_SCENEPOSTUPDATE);
            HideTriggers();
        }

//        if (sRttMaterial_ && sRttTexture_ && GetRenderTarget())
//        {
//            if (enabled)
//                sNumRttNodesEnabled_++;
//            else
//                sNumRttNodesEnabled_--;
//
//            sRttMaterial_->SetTexture(TU_DIFFUSE, sNumRttNodesEnabled_ > 0 ? sRttTexture_ : 0);
//        }

        if (GetRenderTarget())
        {
            GetRenderTarget()->GetNode()->SetEnabled(enabled);
            GetRenderTarget()->visibility_ = enabled;

            if (enabled)
            {
                // mark dirty for the node positioning
                sRttRootNodeDirty_ = true;
                if (sRttMaterial_)
                    SetCustomMaterial(sRttMaterial_);
            }
        }

        for (unsigned i=0; i < renderedAnimations_.Size(); i++)
        {
            renderedAnimations_[i]->GetNode()->SetEnabled(enabled);
        }
    }
}

void AnimatedSprite2D::OnSceneSet(Scene* scene)
{
//    URHO3D_LOGINFOF("AnimatedSprite2D() - OnSceneSet : node=%s(%u) scene=%u enabled=%s",
//             node_->GetName().CString(), node_->GetID(), scene, IsEnabledEffective() ? "true" : "false");

    StaticSprite2D::OnSceneSet(scene);

    if (scene)
    {
        if (scene == node_)
            URHO3D_LOGWARNING(GetTypeName() + " should not be created to the root scene node");

        if (IsEnabledEffective())
        {
            SubscribeToEvent(scene, E_SCENEPOSTUPDATE, URHO3D_HANDLER(AnimatedSprite2D, HandleScenePostUpdate));
        }
    }
    else
    {
        UnsubscribeFromEvent(E_SCENEPOSTUPDATE);
    }
}

void AnimatedSprite2D::HandleScenePostUpdate(StringHash eventType, VariantMap& eventData)
{
#ifdef URHO3D_SPINE
    if (GetSpriterInstance() || (skeleton_ && animationState_))
#else
    if (GetSpriterInstance())
#endif
    {
        if (speed_)
            UpdateAnimation(eventData[ScenePostUpdate::P_TIMESTEP].GetFloat());
    }
    else if (GetRenderTarget())
    {
        worldBoundingBoxDirty_ = true;
    }
}


/// UPDATERS

void AnimatedSprite2D::UpdateAnimation(float timeStep)
{
    /// FROMBONES 20200925 : solve problem when AnimatedSprite2D is not visible on Screen Border
    if (!timeStep)
    {
        drawRectDirty_ = true;
        UpdateDrawRectangle();
    }

    if (IsInView())
//    if (renderer_->IsDrawableVisible(this))
    {
//        URHO3D_PROFILE(AnimatedSprite2D_Update);

    #ifdef URHO3D_SPINE
        if (skeleton_ && animationState_)
            UpdateSpineAnimation(timeStep);
    #endif
        if (GetSpriterInstance() && spriterInstance_->GetAnimation())
        {
            UpdateSpriterAnimation(timeStep);
        }

        if (!visibility_)
        {
            visibility_ = true;

            if (renderEnabled_)
                ForceUpdateBatches();
            else
                ClearSourceBatches();

//            if (enableDebugLog_)
//                URHO3D_LOGINFOF("%s Visible !", node_->GetName().CString());
        }
    }
    else
    {
        /// FROMBONES 20190912 : Allow update even if !visible for physics triggers

    #ifdef URHO3D_SPINE
        if (skeleton_ && animationState_)
            UpdateSpineAnimation(timeStep);
    #endif
        if (spriterInstance_ && spriterInstance_->GetAnimation())
            UpdateSpriterAnimation(timeStep);

        if (visibility_)
        {
            ClearSourceBatches();
            visibility_ = false;

//            URHO3D_LOGINFOF("%s No Visible fwbox=%s !", node_->GetName().CString(), GetWorldBoundingBox().ToString().CString());
        }
    }
}

#ifdef URHO3D_SPINE
void AnimatedSprite2D::SetSpineAnimation()
{
    if (!animationStateData_)
    {
        animationStateData_ = spAnimationStateData_create(animationSet_->GetSkeletonData());
        if (!animationStateData_)
        {
            URHO3D_LOGERROR("Create animation state data failed");
            return;
        }
    }

    if (!animationState_)
    {
        animationState_ = spAnimationState_create(animationStateData_);
        if (!animationState_)
        {
            URHO3D_LOGERROR("Create animation state failed");
            return;
        }
    }

    // Reset slots to setup pose, fix issue #932
    spSkeleton_setSlotsToSetupPose(skeleton_);
    spAnimationState_setAnimationByName(animationState_, 0, animationName_.CString(), loopMode_ != LM_FORCE_CLAMPED ? true : false);

    UpdateAnimation(0.0f);
    MarkNetworkUpdate();
}

void AnimatedSprite2D::UpdateSpineAnimation(float timeStep)
{
    URHO3D_PROFILE(AnimatedSprite2D_UpdateSpine);

    timeStep *= speed_;

    skeleton_->scaleX = flipX_ ? -1.f : 1.f;
    skeleton_->scaleY = flipY_ ? -1.f : 1.f;

    spAnimationState_update(animationState_, timeStep);
    spAnimationState_apply(animationState_, skeleton_);
    spSkeleton_updateWorldTransform(skeleton_);

    sourceBatchesDirty_ = true;
    worldBoundingBoxDirty_ = true;
}

void AnimatedSprite2D::UpdateSourceBatchesSpine()
{
//    if (enableDebugLog_)
//        URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateSourceBatchesSpine : node=%s ... material=%s",
//                    node_->GetName().CString(), sourceBatches_[0][0].material_ ? sourceBatches_[0][0].material_->GetName().CString() : "none");

    const Matrix2x3& worldTransform2D = GetNode()->GetWorldTransform2D();

    SourceBatch2D& sourceBatch = sourceBatches_[0][0];
    sourceBatch.vertices_.Clear();

    const int SLOT_VERTEX_COUNT_MAX = 1024;
    float slotVertices[SLOT_VERTEX_COUNT_MAX];

    for (int i = 0; i < skeleton_->slotsCount; ++i)
    {
        spSlot* slot = skeleton_->drawOrder[i];
        spAttachment* attachment = slot->attachment;
        if (!attachment)
            continue;

        unsigned color = Color(color_.r_ * slot->color.r,
            color_.g_ * slot->color.g,
            color_.b_ * slot->color.b,
            color_.a_ * slot->color.a).ToUInt();

        if (attachment->type == SP_ATTACHMENT_REGION)
        {
            spRegionAttachment* region = (spRegionAttachment*)attachment;
            spRegionAttachment_computeWorldVertices(region, slot, slotVertices, 0 , 2);

            Vertex2D vertices[4];
            vertices[0].position_ = worldTransform2D * Vector2(slotVertices[0], slotVertices[1]);
            vertices[1].position_ = worldTransform2D * Vector2(slotVertices[2], slotVertices[3]);
            vertices[2].position_ = worldTransform2D * Vector2(slotVertices[4], slotVertices[5]);
            vertices[3].position_ = worldTransform2D * Vector2(slotVertices[6], slotVertices[7]);

            vertices[0].color_ = color;
            vertices[1].color_ = color;
            vertices[2].color_ = color;
            vertices[3].color_ = color;

            vertices[0].uv_ = Vector2(region->uvs[0], region->uvs[1]);
            vertices[1].uv_ = Vector2(region->uvs[2], region->uvs[3]);
            vertices[2].uv_ = Vector2(region->uvs[4], region->uvs[5]);
            vertices[3].uv_ = Vector2(region->uvs[6], region->uvs[7]);

            sourceBatch.vertices_.Push(vertices[0]);
            sourceBatch.vertices_.Push(vertices[1]);
            sourceBatch.vertices_.Push(vertices[2]);
            sourceBatch.vertices_.Push(vertices[3]);
        }
        else if (attachment->type == SP_ATTACHMENT_MESH)
        {
            spMeshAttachment* mesh = (spMeshAttachment*)attachment;
            if (mesh->super.worldVerticesLength > SLOT_VERTEX_COUNT_MAX)
                continue;

            spVertexAttachment_computeWorldVertices(&mesh->super, slot, 0, mesh->super.worldVerticesLength, slotVertices, 0, 2);

            Vertex2D vertex;
            vertex.color_ = color;
            for (int j = 0; j < mesh->trianglesCount; ++j)
            {
                int index = mesh->triangles[j] << 1;
                vertex.position_ = worldTransform2D * Vector2(slotVertices[index], slotVertices[index + 1]);
                vertex.uv_ = Vector2(mesh->uvs[index], mesh->uvs[index + 1]);

                sourceBatch.vertices_.Push(vertex);
                // Add padding vertex
                if (j % 3 == 2)
                    sourceBatch.vertices_.Push(vertex);
            }
        }
//		else if (attachment->type == SP_ATTACHMENT_CLIPPING)
//		{
//			spClippingAttachment *clip = (spClippingAttachment *) slot->attachment;
//			spSkeletonClipping_clipStart(clipper, slot, clip);
//			continue;
//		}
		else
                continue;
    }
}
#endif

void AnimatedSprite2D::SetSpriterAnimation(int index, LoopMode2D loopMode)
{
    if (!GetSpriterInstance())
        return;

    if (index == -1)
    {
        if (!spriterInstance_->SetAnimation(animationName_, (Spriter::LoopMode)loopMode_))
        {
//            URHO3D_LOGWARNINGF("AnimatedSprite2D() - SetSpriterAnimation : %s(%u) - Set animation failed ! animationName = %s ...",
//                               node_->GetName().CString(), node_->GetID(), animationName_.CString());
            return;
        }
    }
    else
    {
        if (!spriterInstance_->SetAnimation(index, (Spriter::LoopMode)loopMode))
        {
//            URHO3D_LOGWARNINGF("AnimatedSprite2D() - SetSpriterAnimation : %s(%u) - Set animation failed ! index = %d ...",
//                               node_->GetName().CString(), node_->GetID(), index);
            return;
        }

        animationIndex_ = index;
        animationName_ = spriterInstance_->GetAnimation()->name_;
    }

    for (unsigned i=0; i < renderedAnimations_.Size(); i++)
    {
        renderedAnimations_[i]->SetAnimation(animationName_, loopMode);
    }

    if (IsEnabledEffective())
        HideTriggers();

    worldBoundingBoxDirty_ = drawRectDirty_ = true;

    MarkNetworkUpdate();
}

void AnimatedSprite2D::HideTriggers()
{
    activedEventTriggers_.Clear();

    if (!triggerNodes_.Size())
        return;

//    URHO3D_LOGINFOF("AnimatedSprite2D() - HideTriggers : %s(%u) triggerNodes_=%u...", node_->GetName().CString(), node_->GetID(), triggerNodes_.Size());

    // Inactive Trigger Nodes
    for (Vector<WeakPtr<Node> >::ConstIterator it = triggerNodes_.Begin();it!=triggerNodes_.End();++it)
    {
        if (*it)
            (*it)->SetEnabled(false);
    }
//    URHO3D_LOGINFOF("AnimatedSprite2D() - HideTriggers : %s(%u) ... OK !", node_->GetName().CString(), node_->GetID());
}

void AnimatedSprite2D::ClearTriggers(bool removeNode)
{
    if (removeNode)
    {
        for (Vector<WeakPtr<Node> >::ConstIterator it = triggerNodes_.Begin();it!=triggerNodes_.End();++it)
        {
            if (*it)
                (*it)->Remove();
        }
        triggerNodes_.Clear();
    }

    activedEventTriggers_.Clear();
}

inline void AnimatedSprite2D::LocalToWorld(Spriter::SpatialTimelineKey* key, Vector2& center, float& rotation)
{
    const Spriter::SpatialInfo& spatialinfo = key->info_;

    center.x_ = spatialinfo.x_ * PIXEL_SIZE;
    center.y_ = spatialinfo.y_ * PIXEL_SIZE;
    rotation  = spatialinfo.angle_;

    if (flipX_)
    {
        center.x_ = -center.x_;
        rotation = 180.f - rotation;
    }

    if (flipY_)
    {
        center.y_ = -center.y_;
        rotation = 360.f - rotation;
    }

	if (localPosition_ != Vector2::ZERO)
	{
		sLocalTransform_.Set(localPosition_, localRotation_);
		center = sLocalTransform_ * center;
	}

    sWorldTransform_ = node_->GetWorldTransform2D() * Matrix2x3(center, rotation, Vector2(spatialinfo.scaleX_, spatialinfo.scaleY_));

    center = sWorldTransform_.Translation();
    rotation += localRotation_;
}

void AnimatedSprite2D::UpdateTriggers()
{
    if (!IsEnabledEffective() || !GetSpriterInstance())
        return;

    // Update Event Triggers
    const HashMap<Spriter::Timeline*, Spriter::PointTimelineKey* >& eventTriggers = spriterInstance_->GetEventTriggers();
    if (eventTriggers.Size())
    {
        StringHash triggerEventName;
        StringHash triggerEvent;
        Vector<String> args;
        Spriter::Timeline* timeline;
        Spriter::PointTimelineKey* key;
        Node* triggerNode;

        for (HashMap<Spriter::Timeline*, Spriter::PointTimelineKey* >::ConstIterator it=eventTriggers.Begin(); it!=eventTriggers.End() ; ++it)
        {
            timeline = it->first_;
            if (!timeline)
                continue;

            args = timeline->name_.Split('_');
            triggerEventName = StringHash(timeline->name_);
            triggerEvent = StringHash("SPRITER_" + (args.Size() ? args[0] : timeline->name_));

            if (!activedEventTriggers_.Contains(triggerEventName))
            {
                activedEventTriggers_.Push(triggerEventName);

                if (triggerEvent == SPRITER_SOUND)
                {
                    VariantMap& paramEvent = context_->GetEventDataMap();
                    paramEvent[SPRITER_Event::TYPE] = StringHash(args[1]);
                    node_->SendEvent(triggerEvent, paramEvent);
                }
                else if (triggerEvent == SPRITER_PARTICULE)
                {
                    // spriter timeline name = "Particule_EffectId,Duration"
                    if (args.Size() > 1)
                    {
                        key = it->second_;
                        Vector<String> params = args[1].Split(',');
                        // effectid
                        triggerInfo_.type_ = StringHash(ToUInt(params[0]));
                        // duration
                        triggerInfo_.type2_ = StringHash(ToUInt(params[1]));
                        triggerInfo_.zindex_ = key->zIndex_;
                        LocalToWorld(key, triggerInfo_.position_, triggerInfo_.rotation_);
                        triggerInfo_.rotation_ = key->info_.angle_;
                    }
                    node_->SendEvent(triggerEvent);
                }
                // triggerEvent == SPRITER_ANIMATION, SPRITER_ENTITY or simple SPRITER EVENT (like SPRITER_Explode)
                else
                {
                    // spriter timeline name = "Name_type-entityid,datas"
/// FOR FROMBONES
                    if (args.Size() > 1)
                    {
                        key = it->second_;
                        Vector<String> params  = args[1].Split(',');
                        Vector<String> names   = params.Front().Split('-');
                        triggerInfo_.type_     = StringHash(names.Size() > 0 ? names[0] : args[1]);
                        triggerInfo_.entityid_ = names.Size() > 1 ? ToUInt(names[1]) : 0;
                        triggerInfo_.zindex_   = key->zIndex_;
                        LocalToWorld(key, triggerInfo_.position_, triggerInfo_.rotation_);
                        triggerInfo_.datas_    = params.Size() > 1 ? params[1] : String::EMPTY;

                        if (enableDebugLog_)
                        URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateTriggers : Set Initial Event=%s(%u) type=%u position=%s nodepos=%s...",
                                       timeline->name_.CString(), triggerEvent.Value(), triggerInfo_.type_.Value(), triggerInfo_.position_.ToString().CString(), node_->GetWorldPosition2D().ToString().CString());
                    }
					node_->SendEvent(triggerEvent);
                }

//                if (enableDebugLog_)
//                URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateTriggers : Set Initial Event=%s(%u) got=%u data=%u ...",
//                                       timeline->name_.CString(), triggerEvent.Value(), paramEvent[SPRITER_Event::TYPE].GetStringHash().Value(), paramEvent[SPRITER_Event::DATAS].GetVoidPtr());
            }
        }
    }
    else
    {
        activedEventTriggers_.Clear();
    }

    // Update Tagged Nodes
    HashMap<String, Spriter::NodeUpdater >& nodeUpdaters = spriterInstance_->GetNodeUpdaters();
    if (nodeUpdaters.Size())
    {
        float centerx, centery, angle;

        for (HashMap<String, Spriter::NodeUpdater >::Iterator it=nodeUpdaters.Begin(); it!=nodeUpdaters.End() ; ++it)
        {
            Node* node;
            AnimatedSprite2D* animation = 0;

            Spriter::NodeUpdater& updater = it->second_;
            // Mount Node
            if (it->first_.StartsWith("MT"))
            {
                if (!updater.ucomponent_)
                {
                    node = node_->GetChild(it->first_);
                    if (!node)
                    {
                        node = node_->CreateChild(it->first_, LOCAL);
                        node->SetTemporary(true);
                        node->isPoolNode_ = node_->isPoolNode_;
                        node->SetChangeModeEnable(false);
                    }
                    updater.ucomponent_ = node;
                }
                else
                    node = static_cast<Node*>(updater.ucomponent_);
            }
            // Animation
            else
            {
                if (!updater.ucomponent_)
                {
    //                URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateTriggers : node=%s(%u) no animatedsprite for slot=%s ...", node_->GetName().CString(), node_->GetID(), it->first_.CString());
                    continue;
                }
                 animation = static_cast<AnimatedSprite2D*>(updater.ucomponent_);
                 node = animation->GetNode();
            }

            const Spriter::SpatialInfo& info = updater.timekey_->info_;
            centerx = info.x_;
            centery = info.y_;

            if (flipX_)
                centerx = -centerx;

            if (flipY_)
                centery = -centery;

            node->SetPosition2D(centerx * PIXEL_SIZE, centery * PIXEL_SIZE);

            angle = info.angle_;

            // y orientation on x
            if (flipX_ != flipY_)
                angle = -angle;

            node->SetRotation2D(angle);

            if (animation)
                animation->SetFlip(flipX_, flipY_);

//            URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateTriggers : %s(%u) NodeUpdate On=%s(%u) Update position x=%f y=%f angle=%f flipX=%u ...",
//                            node_->GetName().CString(), node_->GetID(), node->GetName().CString(), node->GetID(), centerx, centery, angle, flipX_);
        }
    }

    updatedPhysicNodes_.Clear();

    // Update Physic Triggers
    const HashMap<Spriter::Timeline*, Spriter::BoxTimelineKey* >& physicTriggers = spriterInstance_->GetPhysicTriggers();
    if (physicTriggers.Size())
    {
        Vector2 center, size, pivot;
        float angle;
        Node* physicNode;
        CollisionCircle2D* collisionCircle;
        CollisionBox2D* collisionBox;
        Spriter::Timeline* timeline;
        Spriter::BoxTimelineKey* key;

        for (HashMap<Spriter::Timeline*, Spriter::BoxTimelineKey* >::ConstIterator it=physicTriggers.Begin();it!=physicTriggers.End();++it)
        {
            timeline = it->first_;
            key = it->second_;
            const Spriter::SpatialInfo& info = key->info_;

            char collidertype = timeline->name_.Front();
            bool isAbox = collidertype == 'B';

            physicNode = node_->GetChild(timeline->name_, LOCAL);

            /*
                Timeline name begin by
                'T' it's a Trigger
                'C' it's a Circle
                'B' it's a Box
            */

            if (!physicNode)
            {
                physicNode = node_->CreateChild(timeline->name_, LOCAL);
                physicNode->isPoolNode_ = node_->isPoolNode_;
                physicNode->SetChangeModeEnable(false);
                physicNode->SetTemporary(true);

                triggerNodes_.Push(WeakPtr<Node>(physicNode));

                if (isAbox)
                {
                    collisionBox = physicNode->CreateComponent<CollisionBox2D>(LOCAL);
                    collisionBox->SetChangeModeEnable(false);
                    collisionBox->SetTrigger(false);
                    collisionBox->SetExtraContactBits(3); // Top Contact Only & Stable
                }
                else
                {
                    collisionCircle = physicNode->CreateComponent<CollisionCircle2D>(LOCAL);
                    collisionCircle->SetChangeModeEnable(false);
                    collisionCircle->SetTrigger(collidertype == 'T');
                    collisionCircle->SetExtraContactBits(3); // Top Contact Only & Stable
                }
            }
            else
            {
                physicNode->SetEnabled(true);
                if (isAbox)
                    collisionBox = physicNode->GetComponent<CollisionBox2D>(LOCAL);
                else
                    collisionCircle = physicNode->GetComponent<CollisionCircle2D>(LOCAL);
            }

            if (isAbox)
            {
                angle = info.angle_;
                if (flipX_)
                    angle = 180.f-angle;

                center.x_ = info.x_ * PIXEL_SIZE + (0.5f - key->pivotX_) * key->width_ * info.scaleX_ * PIXEL_SIZE;
                center.y_ = info.y_ * PIXEL_SIZE + (0.5f - key->pivotY_) * key->height_ * info.scaleY_ * PIXEL_SIZE;

                size.x_ = key->width_ * info.scaleX_ * PIXEL_SIZE;
                size.y_ = key->height_ * info.scaleY_ * PIXEL_SIZE;
                pivot.x_ = info.x_ * PIXEL_SIZE,
                pivot.y_ = info.y_ * PIXEL_SIZE;

//                collisionBox->SetBox(center, size, pivot, angle);
                // Test 01/11/2020 : prevent to recreate fixtures (that destroy contact in box2D without any warning for the End of the Contact : that is problematic for Fall Cases).
                // the following method doesn't destroy fixture, just modify the shape but it's not good because FromBones doesn't temporize and it's a pingpong between FALL-TOUCHGROUND Animations... so keep SetBox Method.
                collisionBox->UpdateBox(center, size, pivot, angle);
            }
            else
            {
                // For Circle : don't handle pivots with Spriter::BoxTimelineKey => you must use default spriter pivot(0.f,0.f)
                // the spriter box
                // 1-----
                // |    |
                // |----2
                // Important : Point1 = in Spriter it's the first point clicked when we create a box

                center.x_ = info.x_ * PIXEL_SIZE + key->width_ * PIXEL_SIZE * 0.5f;
                center.y_ = info.y_ * PIXEL_SIZE - key->height_ * PIXEL_SIZE * 0.5f;

                if (flipX_)
                    center.x_ = -center.x_;
                if (flipY_)
                    center.y_ = -center.y_;

				// 26/11/2022 : Apply the local position
				if (localPosition_ != Vector2::ZERO)
				{
					sLocalTransform_.Set(localPosition_, localRotation_);
					center = sLocalTransform_ * center;
				}

                collisionCircle->SetCenter(center);
                collisionCircle->SetRadius(Max(key->width_, key->height_) * Max(info.scaleX_, info.scaleY_) * 0.5f * PIXEL_SIZE);
            }

            updatedPhysicNodes_.Push(physicNode);

//            URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateTriggers : PhysicTrigger node=%s(%u) physicNode=%s collideType=%c center=%s localposition=%s localrotation=%F ...",
//							node_->GetName().CString(), node_->GetID(), timeline->name_.CString(), collidertype, center.ToString().CString(), localPosition_.ToString().CString(), localRotation_);
        }
    }

    for (Vector<WeakPtr<Node> >::ConstIterator it = triggerNodes_.Begin(); it != triggerNodes_.End(); ++it)
    {
        if (*it && !updatedPhysicNodes_.Contains(*it))
            (*it)->SetEnabled(false);
    }
}

void AnimatedSprite2D::UpdateSpriterAnimation(float timeStep)
{
    if (GetSpriterInstance() && spriterInstance_->Update(timeStep * speed_))
    {
//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSpriterAnimation : node=%s timeStep=%f ...",
//                         node_->GetName().CString(), timeStep);

        for (unsigned i=0; i < renderedAnimations_.Size(); i++)
        {
            renderedAnimations_[i]->UpdateSpriterAnimation(timeStep);
        }

        UpdateTriggers();

        sourceBatchesDirty_ = true;
    }
}

bool AnimatedSprite2D::UpdateDrawRectangle()
{
#ifdef URHO3D_SPINE
    if (skeleton_)
    {
        return true;
    }
#endif
    // if RENDERED TARGET
    if (GetRenderTarget())
    {
        UpdateRenderTarget();

        drawRect_.Clear();

        if (!renderSprite_->GetDrawRectangle(drawRect_, flipX_, flipY_))
            return false;

        drawRectDirty_ = false;
        return true;
    }

    if (!GetSpriterInstance())
        return false;

    if (!drawRectDirty_)
        return true;

    const PODVector<Spriter::SpriteTimelineKey* >& spriteKeys = spriterInstance_->GetSpriteKeys();
    if (!spriteKeys.Size())
        ResetAnimation();

    drawRect_.Clear();

    Rect drawRect;
    Vector2 position;
    Vector2 scale;
    Vector2 pivot;
    float angle;
    Sprite2D* sprite;
    Spriter::SpriteTimelineKey* spriteKey;

    unsigned numSpriteKeys = Min(spriterInstance_->GetNumSpriteKeys(), spriteKeys.Size());
    for (unsigned i = 0; i < numSpriteKeys; ++i)
    {
        spriteKey = spriteKeys[i];
        sprite = animationSet_->GetSpriterFileSprite((spriteKey->folderId_ << 16) + spriteKey->fileId_);

        if (!sprite)
            continue;

        const Spriter::SpatialInfo& spatialinfo = spriteKey->info_;

        if (!flipX_)
        {
            position.x_ = spatialinfo.x_;
            pivot.x_ = spriteKey->pivotX_;
        }
        else
        {
            position.x_ = -spatialinfo.x_;
            pivot.x_ = 1.0f - spriteKey->pivotX_;
        }

        if (!flipY_)
        {
            position.y_ = spatialinfo.y_;
            pivot.y_ = spriteKey->pivotY_;
        }
        else
        {
            position.y_ = -spatialinfo.y_;
            pivot.y_ = 1.0f - spriteKey->pivotY_;
        }

        angle = spatialinfo.angle_;
        if (flipX_ != flipY_)
            angle = -angle;

        scale.x_ = spatialinfo.scaleX_;
        scale.y_ = spatialinfo.scaleY_;

        sLocalTransform_.Set(position * PIXEL_SIZE, angle, scale);
        sprite->GetDrawRectangle(drawRect, pivot);
        drawRect_.Merge(drawRect.Transformed(sLocalTransform_));

//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateDrawRectangle : node=%s(%u) updated drawrect=%s with sprite=%s!", node_->GetName().CString(), node_->GetID(), drawRect_.ToString().CString(), sprite->GetName().CString());
    }

//    URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateDrawRectangle : node=%s(%u) updated numSpriteKeys=%u drawrect=%s !", node_->GetName().CString(), node_->GetID(), spriteKeys.Size(), drawRect_.ToString().CString());

    drawRectDirty_ = false;
    worldBoundingBoxDirty_ = true;
    return true;
}

enum
{
    RESETFIRSTKEY = -1,
    KEEPFIRSTKEY = -2,
};

void AnimatedSprite2D::UpdateSourceBatches()
{
    if (!sourceBatchesDirty_)
        return;

    if (!visibility_ || !renderEnabled_)
    {
        sourceBatchesDirty_ = false;

//        URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatches : node=%s(%u) ... No Visibility or No Render !",
//                             node_->GetName().CString(), node_->GetID());
        return;
    }

//    URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatches : node=%s(%u) drawRectDirty=%s customSourceBatches=%s ...",
//                    node_->GetName().CString(), node_->GetID(), drawRectDirty_ ? "true":"false", customSourceBatches_ ? "true":"false");

//    URHO3D_PROFILE(AnimatedSprite2D_Batch);

#ifdef URHO3D_SPINE
    if (skeleton_ && animationState_)
        UpdateSourceBatchesSpine();
#endif

    if ((GetSpriterInstance() && spriterInstance_->GetAnimation()) || GetRenderTarget())
    {
        if (dynamicBBox_)
        {
//            URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatches : node=%s(%u) dynamicBBox_ ...", node_->GetName().CString(), node_->GetID());
            drawRectDirty_ = true;
        }

        if (!UpdateDrawRectangle())
            return;

        if (GetSpriterInstance())
        {
            Vector<SourceBatch2D>* sourcebatches = customSourceBatches_ ? customSourceBatches_ : sourceBatches_;

            if (renderedAnimations_.Size())
                UpdateSourceBatchesSpriter_RenderAnimations(sourcebatches);
            else if (customSourceBatches_)
                UpdateSourceBatchesSpriter_Custom(sourcebatches, RESETFIRSTKEY, false);
            else if (useCharacterMap_)
                UpdateSourceBatchesSpriter_Custom(sourcebatches);
            else
                UpdateSourceBatchesSpriter(sourcebatches);
        }
        else
        {
            UpdateSourceBatchesSpriter_RenderTarget();
        }
    }
//    else
//        URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatches : node=%s(%u) enabled=%s no spriterInstance or no animation !",
//                                    node_->GetName().CString(), node_->GetID(), IsEnabledEffective() ? "true" : "false");

//    URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatches : node=%s(%u) ... OK !", node_->GetName().CString(), node_->GetID());
	sourceBatchesDirty_ = false;
}



void AnimatedSprite2D::UpdateSourceBatchesSpriter(Vector<SourceBatch2D>* sourceBatches, bool resetBatches)
{
//    if (enableDebugLog_)
//        URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s ... material=%s",
//                    node_->GetName().CString(), sourceBatches[0][0].material_ ? sourceBatches[0][0].material_->GetName().CString() : "none");

    if (!spriterInstance_->GetSpriteKeys().Size())
    {
//        URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s ... No SpriteKeys ! => updateSpriterInstance", node_->GetName().CString());
        UpdateSpriterAnimation(0.f);
    }

    unsigned numSpriteKeys = spriterInstance_->GetNumSpriteKeys();
	const PODVector<Spriter::SpriteTimelineKey* >& spriteKeys = spriterInstance_->GetSpriteKeys();
    if (!numSpriteKeys)
        return;

    Sprite2D* sprite;
    Spriter::SpriteTimelineKey* spriteKey;
    unsigned fileKey;

    if (!sourceBatches[0][0].material_)
    {
        spriteKey = spriteKeys[0];
        sprite = animationSet_->GetSpriterFileSprite((spriteKey->folderId_ << 16) + spriteKey->fileId_);
        sourceBatches[0][0].material_ = sourceBatches[1][0].material_ = customMaterial_ ? customMaterial_ : renderer_->GetMaterial(sprite->GetTexture(), blendMode_);
    }

    Material* material = sourceBatches[0][0].material_;

    int iBatch = resetBatches ? 0 : sourceBatches[0].Size();
    sourceBatches[0].Resize(iBatch+1);
    sourceBatches[0][iBatch].vertices_.Clear();
    sourceBatches[0][iBatch].drawOrder_ = iBatch > 0 ? sourceBatches[0][iBatch-1].drawOrder_ + 1 : GetDrawOrder(0);
    if (iBatch > 0)
        sourceBatches[0][iBatch].material_ = material;

    if (layer_.y_ != -1)
    {
        sourceBatches[1].Resize(iBatch+1);
        sourceBatches[1][iBatch].vertices_.Clear();
        sourceBatches[1][iBatch].drawOrder_ = iBatch > 0 ? sourceBatches[1][iBatch-1].drawOrder_ + 1 : GetDrawOrder(1);
        if (iBatch > 0)
            sourceBatches[1][iBatch].material_ = material;
    }

//    if (node_->GetScene() == sRttScene_)
//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s(%u) iBatch=%d is in rttscene ",
//                        node_->GetName().CString(), node_->GetID(), iBatch);

    // Start Loop

    Matrix2x3 nodeWorldTransform;
    if (localRotation_ != 0.f || localPosition_ != Vector2::ZERO)
        nodeWorldTransform = GetNode()->GetWorldTransform2D() * Matrix2x3(localPosition_, localRotation_, Vector2::ONE);
    else
        nodeWorldTransform = GetNode()->GetWorldTransform2D();

    Rect drawRect;
    Rect textureRect;
    Color color = color_ * spriterInstance_->GetEntity()->color_;
    Color color2 = color2_ * spriterInstance_->GetEntity()->color_;

    Vertex2D vertex0;
    Vertex2D vertex1;
    Vertex2D vertex2;
    Vertex2D vertex3;

#ifdef URHO3D_VULKAN
    vertex0.z_ = vertex1.z_ = vertex2.z_ = vertex3.z_ = node_->GetWorldPosition().z_;
#else
    vertex0.position_.z_ = vertex1.position_.z_ = vertex2.position_.z_ = vertex3.position_.z_ = node_->GetWorldPosition().z_;
#endif

    Vector2 position;
    Vector2 scale;
    Vector2 pivot;

#ifdef URHO3D_VULKAN
    unsigned texmode = 0;
#else
    Vector4 texmode;
#endif

    SetTextureMode(TXM_FX, textureFX_, texmode);

    float angle;

    Vector<Vertex2D>& vertices1 = sourceBatches[0][iBatch].vertices_;
    Vector<Vertex2D>& vertices2 = sourceBatches[1][iBatch].vertices_;

    Texture *texture = 0, *ttexture = 0;

    for (unsigned i = 0; i < numSpriteKeys; ++i)
    {
        spriteKey = spriteKeys[i];
        fileKey = (spriteKey->folderId_ << 16) + spriteKey->fileId_;

        sprite = animationSet_->GetSpriterFileSprite(fileKey);
        if (!sprite)
        {
//            if (GetScene() == sRttScene_)
//                URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s(%u) iBatch=%d no sprite ! ",
//                                node_->GetName().CString(), node_->GetID(), iBatch);
            continue;
        }

        if (!sprite->GetTextureRectangle(textureRect, flipX_, flipY_))
        {
            URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s ... No GetTextureRect !", node_->GetName().CString());
            return;
        }

        // lit or unlit fx
		SetTextureMode(TXM_FX_LIT, spriteKey->fx_ > 0 ? 1U : textureFX_, texmode);

        const Spriter::SpatialInfo& spatialinfo = spriteKey->info_;

        if (!flipX_)
        {
            position.x_ = spatialinfo.x_;
            pivot.x_ = spriteKey->pivotX_;
        }
        else
        {
            position.x_ = -spatialinfo.x_;
            pivot.x_ = 1.0f - spriteKey->pivotX_;
        }

        if (!flipY_)
        {
            position.y_ = spatialinfo.y_;
            pivot.y_ = spriteKey->pivotY_;
        }
        else
        {
            position.y_ = -spatialinfo.y_;
            pivot.y_ = 1.0f - spriteKey->pivotY_;
        }

        angle = spatialinfo.angle_;
        if (flipX_ != flipY_)
            angle = -angle;

        // use the custom hotspot at each time, don't flip again, pivot is already setted
        sprite->GetDrawRectangle(drawRect, pivot);

        ttexture = sprite->GetTexture();
        if (ttexture && ttexture != texture)
        {
            SetTextureMode(TXM_UNIT, material->GetTextureUnit(ttexture), texmode);
//            if (GetScene() == sRttScene_)
//                URHO3D_LOGWARNINGF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s(%u) set texture unit=%d for material=%s texture=%s ! ",
//                                node_->GetName().CString(), node_->GetID(), material->GetTextureUnit(ttexture), material->GetName().CString(), ttexture->GetName().CString());
            texture = ttexture;
        }

        scale.x_ = spatialinfo.scaleX_;
        scale.y_ = spatialinfo.scaleY_;

        sLocalTransform_.Set(position * PIXEL_SIZE, angle, scale); // / texture->GetDpiRatio());

        if (sprite->GetRotated())
        {
            // set the translation part
            sRotatedMatrix_.m02_ = -pivot.x_ * (float)sprite->GetSourceSize().x_ * PIXEL_SIZE;
            sRotatedMatrix_.m12_ = (1.f-pivot.y_) * (float)sprite->GetSourceSize().y_ * PIXEL_SIZE;
            sLocalTransform_ = sLocalTransform_ * sRotatedMatrix_;
        }

        sWorldTransform_ = nodeWorldTransform * sLocalTransform_;
        vertex0.position_ = sWorldTransform_ * drawRect.min_;
        vertex1.position_ = sWorldTransform_ * Vector2(drawRect.min_.x_, drawRect.max_.y_);
        vertex2.position_ = sWorldTransform_ * drawRect.max_;
        vertex3.position_ = sWorldTransform_ * Vector2(drawRect.max_.x_, drawRect.min_.y_);
        vertex0.uv_ = textureRect.min_;
        vertex1.uv_ = Vector2(textureRect.min_.x_, textureRect.max_.y_);
        vertex2.uv_ = textureRect.max_;
        vertex3.uv_ = Vector2(textureRect.max_.x_, textureRect.min_.y_);

        color.a_ = spriteKey->info_.alpha_ * color_.a_;
        vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = (GetSpriteColor(fileKey) * spriteKey->color_ * color).ToUInt();
        vertex0.texmode_ = vertex1.texmode_ = vertex2.texmode_ = vertex3.texmode_ = texmode;

        vertices1.Push(vertex0);
        vertices1.Push(vertex1);
        vertices1.Push(vertex2);
        vertices1.Push(vertex3);

        if (layer_.y_ != -1)
        {
            color2.a_ = spriteKey->info_.alpha_ * color2_.a_;
            vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color2.ToUInt();
            vertices2.Push(vertex0);
            vertices2.Push(vertex1);
            vertices2.Push(vertex2);
            vertices2.Push(vertex3);
        }
    }

//    if (GetScene() == sRttScene_)
//    URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter : node=%s(%u) visibility_=%s ... OK !",
//                    node_->GetName().CString(), node_->GetID(), visibility_ ? "true" : "false");
}

void AnimatedSprite2D::UpdateSourceBatchesSpriter_Custom(Vector<SourceBatch2D>* sourceBatches, int breakZIndex, bool resetBatches)
{
//    if (enableDebugLog_)
//        URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_Custom : node=%s(%u) resetBatches=%s breakz=%d", node_->GetName().CString(), node_->GetID(),
//                        resetBatches ? "true" : "false", breakZIndex);
    if (!sourceBatches)
        return;

    const PODVector<SpriteInfo*>& spriteinfos = AnimatedSprite2D::GetSpriteInfos();

    if (!spriteinfos.Size())
        return;

    // Reset firstkey
    if (resetBatches || !sourceBatches[0].Size() || breakZIndex == RESETFIRSTKEY)
        firstKeyIndex_ = 0;

    // Set the stopkey
    if (breakZIndex > 0)
    {
        if (firstKeyIndex_ >= spritesKeys_.Size()-1)
            return;

        for (size_t i = firstKeyIndex_; i < spritesKeys_.Size(); ++i)
        {
            if (spritesKeys_[i]->zIndex_ > breakZIndex)
            {
                stopKeyIndex_ = i;
                break;
            }
        }
    }
    else
    {
        stopKeyIndex_ = spritesKeys_.Size();
    }

    // Get the material
    Material* material = customMaterial_ ? customMaterial_ : renderer_->GetMaterial(spritesInfos_[0]->sprite_->GetTexture(), blendMode_);
    if (!material)
        return;

    // Reset the batches
    if (resetBatches || !sourceBatches[0].Size())
    {
        sourceBatches[0].Resize(1);
        sourceBatches[0][0].vertices_.Clear();
        sourceBatches[0][0].drawOrder_ = GetDrawOrder(0);
        sourceBatches[0][0].material_ = SharedPtr<Material>(material);
        if (layer_.y_ != -1)
        {
            sourceBatches[1].Resize(1);
            sourceBatches[1][0].vertices_.Clear();
            sourceBatches[1][0].drawOrder_ = GetDrawOrder(1);
            sourceBatches[1][0].material_ = SharedPtr<Material>(material);
        }
    }

    int iBatch = sourceBatches[0].Size()-1;
    Material* prevMaterial = sourceBatches[0][iBatch].material_;

//    if (enableDebugLog_)
//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_Custom : node=%s(%u) srcBatchPtr=%u iBatch=%d resetBatches=%s ifirst=%d istop=%d breakZIndex=%d",
//                    node_->GetName().CString(), node_->GetID(), &sourceBatches, iBatch, resetBatches ? "true" : "false", firstKeyIndex_, stopKeyIndex_, breakZIndex);

    // Start Loop

    const Matrix2x3& nodeWorldTransform = GetNode()->GetWorldTransform2D();

    Rect drawRect;
    Rect textureRect;

    Color color = color_ * spriterInstance_->GetEntity()->color_;
    Color color2 = color2_ * spriterInstance_->GetEntity()->color_;

//    Color color = color_;
//    Color color2 = color2_;

    Vertex2D vertex0;
    Vertex2D vertex1;
    Vertex2D vertex2;
    Vertex2D vertex3;

#ifdef URHO3D_VULKAN
    vertex0.z_ = vertex1.z_ = vertex2.z_ = vertex3.z_ = node_->GetWorldPosition().z_;
#else
    vertex0.position_.z_ = vertex1.position_.z_ = vertex2.position_.z_ = vertex3.position_.z_ = node_->GetWorldPosition().z_;
#endif

    Vector2 position;
    Vector2 scale;
    Vector2 pivot;

    float angle;

    Material* tmaterial;

    Sprite2D* sprite;
    Texture2D* texture = 0;
    Texture2D* ttexture = 0;
    Spriter::SpriteTimelineKey* spriteKey;

    int textureunit = -1;

#ifdef URHO3D_VULKAN
    unsigned texmode = 0;
#else
    Vector4 texmode;
#endif

    SetTextureMode(TXM_FX, textureFX_, texmode);

    for (unsigned i = firstKeyIndex_; i < stopKeyIndex_; i++)
    {
        spriteKey = spritesKeys_[i];
        const SpriteInfo* spriteinfo = spritesInfos_[i];
        sprite = spriteinfo->sprite_;

        if (!sprite->GetTextureRectangle(textureRect, flipX_, flipY_))
            continue;

        ttexture = sprite->GetTexture();
        if (ttexture && texture != ttexture)
        {
            textureunit = (int)material->GetTextureUnit(ttexture);
            // change the material
            if (textureunit == -1)
            {
                tmaterial = customMaterial_ ? customMaterial_ : renderer_->GetMaterial(ttexture, blendMode_);
                if (!tmaterial)
                    continue;

                material = tmaterial;

                // get the new texture unit	const PODVector<Spriter::SpriteTimelineKey* >& spriteKeys = spriterInstance_->GetSpriteKeys();
                textureunit = (int)material->GetTextureUnit(ttexture);
                if (textureunit == -1)
                    continue;
            }

            // change the texture unit
            if (GetTextureMode(TXM_UNIT, texmode) != textureunit)
                SetTextureMode(TXM_UNIT, textureunit, texmode);

            // change the texture
            texture = ttexture;
        }

		// lit or unlit fx
		SetTextureMode(TXM_FX_LIT, spriteKey->fx_ > 0 ? 1U : textureFX_, texmode);

        // Add new Batch
        if (material != prevMaterial)
        {
            iBatch++;

            sourceBatches[0].Resize(iBatch+1);
            sourceBatches[0][iBatch].vertices_.Clear();
            sourceBatches[0][iBatch].drawOrder_ = sourceBatches[0][iBatch-1].drawOrder_+1;
            sourceBatches[0][iBatch].material_ = SharedPtr<Material>(material);
            if (layer_.y_ != -1)
            {
                sourceBatches[1].Resize(iBatch+1);
                sourceBatches[1][iBatch].vertices_.Clear();
                sourceBatches[1][iBatch].drawOrder_ = sourceBatches[1][iBatch-1].drawOrder_+1;
                sourceBatches[1][iBatch].material_ = SharedPtr<Material>(material);
            }
            prevMaterial = material;

//            if (node_->GetID() == 16777270 ||
//                (node_->GetParent() && node_->GetParent()->GetID() == 16777270) ||
//                (node_->GetParent() && node_->GetParent()->GetParent() && node_->GetParent()->GetParent()->GetID() == 16777270))
//                URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_Custom : node=%s(%u) add batch(size=%u) !", node_->GetName().CString(), node_->GetID(), sourceBatches[0].Size());
        }

        const Spriter::SpatialInfo& spatialinfo = spriteKey->info_;
        if (spriteinfo->mapinfo_)
        {
            if (!flipX_)
            {
                position.x_ = spatialinfo.x_ + spriteinfo->mapinfo_->instruction_->targetdx_;
                pivot.x_ = spriteKey->pivotX_ + spriteinfo->dPivot_.x_;
            }
            else
            {
                position.x_ = -spatialinfo.x_ - spriteinfo->mapinfo_->instruction_->targetdx_;
                pivot.x_ = 1.0f - spriteKey->pivotX_ - spriteinfo->dPivot_.x_;
            }
            if (!flipY_)
            {
                position.y_ = spatialinfo.y_ + spriteinfo->mapinfo_->instruction_->targetdy_;
                pivot.y_ = spriteKey->pivotY_ + spriteinfo->dPivot_.y_;
            }
            else
            {
                position.y_ = -spatialinfo.y_ - spriteinfo->mapinfo_->instruction_->targetdy_;
                pivot.y_ = 1.0f - spriteKey->pivotY_ - spriteinfo->dPivot_.y_;
            }
            angle = spatialinfo.angle_ + spriteinfo->mapinfo_->instruction_->targetdangle_;
        }
        else
        {
            if (!flipX_)
            {
                position.x_ = spatialinfo.x_;
                pivot.x_ = spriteKey->pivotX_ + spriteinfo->dPivot_.x_;
            }
            else
            {
                position.x_ = -spatialinfo.x_;
                pivot.x_ = 1.0f - spriteKey->pivotX_ - spriteinfo->dPivot_.x_;
            }
            if (!flipY_)
            {
                position.y_ = spatialinfo.y_;
                pivot.y_ = spriteKey->pivotY_ + spriteinfo->dPivot_.y_;
            }
            else
            {
                position.y_ = -spatialinfo.y_;
                pivot.y_ = 1.0f - spriteKey->pivotY_ - spriteinfo->dPivot_.y_;
            }
            angle = spatialinfo.angle_;
        }
        if (flipX_ != flipY_)
            angle = -angle;

        scale.x_ = spatialinfo.scaleX_ * spriteinfo->scale_.x_;
        scale.y_ = spatialinfo.scaleY_ * spriteinfo->scale_.y_;
        if (spriteinfo->mapinfo_)
        {
            scale.x_ *= spriteinfo->mapinfo_->instruction_->targetscalex_;
            scale.y_ *= spriteinfo->mapinfo_->instruction_->targetscaley_;
        }

        sLocalTransform_.Set(position * PIXEL_SIZE, angle, scale);// / texture->GetDpiRatio());

        if (sprite->GetRotated())
        {
            // set the translation part
            sRotatedMatrix_.m02_ = -pivot.x_ * (float)sprite->GetSourceSize().x_ * PIXEL_SIZE;
            sRotatedMatrix_.m12_ = (1.f-pivot.y_) * (float)sprite->GetSourceSize().y_ * PIXEL_SIZE;
            sLocalTransform_ = sLocalTransform_ * sRotatedMatrix_;
        }

        // use the custom hotspot at each time, don't flip again, pivot is already setted
        sprite->GetDrawRectangle(drawRect, pivot);

        sWorldTransform_ = nodeWorldTransform * sLocalTransform_;
        vertex0.position_ = sWorldTransform_ * drawRect.min_;
        vertex1.position_ = sWorldTransform_ * Vector2(drawRect.min_.x_, drawRect.max_.y_);
        vertex2.position_ = sWorldTransform_ * drawRect.max_;
        vertex3.position_ = sWorldTransform_ * Vector2(drawRect.max_.x_, drawRect.min_.y_);
        vertex0.uv_ = textureRect.min_;
        vertex1.uv_ = Vector2(textureRect.min_.x_, textureRect.max_.y_);
        vertex2.uv_ = textureRect.max_;
        vertex3.uv_ = Vector2(textureRect.max_.x_, textureRect.min_.y_);

        // Set Batch
        Vector<Vertex2D>& vertices1 = sourceBatches[0][iBatch].vertices_;
        color.a_ = spriteKey->info_.alpha_ * color_.a_;
        if (spriteinfo->pcolor_)
            vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = ((*spriteinfo->pcolor_) * spriteKey->color_ * color).ToUInt();
        else
            vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = (GetSpriteColor((spriteKey->folderId_ << 16) + spriteKey->fileId_) * spriteKey->color_ * color).ToUInt();
        vertex0.texmode_ = vertex1.texmode_ = vertex2.texmode_ = vertex3.texmode_ = texmode;

		vertices1.Push(vertex0);
        vertices1.Push(vertex1);
        vertices1.Push(vertex2);
        vertices1.Push(vertex3);
        if (layer_.y_ != -1)
        {
            Vector<Vertex2D>& vertices2 = sourceBatches[1][iBatch].vertices_;
            color2.a_ = spriteKey->info_.alpha_ * color2_.a_;
            vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color2.ToUInt();
            vertices2.Push(vertex0);
            vertices2.Push(vertex1);
            vertices2.Push(vertex2);
            vertices2.Push(vertex3);
        }
    }

//    if (enableDebugLog_)
//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_Custom : node=%s(%u) numBatches=%u ... !",
//                    node_->GetName().CString(), node_->GetID(), sourceBatches[0].Size());

    firstKeyIndex_ = stopKeyIndex_;
}

void AnimatedSprite2D::SetCustomSourceBatches(Vector<SourceBatch2D>* sourceBatches)
{
    customSourceBatches_ = sourceBatches;
    firstKeyIndex_ = 0;
}

void AnimatedSprite2D::UpdateSourceBatchesSpriter_RenderAnimations(Vector<SourceBatch2D>* sourceBatches)
{
    bool hasRendered = false;

//    URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_RenderAnimations : node=%s(%u) srcBatchPtr=%u ...", node_->GetName().CString(), node_->GetID(), &sourceBatches);

    for (unsigned i=0; i < renderedAnimations_.Size(); i++)
    {
        AnimatedSprite2D* animation = renderedAnimations_[i];

        if (!animation->GetSpriterInstance())
            continue;

        UpdateSourceBatchesSpriter_Custom(sourceBatches, animation->renderZIndex_, i == 0 && !customSourceBatches_);

        animation->renderEnabled_ = true;
        animation->SetCustomSourceBatches(sourceBatches);
//        animation->SetMappingScaleRatio(mappingScaleRatio_);
        animation->UpdateSourceBatches();
        animation->SetCustomSourceBatches(0);
        animation->renderEnabled_ = false;

        hasRendered = true;
    }

    UpdateSourceBatchesSpriter_Custom(sourceBatches, KEEPFIRSTKEY, !hasRendered);

//    if (node_->GetID() == 16777270)
//        URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_RenderAnimations : node=%s(%u) numbatches=%u !", node_->GetName().CString(), node_->GetID(), sourceBatches->Size());

//    URHO3D_LOGINFOF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_RenderAnimations : node=%s(%u) ... OK !", node_->GetName().CString(), node_->GetID());
}

void AnimatedSprite2D::UpdateSourceBatchesSpriter_RenderTarget()
{
    Vector<Vertex2D>& vertices1 = sourceBatches_[0][0].vertices_;
    vertices1.Clear();

    if (!renderSprite_->GetTextureRectangle(textureRect_, flipX_, flipY_))
        return;

//    URHO3D_LOGERRORF("AnimatedSprite2D() - UpdateSourceBatchesSpriter_RenderTarget : node=%s(%u) texture=%s drawrect=%s textrect=%s ...",
//                     node_->GetName().CString(), node_->GetID(), renderSprite_->GetTexture()->GetName().CString(),
//                     drawRect_.ToString().CString(), textureRect_.ToString().CString() );

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

    const Matrix2x3& worldTransform = node_->GetWorldTransform2D();
    vertex0.position_ = worldTransform * Vector2(drawRect_.min_.x_, drawRect_.min_.y_);
    vertex1.position_ = worldTransform * Vector2(drawRect_.min_.x_, drawRect_.max_.y_);
    vertex2.position_ = worldTransform * Vector2(drawRect_.max_.x_, drawRect_.max_.y_);
    vertex3.position_ = worldTransform * Vector2(drawRect_.max_.x_, drawRect_.min_.y_);

    vertex0.uv_ = textureRect_.min_;
    vertex1.uv_ = Vector2(textureRect_.min_.x_, textureRect_.max_.y_);
    vertex2.uv_ = textureRect_.max_;
    vertex3.uv_ = Vector2(textureRect_.max_.x_, textureRect_.min_.y_);

    vertex0.color_ = vertex1.color_ = vertex2.color_ = vertex3.color_ = color_.ToUInt();

#ifdef URHO3D_VULKAN
    unsigned texmode = 0;
#else
    Vector4 texmode;
#endif
    SetTextureMode(TXM_UNIT, customMaterial_ ? customMaterial_->GetTextureUnit(renderSprite_->GetTexture()) : TU_DIFFUSE, texmode);
    SetTextureMode(TXM_FX, textureFX_, texmode);
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

void AnimatedSprite2D::Dispose(bool removeNode)
{
#ifdef URHO3D_SPINE
    if (animationState_)
    {
        spAnimationState_dispose(animationState_);
        animationState_ = 0;
    }

    if (animationStateData_)
    {
        spAnimationStateData_dispose(animationStateData_);
        animationStateData_ = 0;
    }

    if (skeleton_)
    {
        spSkeleton_dispose(skeleton_);
        skeleton_ = 0;
    }
#endif
    if (GetSpriterInstance())
    {
		ClearTriggers(removeNode);

        ResetCharacterMapping();

        spriterInstance_.Reset();
    }
    else if (GetRenderTarget())
    {
        renderTarget_->GetNode()->Remove();
        renderTarget_.Reset();
    }

    for (unsigned i=0; i < 2; i++)
    {
        sourceBatches_[i].Clear();
        sourceBatches_[i].Resize(1);
    }

    animationName_.Clear();

    customSourceBatches_ = 0;
    renderEnabled_ = true;
}

}
