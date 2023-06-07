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

#include "../Graphics/Drawable.h"
#include "../Graphics/GraphicsDefs.h"

namespace Urho3D
{

class Drawable2D;
class Renderer2D;
class Texture2D;
class VertexBuffer;

enum TextureModeFlag
{
    TXM_UNIT = 0,
    TXM_FX,
    TXM_FX_LIT,
    TXM_FX_CROPALPHA,
    TXM_FX_BLUR,
    TXM_FX_FXAA,
    TXM_FX_TILEINDEX
};

/// 2D vertex.
#ifdef URHO3D_VULKAN
struct URHO3D_API Vertex2D
{
    /// Position.
    Vector2 position_;
    /// UV.
    Vector2 uv_;
    /// Color.
    unsigned color_;
    /// Position z
    float z_;
    /// Texture Id & Effect
    /// texture unit (bits 0..3)
    /// texture fx   (bits 4..31)
    unsigned texmode_;
    unsigned custom1_;
};
#else
struct URHO3D_API Vertex2D
{
    /// Position.
    Vector3 position_;
    /// Color.
    unsigned color_;
    /// UV.
    Vector2 uv_;
    /// Texture Id & Effect
	Vector4 texmode_;
};
#endif

/// 2D source batch.
struct URHO3D_API SourceBatch2D
{
    /// Construct.
    SourceBatch2D();

    /// Owner.
    WeakPtr<Drawable2D> owner_;
    /// Distance to camera.
    mutable float distance_;
    /// Draw order.
    int drawOrder_;
    /// Material.
    SharedPtr<Material> material_;
    /// Triangle or Quad Vertices ? (base 3 or 4)
    bool quadvertices_;
    /// Vertices.
    Vector<Vertex2D> vertices_;
};

/// Pixel size (equal 0.01f).
extern URHO3D_API const float PIXEL_SIZE;

/// Base class for 2D visible components.
class URHO3D_API Drawable2D : public Drawable
{
    URHO3D_OBJECT(Drawable2D, Drawable);

public:
    /// Construct.
    Drawable2D(Context* context);
    /// Destruct.
    ~Drawable2D();
    /// Register object factory. Drawable must be registered first.
    static void RegisterObject(Context* context);

    /// Handle enabled/disabled state change.
    virtual void OnSetEnabled();

    /// Set layer.
    void SetLayer(int layer);
    /// FromBones : 2 values for layer
    void SetLayer2(const IntVector2& layer);
    void SetLayerModifier(int layermodifier);
    /// Set order in layer.
    void SetOrderInLayer(int orderInLayer);

    void SetTextureFX(int effect) { textureFX_ = effect; }
    int GetTextureFX() const { return textureFX_; }

    static void SetTextureMode(TextureModeFlag flag, unsigned value, unsigned& texmode);
    static unsigned GetTextureMode(TextureModeFlag flag, unsigned texmode);
    static void SetTextureMode(TextureModeFlag flag, unsigned value, Vector4& texmode);
    static unsigned GetTextureMode(TextureModeFlag flag, const Vector4& texmode);

    /// Return layer.
    int GetLayer() const { return layer_.x_; }
    const IntVector2& GetLayer2() const { return layer_; }
    int GetLayerModifier() const { return layerModifier_; }
    /// Return order in layer.
    int GetOrderInLayer() const { return orderInLayer_; }

    const Rect& GetDrawRectangle();

    Renderer2D* GetRenderer() const { return renderer_; }

    virtual BoundingBox GetWorldBoundingBox2D();

    void MarkDirty();

    /// Return all source batches To Renderer (called by Renderer2D).
    virtual const Vector<SourceBatch2D* >& GetSourceBatchesToRender(Camera* camera);

    void ForceUpdateBatches();

    void ClearSourceBatches();

    /// FromBones : for WaterLayer being batched after ObjectTiled
    bool isSourceBatchedAtEnd_;
    /// Frombones : Debug facility
    bool enableDebugLog_;

protected:
    /// Handle scene being assigned.
    virtual void OnSceneSet(Scene* scene);
    /// Handle node transform being dirtied.
    virtual void OnMarkedDirty(Node* node);
    /// Handle draw order changed.
    virtual void OnDrawOrderChanged() = 0;

    /// Update source batches.
    virtual void UpdateSourceBatches() = 0;
    void UpdateSourceBatchesToRender(int id=0);
    virtual bool UpdateDrawRectangle();

    /// Return draw order by layer and order in layer.
    /// FromBones : id used for specific viewZ
    int GetDrawOrder(int id=0) const { return ((id == 0 ? layer_.x_ + layerModifier_ : layer_.y_) << 20) + (orderInLayer_ << 10); }

    /// Layer.
    IntVector2 layer_;
    int layerModifier_;

    /// Order in layer.
    int orderInLayer_;

    int textureFX_;

    /// DrawRect.
	Rect drawRect_;
    bool drawRectDirty_;

    /// Visibility.
	bool visibility_;

	/// FromBones : 2 sets of sourcebatches
    /// Prepared Internal Source batches.
    Vector<SourceBatch2D> sourceBatches_[2];
    /// Source batches to be rendered.
    Vector<SourceBatch2D* > sourceBatchesToRender_[2];

    /// Source batches dirty flag.
    bool sourceBatchesDirty_;

    /// Renderer2D.
    WeakPtr<Renderer2D> renderer_;
};

}
