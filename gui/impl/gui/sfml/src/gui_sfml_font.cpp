#include "lxgui/impl/gui_sfml_font.hpp"
#include "lxgui/impl/gui_sfml_material.hpp"
#include <lxgui/gui_exception.hpp>
#include <lxgui/gui_out.hpp>
#include <lxgui/utils_string.hpp>

namespace lxgui {
namespace gui {
namespace sfml
{
font::font(const std::string& sFontFile, uint uiSize, uint uiOutline,
    const std::vector<code_point_range>& lCodePoints, char32_t uiDefaultCodePoint) :
    uiSize_(uiSize), uiOutline_(uiOutline), uiDefaultCodePoint_(uiDefaultCodePoint),
    lCodePoints_(lCodePoints)
{
    if (!mFont_.loadFromFile(sFontFile))
    {
        throw gui::exception("gui::sfml::font", "Could not load font file '"+sFontFile+"'.");
    }

    // Need to request in advance the glyphs that we will use
    // in order for SFLM to draw them on its internal texture
    for (const code_point_range& mRange : lCodePoints_)
    {
        for (char32_t uiCodePoint = mRange.uiFirst; uiCodePoint <= mRange.uiLast; ++uiCodePoint)
        {
            mFont_.getGlyph(uiCodePoint, uiSize_, false, uiOutline);
        }
    }

    sf::Image mData = mFont_.getTexture(uiSize_).copyToImage();
    sfml::material::premultiply_alpha(mData);
    pTexture_ = std::make_shared<sfml::material>(mData);
}

uint font::get_size() const
{
    return uiSize_;
}

char32_t font::get_character_(char32_t uiChar) const
{
    for (const auto& mRange : lCodePoints_)
    {
        if (uiChar < mRange.uiFirst || uiChar > mRange.uiLast)
            continue;

        return uiChar;
    }

    if (uiChar != uiDefaultCodePoint_)
        return get_character_(uiDefaultCodePoint_);
    else
        return 0;
}

quad2f font::get_character_uvs(char32_t uiChar) const
{
    uiChar = get_character_(uiChar);
    if (uiChar == 0)
        return quad2f{};

    const sf::IntRect& mSFRect = mFont_.getGlyph(uiChar, uiSize_, false, uiOutline_).textureRect;
    const quad2f& mTexRect = pTexture_->get_rect();

    quad2f mRect;
    mRect.left   = mSFRect.left / mTexRect.width();
    mRect.right  = (mSFRect.left + mSFRect.width) / mTexRect.width();
    mRect.top    = mSFRect.top / mTexRect.height();
    mRect.bottom = (mSFRect.top + mSFRect.height) / mTexRect.height();

    vector2f mTopLeft = pTexture_->get_canvas_uv(mRect.top_left(), true);
    vector2f mBottomRight = pTexture_->get_canvas_uv(mRect.bottom_right(), true);
    return quad2f(mTopLeft.x, mBottomRight.x, mTopLeft.y, mBottomRight.y);
}

quad2f font::get_character_bounds(char32_t uiChar) const
{
    uiChar = get_character_(uiChar);
    if (uiChar == 0)
        return quad2f{};

    const float fYOffset = uiSize_;
    const float fOffset = static_cast<float>(uiOutline_);

    const sf::FloatRect& mSFRect = mFont_.getGlyph(uiChar, uiSize_, false, uiOutline_).bounds;

    quad2f mRect;
    mRect.left   = -fOffset;
    mRect.right  = -fOffset + mSFRect.width;
    mRect.top    = -fOffset + mSFRect.top + fYOffset;
    mRect.bottom = -fOffset + mSFRect.top + mSFRect.height + fYOffset;
    return mRect;
}

float font::get_character_width(char32_t uiChar) const
{
    uiChar = get_character_(uiChar);
    if (uiChar == 0)
        return 0.0f;

    return mFont_.getGlyph(uiChar, uiSize_, false, uiOutline_).advance;
}

float font::get_character_height(char32_t uiChar) const
{
    uiChar = get_character_(uiChar);
    if (uiChar == 0)
        return 0.0f;

    return mFont_.getGlyph(uiChar, uiSize_, false, uiOutline_).bounds.height;
}

float font::get_character_kerning(char32_t uiChar1, char32_t uiChar2) const
{
    uiChar1 = get_character_(uiChar1);
    uiChar2 = get_character_(uiChar2);
    if (uiChar1 == 0 || uiChar2 == 0)
        return 0.0f;

    return mFont_.getKerning(uiChar1, uiChar2, uiSize_);
}

std::weak_ptr<gui::material> font::get_texture() const
{
    return pTexture_;
}

void font::update_texture(std::shared_ptr<gui::material> pMat)
{
    pTexture_ = std::static_pointer_cast<sfml::material>(pMat);
}

}
}
}
