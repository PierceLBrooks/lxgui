#include "lxgui/impl/gui_sfml_atlas.hpp"
#include "lxgui/impl/gui_sfml_renderer.hpp"
#include <lxgui/gui_out.hpp>
#include <lxgui/gui_exception.hpp>
#include <lxgui/utils_string.hpp>

namespace lxgui {
namespace gui {
namespace sfml
{

atlas_page::atlas_page(material::filter mFilter) : gui::atlas_page(mFilter)
{
    const uint uiSize = sf::Texture::getMaximumSize();

    if (!mTexture_.create(uiSize, uiSize))
    {
        throw gui::exception("gui::sfml::atlas_page", "Could not create texture with dimensions "+
            utils::to_string(uiSize)+" x "+utils::to_string(uiSize)+".");
    }

    mTexture_.setSmooth(mFilter == material::filter::LINEAR);
}

std::shared_ptr<gui::material> atlas_page::add_material_(const gui::material& mMat,
    const quad2f& mLocation)
{
    const sfml::material& mSFMat = static_cast<const sfml::material&>(mMat);

    const sf::Image mImage = mSFMat.get_texture()->copyToImage();
    mTexture_.update(mImage, mLocation.left, mLocation.top);

    return std::make_shared<sfml::material>(mTexture_, mLocation, mFilter_);
}

float atlas_page::get_width() const
{
    return mTexture_.getSize().x;
}

float atlas_page::get_height() const
{
    return mTexture_.getSize().y;
}

atlas::atlas(material::filter mFilter) : gui::atlas(mFilter) {}

std::unique_ptr<gui::atlas_page> atlas::create_page_() const
{
    return std::make_unique<sfml::atlas_page>(mFilter_);
}

}
}
}