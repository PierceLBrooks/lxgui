#include "lxgui/impl/gui_sdl_atlas.hpp"
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

atlas_page::atlas_page(SDL_Renderer* pRenderer, material::filter mFilter) : gui::atlas_page(mFilter) {}

std::shared_ptr<gui::material> atlas_page::add_material_(const gui::material& mMat,
    const quad2f& mLocation)
{
    // TODO
    return nullptr;
}

float atlas_page::get_width() const
{
    // TODO
    return 0.0f;
}

float atlas_page::get_height() const
{
    // TODO
    return 0.0;
}

atlas::atlas(SDL_Renderer* pRenderer, material::filter mFilter) :
    gui::atlas(mFilter), pRenderer_(pRenderer) {}

std::unique_ptr<gui::atlas_page> atlas::create_page_() const
{
    return std::make_unique<sdl::atlas_page>(pRenderer_, mFilter_);
}

}
}
}