#include "lxgui/gui_texture.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_manager.hpp"

#include <lxgui/xml_document.hpp>
#include <lxgui/utils_string.hpp>

namespace lxgui {
namespace gui
{
void texture::parse_block(xml::block* pBlock)
{
    layered_region::parse_block(pBlock);

    parse_tex_coords_block_(pBlock);

    xml::block* pColorBlock = pBlock->get_block("Color");
    if (pColorBlock)
        set_solid_color(parse_color_block_(pColorBlock));

    parse_gradient_block_(pBlock);
}

void texture::parse_attributes_(xml::block* pBlock)
{
    layered_region::parse_attributes_(pBlock);

    set_filter_mode(pBlock->get_attribute("filter"));
    set_texture(get_manager().parse_file_name(pBlock->get_attribute("file")));
}

void texture::parse_tex_coords_block_(xml::block* pBlock)
{
    xml::block* pTexCoordsBlock = pBlock->get_block("TexCoords");
    if (pTexCoordsBlock)
    {
        std::array<float,4> mRect;
        mRect[0] = utils::string_to_float(pTexCoordsBlock->get_attribute("left"));
        mRect[1] = utils::string_to_float(pTexCoordsBlock->get_attribute("top"));
        mRect[2] = utils::string_to_float(pTexCoordsBlock->get_attribute("right"));
        mRect[3] = utils::string_to_float(pTexCoordsBlock->get_attribute("bottom"));
        set_tex_rect(mRect);
    }
}

void texture::parse_gradient_block_(xml::block* pBlock)
{
    xml::block* pGradientBlock = pBlock->get_block("Gradient");
    if (pGradientBlock)
    {
        std::string sOrientation = pGradientBlock->get_attribute("orientation");
        gradient::orientation mOrient;
        if (sOrientation == "HORIZONTAL")
            mOrient = gradient::orientation::HORIZONTAL;
        else if (sOrientation == "VERTICAL")
            mOrient = gradient::orientation::VERTICAL;
        else
        {
            gui::out << gui::warning << pGradientBlock->get_location() << " : "
                "Unknown gradient orientation for "+sName_+" : \""+sOrientation+"\". "
                "No gradient will be shown for this texture." << std::endl;
            return;
        }

        xml::block* pMinColorBlock = pGradientBlock->get_block("MinColor");
        xml::block* pMaxColorBlock = pGradientBlock->get_block("MaxColor");

        set_gradient(gradient(mOrient,
            parse_color_block_(pMinColorBlock),
            parse_color_block_(pMaxColorBlock)
        ));
    }
}
}
}
