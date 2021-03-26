#include "lxgui/impl/gui_sdl_material.hpp"
#include "lxgui/impl/gui_sdl_renderer.hpp"
#include <lxgui/gui_out.hpp>
#include <lxgui/gui_exception.hpp>
#include <lxgui/utils_string.hpp>

#include <SDL.h>
#include <SDL_image.h>

namespace lxgui {
namespace gui {
namespace sdl
{

int material::get_premultiplied_alpha_blend_mode()
{
    static const SDL_BlendMode mBlend = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD);

    return (int)mBlend;
}

material::material(SDL_Renderer* pRenderer, uint uiWidth, uint uiHeight,
    bool bRenderTarget, wrap mWrap, filter mFilter) : pRenderer_(pRenderer)
{
    SDL_RendererInfo mInfo;
    if (SDL_GetRendererInfo(pRenderer, &mInfo) != 0)
    {
        throw gui::exception("gui::sdl::material", "Could not get renderer information.");
    }

    if (mInfo.max_texture_width != 0)
    {
        if (uiWidth > (uint)mInfo.max_texture_width || uiHeight > (uint)mInfo.max_texture_height)
        {
            throw gui::exception("gui::sdl::material",
                "Texture dimensions not supported by hardware: ("+
                utils::to_string(uiWidth)+" x "+utils::to_string(uiHeight)+").");
        }
    }

    auto& mTexData = mData_.emplace<texture_data>();

    // Set filtering
    if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, mFilter == filter::NONE ? "0" : "1") == SDL_FALSE)
    {
        throw gui::exception("gui::sdl::material", "Could not set filtering hint");
    }

    mTexData.pTexture_ = SDL_CreateTexture(pRenderer,
        SDL_PIXELFORMAT_ABGR8888,
        bRenderTarget ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STREAMING,
        uiWidth, uiHeight);

    if (mTexData.pTexture_ == nullptr)
    {
        throw gui::exception("gui::sdl::material", "Could not create "+
            std::string(bRenderTarget ? "render target" : "texture")+" with dimensions "+
            utils::to_string(uiWidth)+" x "+utils::to_string(uiHeight)+".");
    }

    int iTextureRealWidth = 0, iTextureRealHeight = 0, iAccess = 0;
    Uint32 uiTextureFormat = 0;
    SDL_QueryTexture(mTexData.pTexture_, &uiTextureFormat, &iAccess,
        &iTextureRealWidth, &iTextureRealHeight);

    mTexData.uiWidth_ = uiWidth;
    mTexData.uiHeight_ = uiHeight;
    mTexData.mWrap_ = mWrap;
    mTexData.mFilter_ = mFilter;
    mTexData.uiRealWidth_ = iTextureRealWidth;
    mTexData.uiRealHeight_ = iTextureRealHeight;
    mTexData.bRenderTarget_ = bRenderTarget;
}

material::material(SDL_Renderer* pRenderer, const std::string& sFileName,
    bool bPreMultipliedAlphaSupported, wrap mWrap, filter mFilter) : pRenderer_(pRenderer)
{
    auto& mTexData = mData_.emplace<texture_data>();

    // Load file
    SDL_Surface* pSurface = IMG_Load(sFileName.c_str());
    if (pSurface == nullptr)
    {
        throw gui::exception("gui::sdl::material", "Could not load image file "+sFileName+".");
    }

    // Convert to RGBA 32bit
    SDL_Surface* pConvertedSurface = SDL_ConvertSurfaceFormat(pSurface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(pSurface);
    if (pConvertedSurface == NULL)
    {
        throw gui::exception("gui::sdl::material", "Could convert image file "+
            sFileName+" to RGBA format.");
    }

    // Pre-multiply alpha
    if (bPreMultipliedAlphaSupported)
        premultiply_alpha(pConvertedSurface);

    const uint uiWidth  = pConvertedSurface->w;
    const uint uiHeight = pConvertedSurface->h;

    // Set filtering
    if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, mFilter == filter::NONE ? "0" : "1") == SDL_FALSE)
    {
        throw gui::exception("gui::sdl::material", "Could not set filtering hint");
    }

    // Create streamable texture
    mTexData.pTexture_ = SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING, uiWidth, uiHeight);
    if (mTexData.pTexture_ == nullptr)
    {
        SDL_FreeSurface(pConvertedSurface);
        throw gui::exception("gui::sdl::material", "Could not create texture with dimensions "+
            utils::to_string(uiWidth)+" x "+utils::to_string(uiHeight)+".");
    }

    // Copy data into the texture
    ub32color* pTexturePixels = lock_pointer();
    const ub32color* pSurfacePixelsStart = reinterpret_cast<const ub32color*>(pConvertedSurface->pixels);
    const ub32color* pSurfacePixelsEnd = pSurfacePixelsStart + uiWidth * uiHeight;
    std::copy(pSurfacePixelsStart, pSurfacePixelsEnd, pTexturePixels);
    unlock_pointer();

    int iTextureRealWidth = 0, iTextureRealHeight = 0, iAccess = 0;
    Uint32 uiTextureFormat = 0;
    SDL_QueryTexture(mTexData.pTexture_, &uiTextureFormat, &iAccess,
        &iTextureRealWidth, &iTextureRealHeight);

    mTexData.uiWidth_ = uiWidth;
    mTexData.uiHeight_ = uiHeight;
    mTexData.mWrap_ = mWrap;
    mTexData.mFilter_ = mFilter;
    mTexData.uiRealWidth_ = iTextureRealWidth;
    mTexData.uiRealHeight_ = iTextureRealHeight;
    mTexData.bRenderTarget_ = false;
}

material::material(SDL_Renderer* pRenderer, const color& mColor) : pRenderer_(pRenderer)
{
    auto& mColData = mData_.emplace<color_data>();
    mColData.mColor_ = mColor;
}

material::~material() noexcept
{
    if (std::holds_alternative<texture_data>(mData_))
    {
        SDL_DestroyTexture(std::get<texture_data>(mData_).pTexture_);
    }
}

material::type material::get_type() const
{
    return std::holds_alternative<texture_data>(mData_) ? type::TEXTURE : type::COLOR;
}

color material::get_color() const
{
    return std::get<color_data>(mData_).mColor_;
}

void material::set_wrap(wrap mWrap)
{
    if (!std::holds_alternative<texture_data>(mData_)) return;

    std::get<texture_data>(mData_).mWrap_ = mWrap;
}

material::wrap material::get_wrap() const
{
    if (!std::holds_alternative<texture_data>(mData_)) return wrap::REPEAT;

    return std::get<texture_data>(mData_).mWrap_;
}

void material::set_filter(filter mFilter)
{
    if (!std::holds_alternative<texture_data>(mData_)) return;

    std::get<texture_data>(mData_).mFilter_ = mFilter;
}

void material::premultiply_alpha(SDL_Surface* pSurface)
{
    ub32color* pPixelData = reinterpret_cast<ub32color*>(pSurface->pixels);

    const uint uiArea = pSurface->w * pSurface->h;
    for (uint i = 0; i < uiArea; ++i)
    {
        float a = pPixelData[i].a/255.0f;
        pPixelData[i].r *= a;
        pPixelData[i].g *= a;
        pPixelData[i].b *= a;
    }
}

float material::get_width() const
{
    if (std::holds_alternative<texture_data>(mData_))
    {
        return std::get<texture_data>(mData_).uiWidth_;
    }
    else
    {
        return 1.0f;
    }
}

float material::get_height() const
{
    if (std::holds_alternative<texture_data>(mData_))
    {
        return std::get<texture_data>(mData_).uiHeight_;
    }
    else
    {
        return 1.0f;
    }
}

float material::get_real_width() const
{
    if (std::holds_alternative<texture_data>(mData_))
    {
        return std::get<texture_data>(mData_).uiRealWidth_;
    }
    else
    {
        return 1.0f;
    }
}

float material::get_real_height() const
{
    if (std::holds_alternative<texture_data>(mData_))
    {
        return std::get<texture_data>(mData_).uiRealHeight_;
    }
    else
    {
        return 1.0f;
    }
}

bool material::set_dimensions(uint uiWidth, uint uiHeight)
{
    if (!std::holds_alternative<texture_data>(mData_)) return false;

    auto& mTexData = std::get<texture_data>(mData_);
    if (!mTexData.bRenderTarget_) return false;

    SDL_RendererInfo mInfo;
    if (SDL_GetRendererInfo(pRenderer_, &mInfo) != 0)
    {
        throw gui::exception("gui::sdl::material", "Could not get renderer information.");
    }

    if (mInfo.max_texture_width != 0)
    {
        if (uiWidth > (uint)mInfo.max_texture_width || uiHeight > (uint)mInfo.max_texture_height)
        {
            return false;
        }
    }

    if (uiWidth > mTexData.uiRealWidth_ || uiHeight > mTexData.uiRealHeight_)
    {
        mTexData.uiWidth_      = uiWidth;
        mTexData.uiHeight_     = uiHeight;
        // sdl is not efficient at resizing render texture, so use an exponential growth pattern
        // to avoid re-allocating a new render texture on every resize operation.
        if (uiWidth > mTexData.uiRealWidth_)
            mTexData.uiRealWidth_  = uiWidth + uiWidth/2;
        if (uiHeight > mTexData.uiRealHeight_)
            mTexData.uiRealHeight_ = uiHeight + uiHeight/2;

        SDL_DestroyTexture(mTexData.pTexture_);
        mTexData.pTexture_ = SDL_CreateTexture(pRenderer_, SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_TARGET, mTexData.uiRealWidth_, mTexData.uiRealHeight_);

        if (mTexData.pTexture_ == nullptr)
        {
            throw gui::exception("gui::sdl::material", "Could not create render target "
                "with dimensions "+utils::to_string(uiWidth)+" x "+utils::to_string(uiHeight)+".");
        }

        return true;
    }
    else
    {
        mTexData.uiWidth_  = uiWidth;
        mTexData.uiHeight_ = uiHeight;
        return false;
    }
}

ub32color* material::lock_pointer(int* pPitch)
{
    if (!std::holds_alternative<texture_data>(mData_)) return nullptr;

    void* pPixelData = nullptr;
    int iPitch = 0;
    if (SDL_LockTexture(std::get<texture_data>(mData_).pTexture_, nullptr, &pPixelData, &iPitch) != 0)
    {
        throw gui::exception("gui::sdl::material", "Could not lock texture for copying pixels.");
    }

    if (pPitch) *pPitch = iPitch;

    return reinterpret_cast<ub32color*>(pPixelData);
}

void material::unlock_pointer()
{
    if (!std::holds_alternative<texture_data>(mData_)) return;

    SDL_UnlockTexture(std::get<texture_data>(mData_).pTexture_);
}

SDL_Texture* material::get_render_texture()
{
    if (!std::holds_alternative<texture_data>(mData_)) return nullptr;

    auto& mTexData = std::get<texture_data>(mData_);
    if (!mTexData.bRenderTarget_) return nullptr;

    return mTexData.pTexture_;
}

const SDL_Texture* material::get_texture() const
{
    if (!std::holds_alternative<texture_data>(mData_)) return nullptr;

    return std::get<texture_data>(mData_).pTexture_;
}

SDL_Texture* material::get_texture()
{
    if (!std::holds_alternative<texture_data>(mData_)) return nullptr;

    return std::get<texture_data>(mData_).pTexture_;
}

SDL_Renderer* material::get_renderer()
{
    return pRenderer_;
}

}
}
}