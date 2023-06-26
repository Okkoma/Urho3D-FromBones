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

#include "../Urho2D/StaticSprite2D.h"

#ifdef URHO3D_SPINE
struct spAnimationState;
struct spAnimationStateData;
struct spSkeleton;
#endif

/// Loop mode.
enum LoopMode2D
{
    /// Default, use animation's value.
    LM_DEFAULT = 0,
    /// Force looped.
    LM_FORCE_LOOPED,
    /// Force clamped.
    LM_FORCE_CLAMPED
};



namespace Urho3D
{

namespace Spriter
{
    class SpriterInstance;
    struct Animation;
    struct CharacterMap;
    struct ColorMap;
    struct SpatialTimelineKey;
    struct SpriteTimelineKey;
}

class AnimationSet2D;
class Viewport;
class Texture;

    struct SpriteInfo
    {
        SpriteInfo() : sprite_(0), pcolor_(0) { }

        unsigned key_;
        Sprite2D* sprite_;
        const Color* pcolor_;
        float scalex_;
        float scaley_;
        float deltaHotspotx_;
        float deltaHotspoty_;
    };

    struct EventTriggerInfo
    {
        StringHash type_;
        StringHash type2_;
        unsigned char entityid_;
        Vector2 position_;
        float rotation_;
        int zindex_;
        Node* node_;
        String datas_;
    };

/// Animated sprite component, it uses to play animation created by Spine (http://www.esotericsoftware.com) and Spriter (http://www.brashmonkey.com/).
class URHO3D_API AnimatedSprite2D : public StaticSprite2D
{
    URHO3D_OBJECT(AnimatedSprite2D, StaticSprite2D);

public:
    /// Construct.
    AnimatedSprite2D(Context* context);
    /// Destruct.
    virtual ~AnimatedSprite2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Handle enabled/disabled state change.
    virtual void OnSetEnabled();

/// ENTITY/ANIMATION SETTERS

    /// Set animation set.
    void SetAnimationSet(AnimationSet2D* animationSet);
    /// Set entity name (skin name for spine, entity name for spriter).
    void SetEntity(const String& name);
    /// Set spriter entity by index
    void SetSpriterEntity(int index);
    /// Set animation by name and loop mode.
    void SetAnimation(const String& name = String::EMPTY, LoopMode2D loopMode = LM_DEFAULT);
    /// Set Spriter animation by index.
    void SetSpriterAnimation(int index=-1, LoopMode2D loopMode = LM_DEFAULT);
    /// Set loop mode.
    void SetLoopMode(LoopMode2D loopMode);
    /// Set speed.
    void SetSpeed(float speed);
    /// Set animation enable for rendering
    void SetRenderEnable(bool enable, int zindex=0);
    void SetDynamicBoundingBox(bool enable) { dynamicBBox_ = enable; }
    void SetCustomSpriteSheetAttr(const String& value);
    /// Set animation set attribute.
    void SetAnimationSetAttr(const ResourceRef& value);
    /// Set animation by name.
    void SetAnimationAttr(const String& name);
    /// Set sprite attribute.
//    void SetSpriteAttr(const ResourceRef& value);

    void SetLocalRotation(float angle);
    void SetLocalPosition(const Vector2& position);

    /// Reset variables
    virtual void CleanDependences();
    /// Reset Animation
    void ResetAnimation();

/// ENTITY/ANIMATION GETTERS

    /// Return animation Set.
    AnimationSet2D* GetAnimationSet() const;
    /// Return Spriter Animation by index or the current animation.
    Spriter::Animation* GetSpriterAnimation(int index=-1) const;
    /// Return Spriter Animation by Name
    Spriter::Animation* GetSpriterAnimation(const String& animationName) const;
    /// Return entity name.
	const String& GetEntity() const { return entityName_; }
    const String& GetEntityName() const { return entityName_; }
    /// Return num entities in the AnimationSet
    unsigned GetNumSpriterEntities() const;
    /// Return entity name by index.
    const String& GetSpriterEntity(int index) const;
    unsigned GetSpriterEntityIndex() const;

    /// Return animation name.
    bool HasAnimation(const String& name) const;
    const String& GetAnimation() const { return animationName_; }
    int GetAnimationIndex() const { return animationIndex_; }

    /// Return animation name by default.
    const String& GetDefaultAnimation() const;
    /// Return loop mode.
    LoopMode2D GetLoopMode() const { return loopMode_; }
    /// Return speed.
    float GetSpeed() const { return speed_; }

    /// Return time passed on the current animation.
    float GetCurrentAnimationTime() const;
    bool HasFinishedAnimation() const;

    /// Return SpriterInstance
    Spriter::SpriterInstance* GetSpriterInstance() const;

    void GetLocalSpritePositions(unsigned spriteindex, Vector2& position, float& angle, Vector2& scale);
    Sprite2D* GetSprite(unsigned spriteindex) const; //, bool fromInstance=false) const;

    /// Get Event Trigger Infos
    const EventTriggerInfo& GetEventTriggerInfo() const { return triggerInfo_; }

    /// Return animation set attribute.
    ResourceRef GetAnimationSetAttr() const;

    /// Return sprite attribute.
//    ResourceRef GetSpriteAttr() const;

    float GetLocalRotation() const;
    const Vector2& GetLocalPosition() const;

/// PHYSICAL NODES
    void AddPhysicalNode(Node* node);

/// CHARACTER MAPPING SETTERS

    void SetAppliedCharacterMapsAttr(const VariantVector& characterMapApplied);
    void SetCharacterMapAttr(const String& names);

    bool ApplyCharacterMap(const StringHash& hashname);
    bool ApplyCharacterMap(const String& name);

    bool ApplyColorMap(const StringHash& hashname);
    bool ApplyColorMap(const String& name);
    void SwapSprite(const StringHash& characterMap, Sprite2D* replacement, unsigned index=0, bool keepProportion=false);
    void SwapSprites(const StringHash& characterMap, const PODVector<Sprite2D*>& replacements, bool keepProportion=false);
    void SwapSprite(const String& characterMap, Sprite2D* replacement, unsigned index=0, bool keepProportion=false);
    void SwapSprites(const String& characterMap, const PODVector<Sprite2D*>& replacements, bool keepProportion=false);

    void SetSpriteColor(unsigned key, const Color& color);
    void UnSwapSprite(Sprite2D* original);
    void UnSwapAllSprites();

    void ResetCharacterMapping(bool resetSwappedSprites=true);

    void SetMappingScaleRatio(float ratio);

/// CHARACTER MAPPING GETTERS

//    String GetCharacterMapAttr() const;
    const VariantVector& GetAppliedCharacterMapsAttr() const;
    const PODVector<Spriter::CharacterMap*>& GetAppliedCharacterMaps() const { return characterMaps_; }
    const String& GetEmptyString() const { return String::EMPTY; }

    bool HasCharacterMapping() const;
    bool HasCharacterMap(const StringHash& hashname) const;
    bool HasCharacterMap(const String& name) const;

    bool IsCharacterMapApplied(const StringHash& hashname) const;
    bool IsCharacterMapApplied(const String& name) const;

    Spriter::CharacterMap* GetCharacterMap(const StringHash& characterMap) const;
    Sprite2D* GetCharacterMapSprite(const StringHash& characterMap, unsigned index=0) const;
    Spriter::CharacterMap* GetCharacterMap(const String& characterMap) const;
    Sprite2D* GetCharacterMapSprite(const String& characterMap, unsigned index=0) const;

    void GetMappedSprites(Spriter::CharacterMap* characterMap, PODVector<Sprite2D*>& sprites) const;
    Sprite2D* GetMappedSprite(unsigned key) const;
    Sprite2D* GetMappedSprite(int folderid, int fileid) const;
    Sprite2D* GetSwappedSprite(Sprite2D* original) const;
    Spriter::ColorMap* GetColorMap(const StringHash& hashname) const;
    Spriter::ColorMap* GetColorMap(const String& name) const;
    const Color& GetSpriteColor(unsigned key) const;
    const PODVector<SpriteInfo*>& GetSpriteInfos();
    const HashMap<unsigned, SharedPtr<Sprite2D> >& GetSpriteMapping() const { return spriteMapping_; }
    const HashMap<unsigned, Color >& GetSpriteColorMapping() const { return colorMapping_; }
    const HashMap<Sprite2D*, HashMap<Sprite2D*, SpriteInfo> >& GetSpriteSwapping() const { return spriteInfoMapping_; }

    float GetMappingScaleRatio() const { return mappingScaleRatio_; }

/// RENDER TARGET
    static void SetRenderTargetContext(Texture2D* texture=0, Viewport* viewport=0, Material* material=0);
    void SetRenderTargetAttr(const String& rttNodeParams);
    const String& GetRenderTargetAttr() const { return renderTargetParams_; }
    void SetRenderTargetFrom(const String& rttNodeParams, bool sendevent=false);
    void SetRenderTargetFrom(AnimatedSprite2D* otherAnimation);
    void SetRenderTarget(const String& scmlset, const String& customssheet, int textureEffects, bool sendevent=false);
    void SetRenderSprite(Sprite2D* sprite=0);
    void UpdateRenderTarget();
    Texture* GetRenderTexture() const;
    Sprite2D* GetRenderSprite() const { return renderSprite_; }
    AnimatedSprite2D* GetRenderTarget() const { return renderTarget_; }

/// RENDERED ANIMATIONS
    void ClearRenderedAnimations();
    AnimatedSprite2D* AddRenderedAnimation(const String& characterMapName, AnimationSet2D* animationSet, int textureFX);
    bool RemoveRenderedAnimation(const String& characterMapName);
    const PODVector<AnimatedSprite2D*>& GetRenderedAnimations() const { return renderedAnimations_; }

/// HELPERS

    void DumpSpritesInfos() const;

protected:
    /// Handle scene being assigned.
    virtual void OnSceneSet(Scene* scene);
    /// Handle update vertices.
    virtual void UpdateSourceBatches();

    virtual bool UpdateDrawRectangle();

    /// Handle scene post update.
    void HandleScenePostUpdate(StringHash eventType, VariantMap& eventData);

    /// Update animation.
    void UpdateAnimation(float timeStep);

    /// Update spriter triggers.
    void HideTriggers();
    void ClearTriggers(bool removeNode);
    void UpdateTriggers();
    /// Update spriter animation.
    void UpdateSpriterAnimation(float timeStep);

    inline void LocalToWorld(Spriter::SpatialTimelineKey* key, Vector2& position, float& rotation);

    void SetCustomSourceBatches(Vector<SourceBatch2D>* sourceBatches);

    void UpdateSourceBatchesSpriter(Vector<SourceBatch2D>* sourceBatches, bool resetBatches=true);
    void UpdateSourceBatchesSpriter_Custom(Vector<SourceBatch2D>* sourceBatches, int breakZIndex=-1, bool resetBatches=true);
    void UpdateSourceBatchesSpriter_RenderAnimations(Vector<SourceBatch2D>* sourceBatches);
    void UpdateSourceBatchesSpriter_RenderTarget();

    /// Dispose.
    void Dispose(bool removeNode=false);

    /// Character Maps
    bool ApplyCharacterMap(Spriter::CharacterMap* characterMap);
    bool ApplyColorMap(Spriter::ColorMap* colorMap);

    void SwapSprite(Sprite2D* original, Sprite2D* replacement, bool keepProportion=false);
    void SwapSprites(const PODVector<Sprite2D*>& originals, const PODVector<Sprite2D*>& replacements, bool keepProportion=false);

    SpriteInfo* GetSpriteInfo(unsigned key, Sprite2D* sprite, Sprite2D* origin);

    /// Speed.
    float speed_;
    /// Entity name.
    String entityName_;
    /// Animation set.
    SharedPtr<AnimationSet2D> animationSet_;
    /// Animation name.
    String animationName_;

    /// Local Positioning In Node
    float localRotation_;
    Vector2 localPosition_;

    /// Loop mode.
    LoopMode2D loopMode_;
    /// characterMap using
    bool useCharacterMap_;
    bool characterMapDirty_;
    bool renderEnabled_;
    bool dynamicBBox_;
    bool colorsDirty_;
	int renderZIndex_;
	unsigned firstKeyIndex_, stopKeyIndex_;
    float mappingScaleRatio_;

    /// Spriter instance.
    UniquePtr<Spriter::SpriterInstance> spriterInstance_;

    PODVector<StringHash> activedEventTriggers_;
    PODVector<Node* > updatedPhysicNodes_;
    Vector<WeakPtr<Node> > triggerNodes_;
    PODVector<AnimatedSprite2D* > renderedAnimations_;

    /// Spriter Batch Update
    PODVector<Spriter::SpriteTimelineKey* > spritesKeys_;
    PODVector<SpriteInfo*> spritesInfos_;

    /// Applied Character Maps.
    PODVector<Spriter::CharacterMap* > characterMaps_;
    VariantVector characterMapApplied_;

    PODVector<Spriter::ColorMap* > colorMaps_;
    VariantVector colorMapApplied_;

    /// Current Sprite Mapping (key = Spriter(folder-file) )
    HashMap<unsigned, SharedPtr<Sprite2D> > spriteMapping_;
    /// Color Sprite Mapping (key = Spriter(folder-file) )
    HashMap<unsigned, Color > colorMapping_;
    /// Swap Sprite Mapping
    HashMap<Sprite2D*, SharedPtr<Sprite2D> > swappedSprites_;
    /// Swap Sprite Mapping Info
    HashMap<Sprite2D*, HashMap<Sprite2D*, SpriteInfo> > spriteInfoMapping_;

    /// RENDERTARGET
    SharedPtr<Sprite2D> renderSprite_;
    WeakPtr<AnimatedSprite2D> renderTarget_;
    String renderTargetParams_;

    /// Trigger Infos
    EventTriggerInfo triggerInfo_;

    Vector<SourceBatch2D>* customSourceBatches_;
    int animationIndex_;

#ifdef URHO3D_SPINE
    /// Handle set spine animation.
    void SetSpineAnimation();
    /// Update spine animation.
    void UpdateSpineAnimation(float timeStep);
    /// Update vertices for spine animation;
    void UpdateSourceBatchesSpine();

    /// Skeleton.
    spSkeleton* skeleton_;
    /// Animation state data.
    spAnimationStateData* animationStateData_;
    /// Animation state.
    spAnimationState* animationState_;
#endif
};

}
