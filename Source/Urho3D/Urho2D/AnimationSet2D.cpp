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

#include "../Container/ArrayPtr.h"
#include "../Core/Context.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Texture2D.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Math/AreaAllocator.h"
#include "../Resource/Image.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Urho2D/AnimationSet2D.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/SpriterData2D.h"
#include "../Urho2D/SpriteSheet2D.h"

#include "../DebugNew.h"

#ifdef URHO3D_SPINE
#include <spine/spine.h>
#include <spine/Extension.h>
#include <spine/TextureLoader.h>
#endif

namespace Urho3D
{

#ifdef URHO3D_SPINE

class SpineTextureLoader : public Object, public spine::TextureLoader
{
    URHO3D_OBJECT(SpineTextureLoader, Object);

public :
    SpineTextureLoader(Context* context) : Object(context) { }
    virtual ~SpineTextureLoader() override { }

    virtual void load(spine::AtlasPage &page, const spine::String &path) override
    {
        Sprite2D* sprite = context_->GetSubsystem<ResourceCache>()->GetResource<Sprite2D>(path.buffer());
        // Add reference
        if (sprite)
            sprite->AddRef();

        page.width = sprite->GetTexture()->GetWidth();
        page.height = sprite->GetTexture()->GetHeight();
        page.texture = sprite;
    }

    virtual void unload(void *texture) override
    {
        Sprite2D* sprite = static_cast<Sprite2D*>(texture);
        if (sprite)
            sprite->ReleaseRef();
    }
};

/*
void _spAtlasPage_createTexture(spine::AtlasPage* self, const char* path)
{
    using namespace Urho3D;
    if (!currentAnimationSet)
        return;

    ResourceCache* cache = currentAnimationSet->GetSubsystem<ResourceCache>();
    Sprite2D* sprite = cache->GetResource<Sprite2D>(path);
    // Add reference
    if (sprite)
        sprite->AddRef();

    self->width = sprite->GetTexture()->GetWidth();
    self->height = sprite->GetTexture()->GetHeight();

    self->texture = sprite;
}

void _spAtlasPage_disposeTexture(spine::AtlasPage* self)
{
    using namespace Urho3D;
    Sprite2D* sprite = static_cast<Sprite2D*>(self->texture);
    if (sprite)
        sprite->ReleaseRef();

    self->texture = 0;
}

char* _spUtil_readFile(const char* path, int* length)
{
    using namespace Urho3D;

    if (!currentAnimationSet)
        return 0;

    ResourceCache* cache = currentAnimationSet->GetSubsystem<ResourceCache>();
    SharedPtr<File> file = cache->GetFile(path);
    if (!file)
        return 0;

    unsigned size = file->GetSize();

    char* data = spine::SpineExtension::calloc<char>(size + 1, __FILE__, __LINE__);
    file->Read(data, size);
    data[size] = '\0';

    file.Reset();
    *length = size;

    return data;
}
*/
#endif


String AnimationSet2D::customSpritesheetFile_;

AnimationSet2D::AnimationSet2D(Context* context) :
    Resource(context),
#ifdef URHO3D_SPINE
    skeletonData_(0),
    atlas_(0),
#endif
    hasSpriteSheet_(false)
{
    spriterFileSprites_.Clear();

    // Check has custom sprite sheet
    if (!customSpritesheetFile_.Empty())
    {
        if (GetSubsystem<ResourceCache>()->Exists(customSpritesheetFile_))
        {
            spriteSheetFilePath_ = customSpritesheetFile_;
            hasSpriteSheet_ = true;

//            URHO3D_LOGERRORF("AnimationSet2D : this=%u - Use custom spritesheet file=%s", this, customSpritesheetFile_.CString());
        }
        else
        {
            spriteSheetFilePath_.Clear();
            hasSpriteSheet_ = false;
            URHO3D_LOGERRORF("AnimationSet2D : this=%u - Could not find custom spritesheet file=%s", this, customSpritesheetFile_.CString());
        }
    }
}

AnimationSet2D::~AnimationSet2D()
{
    Dispose();
}

void AnimationSet2D::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimationSet2D>();
}

bool AnimationSet2D::BeginLoad(Deserializer& source)
{
    Dispose();

    if (GetName().Empty())
        SetName(source.GetName());

    String extension = GetExtension(source.GetName());
#ifdef URHO3D_SPINE
    if (extension == ".json")
        return BeginLoadSpine(source);
#endif
    if (extension == ".scml")
        return BeginLoadSpriter(source);

    URHO3D_LOGERROR("Unsupport animation set file: " + source.GetName());

    return false;
}

bool AnimationSet2D::EndLoad()
{
#ifdef URHO3D_SPINE
    if (jsonData_)
        return EndLoadSpine();
#endif
    if (spriterData_)
        return EndLoadSpriter();

    return false;
}

bool AnimationSet2D::Save(const String& fileName) const
{
    File file(context_, fileName, FILE_WRITE);

    return Save(file);
}

bool AnimationSet2D::Save(Serializer& dest) const
{
    bool ok = SaveSpriter(dest);

    return ok;
}

void AnimationSet2D::GetEntityObjectRefs(Spriter::Entity* entity, const String& name, const String& parentname, PODVector<Spriter::Ref*>& objrefs)
{
    // Find the ObjectRef with name and parentname in each animation
    for (PODVector<Spriter::Animation*>::ConstIterator it = entity->animations_.Begin(); it != entity->animations_.End(); ++it)
    {
        Spriter::Animation* animation = *it;
        for (PODVector<Spriter::MainlineKey*>::ConstIterator jt = animation->mainlineKeys_.Begin(); jt != animation->mainlineKeys_.End(); ++jt)
        {
            Spriter::MainlineKey* mkey = *jt;
            for (PODVector<Spriter::Ref*>::ConstIterator kt = mkey->objectRefs_.Begin(); kt != mkey->objectRefs_.End(); ++kt)
            {
                Spriter::Ref* ref = *kt;
                Spriter::Timeline* timeline = animation->timelines_[ref->timeline_];
                if (ref->parent_ != -1 && timeline->name_.StartsWith(name))
                {
                    Spriter::Ref* parentRef = mkey->boneRefs_[ref->parent_];
                    Spriter::Timeline* parentTimeline = animation->timelines_[parentRef->timeline_];
                    if (parentTimeline->name_.StartsWith(parentname))
                        objrefs.Push(ref);
                }
            }
        }
    }
}

void AnimationSet2D::SetEntityObjectRefAttr(const String& entityname, const String& name, const String& parentname, const Color& color, const Vector2& offset, float angle)
{
    // Find the entity
    Spriter::Entity* entity = 0;
    const PODVector<Spriter::Entity*>& entities = spriterData_->entities_;
    for (PODVector<Spriter::Entity*>::ConstIterator it = entities.Begin(); it != entities.End(); ++it)
    {
        if ((*it)->name_ == entityname)
        {
            entity = *it;
            break;
        }
    }

    if (!entity)
        return;

    PODVector<Spriter::Ref*> objRefs;
    GetEntityObjectRefs(entity, name, parentname, objRefs);

    SetObjectRefAttr(objRefs, color, offset, angle);
}

void AnimationSet2D::SetObjectRefAttr(const PODVector<Spriter::Ref*>& objrefs, const Color& color, const Vector2& offset, float angle)
{
    for (PODVector<Spriter::Ref*>::ConstIterator it = objrefs.Begin(); it != objrefs.End(); ++it)
    {
        Spriter::Ref* ref = *it;
        ref->offsetPosition_ = offset;
        ref->offsetAngle_ = angle;
        ref->color_ = color;
    }
}

unsigned AnimationSet2D::GetNumAnimations() const
{
#ifdef URHO3D_SPINE
    if (skeletonData_)
        return (unsigned)skeletonData_->getAnimations().size();
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return (unsigned)spriterData_->entities_[0]->animations_.Size();
    return 0;
}

static String stestspanimname_;
const String& AnimationSet2D::GetAnimation(unsigned index) const
{
    if (index >= GetNumAnimations())
        return String::EMPTY;

#ifdef URHO3D_SPINE
    if (skeletonData_)
    {
        stestspanimname_ = String(skeletonData_->getAnimations()[index]->getName().buffer());
        return stestspanimname_;
    }
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return spriterData_->entities_[0]->animations_[index]->name_;

    return String::EMPTY;
}

bool AnimationSet2D::HasAnimation(const String& animationName) const
{
#ifdef URHO3D_SPINE
    if (skeletonData_)
    {
        spine::String spAnimationName(animationName.CString(), true);
        bool find = skeletonData_->findAnimation(spAnimationName) != 0;
        spAnimationName.unown();
        return find;
    }
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
    {
        const PODVector<Spriter::Animation*>& animations = spriterData_->entities_[0]->animations_;
        for (unsigned i = 0; i < animations.Size(); ++i)
        {
            if (animationName == animations[i]->name_)
                return true;
        }
    }

    return false;
}

#ifdef URHO3D_SPINE
Sprite2D* AnimationSet2D::GetSpineSprite() const
{
    return spineSprite_;
}
#endif

Sprite2D* AnimationSet2D::GetSprite() const
{
    return sprite_;
}

Sprite2D* AnimationSet2D::GetSprite(const String& name) const
{
    return spriteSheet_->GetSprite(name);
}

Sprite2D* AnimationSet2D::GetSpriterFileSprite(int folderId, int fileId) const
{
    unsigned key = (folderId << 16) + fileId;
    HashMap<unsigned, SharedPtr<Sprite2D> >::ConstIterator i = spriterFileSprites_.Find(key);
    if (i != spriterFileSprites_.End())
        return i->second_;

    return 0;
}

Sprite2D* AnimationSet2D::GetSpriterFileSprite(unsigned key) const
{
    HashMap<unsigned, SharedPtr<Sprite2D> >::ConstIterator i = spriterFileSprites_.Find(key);
    if (i != spriterFileSprites_.End())
        return i->second_;

    return 0;
}

Sprite2D* AnimationSet2D::GetCharacterMapSprite(const Spriter::CharacterMap* characterMap, unsigned index) const
{
    if (!characterMap)
        return 0;

    Spriter::MapInstruction* map = characterMap->maps_[index];
    return GetSpriterFileSprite(map->targetFolder_, map->targetFile_);
}

void AnimationSet2D::GetCharacterMapSprites(const Spriter::CharacterMap* characterMap, PODVector<Sprite2D*>& sprites)
{
    if (!characterMap)
        return;

    const PODVector<Spriter::MapInstruction*>& map = characterMap->maps_;

    sprites.Clear();
    sprites.Resize(map.Size());

    for (unsigned i=0; i< map.Size(); ++i)
        sprites[i] = GetSpriterFileSprite(map[i]->targetFolder_, map[i]->targetFile_);
}

void AnimationSet2D::GetSpritesCharacterMapRef(Spriter::CharacterMap* characterMap, ResourceRefList& spriteRefList)
{

}

#ifdef URHO3D_SPINE
bool AnimationSet2D::BeginLoadSpine(Deserializer& source)
{
    URHO3D_LOGERRORF("AnimationSet2D : this=%u - BeginLoadSpine !", this, customSpritesheetFile_.CString());

    if (GetName().Empty())
        SetName(source.GetName());

    unsigned size = source.GetSize();
    jsonData_ = new char[size + 1];
    source.Read(jsonData_, size);
    jsonData_[size] = '\0';
    SetMemoryUse(size);
    return true;
}

bool AnimationSet2D::EndLoadSpine()
{
    String atlasFileName = ReplaceExtension(GetName(), ".atlas");

    SharedPtr<SpineTextureLoader> spineTextureLoader(new SpineTextureLoader(context_));

    atlas_ = new spine::Atlas(atlasFileName.CString(), spineTextureLoader.Get());
    if (!atlas_)
    {
        URHO3D_LOGERROR("Create spine atlas failed");
        return false;
    }

    int numAtlasPages = atlas_->getPages().size();
    if (numAtlasPages > 1)
    {
        URHO3D_LOGERROR("Only one page is supported in Urho3D");
        return false;
    }

    spineSprite_ = static_cast<Sprite2D*>(atlas_->getPages()[0]->texture);

    spine::SkeletonJson* skeletonJson = new spine::SkeletonJson(atlas_);
    if (!skeletonJson)
    {
        URHO3D_LOGERROR("Create skeleton Json failed");
        return false;
    }

    skeletonJson->setScale(0.01f); // PIXEL_SIZE;
    skeletonData_ = skeletonJson->readSkeletonData(&jsonData_[0]);

    delete skeletonJson;
    jsonData_.Reset();

    return true;
}
#endif

bool AnimationSet2D::BeginLoadSpriter(Deserializer& source)
{
    unsigned dataSize = source.GetSize();
    if (!dataSize && !source.GetName().Empty())
    {
        URHO3D_LOGERROR("Zero sized XML data in " + source.GetName());
        return false;
    }

    SharedArrayPtr<char> buffer(new char[dataSize]);
    if (source.Read(buffer.Get(), dataSize) != dataSize)
        return false;

    spriterData_ = new Spriter::SpriterData();
    if (!spriterData_->Load(buffer.Get(), dataSize))
    {
        URHO3D_LOGERROR("Could not spriter data from " + source.GetName());
        return false;
    }

    // Check has sprite sheet
    String parentPath = GetParentPath(GetName());
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    if (spriteSheetFilePath_.Empty())
    {
        String filename = parentPath + GetFileName(GetName());
        String extension = ".xml";

        hasSpriteSheet_ = cache->Exists(filename + extension);
        if (!hasSpriteSheet_)
        {
            extension = ".sjson";
            hasSpriteSheet_ = cache->Exists(filename + extension);

            if (!hasSpriteSheet_)
            {
                extension = ".plist";
                hasSpriteSheet_ = cache->Exists(filename + extension);
            }
        }

        if (hasSpriteSheet_)
        {
            spriteSheetFilePath_ = filename + extension;
        }
        else
        {
            URHO3D_LOGERRORF("AnimationSet2D : this=%u - Could not find spritesheet files=%s (xml, sjson, plist)", this, filename.CString());
        }
    }

    if (GetAsyncLoadState() == ASYNC_LOADING)
    {
        if (hasSpriteSheet_)
        {
            cache->BackgroundLoadResource<SpriteSheet2D>(spriteSheetFilePath_, true, this);
        }
        else
        {
            for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
            {
                Spriter::Folder* folder = spriterData_->folders_[i];
                for (unsigned j = 0; j < folder->files_.Size(); ++j)
                {
                    Spriter::File* file = folder->files_[j];
                    String imagePath = parentPath + file->name_;
                    cache->BackgroundLoadResource<Image>(imagePath, true, this);
                }
            }
        }
    }

    // Note: this probably does not reflect internal data structure size accurately
    SetMemoryUse(dataSize);

    return true;
}

bool AnimationSet2D::SaveSpriter(Serializer& dest) const
{
    XMLFile xmlfile(context_);
    pugi::xml_document* xmldoc = xmlfile.GetDocument();

    URHO3D_LOGINFOF("AnimationSet2D() - SaveSpriter ... xmldoc=%u", xmldoc);

    bool ok = spriterData_->Save(*xmldoc);

    URHO3D_LOGERROR(xmlfile.ToString());

    if (!ok)
    {
        URHO3D_LOGERROR("AnimationSet2D() - SaveSpriter ... Error 0 !");
        return false;
    }

    ok = xmlfile.Save(dest);
    if (!ok)
        URHO3D_LOGERROR("AnimationSet2D() - SaveSpriter ... Error !");

    return ok;
}

struct SpriterInfoFile
{
    int x;
    int y;
    Spriter::File* file_;
    SharedPtr<Image> image_;
};

bool AnimationSet2D::EndLoadSpriter()
{
    if (!spriterData_)
        return false;

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    if (hasSpriteSheet_)
    {
        spriteSheet_ = cache->GetResource<SpriteSheet2D>(spriteSheetFilePath_);
        if (!spriteSheet_)
            return false;

//        URHO3D_LOGERRORF("AnimationSet2D this=%u %s spritesheet=%s =>", this, GetName().CString(), spriteSheet_->GetName().CString());

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                unsigned key = (folder->id_ << 16) + file->id_;
                SharedPtr<Sprite2D>& sprite = spriterFileSprites_[key];
                sprite = spriteSheet_->GetSprite(GetFileName(file->name_));

                if (sprite)
                {
                    Vector2 hotSpot(file->pivotX_, file->pivotY_);

                    // If sprite is trimmed, recalculate hot spot
                    const IntVector2& offset = sprite->GetOffset();
                    if (offset != IntVector2::ZERO)
                    {
                        float pivotX = file->width_ * hotSpot.x_;
                        float pivotY = file->height_ * (1.0f - hotSpot.y_);

                        hotSpot.x_ = ((float)offset.x_ + pivotX) / sprite->GetSourceSize().x_;
                        hotSpot.y_ = 1.0f - ((float)offset.y_ + pivotY) / sprite->GetSourceSize().y_;
                    }

                    sprite->SetHotSpot(hotSpot);
//                    URHO3D_LOGINFOF("  -> %s", sprite->Dump().CString());
                }
                else
                    sprite.Reset();

//                URHO3D_LOGERRORF("... sprite=%s => mapping size=%u", sprite ? sprite->GetName().CString() : "null",  spriterFileSprites_.Size());
            }
        }
        URHO3D_LOGERRORF("AnimationSet2D this=%u %s spritesheet=%s ==> mapping size=%u", this, GetName().CString(), spriteSheet_->GetName().CString(), spriterFileSprites_.Size());
        sprite_ = spriteSheet_->GetSpriteMapping().Front().second_;
    }
    else
    {
        Vector<SpriterInfoFile> spriteInfos;
        String parentPath = GetParentPath(GetName());

//        URHO3D_LOGINFOF("no SpriteSheet =>");

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                String imagePath = parentPath + file->name_;
                SharedPtr<Image> image(cache->GetResource<Image>(imagePath));
                if (!image)
                {
                    URHO3D_LOGERROR("Could not load image");
                    return false;
                }
                if (image->IsCompressed())
                {
                    URHO3D_LOGERROR("Compressed image is not support");
                    return false;
                }
                if (image->GetComponents() != 4)
                {
                    URHO3D_LOGERROR("Only support image with 4 components");
                    return false;
                }

                SpriterInfoFile def;
                def.x = 0;
                def.y = 0;
                def.file_ = file;
                def.image_ = image;
                spriteInfos.Push(def);
            }
        }

        if (spriteInfos.Empty())
            return false;

        if (spriteInfos.Size() > 1)
        {
            URHO3D_LOGERRORF("AnimationSet2D() - EndLoadSpriter : create texture ...");

            AreaAllocator allocator(128, 128, 2048, 2048);
            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriterInfoFile& info = spriteInfos[i];
                Image* image = info.image_;
                if (!allocator.Allocate(image->GetWidth() + 1, image->GetHeight() + 1, info.x, info.y))
                {
                    URHO3D_LOGERROR("Could not allocate area");
                    return false;
                }
            }

            SharedPtr<Texture2D> texture(new Texture2D(context_));
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);
            texture->SetSize(allocator.GetWidth(), allocator.GetHeight(), Graphics::GetRGBAFormat());

            unsigned textureDataSize = allocator.GetWidth() * allocator.GetHeight() * 4;
            SharedArrayPtr<unsigned char> textureData(new unsigned char[textureDataSize]);
            memset(textureData.Get(), 0, textureDataSize);

            sprite_ = new Sprite2D(context_);
            sprite_->SetTexture(texture);

            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriterInfoFile& info = spriteInfos[i];
                Image* image = info.image_;
                //URHO3D_LOGINFOF("AnimationSet2D() - EndLoadSpriter : copy image %s to texture !", image->GetName().CString());

                for (int y = 0; y < image->GetHeight(); ++y)
                {
                    memcpy(textureData.Get() + ((info.y + y) * allocator.GetWidth() + info.x) * 4,
                        image->GetData() + y * image->GetWidth() * 4, image->GetWidth() * 4);
                }

                SharedPtr<Sprite2D> sprite(new Sprite2D(context_));
                sprite->SetName(image->GetName());
                sprite->SetTexture(texture);
                sprite->SetRectangle(IntRect(info.x, info.y, info.x + image->GetWidth(), info.y + image->GetHeight()));
                sprite->SetSourceSize(image->GetWidth(), image->GetHeight());
                sprite->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

//                URHO3D_LOGINFOF("  -> %s", sprite->Dump().CString());

                unsigned key = (info.file_->folder_->id_ << 16) + info.file_->id_;
                spriterFileSprites_[key] = sprite;
            }

            texture->SetData(0, 0, 0, allocator.GetWidth(), allocator.GetHeight(), textureData.Get());
            URHO3D_LOGINFOF("AnimationSet2D() - EndLoadSpriter : texture size=%ux%u!", allocator.GetWidth(), allocator.GetHeight());
        }
        else
        {
            SharedPtr<Texture2D> texture(new Texture2D(context_));
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);

            SpriterInfoFile& info = spriteInfos[0];
            texture->SetData(info.image_, true);

            sprite_ = new Sprite2D(context_);
            sprite_->SetTexture(texture);
            sprite_->SetRectangle(IntRect(info.x, info.y, info.x + info.image_->GetWidth(), info.y + info.image_->GetHeight()));
            sprite_->SetSourceSize(info.image_->GetWidth(), info.image_->GetHeight());
            sprite_->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

            unsigned key = (info.file_->folder_->id_ << 16) + info.file_->id_;
            spriterFileSprites_[key] = sprite_;
        }

        sprite_ = spriterFileSprites_.Front().second_;
    }

    return true;
}

void AnimationSet2D::Dispose()
{
#ifdef URHO3D_SPINE
    spineSprite_.Reset();

    if (skeletonData_)
    {
        delete skeletonData_;
        skeletonData_ = 0;
    }

    if (atlas_)
    {
        delete atlas_;
        atlas_ = 0;
    }
#endif

    sprite_.Reset();
    spriterData_.Reset();
    spriteSheet_.Reset();
    spriterFileSprites_.Clear();
}


}
