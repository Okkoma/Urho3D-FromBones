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

namespace pugi
{
class xml_node;
class xml_document;
}

//#define USE_KEYPOOLS

namespace Urho3D
{

namespace Spriter
{

struct SpriterData;

struct File;
struct Folder;

struct Entity;
struct ObjInfo;
struct CharacterMap;
struct MapInstruction;
struct ColorMap;
struct ColorMapInstruction;
struct Animation;

struct Ref;
struct Timeline;
struct SpatialInfo;

struct TimeKey;
struct MainlineKey;
struct TimelineKey;

struct SpatialTimelineKey;
struct SpriteTimelineKey;
struct BoneTimelineKey;
struct BoxTimelineKey;

/// Object type.
enum ObjectType
{
    BONE = 0,
    SPRITE,
    POINT,
    BOX
};

/// Curve type.
enum CurveType
{
    INSTANT = 0,
    LINEAR,
    QUADRATIC,
    CUBIC,
    QUARTIC,
    QUINTIC,
    BEZIER
};

/// Spriter data.
struct URHO3D_API SpriterData
{
    SpriterData();
    ~SpriterData();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Load(const void* data, size_t size);
    bool Save(pugi::xml_document& document) const;
    void UpdateKeyInfos();

    static void Register();
    static float GetFactor(TimeKey* keyA, TimeKey* keyB, float length, float targetTime);
    static float AdjustTime(TimeKey* keyA, TimeKey* keyB, float length, float targetTime);

    int scmlVersion_;
    String generator_;
    String generatorVersion_;
    PODVector<Folder*> folders_;
    PODVector<Entity*> entities_;
};

#ifdef USE_KEYPOOLS
namespace KeyPool
{
    template< typename T > static void Restore()
    {
        T::freeindexes_.Resize(T::keypool_.Size());
        for (unsigned i=0; i < T::keypool_.Size(); i++)
            T::freeindexes_[i] = &T::keypool_[i];
    }
    template< typename T > static void Create(unsigned poolsize)
    {
        T::keypool_.Resize(poolsize);
        Restore<T>();
    }
    template< typename T > static T* Get()
    {
        if (!T::freeindexes_.Size())
        {
            URHO3D_LOGERRORF("No More Key for %s", T::GetStaticType());
            return 0;
        }
        else
        {
            T* key = T::freeindexes_.Back();
            T::freeindexes_.Pop();
            return key;
        }
    }
    template< typename T > static void Free(T* elt)
    {
        T::freeindexes_.Push(elt);
    }
}
#endif

/// Folder.
struct URHO3D_API Folder
{
    Folder();
    ~Folder();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    unsigned id_;
    String name_;
    PODVector<File*> files_;
};

/// File.
struct URHO3D_API File
{
    File(Folder* folder);
    ~File();

    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    Folder* folder_;
    unsigned id_;
    unsigned fx_;
    String name_;
    float width_;
    float height_;
    float pivotX_;
    float pivotY_;
};

/// Entity.
struct URHO3D_API Entity
{
    Entity();
    ~Entity();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    unsigned id_;
    String name_;
    Color color_;

    HashMap<StringHash, ObjInfo > objInfos_;
    PODVector<CharacterMap*> characterMaps_;
    PODVector<ColorMap*> colorMaps_;
    PODVector<Animation*> animations_;
};

/// Object Info.
struct URHO3D_API ObjInfo
{
    ObjInfo();
    ~ObjInfo();

    static bool Load(const pugi::xml_node& node, ObjInfo& objinfo);
    bool Save(pugi::xml_node& node) const;

    String name_;
    ObjectType type_;
    float width_;
    float height_;
    float pivotX_;
    float pivotY_;
};

/// Character map.
struct URHO3D_API CharacterMap
{
    CharacterMap();
    ~CharacterMap();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    MapInstruction* GetInstruction(unsigned key) const;

    bool GetTargetKey(unsigned key, int& targetfolder, int& targetfile) const;

    unsigned id_;
    String name_;
    StringHash hashname_;
    PODVector<MapInstruction*> maps_;
};

/// Map instruction.
struct URHO3D_API MapInstruction
{
    MapInstruction();
    ~MapInstruction();

    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    void SetOrigin(unsigned spritekey);
    void SetTarget(unsigned targetkey);
    void RemoveTarget();

    unsigned folder_;
    unsigned file_;
    int targetFolder_;
    int targetFile_;

    float targetdx_;
    float targetdy_;
    float targetdangle_;
};

/// Color map.
struct URHO3D_API ColorMap
{
    ColorMap();
    ~ColorMap();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    void SetColor(unsigned key, const Color& color);
    const Color& GetColor(unsigned key) const;

    int id_;
    String name_;
    StringHash hashname_;
    PODVector<ColorMapInstruction*> maps_;
};

/// Color Map instruction.
struct URHO3D_API ColorMapInstruction
{
    ColorMapInstruction();
    ~ColorMapInstruction();

    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    int folder_;
    int file_;
    Color color_;
};
/// Animation.
struct URHO3D_API Animation
{
    Animation();
    ~Animation();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    unsigned id_;
    String name_;
    float length_;
    bool looping_;
    PODVector<MainlineKey*> mainlineKeys_;
    PODVector<Timeline*> timelines_;
};

/// Ref.
struct Ref
{
    Ref();
    ~Ref();

    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    unsigned id_;
    int parent_;
    unsigned timeline_;
    unsigned key_;
    int zIndex_;

    Color color_;
    Vector2 offsetPosition_;
    float offsetAngle_;
};



/// Timeline.
struct URHO3D_API Timeline
{
    Timeline();
    ~Timeline();

    void Reset();
    bool Load(const pugi::xml_node& node);
    bool Save(pugi::xml_node& node) const;

    unsigned id_;
    String name_;
    StringHash hashname_;
    ObjectType objectType_;
    PODVector<SpatialTimelineKey*> keys_;
};


/// Spatial info.
struct URHO3D_API SpatialInfo
{
    float x_;
    float y_;
    float angle_;
    float scaleX_;
    float scaleY_;
    float alpha_;
    int spin;

    SpatialInfo(float x = 0.0f, float y = 0.0f, float angle = 0.0f, float scale_x = 1, float scale_y = 1, float a = 1, int sp = 1);
    void UnmapFromParent(const SpatialInfo& parentInfo);
    void Interpolate(const SpatialInfo& other, float t);
};


struct URHO3D_API TimeKey
{
    TimeKey();
    virtual ~TimeKey();

    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;

    float ApplyCurveType(float factor);
    float AdjustTime(float timeA, float timeB, float length, float targetTime);
    float GetFactor(float timeA, float timeB, float length, float targetTime);

    unsigned id_;
    float time_;
    CurveType curveType_;
    float c1_;
    float c2_;
    float c3_;
    float c4_;
};

/// Mainline key.
struct URHO3D_API MainlineKey : public TimeKey
{
    MainlineKey();
    virtual ~MainlineKey();

    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;

    void Reset();

    PODVector<Ref*> boneRefs_;
    PODVector<Ref*> objectRefs_;
};

/// Timeline key.
struct URHO3D_API TimelineKey : public TimeKey
{
    TimelineKey(Timeline* timeline);
    virtual ~TimelineKey();

    ObjectType GetObjectType() const { return timeline_->objectType_; }
    virtual TimelineKey* Clone() const = 0;
    virtual void Copy(TimelineKey* copy) const = 0;

    virtual void Interpolate(const TimelineKey& other, float t) = 0;
    TimelineKey& operator=(const TimelineKey& rhs);

    Timeline* timeline_;
};

/// Spatial timeline key.
struct URHO3D_API SpatialTimelineKey : TimelineKey
{
    SpatialInfo info_;

    SpatialTimelineKey(Timeline* timeline);
    virtual ~SpatialTimelineKey();

    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;
    virtual void Interpolate(const TimelineKey& other, float t);
    SpatialTimelineKey& operator=(const SpatialTimelineKey& rhs);
};

/// Bone timeline key.
struct URHO3D_API BoneTimelineKey : SpatialTimelineKey
{
//    float length_;
//    float width_;

    BoneTimelineKey();
    BoneTimelineKey(Timeline* timeline);
    virtual ~BoneTimelineKey();

    static const char* GetStaticType() { return "BoneTimelineKey"; }

    virtual TimelineKey* Clone() const;
    virtual void Copy(TimelineKey* copy) const;
    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;
    virtual void Interpolate(const TimelineKey& other, float t);
    BoneTimelineKey& operator=(const BoneTimelineKey& rhs);

#ifdef USE_KEYPOOLS
    static PODVector<BoneTimelineKey*> freeindexes_;
    static Vector<BoneTimelineKey> keypool_;
#endif
};

/// Sprite timeline key.
struct URHO3D_API SpriteTimelineKey : SpatialTimelineKey
{
    bool useDefaultPivot_;
    float pivotX_;
    float pivotY_;
    unsigned folderId_;
    unsigned fileId_;
    unsigned fx_;

    // Run time data.
    int zIndex_;
    Color color_;

    SpriteTimelineKey();
    SpriteTimelineKey(Timeline* timeline);
    virtual ~SpriteTimelineKey();

    static const char* GetStaticType() { return "SpriteTimelineKey"; }

    virtual TimelineKey* Clone() const;
    virtual void Copy(TimelineKey* copy) const;
    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;
    virtual void Interpolate(const TimelineKey& other, float t);
    SpriteTimelineKey& operator=(const SpriteTimelineKey& rhs);

#ifdef USE_KEYPOOLS
    static PODVector<SpriteTimelineKey*> freeindexes_;
    static Vector<SpriteTimelineKey> keypool_;
#endif
};

/// Box timeline key.
struct URHO3D_API BoxTimelineKey : SpatialTimelineKey
{
    bool useDefaultPivot_;
    float pivotX_;
    float pivotY_;
    float width_;
    float height_;

    BoxTimelineKey();
    BoxTimelineKey(Timeline* timeline);
    virtual ~BoxTimelineKey();

    static const char* GetStaticType() { return "BoxTimelineKey"; }

    virtual TimelineKey* Clone() const;
    virtual void Copy(TimelineKey* copy) const;
    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;
    virtual void Interpolate(const TimelineKey& other, float t);
    BoxTimelineKey& operator=(const BoxTimelineKey& rhs);

#ifdef USE_KEYPOOLS
    static PODVector<BoxTimelineKey*> freeindexes_;
    static Vector<BoxTimelineKey> keypool_;
#endif
};

/// Point timeline key.
struct URHO3D_API PointTimelineKey : SpatialTimelineKey
{
    // Run time data.
    int zIndex_;

    PointTimelineKey();
    PointTimelineKey(Timeline* timeline);
    virtual ~PointTimelineKey();

    static const char* GetStaticType() { return "PointTimelineKey"; }

    virtual TimelineKey* Clone() const;
    virtual void Copy(TimelineKey* copy) const;
    virtual bool Load(const pugi::xml_node& node);
    virtual bool Save(pugi::xml_node& node) const;
    virtual void Interpolate(const TimelineKey& other, float t);
    PointTimelineKey& operator=(const PointTimelineKey& rhs);

#ifdef USE_KEYPOOLS
    static PODVector<PointTimelineKey*> freeindexes_;
    static Vector<PointTimelineKey> keypool_;
#endif
};

}

}
