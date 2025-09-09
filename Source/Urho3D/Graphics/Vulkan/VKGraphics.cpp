//
// Copyright (c) 2008-2022 the Urho3D project.
// Copyright (c) 2022-2025 - Christophe VILLE.
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

#include "../../Precompiled.h"

#include "../../Core/Context.h"
#include "../../Core/Mutex.h"
#include "../../Core/ProcessUtils.h"
#include "../../Core/Profiler.h"
#include "../../Graphics/ConstantBuffer.h"
#include "../../Graphics/Graphics.h"
#include "../../Graphics/GraphicsEvents.h"
#include "../../Graphics/GraphicsImpl.h"
#include "../../Graphics/IndexBuffer.h"
#include "../../Graphics/RenderSurface.h"
#include "../../Graphics/Shader.h"
#include "../../Graphics/ShaderPrecache.h"
#include "../../Graphics/ShaderProgram.h"
#include "../../Graphics/ShaderVariation.h"
#include "../../Graphics/Texture2D.h"
#include "../../Graphics/TextureCube.h"
#include "../../Graphics/VertexBuffer.h"
#include "../../IO/File.h"
#include "../../IO/Log.h"
#include "../../Resource/ResourceCache.h"
#include "Graphics/Vulkan/VKGraphicsImpl.h"

#ifdef URHO3D_VOLK
#define VOLK_IMPLEMENTATION
#include <volk/volk.h>
#else
#include <vulkan/vulkan.h>
#endif

#include <SDL/SDL.h>
#include <SDL/SDL_vulkan.h>


#include "../../DebugNew.h"

#define ACTIVE_DESCRIPTOR_UPDATEANDBIND_NEW

namespace Urho3D
{

const Vector2 Graphics::pixelUVOffset(0.0f, 0.0f);
bool Graphics::gl3Support = false;

Graphics::Graphics(Context* context) :
    Object(context),
    impl_(new GraphicsImpl()),
    window_(0),
    externalWindow_(0),
    width_(0),
    height_(0),
    position_(SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED),
    multiSample_(1),
    fullscreen_(false),
    borderless_(false),
    resizable_(false),
    highDPI_(false),
    vsync_(false),
    monitor_(0),
    refreshRate_(0),
    tripleBuffer_(false),
    sRGB_(false),
    forceGL2_(false),
    instancingSupport_(false),
    lightPrepassSupport_(false),
    deferredSupport_(false),
    anisotropySupport_(false),
    dxtTextureSupport_(false),
    etcTextureSupport_(false),
    pvrtcTextureSupport_(false),
    hardwareShadowSupport_(false),
    sRGBSupport_(false),
    sRGBWriteSupport_(false),
    numPrimitives_(0),
    numBatches_(0),
    maxScratchBufferRequest_(0),
    dummyColorFormat_(0),
//    shadowMapFormat_(GL_DEPTH_COMPONENT16),
//    hiresShadowMapFormat_(GL_DEPTH_COMPONENT24),
//    defaultTextureFilterMode_(FILTER_TRILINEAR),
    defaultTextureAnisotropy_(4),
    shaderPath_("Shaders/Vulkan/"),
    orientations_("LandscapeLeft LandscapeRight"),
    lineWidth_(1.f),
    apiName_("VULKAN")
{
    SetTextureUnitMappings();
    ResetCachedState();

    context_->RequireSDL(SDL_INIT_VIDEO);

    impl_->graphics_ = this;

    // Register Graphics library object factories
    RegisterGraphicsLibrary(context_);
}

Graphics::~Graphics()
{
    Close();

    delete impl_;
    impl_ = 0;

    context_->ReleaseSDL();
}


bool Graphics::SetMode(int width, int height, bool fullscreen, bool borderless, bool resizable, bool highDPI, bool vsync, bool tripleBuffer, int multiSample, int monitor, int refreshRate)
{
    URHO3D_PROFILE(SetScreenMode);

    bool maximize = false;

    URHO3D_LOGDEBUGF("Graphics() - SetMode on monitor=%d ...", monitor);

#if defined(IOS) || defined(TVOS)
    // iOS and tvOS app always take the fullscreen (and with status bar hidden)
    fullscreen = true;
#endif

    // check video driver
    if (!SDL_GetCurrentVideoDriver())
    {
        URHO3D_LOGERRORF("Graphics() - api=%s no video driver !", GetApiName().CString());
        return false;
    }

    // check the number of video devices
    int numvideodisplays = SDL_GetNumVideoDisplays();
    if (numvideodisplays <= 0)
    {
        URHO3D_LOGERRORF("Graphics() - api=%s driver=%s no video display ... root cause: '%s'", GetApiName().CString(), SDL_GetCurrentVideoDriver(), SDL_GetError());
        return false;
    }

    // Make sure monitor index is not bigger than the currently detected monitors
    if (monitor >= numvideodisplays || monitor < 0)
        monitor = 0; // this monitor is not present, use first monitor

    // Fullscreen or Borderless can not be resizable
    if (fullscreen || borderless)
        resizable = false;

    // Borderless cannot be fullscreen, they are mutually exclusive
    if (borderless)
        fullscreen = false;

    multiSample = Clamp(multiSample, 1, 16);

    if (IsInitialized() && width == width_ && height == height_ && fullscreen == fullscreen_ && borderless == borderless_ &&
        resizable == resizable_ && vsync == vsync_ && tripleBuffer == tripleBuffer_ && multiSample == multiSample_ &&
        monitor == monitor_ && refreshRate == refreshRate_)
        return true;

    // If zero dimensions in windowed mode, set windowed mode to maximize and set a predefined default restored window size.
    // If zero in fullscreen, use desktop mode
    if (!width || !height)
    {
        if (fullscreen || borderless)
        {
            SDL_DisplayMode mode;
            SDL_GetDesktopDisplayMode(monitor, &mode);
            width = mode.w;
            height = mode.h;
        }
        else
        {
            maximize = resizable;
            width = 1024;
            height = 768;
        }
    }

    // Check fullscreen mode validity (desktop only). Use a closest match if not found
#ifdef DESKTOP_GRAPHICS
    if (fullscreen)
    {
        PODVector<IntVector3> resolutions = GetResolutions(monitor);
        if (resolutions.Size())
        {
            unsigned best = 0;
            unsigned bestError = M_MAX_UNSIGNED;

            for (unsigned i = 0; i < resolutions.Size(); ++i)
            {
                unsigned error = (unsigned)(Abs(resolutions[i].x_ - width) + Abs(resolutions[i].y_ - height));
                if (refreshRate != 0)
                    error += (unsigned)(Abs(resolutions[i].z_ - refreshRate));
                if (error < bestError)
                {
                    best = i;
                    bestError = error;
                }
            }

            width = resolutions[best].x_;
            height = resolutions[best].y_;
            refreshRate = resolutions[best].z_;
        }
    }
#endif

    // With an external window, only the size can change after initial setup, so do not recreate context
    if (!externalWindow_ || !impl_->GetInstance())
    {
#ifdef IOS
        // On iOS window needs to be resizable to handle orientation changes properly
        resizable = true;
#endif

        SDL_Rect display_rect;
        SDL_GetDisplayBounds(monitor, &display_rect);
        bool reposition = fullscreen || (borderless && width >= display_rect.w && height >= display_rect.h);

//        SDL_SetHint(SDL_HINT_ORIENTATIONS, orientations_.CString());

        if (!window_)
        {
            URHO3D_LOGINFOF("Graphics() - %s %s Try to create window with w=%d h=%d fullscreen=%s borderless=%s maximize=%s externalWindow_=%u...",
                             GetApiName().CString(), SDL_GetCurrentVideoDriver(), width, height,
                             fullscreen?"true":"false", borderless?"true":"false", maximize?"true":"false", externalWindow_);
            if (!externalWindow_)
            {
                unsigned flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;
                if (fullscreen)
                    flags |= SDL_WINDOW_FULLSCREEN;
                if (borderless)
                    flags |= SDL_WINDOW_BORDERLESS;
                if (resizable)
                    flags |= SDL_WINDOW_RESIZABLE;
                if (highDPI)
                    flags |= SDL_WINDOW_ALLOW_HIGHDPI;

                window_ = SDL_CreateWindow(windowTitle_.CString(), reposition ? display_rect.x : position_.x_, reposition ? display_rect.y : position_.y_, width, height, flags);
            }
            else
            {
    #ifndef __EMSCRIPTEN__
                window_ = SDL_CreateWindowFrom(externalWindow_, SDL_WINDOW_VULKAN);
                fullscreen = false;
    #endif
            }

            URHO3D_LOGDEBUGF("create window=%u ", window_);
        }

        if (!window_)
        {
            URHO3D_LOGERRORF("Graphics() - api=%s driver=%s Could not create window, root cause: '%s'", GetApiName().CString(), SDL_GetCurrentVideoDriver(), SDL_GetError());
            return false;
        }

        // Create Vulkan Instance
        if (!impl_->GetInstance())
        {
            Vector<String> requestedLayers;
            // Add Validation Layer
        #ifdef URHO3D_VULKAN_VALIDATION
            URHO3D_LOGINFOF("Graphics() - api=%s driver=%s using validation layers ...", GetApiName().CString(), SDL_GetCurrentVideoDriver());
            requestedLayers.Push("VK_LAYER_KHRONOS_validation");
        #endif

            if (!impl_->CreateVulkanInstance(context_, "URHO3D", window_, requestedLayers))
            {
                URHO3D_LOGERRORF("Graphics() - api=%s driver=%s Could not initialize Instance", GetApiName().CString(), SDL_GetCurrentVideoDriver());
                return false;
            }
        }

        // Reposition the window on the specified monitor
        if (reposition)
        {
            SDL_Rect display_rect;
            SDL_GetDisplayBounds(monitor, &display_rect);
            SDL_SetWindowPosition(window_, display_rect.x, display_rect.y);
        }

        CreateWindowIcon();

        if (maximize)
        {
            Maximize();
            SDL_Vulkan_GetDrawableSize(window_, &width, &height);
        }
    }

    if (!fullscreen)
    {
        if (SDL_SetWindowFullscreen(window_, 0) == 0)
        {
            SDL_SetWindowSize(window_, width, height);
            fullscreen_ = false;
        }
    }
    else
    {
        SDL_DisplayMode mode;
        mode.w = width;
        mode.h = height;
        mode.refresh_rate = refreshRate;
        SDL_SetWindowDisplayMode(window_, &mode);
        int fullscreenflag = SDL_WINDOW_FULLSCREEN;
        // allow fullscreen desktop with wayland
        if (strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0)
            fullscreenflag |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (SDL_SetWindowFullscreen(window_, fullscreenflag) != 0)
        {
            URHO3D_LOGERRORF("Graphics() - api=%s driver=%s Could not change to fullscreen, root cause: '%s'", GetApiName().CString(), SDL_GetCurrentVideoDriver(), SDL_GetError());
            return false;
        }

        fullscreen_ = true;
    }

    borderless_ = borderless;
    resizable_ = resizable;
    vsync_ = vsync;
    tripleBuffer_ = tripleBuffer;
    multiSample_ = multiSample;
    monitor_ = monitor;
    refreshRate_ = refreshRate;

    // Recreate the SwapChain
    //impl_->UpdateSwapChain(width, height, 0, &vsync_, &tripleBuffer_);
    URHO3D_LOGDEBUG("Graphics() - SetMode ...");

    impl_->UpdateSwapChain(width, height, &sRGB_, &vsync_, &tripleBuffer_);

    SDL_Vulkan_GetDrawableSize(window_, &width_, &height_);
    SDL_GetWindowPosition(window_, &position_.x_, &position_.y_);

    int logicalWidth, logicalHeight;
    SDL_GetWindowSize(window_, &logicalWidth, &logicalHeight);
    highDPI_ = (width_ != logicalWidth) || (height_ != logicalHeight);

    // Reset rendertargets and viewport for the new screen mode
    ResetRenderTargets();

    // Clear the initial window contents to black
    Clear(CLEAR_COLOR);

    CheckFeatureSupport();

#ifdef URHO3D_LOGGING
//    URHO3D_LOGINFOF("Graphics() - Adapter used %s %s", (const char *) glGetString(GL_VENDOR), (const char *) glGetString(GL_RENDERER));
    String msg;
    msg.AppendWithFormat("Graphics() - api=%s driver=%s Set screen mode %dx%d %s monitor %d", GetApiName().CString(), SDL_GetCurrentVideoDriver(), width_, height_, (fullscreen_ ? "fullscreen" : "windowed"), monitor_);
    if (borderless_)
        msg.Append(" borderless");
    if (resizable_)
        msg.Append(" resizable");
    if (highDPI_)
        msg.Append(" highDPI");
    if (multiSample > 1)
        msg.AppendWithFormat(" multisample %d", multiSample);
    URHO3D_LOGINFO(msg);
#endif

    using namespace ScreenMode;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_WIDTH] = width_;
    eventData[P_HEIGHT] = height_;
    eventData[P_FULLSCREEN] = fullscreen_;
    eventData[P_BORDERLESS] = borderless_;
    eventData[P_RESIZABLE] = resizable_;
    eventData[P_HIGHDPI] = highDPI_;
    eventData[P_MONITOR] = monitor_;
    eventData[P_REFRESHRATE] = refreshRate_;
    SendEvent(E_SCREENMODE, eventData);

    return true;
}

bool Graphics::SetMode(int width, int height)
{
    return SetMode(width, height, fullscreen_, borderless_, resizable_, highDPI_, vsync_, tripleBuffer_, multiSample_, monitor_, refreshRate_);
}

void Graphics::SetSRGB(bool enable)
{
#ifndef DISABLE_SRGB
    enable &= sRGBWriteSupport_;
#else
    enable = false;
#endif

    if (enable != sRGB_)
    {
        sRGB_ = enable;
        impl_->swapChainDirty_ = true;
        URHO3D_LOGERRORF("Graphics() - SetSRGB ...");
        impl_->UpdateSwapChain(width_, height_, &sRGB_);
    }
}

void Graphics::SetDither(bool enable)
{
    // unused on Vulkan
}

void Graphics::SetFlushGPU(bool enable)
{
    // unused on Vulkan
}

void Graphics::SetForceGL2(bool enable)
{
    // unused on Vulkan
}

void Graphics::Close()
{
    if (!IsInitialized())
        return;

    // Actually close the window
    Release(true, true);
}

bool Graphics::TakeScreenShot(Image& destImage)
{
//    URHO3D_PROFILE(TakeScreenShot);
//
//    if (!IsInitialized())
//        return false;
//
//    if (IsDeviceLost())
//    {
//        URHO3D_LOGERROR("Can not take screenshot while device is lost");
//        return false;
//    }
//
//    ResetRenderTargets();
//
//#ifndef GL_ES_VERSION_2_0
//    destImage.SetSize(width_, height_, 3);
//    glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, destImage.GetData());
//#else
//    // Use RGBA format on OpenGL ES, as otherwise (at least on Android) the produced image is all black
//    destImage.SetSize(width_, height_, 4);
//    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, destImage.GetData());
//#endif
//
//    // On OpenGL we need to flip the image vertically after reading
//    destImage.FlipVertical();

    return true;
}


bool Graphics::BeginFrame()
{
    if (!IsInitialized() || IsDeviceLost())
        return false;

    if (impl_->swapChainDirty_)
    {
        URHO3D_LOGERRORF("Graphics() - BeginFrame ...");
        impl_->UpdateSwapChain(width_, height_, &sRGB_);
    }

    // Acquire the next frame from the swapchain
    if (!impl_->AcquireFrame())
        return false;

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("-> Begin Frame=%u ...", impl_->currentFrame_);
#endif

    // Set default rendertarget and depth buffer
    ResetRenderTargets();

    // Begin Command Buffer

//    // If using an external window, check it for size changes, and reset screen mode if necessary
//    if (externalWindow_)
//    {
//        int width, height;
//
//        SDL_GL_GetDrawableSize(window_, &width, &height);
//        if (width != width_ || height != height_)
//            SetMode(width, height);
//    }
//
//    // Re-enable depth test and depth func in case a third party program has modified it
//    glEnable(GL_DEPTH_TEST);
//    glDepthFunc(glCmpFunc[depthTestMode_]);
//
//    // Set default rendertarget and depth buffer
//    ResetRenderTargets();
//
    // Cleanup textures from previous frame
    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
        SetTexture(i, 0);
//
//    // Enable color and depth write
//    SetColorWrite(true);
//    SetDepthWrite(true);
//
    numPrimitives_ = 0;
    numBatches_ = 0;

    SendEvent(E_BEGINRENDERING);

    return true;
}

void Graphics::EndFrame()
{
    if (!IsInitialized())
    {
		URHO3D_LOGERROR("Graphics - EndFrame() : Not initialized !");
		return;
	}

    URHO3D_PROFILE(Present);

    SendEvent(E_ENDRENDERING);

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("-> ... End Frame=%u !", impl_->currentFrame_);
#endif

    // Present / Swap
    impl_->PresentFrame();
}

void Graphics::Clear(unsigned flags, const Color& color, float depth, unsigned stencil)
{
    impl_->SetClearValue(color, depth, stencil);

#ifdef URHO3D_VULKAN_USE_SEPARATE_CLEARPASS
    PrepareDraw();
#endif
}

bool Graphics::ResolveToTexture(Texture2D* destination, const IntRect& viewport)
{
#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics() - ResolveToTexture : texture=%s(%u) viewport=%s !", destination->GetName().CString(), destination, viewport.ToString().CString());
#endif
//    if (!destination || !destination->GetRenderSurface())
//        return false;
//
//    URHO3D_PROFILE(ResolveToTexture);
//
//    IntRect vpCopy = viewport;
//    if (vpCopy.right_ <= vpCopy.left_)
//        vpCopy.right_ = vpCopy.left_ + 1;
//    if (vpCopy.bottom_ <= vpCopy.top_)
//        vpCopy.bottom_ = vpCopy.top_ + 1;
//    vpCopy.left_ = Clamp(vpCopy.left_, 0, width_);
//    vpCopy.top_ = Clamp(vpCopy.top_, 0, height_);
//    vpCopy.right_ = Clamp(vpCopy.right_, 0, width_);
//    vpCopy.bottom_ = Clamp(vpCopy.bottom_, 0, height_);
//
//    // Make sure the FBO is not in use
//    ResetRenderTargets();
//
//    // Use Direct3D convention with the vertical coordinates ie. 0 is top
//    SetTextureForUpdate(destination);
//    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vpCopy.left_, height_ - vpCopy.bottom_, vpCopy.Width(), vpCopy.Height());
//    SetTexture(0, 0);

    return true;
}

bool Graphics::ResolveToTexture(Texture2D* texture)
{
#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics() - ResolveToTexture : texture=%s(%u) !", texture->GetName().CString(), texture);
#endif
//#ifndef GL_ES_VERSION_2_0
//    if (!texture)
//        return false;
//    RenderSurface* surface = texture->GetRenderSurface();
//    if (!surface || !surface->GetRenderBuffer())
//        return false;
//
//    URHO3D_PROFILE(ResolveToTexture);
//
//    texture->SetResolveDirty(false);
//    surface->SetResolveDirty(false);
//
//    // Use separate FBOs for resolve to not disturb the currently set rendertarget(s)
//    if (!impl_->resolveSrcFBO_)
//        impl_->resolveSrcFBO_ = CreateFramebuffer();
//    if (!impl_->resolveDestFBO_)
//        impl_->resolveDestFBO_ = CreateFramebuffer();
//
//    if (!gl3Support)
//    {
//        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, impl_->resolveSrcFBO_);
//        glFramebufferRenderbufferEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT,
//            surface->GetRenderBuffer());
//        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, impl_->resolveDestFBO_);
//        glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture->GetGPUObjectName(),
//            0);
//        glBlitFramebufferEXT(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
//            GL_COLOR_BUFFER_BIT, GL_NEAREST);
//        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
//        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
//    }
//    else
//    {
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->resolveSrcFBO_);
//        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, surface->GetRenderBuffer());
//        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->resolveDestFBO_);
//        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->GetGPUObjectName(), 0);
//        glBlitFramebuffer(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
//            GL_COLOR_BUFFER_BIT, GL_NEAREST);
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
//        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//    }
//
//    // Restore previously bound FBO
//    BindFramebuffer(impl_->boundFBO_);
//    return true;
//#else
//    // Not supported on GLES
//    return false;
//#endif
    return true;
}

bool Graphics::ResolveToTexture(TextureCube* texture)
{
//#ifndef GL_ES_VERSION_2_0
//    if (!texture)
//        return false;
//
//    URHO3D_PROFILE(ResolveToTexture);
//
//    texture->SetResolveDirty(false);
//
//    // Use separate FBOs for resolve to not disturb the currently set rendertarget(s)
//    if (!impl_->resolveSrcFBO_)
//        impl_->resolveSrcFBO_ = CreateFramebuffer();
//    if (!impl_->resolveDestFBO_)
//        impl_->resolveDestFBO_ = CreateFramebuffer();
//
//    if (!gl3Support)
//    {
//        for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
//        {
//            // Resolve only the surface(s) that were actually rendered to
//            RenderSurface* surface = texture->GetRenderSurface((CubeMapFace)i);
//            if (!surface->IsResolveDirty())
//                continue;
//
//            surface->SetResolveDirty(false);
//            glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, impl_->resolveSrcFBO_);
//            glFramebufferRenderbufferEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT,
//                surface->GetRenderBuffer());
//            glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, impl_->resolveDestFBO_);
//            glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
//                texture->GetGPUObjectName(), 0);
//            glBlitFramebufferEXT(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
//                GL_COLOR_BUFFER_BIT, GL_NEAREST);
//        }
//
//        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, 0);
//        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
//    }
//    else
//    {
//        for (unsigned i = 0; i < MAX_CUBEMAP_FACES; ++i)
//        {
//            RenderSurface* surface = texture->GetRenderSurface((CubeMapFace)i);
//            if (!surface->IsResolveDirty())
//                continue;
//
//            surface->SetResolveDirty(false);
//            glBindFramebuffer(GL_READ_FRAMEBUFFER, impl_->resolveSrcFBO_);
//            glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, surface->GetRenderBuffer());
//            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, impl_->resolveDestFBO_);
//            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
//                texture->GetGPUObjectName(), 0);
//            glBlitFramebuffer(0, 0, texture->GetWidth(), texture->GetHeight(), 0, 0, texture->GetWidth(), texture->GetHeight(),
//                GL_COLOR_BUFFER_BIT, GL_NEAREST);
//        }
//
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
//        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
//    }
//
//    // Restore previously bound FBO
//    BindFramebuffer(impl_->boundFBO_);
//    return true;
//#else
//    // Not supported on GLES
    return false;
//#endif
}


void Graphics::Draw(PrimitiveType type, unsigned vertexStart, unsigned vertexCount)
{
    if (!vertexCount)
        return;

    impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_PRIMITIVE, (unsigned)type);

//    unsigned primitiveCount;
//    GLenum glPrimitiveType;
//
//    GetGLPrimitiveType(vertexCount, type, primitiveCount, glPrimitiveType);
//    glDrawArrays(glPrimitiveType, vertexStart, vertexCount);
//
//    numPrimitives_ += primitiveCount;
    ++numBatches_;

    SetIndexBuffer(0);
    //impl_->indexBufferDirty_ = false;

    PrepareDraw();

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics - Draw() ");
#endif
#if defined(DEBUG_VULKANCOMMANDS)
    URHO3D_LOGDEBUGF("vkCmdDraw               (pass:%d  sub:%d)", impl_->frame_->renderPassIndex_, impl_->frame_->subpassIndex_);
#endif
    vkCmdDraw(impl_->frame_->commandBuffer_, vertexCount, 1, vertexStart, 0);
}

void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject())
        return;

    impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_PRIMITIVE, (unsigned)type);

    PrepareDraw();

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics - Draw() indexed 1 ");
#endif
#if defined(DEBUG_VULKANCOMMANDS)    
    URHO3D_LOGDEBUGF("vkCmdDrawIndexed        (pass:%d  sub:%d)", impl_->frame_->renderPassIndex_, impl_->frame_->subpassIndex_);
#endif
    vkCmdDrawIndexed(impl_->frame_->commandBuffer_, indexCount, 1, indexStart, 0, 0);

//    unsigned indexSize = indexBuffer_->GetIndexSize();
//    unsigned primitiveCount;
//    GLenum glPrimitiveType;
//
//    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
//    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
//    glDrawElements(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize));
//
//    numPrimitives_ += primitiveCount;
    ++numBatches_;
}

void Graphics::Draw(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex, unsigned vertexCount)
{
    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObject())
        return;

    impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_PRIMITIVE, (unsigned)type);

    PrepareDraw();

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics - Draw() indexed 2 ");
#endif
#if defined(DEBUG_VULKANCOMMANDS)    
    URHO3D_LOGDEBUGF("vkCmdDrawIndexed        (pass:%d  sub:%d)", impl_->frame_->renderPassIndex_, impl_->frame_->subpassIndex_);
#endif
    vkCmdDrawIndexed(impl_->frame_->commandBuffer_, indexCount, 1, indexStart, baseVertexIndex, 0);

//    unsigned indexSize = indexBuffer_->GetIndexSize();
//    unsigned primitiveCount;
//    GLenum glPrimitiveType;
//
//    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
//    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
//    glDrawElementsBaseVertex(glPrimitiveType, indexCount, indexType, reinterpret_cast<GLvoid*>(indexStart * indexSize), baseVertexIndex);
//
//    numPrimitives_ += primitiveCount;
    ++numBatches_;
}

void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned minVertex, unsigned vertexCount,
    unsigned instanceCount)
{
//#if !defined(GL_ES_VERSION_2_0) || defined(__EMSCRIPTEN__)
//    if (!indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObjectName() || !instancingSupport_)
//        return;
//
//    PrepareDraw();
//
//    unsigned indexSize = indexBuffer_->GetIndexSize();
//    unsigned primitiveCount;
//    GLenum glPrimitiveType;
//
//    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
//    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
//#ifdef __EMSCRIPTEN__
//    glDrawElementsInstancedANGLE(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
//        instanceCount);
//#else
//    if (gl3Support)
//    {
//        glDrawElementsInstanced(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
//            instanceCount);
//    }
//    else
//    {
//        glDrawElementsInstancedARB(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
//            instanceCount);
//    }
//#endif
//
//    numPrimitives_ += instanceCount * primitiveCount;
//    ++numBatches_;
//#endif
}

void Graphics::DrawInstanced(PrimitiveType type, unsigned indexStart, unsigned indexCount, unsigned baseVertexIndex, unsigned minVertex,
        unsigned vertexCount, unsigned instanceCount)
{
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support || !indexCount || !indexBuffer_ || !indexBuffer_->GetGPUObjectName() || !instancingSupport_)
//        return;
//
//    PrepareDraw();
//
//    unsigned indexSize = indexBuffer_->GetIndexSize();
//    unsigned primitiveCount;
//    GLenum glPrimitiveType;
//
//    GetGLPrimitiveType(indexCount, type, primitiveCount, glPrimitiveType);
//    GLenum indexType = indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
//
//    glDrawElementsInstancedBaseVertex(glPrimitiveType, indexCount, indexType, reinterpret_cast<const GLvoid*>(indexStart * indexSize),
//        instanceCount, baseVertexIndex);
//
//    numPrimitives_ += instanceCount * primitiveCount;
//    ++numBatches_;
//#endif
}


void Graphics::SetVertexBuffer(VertexBuffer* buffer)
{
//    URHO3D_LOGDEBUGF("SetVertexBuffer");

    // Note: this is not multi-instance safe
    static PODVector<VertexBuffer*> vertexBuffers(1);
    vertexBuffers[0] = buffer;
    SetVertexBuffers(vertexBuffers);
}

bool Graphics::SetVertexBuffers(const PODVector<VertexBuffer*>& buffers, unsigned instanceOffset)
{
/*
    if (buffers.Size() > MAX_VERTEX_STREAMS)
    {
        URHO3D_LOGERROR("Too many vertex buffers");
        return false;
    }

    if (instanceOffset != impl_->lastInstanceOffset_)
    {
        impl_->lastInstanceOffset_ = instanceOffset;
        impl_->vertexBuffersDirty_ = true;
    }

    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
    {
        VertexBuffer* buffer = 0;
        if (i < buffers.Size())
            buffer = buffers[i];

        if (buffer != vertexBuffers_[i])
        {
            vertexBuffers_[i] = buffer;

        }
    }
*/

//    URHO3D_LOGDEBUGF("SetVertexBuffers");

    // Keep only the buffers that are not empty
    unsigned numVertexBuffers = 0;
    for (unsigned i=0; i < buffers.Size(); i++)
    {
        if (numVertexBuffers > MAX_VERTEX_STREAMS)
            break;

        VertexBuffer* buffer = buffers[i];
        if (!buffer || !buffer->GetGPUObject())
            continue;

        if (buffer != vertexBuffers_[numVertexBuffers])
        {
            vertexBuffers_[numVertexBuffers] = buffer;
            impl_->vertexBuffersDirty_ = true;
        }

        numVertexBuffers++;
    }

    // buffers have changed, update implementation side
    if (impl_->vertexBuffersDirty_)
    {
        impl_->vertexBuffers_.Resize(numVertexBuffers);
        impl_->vertexOffsets_.Resize(numVertexBuffers);
    }

    return true;
}

bool Graphics::SetVertexBuffers(const Vector<SharedPtr<VertexBuffer> >& buffers, unsigned instanceOffset)
{
    return SetVertexBuffers(reinterpret_cast<const PODVector<VertexBuffer*>&>(buffers), instanceOffset);
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (indexBuffer_ == buffer)
        return;

    indexBuffer_ = buffer;

    if (buffer)
        impl_->indexBufferDirty_ = true;
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    if (vs == vertexShader_ && ps == pixelShader_ && !impl_->viewportChanged_)
        return;

//    URHO3D_LOGDEBUGF("SetShader() ... Begin ...");

    if (vs != vertexShader_)
    {
        if (vs && !vs->GetByteCode().Size())
        {
            // Compile or Load Byte Code
            if (!vs->Create())
            {
                URHO3D_LOGERRORF("Failed to load vertex shader %s no bytecode !", vs->GetFullName().CString());
                vs = 0;
            }
        }

        vertexShader_ = vs;
    }

    if (ps != pixelShader_)
    {
        if (ps && !ps->GetByteCode().Size())
        {
            // Compile or Load Byte Code
            if (!ps->Create())
            {
                URHO3D_LOGERRORF("Failed to load pixel shader %s no bytecode !", ps->GetFullName().CString());
                ps = 0;
            }
        }

        pixelShader_ = ps;
    }

    // Update current shader parameters & constant buffers
    if (vertexShader_ && pixelShader_)
    {
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("SetShader() %s vs=%u %s ps=%u %s", vertexShader_->GetName().CString(), vertexShader_, vertexShader_->GetDefines().CString(), pixelShader_,pixelShader_->GetDefines().CString());
    #endif

        Pair<ShaderVariation*, ShaderVariation*> key = MakePair(vertexShader_, pixelShader_);
        ShaderProgramMap::ConstIterator it = impl_->shaderPrograms_.Find(key);
        if (it != impl_->shaderPrograms_.End())
        {
            impl_->shaderProgram_ = it->second_.Get();
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("SetShader() %s active program=%u", vs->GetName().CString(), impl_->shaderProgram_);
        #endif
        }
        else
        {
            URHO3D_LOGDEBUGF("SetShader() - new ShaderProgram");

            ShaderProgram* newProgram = impl_->shaderPrograms_[key] = new ShaderProgram(this, vertexShader_, pixelShader_);
            impl_->shaderProgram_ = newProgram;
        }

        for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
        {
            if (impl_->constantBuffers_[VS][i] != impl_->shaderProgram_->vsConstantBuffers_[i])
            {
                impl_->constantBuffers_[VS][i] = impl_->shaderProgram_->vsConstantBuffers_[i].Get();
                shaderParameterSources_[i] = (const void*)M_MAX_UNSIGNED;
            }

            if (impl_->constantBuffers_[PS][i] != impl_->shaderProgram_->psConstantBuffers_[i])
            {
                impl_->constantBuffers_[PS][i] = impl_->shaderProgram_->psConstantBuffers_[i].Get();
                shaderParameterSources_[i] = (const void*)M_MAX_UNSIGNED;
            }
        }
        impl_->pipelineDirty_ = true;
    }
    else
    {
        impl_->shaderProgram_ = 0;
    }

    // Store shader combination if shader dumping in progress
    if (shaderPrecache_)
        shaderPrecache_->StoreShaders(vertexShader_, pixelShader_);

    if (impl_->shaderProgram_)
    {
        SetShaderParameter(VSP_CLIPPLANE, useClipPlane_ ? clipPlane_ : Vector4(0.0f, 0.0f, 0.0f, 1.0f));
//        impl_->usedVertexAttributes_ = impl_->shaderProgram_->GetUsedVertexAttributes();
//        impl_->vertexAttributes_     = &impl_->shaderProgram_->GetVertexAttributes();
    }
    else
    {
//        impl_->usedVertexAttributes_ = 0;
//        impl_->vertexAttributes_     = 0;
    }

//    URHO3D_LOGDEBUGF("SetShader() ... End !");
}

void Graphics::SetShaderParameter(StringHash param, const float* data, unsigned count)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);

#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == VSP_VERTEXLIGHTS)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : VSP_VERTEXLIGHTS constantbuffer=%u ", buffer);
    else if (param == PSP_LIGHTCOLOR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_LIGHTCOLOR constantbuffer=%u ...", buffer);
#endif

    buffer->SetParameter(parameter.offset_, (unsigned)(count * sizeof(float)), data);
}

void Graphics::SetShaderParameter(StringHash param, float value)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);
    buffer->SetParameter(parameter.offset_, sizeof(float), &value);
}

void Graphics::SetShaderParameter(StringHash param, int value)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);

#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == VSP_NUMVERTEXLIGHTS)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : VSP_NUMVERTEXLIGHTS constantbuffer=%u ", buffer);
#endif
    buffer->SetParameter(parameter.offset_, sizeof(int), &value);
}

void Graphics::SetShaderParameter(StringHash param, bool value)
{
    // \todo Not tested
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);
    buffer->SetParameter(parameter.offset_, sizeof(bool), &value);
}

void Graphics::SetShaderParameter(StringHash param, const Color& color)
{
#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == PSP_LIGHTCOLOR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_LIGHTCOLOR color=%s ...", color.ToString().CString());
    else if (param == PSP_AMBIENTCOLOR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_AMBIENTCOLOR color=%s ...", color.ToString().CString());
    else if (param == PSP_MATDIFFCOLOR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_MATDIFFCOLOR color=%s ...", color.ToString().CString());
    else if (param == PSP_MATSPECCOLOR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_MATSPECCOLOR color=%s ...", color.ToString().CString());
#endif
    SetShaderParameter(param, color.Data(), 4);
}

void Graphics::SetShaderParameter(StringHash param, const Vector2& vector)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);
    buffer->SetParameter(parameter.offset_, sizeof(Vector2), &vector);
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3& matrix)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);
    buffer->SetVector3ArrayParameter(parameter.offset_, 3, &matrix);
}

void Graphics::SetShaderParameter(StringHash param, const Vector3& vector)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);

#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == PSP_LIGHTDIR)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_LIGHTDIR constantbuffer=%u ...", buffer);
#endif

    buffer->SetParameter(parameter.offset_, sizeof(Vector3), &vector);
}

void Graphics::SetShaderParameter(StringHash param, const Matrix4& matrix)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);
#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == VSP_VIEWPROJ)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : VSP_VIEWPROJ constantbuffer=%u matrix=%s", buffer, matrix.ToString().CString());
#endif
    buffer->SetParameter(parameter.offset_, sizeof(Matrix4), &matrix);
}

void Graphics::SetShaderParameter(StringHash param, const Vector4& vector)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);

#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == PSP_LIGHTPOS)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : PSP_LIGHTPOS constantbuffer=%u pos=%s...",
                         buffer, vector.ToString().CString());
#endif

    buffer->SetParameter(parameter.offset_, sizeof(Vector4), &vector);
}

void Graphics::SetShaderParameter(StringHash param, const Matrix3x4& matrix)
{
    if (!impl_->shaderProgram_)
        return;

    HashMap<StringHash, ShaderParameter>::Iterator it = impl_->shaderProgram_->parameters_.Find(param);
    if (it == impl_->shaderProgram_->parameters_.End())
        return;

    ShaderParameter& parameter = it->second_;
    ConstantBuffer* buffer = parameter.bufferPtr_;
    if (!buffer->IsDirty())
        impl_->dirtyConstantBuffers_.Push(buffer);

    // Expand to a full Matrix4
    static Matrix4 fullMatrix;
    fullMatrix.m00_ = matrix.m00_;
    fullMatrix.m01_ = matrix.m01_;
    fullMatrix.m02_ = matrix.m02_;
    fullMatrix.m03_ = matrix.m03_;
    fullMatrix.m10_ = matrix.m10_;
    fullMatrix.m11_ = matrix.m11_;
    fullMatrix.m12_ = matrix.m12_;
    fullMatrix.m13_ = matrix.m13_;
    fullMatrix.m20_ = matrix.m20_;
    fullMatrix.m21_ = matrix.m21_;
    fullMatrix.m22_ = matrix.m22_;
    fullMatrix.m23_ = matrix.m23_;

#ifdef ACTIVE_FRAMELOGDEBUG
    if (param == VSP_MODEL)
        URHO3D_LOGDEBUGF("Graphics - SetShaderParameter() : VSP_MODEL constantbuffer=%u program=%u matrix=%s",
                         buffer, impl_->shaderProgram_, fullMatrix.ToString().CString());
#endif

    buffer->SetParameter(parameter.offset_, sizeof(Matrix4), &fullMatrix);
}

bool Graphics::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
{
    if ((unsigned)(size_t)shaderParameterSources_[group] == M_MAX_UNSIGNED || shaderParameterSources_[group] != source)
    {
        shaderParameterSources_[group] = source;
        return true;
    }
    else
        return false;
}

bool Graphics::HasShaderParameter(StringHash param)
{
    return impl_->shaderProgram_ && impl_->shaderProgram_->parameters_.Find(param) != impl_->shaderProgram_->parameters_.End();
}

bool Graphics::HasTextureUnit(TextureUnit unit)
{
    // don't use textureUnit Slot with VULKAN.
    return true;//(vertexShader_ && vertexShader_->HasTextureUnit(unit)) || (pixelShader_ && pixelShader_->HasTextureUnit(unit));
}

void Graphics::ClearParameterSource(ShaderParameterGroup group)
{
    shaderParameterSources_[group] = (const void*)M_MAX_UNSIGNED;
}

void Graphics::ClearParameterSources()
{
    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
        shaderParameterSources_[i] = (const void*)M_MAX_UNSIGNED;
}

void Graphics::ClearTransformSources()
{
    shaderParameterSources_[SP_CAMERA] = (const void*)M_MAX_UNSIGNED;
    shaderParameterSources_[SP_OBJECT] = (const void*)M_MAX_UNSIGNED;
}

void Graphics::SetTexture(unsigned index, Texture* texture)
{
//    URHO3D_LOGDEBUGF("SetTexture ... index=%u texture=%s(%u) ...", index, texture ? texture->GetName().CString():"null", texture);

    if (index >= MAX_TEXTURE_UNITS)
        return;
//
//    // Check if texture is currently bound as a rendertarget. In that case, use its backup texture, or blank if not defined
//    if (texture)
//    {
//        if (renderTargets_[0] && renderTargets_[0]->GetParentTexture() == texture)
//            texture = texture->GetBackupTexture();
//        else
//        {
//            // Resolve multisampled texture now as necessary
//            if (texture->GetMultiSample() > 1 && texture->GetAutoResolve() && texture->IsResolveDirty())
//            {
//                if (texture->GetType() == Texture2D::GetTypeStatic())
//                    ResolveToTexture(static_cast<Texture2D*>(texture));
//                if (texture->GetType() == TextureCube::GetTypeStatic())
//                    ResolveToTexture(static_cast<TextureCube*>(texture));
//            }
//        }
//    }
//
    if (textures_[index] != texture)
    {
        for (unsigned i=0; i < impl_->numFrames_; i++)
            impl_->frames_[i].textureDirty_ = true;

        if (texture)
        {
//            unsigned glType = texture->GetTarget();
//            // Unbind old texture type if necessary
//            if (impl_->textureTypes_[index] && impl_->textureTypes_[index] != glType)
//                glBindTexture(impl_->textureTypes_[index], 0);
//            glBindTexture(glType, texture->GetGPUObjectName());
//            impl_->textureTypes_[index] = glType;
//
            if (texture->GetParametersDirty())
                texture->UpdateParameters();
            if (texture->GetLevelsDirty())
                texture->RegenerateLevels();
        }
//        else if (impl_->textureTypes_[index])
//        {
//            glBindTexture(impl_->textureTypes_[index], 0);
//            impl_->textureTypes_[index] = 0;
//        }
        textures_[index] = texture;

    #ifdef ACTIVE_FRAMELOGDEBUG
        if (texture)
            URHO3D_LOGDEBUGF("SetTexture ... unit=%u name=%s !", index, texture ? texture->GetName().CString():"null");
    #endif
    }
    else
    {
        if (texture && (texture->GetParametersDirty() || texture->GetLevelsDirty()))
        {
//            if (impl_->activeTexture_ != index)
//            {
//                glActiveTexture(GL_TEXTURE0 + index);
//                impl_->activeTexture_ = index;
//            }
//
//            glBindTexture(texture->GetTarget(), texture->GetGPUObjectName());
            if (texture->GetParametersDirty())
                texture->UpdateParameters();
            if (texture->GetLevelsDirty())
                texture->RegenerateLevels();
        }
    }
}

void Graphics::SetTextureForUpdate(Texture* texture)
{
#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics() - SetTextureForUpdate : texture=%s(%u) !", texture->GetName().CString(), texture);
#endif
//    if (impl_->activeTexture_ != 0)
//    {
//        glActiveTexture(GL_TEXTURE0);
//        impl_->activeTexture_ = 0;
//    }
//
//    unsigned glType = texture->GetTarget();
//    // Unbind old texture type if necessary
//    if (impl_->textureTypes_[0] && impl_->textureTypes_[0] != glType)
//        glBindTexture(impl_->textureTypes_[0], 0);
//    glBindTexture(glType, texture->GetGPUObjectName());
//    impl_->textureTypes_[0] = glType;
//    textures_[0] = texture;
}

void Graphics::SetDefaultTextureFilterMode(TextureFilterMode mode)
{
    if (mode != defaultTextureFilterMode_)
    {
        defaultTextureFilterMode_ = mode;
        SetTextureParametersDirty();
    }
}

void Graphics::SetDefaultTextureAnisotropy(unsigned level)
{
    level = Max(level, 1U);

    if (level != defaultTextureAnisotropy_)
    {
        defaultTextureAnisotropy_ = level;
        SetTextureParametersDirty();
    }
}

void Graphics::SetTextureParametersDirty()
{
//    MutexLock lock(gpuObjectMutex_);
//
//    for (PODVector<GPUObject*>::Iterator i = gpuObjects_.Begin(); i != gpuObjects_.End(); ++i)
//    {
//        Texture* texture = dynamic_cast<Texture*>(*i);
//        if (texture)
//            texture->SetParametersDirty();
//    }
}

void Graphics::ResetRenderTargets()
{
    for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        SetRenderTarget(i, (RenderSurface*)0);
    SetDepthStencil((RenderSurface*)0);
    SetViewport(IntRect(0, 0, width_, height_));
}

void Graphics::ResetRenderTarget(unsigned index)
{
    SetRenderTarget(index, (RenderSurface*)0);
}

void Graphics::SetRenderTarget(unsigned index, Texture2D* texture)
{
    RenderSurface* renderTarget = 0;
    if (texture)
        renderTarget = texture->GetRenderSurface();

    SetRenderTarget(index, renderTarget);
}

void Graphics::SetRenderTarget(unsigned index, RenderSurface* renderTarget)
{
    if (index >= MAX_RENDERTARGETS)
        return;

    if (renderTarget != renderTargets_[index])
    {
        renderTargets_[index] = renderTarget;

       // If the rendertarget is also bound as a texture, replace with backup texture or null
       if (renderTarget)
       {
           Texture* parentTexture = renderTarget->GetParentTexture();

           for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
           {
               if (textures_[i] == parentTexture)
                   SetTexture(i, textures_[i]->GetBackupTexture());
           }

           // If multisampled, mark the texture & surface needing resolve
           if (parentTexture->GetMultiSample() > 1 && parentTexture->GetAutoResolve())
           {
               parentTexture->SetResolveDirty(true);
               renderTarget->SetResolveDirty(true);
           }

           // If mipmapped, mark the levels needing regeneration
           if (parentTexture->GetLevels() > 1)
               parentTexture->SetLevelsDirty();
       }
       impl_->fboDirty_ = true;
    }
}

void Graphics::ResetDepthStencil()
{
//    SetDepthStencil((RenderSurface*)0);
}

void Graphics::SetDepthStencil(RenderSurface* depthStencil)
{
//    // If we are using a rendertarget texture, it is required in OpenGL to also have an own depth-stencil
//    // Create a new depth-stencil texture as necessary to be able to provide similar behaviour as Direct3D9
//    // Only do this for non-multisampled rendertargets; when using multisampled target a similarly multisampled
//    // depth-stencil should also be provided (backbuffer depth isn't compatible)
//    if (renderTargets_[0] && renderTargets_[0]->GetMultiSample() == 1 && !depthStencil)
//    {
//        int width = renderTargets_[0]->GetWidth();
//        int height = renderTargets_[0]->GetHeight();
//
//        // Direct3D9 default depth-stencil can not be used when rendertarget is larger than the window.
//        // Check size similarly
//        if (width <= width_ && height <= height_)
//        {
//            unsigned searchKey = (width << 16u) | height;
//            HashMap<unsigned, SharedPtr<Texture2D> >::Iterator i = impl_->depthTextures_.Find(searchKey);
//            if (i != impl_->depthTextures_.End())
//                depthStencil = i->second_->GetRenderSurface();
//            else
//            {
//                SharedPtr<Texture2D> newDepthTexture(new Texture2D(context_));
//                newDepthTexture->SetSize(width, height, GetDepthStencilFormat(), TEXTURE_DEPTHSTENCIL);
//                impl_->depthTextures_[searchKey] = newDepthTexture;
//                depthStencil = newDepthTexture->GetRenderSurface();
//            }
//        }
//    }
//
//    if (depthStencil != depthStencil_)
//    {
//        depthStencil_ = depthStencil;
//        impl_->fboDirty_ = true;
//    }
}

void Graphics::SetDepthStencil(Texture2D* texture)
{
//    RenderSurface* depthStencil = 0;
//    if (texture)
//        depthStencil = texture->GetRenderSurface();
//
//    SetDepthStencil(depthStencil);
}

void Graphics::SetViewport(const IntRect& rect, int index)
{
    // Use Direct3D convention with the vertical coordinates ie. 0 is top

    IntVector2 rtSize = GetRenderTargetDimensions();
    IntRect rectCopy = rect;

    if (rectCopy.right_ <= rectCopy.left_)
        rectCopy.right_  = rectCopy.left_ + 1;
    if (rectCopy.bottom_ <= rectCopy.top_)
        rectCopy.bottom_ = rectCopy.top_ + 1;

    viewport_.left_   = Clamp(rectCopy.left_, 0, rtSize.x_);
    viewport_.top_    = Clamp(rectCopy.top_, 0, rtSize.y_);
    viewport_.right_  = Clamp(rectCopy.right_, 0, rtSize.x_);
    viewport_.bottom_ = Clamp(rectCopy.bottom_, 0, rtSize.y_);

#ifdef ACTIVE_FRAMELOGDEBUG
    URHO3D_LOGDEBUGF("Graphics() - SetViewport : index=%d rect=%s rtsize=%s => viewport=%s", index, 
                    rect.ToString().CString(), rtSize.ToString().CString(), viewport_.ToString().CString());
#endif    
    impl_->SetViewport(index, viewport_);

    // Disable scissor test, needs to be re-enabled by the user
    SetScissorTest(false);
}

void Graphics::SetBlendMode(BlendMode mode, bool alphaToCoverage)
{
    // TEST
//    if (mode == BLEND_ADDALPHA)
//        mode = BLEND_ALPHA;

    if (mode != blendMode_)
    {
        blendMode_ = mode;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_BLENDMODE, mode);
    }

    if (alphaToCoverage != alphaToCoverage_)
    {
        alphaToCoverage_ = alphaToCoverage;
    }
}

void Graphics::SetColorWrite(bool enable)
{
    if (enable != colorWrite_)
    {
        colorWrite_ = enable;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_COLORMASK, enable ? 0xF : 0x0);
    }
}

void Graphics::SetCullMode(CullMode mode)
{
    if (mode != cullMode_)
    {
//        if (mode == CULL_NONE)
//            glDisable(GL_CULL_FACE);
//        else
//        {
//
//            glEnable(GL_CULL_FACE);
//            glCullFace(mode == CULL_CCW ? GL_FRONT : GL_BACK);
//        }

        // Use Direct3D convention, ie. clockwise vertices define a front face
        cullMode_ = mode;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_CULLMODE, mode);
    }
}

void Graphics::SetDepthBias(float constantBias, float slopeScaledBias)
{
    if (constantBias != constantDepthBias_ || slopeScaledBias != slopeScaledDepthBias_)
    {
        constantDepthBias_ = constantBias;
        slopeScaledDepthBias_ = slopeScaledBias;
        // Force update of the projection matrix shader parameter
        ClearParameterSource(SP_CAMERA);
    }
}

void Graphics::SetDepthTest(CompareMode mode)
{
    if (mode != depthTestMode_)
    {
        depthTestMode_ = mode;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_DEPTHTEST, mode);
//        URHO3D_LOGERRORF("depth test=%u reverse=%u", (unsigned)depthTestMode_,
//                         impl_->GetPipelineState(impl_->pipelineStates_, PIPELINESTATE_DEPTHTEST));
    }
}

void Graphics::SetDepthWrite(bool enable)
{
    if (enable != depthWrite_)
    {
        depthWrite_ = enable;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_DEPTHWRITE, enable);
//        URHO3D_LOGDEBUGF("depth write=%u reverse=%u", (unsigned)depthWrite_,
//                         impl_->GetPipelineState(impl_->pipelineStates_, STATE_DEPTHWRITE));
    }
}

void Graphics::SetFillMode(FillMode mode)
{
    if (mode != fillMode_)
    {
        fillMode_ = mode;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_FILLMODE, mode);
    }
}

void Graphics::SetLineAntiAlias(bool enable)
{
    if (enable != lineAntiAlias_)
    {
        lineAntiAlias_ = enable;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_SAMPLES, enable ? 2 : 0);
    }
}

void Graphics::SetLineWidth(float width)
{
    if (width != lineWidth_)
    {
        lineWidth_ = width;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_LINEWIDTH, GraphicsImpl::GetLineWidthIndex(lineWidth_));
    }
}

void Graphics::SetScissorTest(bool enable, const Rect& rect, bool borderInclusive)
{
    // During some light rendering loops, a full rect is toggled on/off repeatedly.
    // Disable scissor in that case to reduce state changes
    if (rect.min_.x_ <= 0.0f && rect.min_.y_ <= 0.0f && rect.max_.x_ >= 1.0f && rect.max_.y_ >= 1.0f)
        enable = false;

    IntRect intRect;

    if (enable)
    {
        IntVector2 rtSize(GetRenderTargetDimensions());
        IntVector2 viewSize(viewport_.Size());
        IntVector2 viewPos(viewport_.left_, viewport_.top_);

        int expand = borderInclusive ? 1 : 0;

        intRect.left_ = Clamp((int)((rect.min_.x_ + 1.0f) * 0.5f * viewSize.x_) + viewPos.x_, 0, rtSize.x_ - 1);
        intRect.top_ = Clamp((int)((-rect.max_.y_ + 1.0f) * 0.5f * viewSize.y_) + viewPos.y_, 0, rtSize.y_ - 1);
        intRect.right_ = Clamp((int)((rect.max_.x_ + 1.0f) * 0.5f * viewSize.x_) + viewPos.x_ + expand, 0, rtSize.x_);
        intRect.bottom_ = Clamp((int)((-rect.min_.y_ + 1.0f) * 0.5f * viewSize.y_) + viewPos.y_ + expand, 0, rtSize.y_);

        if (intRect.right_ == intRect.left_)
            intRect.right_++;
        if (intRect.bottom_ == intRect.top_)
            intRect.bottom_++;

        if (intRect.right_ < intRect.left_ || intRect.bottom_ < intRect.top_)
            enable = false;
    }

    if (enable)
    {
        if (scissorRect_ != intRect)
        {
            scissorRect_ = intRect;
            impl_->frameScissor_.offset = { intRect.left_, intRect.top_ };
            impl_->frameScissor_.extent = { (unsigned int)intRect.Width(), (unsigned int)intRect.Height() };
        }
    }
    else
    {
        scissorRect_ = IntRect::ZERO;
        impl_->frameScissor_ = impl_->screenScissor_;
    }

    scissorTest_ = enable;
}

void Graphics::SetScissorTest(bool enable, const IntRect& rect)
{
    IntRect intRect;

    if (enable)
    {
        IntVector2 rtSize(GetRenderTargetDimensions());
        IntVector2 viewPos(viewport_.left_, viewport_.top_);

        intRect.left_ = Clamp(rect.left_ + viewPos.x_, 0, rtSize.x_ - 1);
        intRect.top_ = Clamp(rect.top_ + viewPos.y_, 0, rtSize.y_ - 1);
        intRect.right_ = Clamp(rect.right_ + viewPos.x_, 0, rtSize.x_);
        intRect.bottom_ = Clamp(rect.bottom_ + viewPos.y_, 0, rtSize.y_);

        if (intRect.right_ == intRect.left_)
            intRect.right_++;
        if (intRect.bottom_ == intRect.top_)
            intRect.bottom_++;

        if (intRect.right_ < intRect.left_ || intRect.bottom_ < intRect.top_)
            enable = false;
    }

    if (enable)
    {
        if (scissorRect_ != intRect)
        {
            scissorRect_ = intRect;
            impl_->frameScissor_.offset = { intRect.left_, intRect.top_ };
            impl_->frameScissor_.extent = { (unsigned int)intRect.Width(), (unsigned int)intRect.Height() };
        }
    }
    else
    {
        scissorRect_ = IntRect::ZERO;
        impl_->frameScissor_ = impl_->screenScissor_;
    }

    scissorTest_ = enable;
}

void Graphics::SetClipPlane(bool enable, const Plane& clipPlane, const Matrix3x4& view, const Matrix4& projection)
{
    useClipPlane_ = enable;

    if (enable)
    {
        Matrix4 viewProj = projection * view;
        clipPlane_ = clipPlane.Transformed(viewProj).ToVector4();
        SetShaderParameter(VSP_CLIPPLANE, clipPlane_);
    }
}

void Graphics::SetStencilTest(bool enable, CompareMode mode, StencilOp pass, StencilOp fail, StencilOp zFail, unsigned stencilRef, unsigned compareMask, unsigned writeMask)
{
    if (enable != stencilTest_)
    {
        stencilTest_ = enable;
        impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_STENCILTEST, enable);
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("SetStencilTest %s stencilvalue=%u", stencilTest_ ? "true":"false", stencilRef);
    #endif

        if (!stencilTest_)
            impl_->stencilValue_ = 0;
    }

    if (enable)
    {
        if (mode != stencilTestMode_ || pass != stencilPass_ || fail != stencilFail_ || zFail != stencilZFail_)
        {
            stencilTestMode_ = mode;
            stencilPass_ = pass;
            stencilFail_ = fail;
            stencilZFail_ = zFail;
            impl_->SetPipelineState(impl_->pipelineStates_, PIPELINESTATE_STENCILMODE, StencilMode(mode, pass, fail, zFail));

        }

        if (stencilRef != stencilRef_)
        {
            stencilRef_ = stencilRef;
            impl_->stencilValue_ = stencilRef;
        }

        if (compareMask != stencilCompareMask_)
            stencilCompareMask_ = compareMask;

        if (writeMask != stencilWriteMask_)
            stencilWriteMask_ = writeMask;
    }
}

bool Graphics::IsInitialized() const
{
    return window_ != 0;
}

bool Graphics::GetDither() const
{
    return false;
}

bool Graphics::IsDeviceLost() const
{
//    // On iOS and tvOS treat window minimization as device loss, as it is forbidden to access OpenGL when minimized
//#if defined(IOS) || defined(TVOS)
//    if (window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) != 0)
//        return true;
//#endif
//    return impl_->context_ == 0;
    return 0;
}

PODVector<int> Graphics::GetMultiSampleLevels() const
{
    PODVector<int> ret;
//    // No multisampling always supported
//    ret.Push(1);
//
//#ifndef GL_ES_VERSION_2_0
//    int maxSamples = 0;
//    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
//    for (int i = 2; i <= maxSamples && i <= 16; i *= 2)
//        ret.Push(i);
//#endif

    return ret;
}

unsigned Graphics::GetFormat(CompressedFormat format) const
{
//    switch (format)
//    {
//    case CF_RGBA:
//        return GL_RGBA;
//
//    case CF_DXT1:
//        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : 0;
//
//#if !defined(GL_ES_VERSION_2_0) || defined(__EMSCRIPTEN__)
//    case CF_DXT3:
//        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : 0;
//
//    case CF_DXT5:
//        return dxtTextureSupport_ ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : 0;
//#endif
//#ifdef GL_ES_VERSION_2_0
//    case CF_ETC1:
//        return etcTextureSupport_ ? GL_ETC1_RGB8_OES : 0;
//
//    case CF_ETC2_RGB:
//        return etc2TextureSupport_ ? GL_ETC2_RGB8_OES : 0;
//
//    case CF_ETC2_RGBA:
//        return etc2TextureSupport_ ? GL_ETC2_RGBA8_OES : 0;
//
//    case CF_PVRTC_RGB_2BPP:
//        return pvrtcTextureSupport_ ? COMPRESSED_RGB_PVRTC_2BPPV1_IMG : 0;
//
//    case CF_PVRTC_RGB_4BPP:
//        return pvrtcTextureSupport_ ? COMPRESSED_RGB_PVRTC_4BPPV1_IMG : 0;
//
//    case CF_PVRTC_RGBA_2BPP:
//        return pvrtcTextureSupport_ ? COMPRESSED_RGBA_PVRTC_2BPPV1_IMG : 0;
//
//    case CF_PVRTC_RGBA_4BPP:
//        return pvrtcTextureSupport_ ? COMPRESSED_RGBA_PVRTC_4BPPV1_IMG : 0;
//#endif
//
//    default:
//        return 0;
//    }

    return 0;
}

unsigned Graphics::GetMaxBones()
{
//#ifdef RPI
//    // At the moment all RPI GPUs are low powered and only have limited number of uniforms
//    return 32;
//#else
//    return gl3Support ? 128 : 64;
//#endif

    return 0;
}

bool Graphics::GetGL3Support()
{
//    return gl3Support;
    return false;
}

ShaderVariation* Graphics::GetShader(ShaderType type, const String& name, const String& defines) const
{
    return GetShader(type, name.CString(), defines.CString());
}

ShaderVariation* Graphics::GetShader(ShaderType type, const char* name, const char* defines) const
{
    if (lastShaderName_ != name || !lastShader_)
    {
        ResourceCache* cache = GetSubsystem<ResourceCache>();

        String fullShaderName = shaderPath_ + name + shaderExtension_;

        // get existing shader in cache memory
        Shader* shader = (Shader*)cache->GetExistingResource(Shader::GetTypeStatic(), fullShaderName);
        if (!shader)
        {
            // if not in cache memory create a new one
            // we just need to create a Shader for ShaderVariation Storage (so don't load Shader File)

            shader = new Shader(context_);
            shader->SetName(fullShaderName);
            cache->AddManualResource(shader);

            URHO3D_LOGDEBUGF("GetShader : create manual resource shader this=%u %s !", shader, fullShaderName.CString(), defines);
        }

        lastShader_ = shader;
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("GetShader : shader %u %s defines=%s !", lastShader_.Get(), name, defines);
    #endif
        lastShaderName_ = name;
    }

    return lastShader_ ? lastShader_->GetVariation(type, defines) : (ShaderVariation*)0;
}

VertexBuffer* Graphics::GetVertexBuffer(unsigned index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers_[index] : 0;
    return 0;
}

ShaderProgram* Graphics::GetShaderProgram() const
{
    return impl_->shaderProgram_;
}

TextureUnit Graphics::GetTextureUnit(const String& name)
{
//    HashMap<String, TextureUnit>::Iterator i = textureUnits_.Find(name);
//    if (i != textureUnits_.End())
//        return i->second_;
//    else
//        return MAX_TEXTURE_UNITS;
    return MAX_TEXTURE_UNITS;
}

const String& Graphics::GetTextureUnitName(TextureUnit unit)
{
//    for (HashMap<String, TextureUnit>::Iterator i = textureUnits_.Begin(); i != textureUnits_.End(); ++i)
//    {
//        if (i->second_ == unit)
//            return i->first_;
//    }
    return String::EMPTY;
}

Texture* Graphics::GetTexture(unsigned index) const
{
//    return index < MAX_TEXTURE_UNITS ? textures_[index] : 0;
    return 0;
}

RenderSurface* Graphics::GetRenderTarget(unsigned index) const
{
    return index < MAX_RENDERTARGETS ? renderTargets_[index] : 0;
}

IntVector2 Graphics::GetRenderTargetDimensions() const
{
    int width, height;

    if (renderTargets_[0])
    {
        width = renderTargets_[0]->GetWidth();
        height = renderTargets_[0]->GetHeight();
    }
    else if (depthStencil_)
    {
        width = depthStencil_->GetWidth();
        height = depthStencil_->GetHeight();
    }
    else
    {
        width = width_;
        height = height_;
    }

    return IntVector2(width, height);
}

void Graphics::OnWindowResized()
{
    if (!window_)
        return;

    int newWidth, newHeight;

#if defined(__ANDROID__) || defined(ANDROID)
	impl_->surfaceDirty_ = true;
#endif

    SDL_GL_GetDrawableSize(window_, &newWidth, &newHeight);
    if (!impl_->surfaceDirty_ && newWidth == width_ && newHeight == height_)
        return;

    width_ = newWidth;
    height_ = newHeight;

    int logicalWidth, logicalHeight;
    SDL_GetWindowSize(window_, &logicalWidth, &logicalHeight);
    highDPI_ = (width_ != logicalWidth) || (height_ != logicalHeight);

//    // Reset rendertargets and viewport for the new screen size. Also clean up any FBO's, as they may be screen size dependent
//    CleanupFramebuffers();
//    ResetRenderTargets();
//
    if ((uint32_t)width_ != impl_->swapChainExtent_.width || (uint32_t)height_ != impl_->swapChainExtent_.height)
    {
        URHO3D_LOGERRORF("Graphics() - OnWindowResized ...");
        impl_->UpdateSwapChain(width_, height_, &sRGB_);

        URHO3D_LOGDEBUGF("Window was resized to %dx%d sRGB=%u", width_, height_, sRGB_);

        using namespace ScreenMode;

        VariantMap& eventData = GetEventDataMap();
        eventData[P_WIDTH] = width_;
        eventData[P_HEIGHT] = height_;
        eventData[P_FULLSCREEN] = fullscreen_;
        eventData[P_RESIZABLE] = resizable_;
        eventData[P_BORDERLESS] = borderless_;
        eventData[P_HIGHDPI] = highDPI_;
        SendEvent(E_SCREENMODE, eventData);
    }
}

void Graphics::OnWindowMoved()
{
    if (!window_ || fullscreen_)
        return;

    int newX, newY;

    SDL_GetWindowPosition(window_, &newX, &newY);
    if (newX == position_.x_ && newY == position_.y_)
        return;

    position_.x_ = newX;
    position_.y_ = newY;

    URHO3D_LOGDEBUGF("Window was moved to %d,%d", position_.x_, position_.y_);

    using namespace WindowPos;

    VariantMap& eventData = GetEventDataMap();
    eventData[P_X] = position_.x_;
    eventData[P_Y] = position_.y_;
    SendEvent(E_WINDOWPOS, eventData);
}

void Graphics::CleanupShaderPrograms(ShaderVariation* variation)
{
    for (ShaderProgramMap::Iterator i = impl_->shaderPrograms_.Begin(); i != impl_->shaderPrograms_.End();)
    {
        if (i->first_.first_ == variation || i->first_.second_ == variation)
            i = impl_->shaderPrograms_.Erase(i);
        else
            ++i;
    }

    if (vertexShader_ == variation || pixelShader_ == variation)
        impl_->shaderProgram_ = 0;
}

void Graphics::CleanupRenderSurface(RenderSurface* surface)
{
    // No-op on Vulkan
}

ConstantBuffer* Graphics::GetOrCreateConstantBuffer(ShaderType type, unsigned index, unsigned size)
{
//    // Note: shaderType parameter is not used on OpenGL, instead binding index should already use the PS range
//    // for PS constant buffers
//
//    unsigned key = (index << 16u) | size;
//    HashMap<unsigned, SharedPtr<ConstantBuffer> >::Iterator i = impl_->allConstantBuffers_.Find(key);
//    if (i == impl_->allConstantBuffers_.End())
//    {
//        i = impl_->allConstantBuffers_.Insert(MakePair(key, SharedPtr<ConstantBuffer>(new ConstantBuffer(context_))));
//        i->second_->SetSize(size);
//    }
//    return i->second_.Get();
    // Ensure that different shader types and index slots get unique buffers, even if the size is same
    unsigned key = (type << 30) | index;

    URHO3D_LOGDEBUGF("GetOrCreateConstantBuffer : key=%u ...", key);

    HashMap<unsigned, SharedPtr<ConstantBuffer> >::Iterator i = impl_->allConstantBuffers_.Find(key);
    if (i == impl_->allConstantBuffers_.End())
    {
        i = impl_->allConstantBuffers_.Insert(MakePair(key, SharedPtr<ConstantBuffer>(new ConstantBuffer(context_))));
        i->second_->SetSize(size);
        URHO3D_LOGDEBUGF("... new constantbuffer=%u created !", key, i->second_.Get());
    }
    return i->second_.Get();
}

void Graphics::Release(bool clearGPUObjects, bool closeWindow)
{
    if (!window_)
        return;

    URHO3D_LOGERRORF("Graphics - Release(%s, %s) ...", clearGPUObjects ? "true":"false", closeWindow ? "true":"false");

//    if (impl_->device_)
//        vkDeviceWaitIdle(impl_->device_);

    if (closeWindow)
    {
        impl_->CleanUpSwapChain();
    }

    {
        MutexLock lock(gpuObjectMutex_);

        if (clearGPUObjects)
        {
            // Shutting down: release all GPU objects that still exist
            // Shader programs are also GPU objects; clear them first to avoid list modification during iteration
            impl_->shaderPrograms_.Clear();

            for (PODVector<GPUObject*>::Iterator i = gpuObjects_.Begin(); i != gpuObjects_.End(); ++i)
                (*i)->Release();
            gpuObjects_.Clear();
        }
        else
        {
            // We are not shutting down, but recreating the context: mark GPU objects lost
            for (PODVector<GPUObject*>::Iterator i = gpuObjects_.Begin(); i != gpuObjects_.End(); ++i)
                (*i)->OnDeviceLost();

            // In this case clear shader programs last so that they do not attempt to delete their OpenGL program
            // from a context that may no longer exist
            impl_->shaderPrograms_.Clear();

            SendEvent(E_DEVICELOST);
        }
    }

    if (clearGPUObjects && closeWindow)
    {
        impl_->CleanUpVulkan();
    }
//
//    CleanupFramebuffers();
//    impl_->depthTextures_.Clear();
//
//    // End fullscreen mode first to counteract transition and getting stuck problems on OS X
//#if defined(__APPLE__) && !defined(IOS) && !defined(TVOS)
//    if (closeWindow && fullscreen_ && !externalWindow_)
//        SDL_SetWindowFullscreen(window_, 0);
//#endif
//

    if (closeWindow)
    {
        SDL_ShowCursor(SDL_TRUE);

        // Do not destroy external window except when shutting down
        if (!externalWindow_ || clearGPUObjects)
        {
            SDL_DestroyWindow(window_);
            window_ = 0;
        }
    }

    URHO3D_LOGERRORF("Graphics - Release() !");
}

void Graphics::Restore()
{
    if (!window_)
        return;

//#ifdef __ANDROID__
//    // On Android the context may be lost behind the scenes as the application is minimized
//    if (impl_->context_ && !SDL_GL_GetCurrentContext())
//    {
//        impl_->context_ = 0;
//        // Mark GPU objects lost without a current context. In this case they just mark their internal state lost
//        // but do not perform OpenGL commands to delete the GL objects
//        Release(false, false);
//    }
//#endif
//
//    // Ensure first that the context exists
//    if (!impl_->context_)
//    {
//        impl_->context_ = SDL_GL_CreateContext(window_);
//
//#ifndef GL_ES_VERSION_2_0
//        // If we're trying to use OpenGL 3, but context creation fails, retry with 2
//        if (!forceGL2_ && !impl_->context_)
//        {
//            forceGL2_ = true;
//            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
//            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
//            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
//            impl_->context_ = SDL_GL_CreateContext(window_);
//        }
//#endif
//
//#if defined(IOS) || defined(TVOS)
//        glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&impl_->systemFBO_);
//#endif
//
//        if (!impl_->context_)
//        {
//            URHO3D_LOGERRORF("Could not create OpenGL context, root cause '%s'", SDL_GetError());
//            return;
//        }
//
//        // Clear cached extensions string from the previous context
//        extensions.Clear();
//
//        // Initialize OpenGL extensions library (desktop only)
//#ifndef GL_ES_VERSION_2_0
//        GLenum err = glewInit();
//        if (GLEW_OK != err)
//        {
//            URHO3D_LOGERRORF("Could not initialize OpenGL extensions, root cause: '%s'", glewGetErrorString(err));
//            return;
//        }
//
//        if (!forceGL2_ && GLEW_VERSION_3_2)
//        {
//            gl3Support = true;
//            apiName_ = "GL3";
//
//            // Create and bind a vertex array object that will stay in use throughout
//            unsigned vertexArrayObject;
//            glGenVertexArrays(1, &vertexArrayObject);
//            glBindVertexArray(vertexArrayObject);
//        }
//        else if (GLEW_VERSION_2_0)
//        {
//            if (!GLEW_EXT_framebuffer_object || !GLEW_EXT_packed_depth_stencil)
//            {
//                URHO3D_LOGERROR("EXT_framebuffer_object and EXT_packed_depth_stencil OpenGL extensions are required");
//                return;
//            }
//
//            gl3Support = false;
//            apiName_ = "GL2";
//        }
//        else
//        {
//            URHO3D_LOGERROR("OpenGL 2.0 is required");
//            return;
//        }
//
//        // Enable seamless cubemap if possible
//        // Note: even though we check the extension, this can lead to software fallback on some old GPU's
//        // See https://github.com/urho3d/Urho3D/issues/1380 or
//        // http://distrustsimplicity.net/articles/gl_texture_cube_map_seamless-on-os-x/
//        // In case of trouble or for wanting maximum compatibility, simply remove the glEnable below.
//        if (gl3Support || GLEW_ARB_seamless_cube_map)
//            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
//#endif
//
//        // Set up texture data read/write alignment. It is important that this is done before uploading any texture data
//        glPixelStorei(GL_PACK_ALIGNMENT, 1);
//        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//        ResetCachedState();
//    }
//
    {
        MutexLock lock(gpuObjectMutex_);

        for (PODVector<GPUObject*>::Iterator i = gpuObjects_.Begin(); i != gpuObjects_.End(); ++i)
            (*i)->OnDeviceReset();
    }

    SendEvent(E_DEVICERESET);
}

void Graphics::MarkFBODirty()
{
    impl_->fboDirty_ = true;
}

void Graphics::SetVBO(unsigned index)
{
//    if (impl_->boundVBO_ == index)
//        return;
//
//    VertexBuffer* buffer = GetVertexBuffer(index);
//    if (buffer && buffer->GetGPUObject())
//    {
//        VkBuffer buffers[] = { (VkBuffer)buffer->GetGPUObject() };
//        VkDeviceSize offsets[] = { 0 };
//        vkCmdBindVertexBuffers(impl_->GetFrame().commandBuffer_, 0, 1, buffers, offsets);
//
//        impl_->boundVBO_ = index;
//    }
}

void Graphics::SetUBO(unsigned object)
{
//#ifndef GL_ES_VERSION_2_0
//    if (impl_->boundUBO_ != object)
//    {
//        if (object)
//            glBindBuffer(GL_UNIFORM_BUFFER, object);
//        impl_->boundUBO_ = object;
//    }
//#endif
}

unsigned Graphics::GetAlphaFormat()
{
    return VK_FORMAT_R8_UNORM;
}

unsigned Graphics::GetLuminanceFormat()
{
    return VK_FORMAT_R8_UNORM;
}

unsigned Graphics::GetLuminanceAlphaFormat()
{
    return VK_FORMAT_R8G8_UNORM;
}

unsigned Graphics::GetRGBFormat()
{
    return VK_FORMAT_R8G8B8_UNORM;
}

unsigned Graphics::GetRGBAFormat()
{
    return VK_FORMAT_R8G8B8A8_UNORM;
}

unsigned Graphics::GetRGBA16Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RGBA16;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R16G16B16A16_SFLOAT;

}

unsigned Graphics::GetRGBAFloat16Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RGBA16F_ARB;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R16G16B16A16_SFLOAT;
}

unsigned Graphics::GetRGBAFloat32Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RGBA32F_ARB;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R32G32B32A32_SFLOAT;
}

unsigned Graphics::GetRG16Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RG16;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R16G16_SFLOAT;
}

unsigned Graphics::GetRGFloat16Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RG16F;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R16G16_SFLOAT;
}

unsigned Graphics::GetRGFloat32Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_RG32F;
//#else
//    return GL_RGBA;
//#endif
    return VK_FORMAT_R32G32_SFLOAT;
}

unsigned Graphics::GetFloat16Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_R16F;
//#else
//    return GL_LUMINANCE;
//#endif
    return VK_FORMAT_R16_SFLOAT;
}

unsigned Graphics::GetFloat32Format()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_R32F;
//#else
//    return GL_LUMINANCE;
//#endif
    return VK_FORMAT_R32_SFLOAT;
}

unsigned Graphics::GetLinearDepthFormat()
{
//#ifndef GL_ES_VERSION_2_0
//    // OpenGL 3 can use different color attachment formats
//    if (gl3Support)
//        return GL_R32F;
//#endif
//    // OpenGL 2 requires color attachments to have the same format, therefore encode deferred depth to RGBA manually
//    // if not using a readable hardware depth texture
//    return GL_RGBA;
    return VK_FORMAT_R32_SFLOAT;
}

unsigned Graphics::GetDepthStencilFormat()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_DEPTH24_STENCIL8_EXT;
//#else
//    return glesDepthStencilFormat;
//#endif
    return (unsigned)GraphicsImpl::GetDepthStencilFormat();
}

unsigned Graphics::GetReadableDepthFormat()
{
//#ifndef GL_ES_VERSION_2_0
//    return GL_DEPTH_COMPONENT24;
//#else
//    return glesReadableDepthFormat;
//#endif
    return (unsigned)GraphicsImpl::GetDepthStencilFormat();
}

unsigned Graphics::GetFormat(const String& formatName)
{
    String nameLower = formatName.ToLower().Trimmed();

    if (nameLower == "a")
        return GetAlphaFormat();
    if (nameLower == "l")
        return GetLuminanceFormat();
    if (nameLower == "la")
        return GetLuminanceAlphaFormat();
    if (nameLower == "rgb")
        return GetRGBFormat();
    if (nameLower == "rgba")
        return GetRGBAFormat();
    if (nameLower == "rgba16")
        return GetRGBA16Format();
    if (nameLower == "rgba16f")
        return GetRGBAFloat16Format();
    if (nameLower == "rgba32f")
        return GetRGBAFloat32Format();
    if (nameLower == "rg16")
        return GetRG16Format();
    if (nameLower == "rg16f")
        return GetRGFloat16Format();
    if (nameLower == "rg32f")
        return GetRGFloat32Format();
    if (nameLower == "r16f")
        return GetFloat16Format();
    if (nameLower == "r32f" || nameLower == "float")
        return GetFloat32Format();
    if (nameLower == "lineardepth" || nameLower == "depth")
        return GetLinearDepthFormat();
    if (nameLower == "d24s8")
        return GetDepthStencilFormat();
    if (nameLower == "readabledepth" || nameLower == "hwdepth")
        return GetReadableDepthFormat();
//
    return GetRGBFormat();
}

void Graphics::CheckFeatureSupport()
{
    sRGBWriteSupport_ = false;

    const PhysicalDeviceInfo& physicalDeviceInfo = GraphicsImpl::GetPhysicalDeviceInfo();
    for (unsigned int i=0; i < physicalDeviceInfo.surfaceFormats_.Size(); i++)
    {
        const VkSurfaceFormatKHR& availableFormat = physicalDeviceInfo.surfaceFormats_[i];
        if ((availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB || availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB)
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            sRGBWriteSupport_ = true;
            break;
        }
    }

    sRGBSupport_ = sRGBWriteSupport_;
}

void Graphics::PrepareDraw()
{
    if (!impl_->frame_)
    {
        URHO3D_LOGERRORF("Graphics() - PrepareDraw ... no frame !");
        return;
    }

    FrameData& frame = *impl_->frame_;
    impl_->viewportIndex_   = Max(0, impl_->viewportIndex_);
    
#if defined(ACTIVE_FRAMELOGDEBUG) || defined(DEBUG_VULKANCOMMANDS)
    URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... frame=%u ... pipelineDirty=%s textureDirty=%s frameRenderPassIndex=(%d,%d) implRenderPassIndex=(%d,%d) viewportIndex_(impl:%d,frame:%d)",
                     impl_->GetFrameIndex(), impl_->pipelineDirty_ ? "true":"false",
                     frame.textureDirty_ && textures_[0] ? (!textures_[0]->GetName().Empty() ? textures_[0]->GetName().CString() : "noname") : "false",
                     frame.renderPassIndex_, frame.subpassIndex_, impl_->renderPassIndex_, impl_->subpassIndex_, impl_->viewportIndex_, frame.viewportIndex_);
#endif

    // End of the current renderpass.
    if (frame.renderPassBegun_ && frame.renderPassIndex_ != -1 &&
		(frame.renderPassIndex_ != impl_->renderPassIndex_ || frame.viewportIndex_ != impl_->viewportIndex_))
    {
        // Execute all remaining subpasses to ensure the correct transition of the attachment layouts
        const unsigned remaingSubpasses = impl_->renderPathData_->passInfos_[frame.renderPassIndex_]->subpasses_.Size() - 1;
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... Render Pass End : subpassindex=%d remain=%d", frame.subpassIndex_, remaingSubpasses);
    #endif

        while (frame.subpassIndex_ < remaingSubpasses)
        {
            frame.subpassIndex_++;
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdNextSubpass        (finish prev pass)(pass:%u  sub:%u)", frame.renderPassIndex_, frame.subpassIndex_);
        #endif
            vkCmdNextSubpass(frame.commandBuffer_, VK_SUBPASS_CONTENTS_INLINE);
        }
    #if defined(DEBUG_VULKANCOMMANDS)
        URHO3D_LOGDEBUGF("vkCmdEndRenderPass      (finish prev pass)(pass:%u)", frame.renderPassIndex_);
    #endif
        vkCmdEndRenderPass(frame.commandBuffer_);
        frame.renderPassBegun_ = false;
    }

	// Begin command recording.
	if (!frame.commandBufferBegun_)
	{
    #ifdef ACTIVE_FRAMELOGDEBUG
	    URHO3D_LOGDEBUG("Graphics() - PrepareDraw ... Command Buffer Not Begin => Begin !");
    #endif
    #if defined(DEBUG_VULKANCOMMANDS)
        URHO3D_LOGDEBUGF("vkBeginCommandBuffer    (pass:%u)", frame.renderPassIndex_);
    #endif
	    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult result = vkBeginCommandBuffer(frame.commandBuffer_, &beginInfo);
        frame.commandBufferBegun_ = true;
	}

#ifdef URHO3D_VULKAN_BEGINFRAME_WITH_CLEARPASS
    // start with a clear pass on the acquired image
    if (frame.renderPassIndex_ == -1)
    {
        VkClearValue* cval = &impl_->clearColor_;//&impl_->renderPathData_->passInfos_.Front()->clearValues_[0];
        VkRenderPassBeginInfo renderPassBI{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBI.renderPass         = impl_->renderPathData_->passInfos_.Front()->renderPass_;
        renderPassBI.framebuffer        = frame.framebuffers_.Front();
        renderPassBI.renderArea.offset  = { 0, 0 };
        renderPassBI.renderArea.extent  = impl_->swapChainExtent_;
        renderPassBI.clearValueCount    = 1;
        renderPassBI.pClearValues       = cval;

    #if defined(DEBUG_VULKANCOMMANDS)
        URHO3D_LOGDEBUGF("vkCmdBeginRenderPass    (beginframe with clearpass color=%F,%F,%F,%F)(pass:%d)", 
            cval->color.float32[0], cval->color.float32[1], cval->color.float32[2], cval->color.float32[3], frame.renderPassIndex_);
        URHO3D_LOGDEBUGF("vkCmdEndRenderPass      (pass:%d)", frame.renderPassIndex_);
    #endif
        vkCmdBeginRenderPass(frame.commandBuffer_, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(frame.commandBuffer_);
    }
#endif

	// Begin the next renderpass.
    if (frame.renderPassIndex_ != impl_->renderPassIndex_ || frame.viewportIndex_ != impl_->viewportIndex_)
    {
        frame.renderPassIndex_ = impl_->renderPassIndex_;
        frame.subpassIndex_    = 0;
		frame.viewportIndex_   = Max(0, impl_->viewportIndex_);

        RenderPassInfo* renderPassInfo = impl_->renderPathData_->passInfos_[frame.renderPassIndex_];

        // Begin the render pass.
        VkRenderPassBeginInfo renderPassBI{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBI.renderPass = renderPassInfo->renderPass_;

        unsigned fbindex = renderPassInfo->id_;

        if (frame.viewportIndex_ >= (int)impl_->viewportInfos_.Size())
        {
            VkFramebuffer* framebuffers = impl_->GetRenderSurfaceFrameBuffers(renderTargets_[0],renderPassInfo);
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGERRORF("Graphics() - PrepareDraw ... renderpassindex=%d viewportIndex_=%d numviewports=%u renderTargets_=%u", 
                            impl_->renderPassIndex_, impl_->viewportIndex_, impl_->viewportInfos_.Size(), renderTargets_[0]);
        #endif             
            renderPassBI.renderArea = impl_->screenScissor_;
            renderPassBI.framebuffer = framebuffers ? framebuffers[frame.id_] : frame.framebuffers_[fbindex];
        }
        else if (renderPassInfo->type_ & (PASS_CLEAR|PASS_PRESENT))
        {
            renderPassBI.renderArea.offset = { 0, 0 };
			renderPassBI.renderArea.extent = impl_->swapChainExtent_;
            renderPassBI.framebuffer = frame.framebuffers_[fbindex];
        }
        else
        {
            fbindex += impl_->viewportInfos_[frame.viewportIndex_].viewSizeIndex_ * impl_->renderPassInfos_.Size();
			renderPassBI.renderArea.offset = impl_->screenScissor_.offset;
			renderPassBI.renderArea.extent = impl_->viewportInfos_[frame.viewportIndex_].rect_.extent;
            renderPassBI.framebuffer = frame.framebuffers_[fbindex];
        }  
        
    #ifdef ACTIVE_FRAMELOGDEBUG
        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... Begin New Render passindex=%d passtype=%d viewportindex=%d(max=%d) fbindex=%u viewport=%F,%F,%F,%F renderArea=%d,%d,%u,%u ...",
                         frame.renderPassIndex_, renderPassInfo->type_, frame.viewportIndex_, impl_->viewportInfos_.Size()-1, fbindex,
                         impl_->viewport_.x, impl_->viewport_.y, impl_->viewport_.width, impl_->viewport_.height,
                         renderPassBI.renderArea.offset.x, renderPassBI.renderArea.offset.y,
                         renderPassBI.renderArea.extent.width, renderPassBI.renderArea.extent.height);
    #endif

		// Start with the first subpass

    #ifdef URHO3D_VULKAN_USE_SEPARATE_CLEARPASS
        Vector<VkClearValue>* pClearValues = renderPassInfo->type_ == PASS_CLEAR ? &renderPassInfo->clearValues_ : 0;
    #else
        Vector<VkClearValue>* pClearValues = &renderPassInfo->clearValues_;
    #endif
        if (pClearValues)
        {
            for (unsigned i = 0; i < pClearValues->Size(); i++)        
                pClearValues->At(i) = renderPassInfo->attachments_[i].slot_ == RENDERSLOT_DEPTH ? impl_->clearDepth_ : impl_->clearColor_;
            renderPassBI.clearValueCount = pClearValues->Size();
            renderPassBI.pClearValues    = pClearValues->Size() ? pClearValues->Buffer() : 0;
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBeginRenderPass    (clearcolor:%F,%F,%F,%F)(pass:%d)", 
                impl_->clearColor_.color.float32[0], impl_->clearColor_.color.float32[1], 
                impl_->clearColor_.color.float32[2], impl_->clearColor_.color.float32[3], frame.renderPassIndex_);
        #endif                
        }
        else 
        {
            renderPassBI.clearValueCount = 0;
            renderPassBI.pClearValues    = 0;
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBeginRenderPass    (noclear)(pass:%d)", frame.renderPassIndex_);
        #endif               
        }
        
        vkCmdBeginRenderPass(frame.commandBuffer_, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    #ifdef URHO3D_VULKAN_USE_SEPARATE_CLEARPASS
        if (renderPassInfo->type_ == PASS_CLEAR)
        {
        #ifdef ACTIVE_FRAMELOGDEBUG            
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... URHO3D_VULKAN_USE_SEPARATE_CLEARPASS !");
        #endif  
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdEndRenderPass      (separateClearPass)(pass:%d)", frame.renderPassIndex_);
        #endif
            vkCmdEndRenderPass(frame.commandBuffer_);
            return;
        }
    #endif
        frame.renderPassBegun_ = true;
        impl_->pipelineDirty_  = true;
    }

    // change to the required subpass
    // that's execute all previous subpasses (like "clear subpass")
    if (frame.renderPassBegun_ && frame.subpassIndex_ != impl_->subpassIndex_)
    {
        while (frame.subpassIndex_ < impl_->subpassIndex_)
        {
            frame.subpassIndex_++;
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdNextSubpass        (pass:%d  sub:%d)", frame.renderPassIndex_, frame.subpassIndex_);
        #endif
            vkCmdNextSubpass(frame.commandBuffer_, VK_SUBPASS_CONTENTS_INLINE);
        }
    }

    // Set the Pipeline if dirty (shaders changed or/and states changed)
    if (vertexShader_ && pixelShader_ && frame.renderPassIndex_ !=-1 && (!impl_->pipelineInfo_ || impl_->pipelineDirty_))
    {
        unsigned renderPassKey = impl_->renderPathData_->passInfos_[frame.renderPassIndex_]->key_;
        impl_->SetPipeline(renderPassKey, vertexShader_, pixelShader_, impl_->pipelineStates_, vertexBuffers_);
    }

    // Set Descriptors.
#ifdef ACTIVE_DESCRIPTOR_UPDATEANDBIND_NEW
    if (impl_->pipelineInfo_ && impl_->pipelineInfo_->descriptorsGroups_.Size())
    {
        const unsigned MaxBindingsBySet = 16;
        const unsigned numDescriptorSets = impl_->pipelineInfo_->descriptorsGroups_.Size();
        const int compatibleSetIndex = impl_->GetMaxCompatibleDescriptorSets(frame.lastPipelineInfoBound_, impl_->pipelineInfo_);

        struct DescriptorSetGroupBindInfo
        {
            uint32_t firstset_;
            Vector<VkDescriptorSet> handles_;
            Vector<uint32_t> dynoffsets_;
        };

        static Vector<DescriptorSetGroupBindInfo> descriptorSetGroupsBindInfos;
        descriptorSetGroupsBindInfos.Clear();

        static Vector<VkWriteDescriptorSet> descriptorWrites;
        descriptorWrites.Resize(numDescriptorSets * MaxBindingsBySet);

        static Vector<VkDescriptorBufferInfo> bufferInfos;
        bufferInfos.Resize(descriptorWrites.Size());

        static Vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.Resize(descriptorWrites.Size());

        static Vector<VkDescriptorImageInfo> inputInfos;
        inputInfos.Resize(descriptorWrites.Size());

        unsigned descriptorWritesCount = 0;

        int lastSetToBind = -1;
        Vector<uint32_t> dynamicOffsets;
        for (int i = 0; i < numDescriptorSets; i++)
        {
            bool descriptorSetBindDirty = compatibleSetIndex < i;

            DescriptorsGroup& descGroup = impl_->pipelineInfo_->descriptorsGroups_[i];

            // Get the allocated Descriptor Sets for the current frame
            DescriptorsGroupAllocation& alloc = descGroup.setsByFrame_[impl_->currentFrame_];

            // Get the index of the last descriptorSet used in the pool
            // for the first update, always use a new descriptorSet
            bool newDecriptorSet = alloc.index_ >= impl_->pipelineInfo_->maxAllocatedDescriptorSets_;

            unsigned set = descGroup.id_;
            unsigned numSamplerUpdate = 0, numInputsUpdate = 0;
            const unsigned startWritesCount = descriptorWritesCount;
            const Vector<ShaderBind>& bindings = descGroup.bindings_;
            for (unsigned j = 0; j < bindings.Size(); j++)
            {
                const ShaderBind& binding = bindings[j];

                int shaderStage = binding.stageFlag_ == VK_SHADER_STAGE_VERTEX_BIT ? VS : PS;

                // Uniform Buffer
                if (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                {
                    ConstantBuffer* buffer = impl_->constantBuffers_[shaderStage][binding.unitStart_];
                    if (!buffer)
                    {
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u no buffer !", shaderStage == VS ? "VS":"PS", set, binding.id_);
                    #endif
                        continue;
                    }

                    unsigned sizePerObject = shaderStage == VS ? vertexShader_->GetConstantBufferSizes()[binding.unitStart_] : pixelShader_->GetConstantBufferSizes()[binding.unitStart_];

                    if (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                    {
                        descriptorSetBindDirty = true;
                        dynamicOffsets.Push(buffer->GetObjectIndex() * impl_->GetUBOPaddedSize(sizePerObject));
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u obj=%u dynamic update buffer=%u dyncount=%u dynoffset=%u !",
                                        shaderStage == VS ? "VS":"PS", set, binding.id_, buffer->GetObjectIndex(), buffer, dynamicOffsets.Size(), dynamicOffsets.Back());
                    #endif
                    }

                    if ((buffer->IsDirty() && binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) || newDecriptorSet)
                    {
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos[descriptorWritesCount];
                        bufferInfo.buffer = (VkBuffer)buffer->GetGPUObject();
                        bufferInfo.offset = 0;
                        bufferInfo.range  = sizePerObject;

                        VkWriteDescriptorSet& descriptorWrite = descriptorWrites[descriptorWritesCount];
                        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrite.dstBinding       = binding.id_;
                        descriptorWrite.dstArrayElement  = 0;
                        descriptorWrite.descriptorType   = (VkDescriptorType)binding.type_;
                        descriptorWrite.descriptorCount  = 1;
                        descriptorWrite.pBufferInfo      = &bufferInfo;
                        descriptorWrite.pNext            = nullptr;
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u write=%u SPGroup=%u size=%u descInd=%u update buffer=%u !",
                                          shaderStage == VS ? "VS":"PS", set, binding.id_, descriptorWritesCount+1, binding.unitStart_, sizePerObject, alloc.index_, buffer);
                    #endif
                        descriptorWritesCount++;
                    }

                    // Update To GPU
                    if (buffer->IsDirty())
                        buffer->Apply();
                }
                // Input Attachment (for subpass)
                else if (binding.type_ == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
                {
                    VkDescriptorImageInfo& inputInfo = inputInfos[numInputsUpdate];
                    inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    inputInfo.imageView = VK_NULL_HANDLE;//TODO attachments[i].color.view;
                    inputInfo.sampler = VK_NULL_HANDLE;

                    VkWriteDescriptorSet& descriptorWrite = descriptorWrites[descriptorWritesCount];
                    descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstBinding       = binding.id_;
                    descriptorWrite.dstArrayElement  = 0;
                    descriptorWrite.descriptorType   = (VkDescriptorType)binding.type_;
                    descriptorWrite.descriptorCount  = 1;
                    descriptorWrite.pImageInfo      = &inputInfos[numInputsUpdate];
                    descriptorWrite.pNext           = nullptr;

                    numInputsUpdate++;

                    descriptorWritesCount++;
                }
                // Sampler
                else if (binding.type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                {
                    if (frame.textureDirty_)
					{
						newDecriptorSet = true;
					#ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... consume a new descriptor for %s Set=%u.%u dexInd=%u !", shaderStage == VS ? "VS":"PS", set, binding.id_, alloc.index_);
                    #endif
					}
					if (!frame.textureDirty_ && !newDecriptorSet)
                        continue;

                    unsigned numTexturesToUpdate = 0;
                    Texture* lasttexture = 0;

                    for (unsigned unit = binding.unitStart_; unit < MAX_TEXTURE_UNITS; unit++)
                    {
                        Texture* texture = textures_[unit];

                        if (!texture)
                        {
//                        #ifdef ACTIVE_FRAMELOGDEBUG
//                            URHO3D_LOGERRORF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u check texture unit=%u %u/%u no Texture in the unit ... SKIP !",
//                                             shaderStage == VS ? "VS":"PS", set, binding.id_+numTexturesToUpdate, unit, numTexturesToUpdate, binding.unitRange_);
//                        #endif
                            continue;
                        }

                        if (!texture->GetShaderResourceView() || !texture->GetSampler())
                        {
//                        #ifdef ACTIVE_FRAMELOGDEBUG
//                            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u check texture unit=%u %u/%u name=%s imageview=%u sampler=%u ... SKIP !",
//                                             shaderStage == VS ? "VS":"PS", set, binding.id_+numTexturesToUpdate, unit, numTexturesToUpdate, binding.unitRange_,
//                                             texture->GetName().CString(), texture->GetShaderResourceView(), texture->GetSampler());
//                        #endif
                            continue;
                        }

                        VkDescriptorImageInfo& imageInfo = imageInfos[numSamplerUpdate+numTexturesToUpdate];
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.imageView   = (VkImageView)(texture->GetShaderResourceView());
                        imageInfo.sampler     = (VkSampler)(texture->GetSampler());

                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u update unit=%u texture=%s imageview=%u sampler=%u !",
                                         shaderStage == VS ? "VS":"PS", set, binding.id_+numTexturesToUpdate, unit,
                                         texture->GetName().CString(), texture->GetShaderResourceView(), texture->GetSampler());
                    #endif
                        numTexturesToUpdate++;

                        lasttexture = texture;

                        if (numTexturesToUpdate >= binding.unitRange_)
                            break;
                    }

                    if (numTexturesToUpdate)
                    {
                        // complete empty sampler
                        for (unsigned unit = numTexturesToUpdate; unit < binding.unitRange_; unit++)
                        {
                            // use the last updated texture
                            VkDescriptorImageInfo& imageInfo = imageInfos[numSamplerUpdate+unit];
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.imageView   = (VkImageView)(lasttexture->GetShaderResourceView());
                            imageInfo.sampler     = (VkSampler)(lasttexture->GetSampler());
                        }
                        numTexturesToUpdate = binding.unitRange_;

                        VkWriteDescriptorSet& descriptorWrite = descriptorWrites[descriptorWritesCount];
                        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrite.dstBinding      = binding.id_;
                        descriptorWrite.dstArrayElement = 0;
                        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        descriptorWrite.descriptorCount = numTexturesToUpdate;
                        descriptorWrite.pImageInfo      = &imageInfos[numSamplerUpdate];
                        descriptorWrite.pNext           = nullptr;

                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u write=%u descInd=%u update %u samplers !",
                                         shaderStage == VS ? "VS":"PS", set, binding.id_, descriptorWritesCount+1, alloc.index_, numTexturesToUpdate);
                    #endif
                        descriptorWritesCount++;

                        numSamplerUpdate += numTexturesToUpdate;
                    }
                }
            }

            // Consume a new descriptor set
            if (newDecriptorSet)
                alloc.index_ = alloc.index_ + 1 < impl_->pipelineInfo_->maxAllocatedDescriptorSets_ ? alloc.index_ + 1 : 0;

            // Get the DescriptorSet from pool allocation
            VkDescriptorSet descriptorSet = alloc.sets_[alloc.index_];

            // Update the DescriptorWrites with the good DescriptorSet Handle
            for (unsigned j = startWritesCount; j < descriptorWritesCount; j++)
                descriptorWrites[j].dstSet = descriptorSet;

            if (descriptorSetBindDirty || newDecriptorSet)
            {
                if (lastSetToBind == -1 || lastSetToBind != i-1)
                {
                    descriptorSetGroupsBindInfos.Resize(descriptorSetGroupsBindInfos.Size()+1);
                    descriptorSetGroupsBindInfos.Back().firstset_ = i;
                #ifdef ACTIVE_FRAMELOGDEBUG
                    URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... push bind group[%d] firstset=%d", i, descriptorSetGroupsBindInfos.Size()-1);
                #endif
                }

                lastSetToBind = i;

                descriptorSetGroupsBindInfos.Back().handles_.Push(descriptorSet);
                if (dynamicOffsets.Size())
                {
                    descriptorSetGroupsBindInfos.Back().dynoffsets_ += dynamicOffsets;
                    dynamicOffsets.Clear();
                }

            #ifdef ACTIVE_FRAMELOGDEBUG
                URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... push set=%d to bind group[%d]", i, descriptorSetGroupsBindInfos.Size()-1);
            #endif
            }
        }

        // Update the descriptorSets.
        if (descriptorWritesCount > 0)
        {
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update descriptor Sets num writes = %u !", descriptorWritesCount);
        #endif
            vkUpdateDescriptorSets(impl_->device_, descriptorWritesCount, descriptorWrites.Buffer(), 0, nullptr);
        }

        // Bind Consecutive descriptorSets.
        for (unsigned j = 0; j < descriptorSetGroupsBindInfos.Size(); j++)
        {
            DescriptorSetGroupBindInfo& info = descriptorSetGroupsBindInfos[j];
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... bind descriptor Sets Group started sets=%u->%u (numsets=%u/%u)!", info.firstset_, info.firstset_+info.handles_.Size()-1, info.handles_.Size(), numDescriptorSets);
        #endif
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBindDescriptorSets (pass:%d)", frame.renderPassIndex_);
        #endif
            vkCmdBindDescriptorSets(frame.commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipelineInfo_->pipelineLayout_,
                                    info.firstset_, info.handles_.Size(), info.handles_.Buffer(), info.dynoffsets_.Size(), info.dynoffsets_.Size() ? info.dynoffsets_.Buffer() : nullptr);
        }

        frame.textureDirty_ = false;
    }
#else
    if (impl_->pipelineInfo_ && impl_->pipelineInfo_->descriptorsGroups_.Size())
    {
        const unsigned MaxBindingsBySet = 16;

        // descriptorSets container to update
        static Vector<VkDescriptorSet> descriptorSets;
        descriptorSets.Resize(impl_->pipelineInfo_->descriptorsGroups_.Size());

        static Vector<VkWriteDescriptorSet> descriptorWrites;
        descriptorWrites.Resize(impl_->pipelineInfo_->descriptorsGroups_.Size() * MaxBindingsBySet);

        static Vector<VkDescriptorBufferInfo> bufferInfos;
        bufferInfos.Resize(descriptorWrites.Size());

        static Vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.Resize(descriptorWrites.Size());

        uint32_t descriptorWritesCount = 0;
        uint32_t dynamicOffsetCount = 0;
        uint32_t dynamicOffsets[MAX_SHADER_PARAMETER_GROUPS];

        for (unsigned i = 0; i < descriptorSets.Size(); i++)
        {
            DescriptorsGroup& descGroup = impl_->pipelineInfo_->descriptorsGroups_[i];

            // Get the allocated Descriptor Sets for the current frame
            DescriptorsGroupAllocation& alloc = descGroup.setsByFrame_[impl_->currentFrame_];

            // Get the index of the last descriptorSet used in the pool
            // for the first update, a new descriptorSet will be used
            unsigned& descriptorIndex = alloc.index_;
            unsigned lastDescriptorIndex = descriptorIndex;
            // feed the descriptorSets container to update
            VkDescriptorSet& descriptorSet = descriptorSets[i];
            descriptorSet = alloc.sets_[descriptorIndex < impl_->pipelineInfo_->maxAllocatedDescriptorSets_ ? descriptorIndex : 0];

            unsigned set = descGroup.id_;
            unsigned numSamplerUpdate = 0;

            const Vector<ShaderBind>& bindings = descGroup.bindings_;

            for (unsigned j = 0; j < bindings.Size(); j++)
            {
                const ShaderBind& binding = bindings[j];

                uint32_t bind = binding.id_;

                // TODO : check common binding for Vertex and Fragment ?
                int shaderStage = binding.stageFlag_ == VK_SHADER_STAGE_VERTEX_BIT ? VS : PS;
                ShaderVariation* shader = shaderStage == VS ? vertexShader_ : pixelShader_;

                // Uniform Buffer
                if (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                {
                    ConstantBuffer* buffer = impl_->constantBuffers_[shaderStage][binding.unitStart_];
                    if (!buffer)
                    {
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u no buffer !", shaderStage == VS ? "VS":"PS", set, bind);
                    #endif
                        continue;
                    }

                    if (frame.lastPipelineBound_ == impl_->pipelineInfo_->pipeline_)
                    if (!buffer->IsDirty() && (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && descriptorIndex < impl_->pipelineInfo_->maxAllocatedDescriptorSets_))
                    {
//                    #ifdef ACTIVE_FRAMELOGDEBUG
//                        URHO3D_LOGDEBUGF("PrepareDraw ... update %s Set=%u.%u dexInd=%u no need to update buffer=%u  !", shaderStage == VS ? "VS":"PS", set, bind, descriptorIndex, buffer);
//                    #endif
                        continue;
                    }

                    // Update To GPU
                    bool dirtybuffer = buffer->IsDirty();
                    if (dirtybuffer)
                        buffer->Apply();

                    unsigned sizePerObject = shader->GetConstantBufferSizes()[binding.unitStart_];

                    bool updateDescriptor = dirtybuffer || lastDescriptorIndex == descriptorIndex;

                    if (binding.type_ == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                    {
                        // Get the object index
                        uint32_t objindex = Clamp(buffer->GetObjectIndex(), 0U, buffer->GetNumObjects()-1);

                        dynamicOffsets[dynamicOffsetCount] = objindex * impl_->GetUBOPaddedSize(sizePerObject);
                        dynamicOffsetCount++;
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u obj=%u dynamic update buffer=%u dyncount=%u dynoffset=%u !",
                                         shaderStage == VS ? "VS":"PS", set, bind, objindex, buffer, dynamicOffsetCount, dynamicOffsets[dynamicOffsetCount-1]);
                    #endif
                        // reuse the same dynamic descriptor, if it's not a new descriptor set
                        if (lastDescriptorIndex != descriptorIndex && descriptorIndex < impl_->pipelineInfo_->maxAllocatedDescriptorSets_)
                            updateDescriptor = false;
                    }

                    if (updateDescriptor)
                    {
                        // consume a new descriptorSet
                        if (lastDescriptorIndex == descriptorIndex)
                        {
                            descriptorIndex = descriptorIndex + 1 < impl_->pipelineInfo_->maxAllocatedDescriptorSets_ ? descriptorIndex + 1 : 0;
                            descriptorSet = alloc.sets_[descriptorIndex];
                        }

                        VkDescriptorBufferInfo& bufferInfo = bufferInfos[descriptorWritesCount];
                        bufferInfo.buffer = (VkBuffer)buffer->GetGPUObject();
                        bufferInfo.offset = 0;
                        bufferInfo.range  = sizePerObject;

                        VkWriteDescriptorSet& descriptorWrite = descriptorWrites[descriptorWritesCount];
                        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrite.dstSet           = descriptorSet;
                        descriptorWrite.dstBinding       = bind;
                        descriptorWrite.dstArrayElement  = 0;
                        descriptorWrite.descriptorType   = (VkDescriptorType)binding.type_;
                        descriptorWrite.descriptorCount  = 1;
                        descriptorWrite.pBufferInfo      = &bufferInfo;
                        descriptorWrite.pNext            = nullptr;
                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u write=%u constantGroup=%u size=%u descInd=%u update buffer=%u !",
                                         shaderStage == VS ? "VS":"PS", set, bind, descriptorWritesCount+1, binding.unitStart_, sizePerObject, descriptorIndex, buffer);
                    #endif
                        descriptorWritesCount++;
                    }
                }
                // Sampler
                else if (binding.type_ == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                {
                    if (!frame.textureDirty_ && descriptorIndex < impl_->pipelineInfo_->maxAllocatedDescriptorSets_)
                    {
//                    #ifdef ACTIVE_FRAMELOGDEBUG
//                        URHO3D_LOGDEBUGF("PrepareDraw ... update %s Set=%u.%u dexInd=%u no need to update sampler !", shaderStage == VS ? "VS":"PS", set, bind, descriptorIndex);
//                    #endif
                        continue;
                    }

                    unsigned numTexturesToUpdate = 0;
                    Texture* lasttexture = 0;

                    for (unsigned unit = binding.unitStart_; unit < MAX_TEXTURE_UNITS; unit++)
                    {
                        Texture* texture = textures_[unit];

                        if (!texture)
                        {
//                        #ifdef ACTIVE_FRAMELOGDEBUG
//                            URHO3D_LOGERRORF("PrepareDraw ... update stage=%s Set=%u.%u check texture unit=%u %u/%u no Texture in the unit ... SKIP !",
//                                             shaderStage == VS ? "VS":"PS", set, bind+numTexturesToUpdate, unit, numTexturesToUpdate, binding.unitRange_);
//                        #endif
                            continue;
                        }

                        if (!texture->GetShaderResourceView() || !texture->GetSampler())
                        {
                        #ifdef ACTIVE_FRAMELOGDEBUG
                            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u check texture unit=%u %u/%u name=%s imageview=%u sampler=%u ... SKIP !",
                                             shaderStage == VS ? "VS":"PS", set, bind+numTexturesToUpdate, unit, numTexturesToUpdate, binding.unitRange_,
                                             texture->GetName().CString(), texture->GetShaderResourceView(), texture->GetSampler());
                        #endif
                            continue;
                        }

                        VkDescriptorImageInfo& imageInfo = imageInfos[numSamplerUpdate+numTexturesToUpdate];
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        imageInfo.imageView   = (VkImageView)(texture->GetShaderResourceView());
                        imageInfo.sampler     = (VkSampler)(texture->GetSampler());

                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u update unit=%u texture=%s imageview=%u sampler=%u !",
                                         shaderStage == VS ? "VS":"PS", set, bind+numTexturesToUpdate, unit,
                                         texture->GetName().CString(), texture->GetShaderResourceView(), texture->GetSampler());
                    #endif
                        numTexturesToUpdate++;

                        lasttexture = texture;

                        if (numTexturesToUpdate >= binding.unitRange_)
                            break;
                    }

                    if (numTexturesToUpdate)
                    {
                        // consume a new descriptorSet
                        if (lastDescriptorIndex == descriptorIndex)
                        {
                            descriptorIndex = descriptorIndex + 1 < impl_->pipelineInfo_->maxAllocatedDescriptorSets_ ? descriptorIndex + 1 : 0;
                            descriptorSet = alloc.sets_[descriptorIndex];
                        }
                        // complete empty sampler
                        for (unsigned unit = numTexturesToUpdate; unit < binding.unitRange_; unit++)
                        {
                            // use the last updated texture
                            VkDescriptorImageInfo& imageInfo = imageInfos[numSamplerUpdate+unit];
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.imageView   = (VkImageView)(lasttexture->GetShaderResourceView());
                            imageInfo.sampler     = (VkSampler)(lasttexture->GetSampler());
                        }
                        numTexturesToUpdate = binding.unitRange_;

                        VkWriteDescriptorSet& descriptorWrite = descriptorWrites[descriptorWritesCount];
                        descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptorWrite.dstSet          = descriptorSet;
                        descriptorWrite.dstBinding      = bind;
                        descriptorWrite.dstArrayElement = 0;
                        descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                        descriptorWrite.descriptorCount = numTexturesToUpdate;
                        descriptorWrite.pImageInfo      = &imageInfos[numSamplerUpdate];
                        descriptorWrite.pNext           = nullptr;

                    #ifdef ACTIVE_FRAMELOGDEBUG
                        URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update stage=%s Set=%u.%u write=%u descInd=%u update %u samplers !",
                                         shaderStage == VS ? "VS":"PS", set, bind, descriptorWritesCount+1, descriptorIndex, numTexturesToUpdate);
                    #endif
                        descriptorWritesCount++;

                        numSamplerUpdate += numTexturesToUpdate;
                    }
//                    else
//                    {
//                        URHO3D_LOGERRORF("PrepareDraw ... frame=%u pipeline=%s no Textures can't update samplers !", impl_->GetFrameIndex(), impl_->pipelineInfo_->vs_->GetName().CString());
//                    }
                }
            }
        }

        // Update the descriptorSets.
        if (descriptorWritesCount > 0)
        {
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... update descriptor Sets num writes = %u !", descriptorWritesCount);
        #endif
            vkUpdateDescriptorSets(impl_->device_, descriptorWritesCount, descriptorWrites.Buffer(), 0, nullptr);
        }

        // Bind the descriptorSets.
        if (descriptorWritesCount || dynamicOffsetCount || (frame.lastPipelineBound_ != impl_->pipelineInfo_->pipeline_))
        {
            for (unsigned i = 0; i < descriptorSets.Size(); i++)
            {
                const DescriptorsGroupAllocation& alloc = impl_->pipelineInfo_->descriptorsGroups_[i].setsByFrame_[impl_->currentFrame_];
                descriptorSets[i] = alloc.sets_[alloc.index_];
            }

            vkCmdBindDescriptorSets(frame.commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipelineInfo_->pipelineLayout_,
                                    0, descriptorSets.Size(), descriptorSets.Buffer(), dynamicOffsetCount, dynamicOffsetCount ? &dynamicOffsets[0] : nullptr);
        }

        frame.textureDirty_ = false;
    }
#endif

    // Bind the pipeline.
    if (impl_->pipelineInfo_ && frame.lastPipelineBound_ != impl_->pipelineInfo_->pipeline_)
    {
        if (impl_->pipelineInfo_->pipeline_)
        {
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... frame=%u bind pipeline(%u) %s vs=%s ps=%s states=%u stencilvalue=%u !", impl_->GetFrameIndex(), impl_->pipelineInfo_->pipeline_,
                             impl_->pipelineInfo_->vs_->GetName().CString(), impl_->pipelineInfo_->vs_->GetDefines().CString(),
                             impl_->pipelineInfo_->ps_->GetDefines().CString(), impl_->pipelineInfo_->pipelineStates_, impl_->pipelineInfo_->stencilValue_);
        #endif
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBindPipeline       (pass:%d)", frame.renderPassIndex_);
        #endif
            vkCmdBindPipeline(frame.commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, impl_->pipelineInfo_->pipeline_);
            frame.lastPipelineBound_ = impl_->pipelineInfo_->pipeline_;
            frame.lastPipelineInfoBound_ = impl_->pipelineInfo_;
            impl_->vertexBuffersDirty_ = true;
            impl_->indexBufferDirty_ = true;
        }
        else
        {
            URHO3D_LOGERRORF("PrepareDraw ... frame=%u pipeline=%s no pipeline to bind !", impl_->GetFrameIndex(), impl_->pipelineInfo_->vs_->GetName().CString());
        }
    }

    // Bind the Index Buffer.
    if (impl_->indexBufferDirty_)
    {
        if (indexBuffer_)
        {
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... frame=%u bind index buffer=%u !", impl_->GetFrameIndex(), indexBuffer_->GetGPUObject());
        #endif
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBindIndexBuffer    (pass:%d)", frame.renderPassIndex_);
        #endif
            vkCmdBindIndexBuffer(frame.commandBuffer_, (VkBuffer)indexBuffer_->GetGPUObject(), 0, indexBuffer_->GetIndexSize() == sizeof(unsigned) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
        }

        impl_->indexBufferDirty_ = false;
    }

    // Bind the Vertex Buffers.
    if (impl_->vertexBuffersDirty_)
    {
        impl_->vertexBuffersDirty_ = false;

        if (impl_->vertexBuffers_.Size())
        {
        #ifdef ACTIVE_FRAMELOGDEBUG
            URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ... frame=%u bind vertex buffers numVertexBuffers=%u ...", impl_->GetFrameIndex(), impl_->vertexBuffers_.Size());
        #endif
            for (unsigned i=0; i < impl_->vertexBuffers_.Size(); i++)
            {
            #ifdef ACTIVE_FRAMELOGDEBUG
                URHO3D_LOGDEBUGF("Graphics() - PrepareDraw ...         bind vertex buffer=%u", vertexBuffers_[i]->GetGPUObject());
            #endif
                impl_->vertexBuffers_[i] = (VkBuffer)vertexBuffers_[i]->GetGPUObject();
                impl_->vertexOffsets_[i] = 0;
            }
        #if defined(DEBUG_VULKANCOMMANDS)
            URHO3D_LOGDEBUGF("vkCmdBindVertexBuffers  (pass:%d)", frame.renderPassIndex_);
        #endif
            vkCmdBindVertexBuffers(frame.commandBuffer_, 0, impl_->vertexBuffers_.Size(), impl_->vertexBuffers_.Buffer(), impl_->vertexOffsets_.Buffer());
        }
        else
        {
            URHO3D_LOGERRORF("Graphics() - PrepareDraw ... frame=%u can't bind buffers null size !", impl_->GetFrameIndex());
        }
    }

    // Set viewport
    vkCmdSetViewport(frame.commandBuffer_, 0, 1, &impl_->viewport_);
#if defined(DEBUG_VULKANCOMMANDS)
    URHO3D_LOGDEBUGF("vkCmdSetViewport        (pass:%d viewport:%F %F %F %F)", frame.renderPassIndex_,
        impl_->viewport_.x, impl_->viewport_.y, impl_->viewport_.width, impl_->viewport_.height);
#endif
    // Set scissor
    if (scissorTest_)
    {
    #if defined(DEBUG_VULKANCOMMANDS)
        URHO3D_LOGDEBUGF("vkCmdSetScissor         (pass:%d scissor:%d %d %u %u Framed)", frame.renderPassIndex_,
            impl_->frameScissor_.offset.x, impl_->frameScissor_.offset.y, impl_->frameScissor_.extent.width, impl_->frameScissor_.extent.height);        
    #endif        
        vkCmdSetScissor(frame.commandBuffer_, 0, 1, &impl_->frameScissor_);
    }
    else
    {
    #if defined(DEBUG_VULKANCOMMANDS)
        URHO3D_LOGDEBUGF("vkCmdSetScissor         (pass:%d scissor:%d %d %d %d)", frame.renderPassIndex_,
            impl_->screenScissor_.offset.x, impl_->screenScissor_.offset.y, impl_->screenScissor_.extent.width, impl_->screenScissor_.extent.height);
    #endif
        vkCmdSetScissor(frame.commandBuffer_, 0, 1, &impl_->screenScissor_);
    }

//    URHO3D_LOGERRORF("PrepareDraw ... End : scissorTest=%u ", scissorTest_);
}

void Graphics::CleanupFramebuffers()
{

}

void Graphics::ResetCachedState()
{
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers_[i] = 0;

//    for (unsigned i = 0; i < MAX_TEXTURE_UNITS; ++i)
//    {
//        textures_[i] = 0;
//        impl_->textureTypes_[i] = 0;
//    }

    for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
        renderTargets_[i] = 0;

    depthStencil_ = 0;
    viewport_ = IntRect(0, 0, 0, 0);
    indexBuffer_ = 0;
    vertexShader_ = 0;
    pixelShader_ = 0;
    blendMode_ = BLEND_REPLACE;
    alphaToCoverage_ = false;
    colorWrite_ = true;
    cullMode_ = CULL_NONE;
    constantDepthBias_ = 0.0f;
    slopeScaledDepthBias_ = 0.0f;
    depthTestMode_ = CMP_ALWAYS;
    depthWrite_ = false;
    lineAntiAlias_ = false;
    fillMode_ = FILL_SOLID;
    scissorTest_ = false;
    scissorRect_ = IntRect::ZERO;
    stencilTest_ = false;
    stencilTestMode_ = CMP_ALWAYS;
    stencilPass_ = OP_KEEP;
    stencilFail_ = OP_KEEP;
    stencilZFail_ = OP_KEEP;
    stencilRef_ = 0;
    stencilCompareMask_ = M_MAX_UNSIGNED;
    stencilWriteMask_ = M_MAX_UNSIGNED;
    useClipPlane_ = false;

    impl_->swapChainDirty_     = true;
    impl_->scissorDirty_       = true;
    impl_->vertexBuffersDirty_ = true;
    impl_->pipelineDirty_      = true;

//    impl_->shaderProgram_ = 0;
//    impl_->lastInstanceOffset_ = 0;
//    impl_->activeTexture_ = 0;
//    impl_->enabledVertexAttributes_ = 0;
//    impl_->usedVertexAttributes_ = 0;
//    impl_->instancingVertexAttributes_ = 0;
//    impl_->boundFBO_ = impl_->systemFBO_;
//    impl_->boundUBO_ = 0;
//    impl_->sRGBWrite_ = false;
//
//    // Set initial state to match Direct3D
//    if (impl_->context_)
//    {
//        glEnable(GL_DEPTH_TEST);
//        SetCullMode(CULL_CCW);
//        SetDepthTest(CMP_LESSEQUAL);
//        SetDepthWrite(true);
//    }
//
//    for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS * 2; ++i)
//        impl_->constantBuffers_[i] = 0;
//    impl_->dirtyConstantBuffers_.Clear();
}

void Graphics::SetTextureUnitMappings()
{
    textureUnits_["DiffMap"] = TU_DIFFUSE;
    textureUnits_["DiffCubeMap"] = TU_DIFFUSE;
    textureUnits_["AlbedoBuffer"] = TU_ALBEDOBUFFER;
    textureUnits_["NormalMap"] = TU_NORMAL;
    textureUnits_["NormalBuffer"] = TU_NORMALBUFFER;
    textureUnits_["SpecMap"] = TU_SPECULAR;
    textureUnits_["EmissiveMap"] = TU_EMISSIVE;
    textureUnits_["EnvMap"] = TU_ENVIRONMENT;
    textureUnits_["EnvCubeMap"] = TU_ENVIRONMENT;
    textureUnits_["LightRampMap"] = TU_LIGHTRAMP;
    textureUnits_["LightSpotMap"] = TU_LIGHTSHAPE;
    textureUnits_["LightCubeMap"] = TU_LIGHTSHAPE;
    textureUnits_["ShadowMap"] = TU_SHADOWMAP;
#ifdef DESKTOP_GRAPHICS
    textureUnits_["VolumeMap"] = TU_VOLUMEMAP;
    textureUnits_["FaceSelectCubeMap"] = TU_FACESELECT;
    textureUnits_["IndirectionCubeMap"] = TU_INDIRECTION;
    textureUnits_["DepthBuffer"] = TU_DEPTHBUFFER;
    textureUnits_["LightBuffer"] = TU_LIGHTBUFFER;
    textureUnits_["ZoneCubeMap"] = TU_ZONE;
    textureUnits_["ZoneVolumeMap"] = TU_ZONE;
#endif

}

unsigned Graphics::CreateFramebuffer()
{
    unsigned newFbo = 0;
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//        glGenFramebuffersEXT(1, &newFbo);
//    else
//#endif
//        glGenFramebuffers(1, &newFbo);
    return newFbo;
}

void Graphics::DeleteFramebuffer(unsigned fbo)
{
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//        glDeleteFramebuffersEXT(1, &fbo);
//    else
//#endif
//        glDeleteFramebuffers(1, &fbo);
}

void Graphics::BindFramebuffer(unsigned fbo)
{
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
//    else
//#endif
//        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void Graphics::BindColorAttachment(unsigned index, unsigned target, unsigned object, bool isRenderBuffer)
{
//    if (!object)
//        isRenderBuffer = false;
//
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, target, object, 0);
//        else
//            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + index, GL_RENDERBUFFER_EXT, object);
//    }
//    else
//#endif
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, target, object, 0);
//        else
//            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_RENDERBUFFER, object);
//    }
}

void Graphics::BindDepthAttachment(unsigned object, bool isRenderBuffer)
{
//    if (!object)
//        isRenderBuffer = false;
//
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, object, 0);
//        else
//            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, object);
//    }
//    else
//#endif
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, object, 0);
//        else
//            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, object);
//    }
}

void Graphics::BindStencilAttachment(unsigned object, bool isRenderBuffer)
{
//    if (!object)
//        isRenderBuffer = false;
//
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, object, 0);
//        else
//            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, object);
//    }
//    else
//#endif
//    {
//        if (!isRenderBuffer)
//            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, object, 0);
//        else
//            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, object);
//    }
}

bool Graphics::CheckFramebuffer()
{
//#ifndef GL_ES_VERSION_2_0
//    if (!gl3Support)
//        return glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT;
//    else
//#endif
//        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    return true;
}

void Graphics::SetVertexAttribDivisor(unsigned location, unsigned divisor)
{
//#ifndef GL_ES_VERSION_2_0
//    if (gl3Support && instancingSupport_)
//        glVertexAttribDivisor(location, divisor);
//    else if (instancingSupport_)
//        glVertexAttribDivisorARB(location, divisor);
//#else
//#ifdef __EMSCRIPTEN__
//    if (instancingSupport_)
//        glVertexAttribDivisorANGLE(location, divisor);
//#endif
//#endif
}

}
