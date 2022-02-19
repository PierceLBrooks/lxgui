#include "lxgui/gui_layoutnode.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_parser_common.hpp"
#include "lxgui/gui_statusbar.hpp"
#include "lxgui/gui_texture.hpp"

namespace lxgui::gui {

void status_bar::parse_attributes_(const layout_node& node) {
    frame::parse_attributes_(node);

    if (const layout_attribute* p_attr = node.try_get_attribute("minValue"))
        set_min_value(p_attr->get_value<float>());
    if (const layout_attribute* p_attr = node.try_get_attribute("maxValue"))
        set_max_value(p_attr->get_value<float>());
    if (const layout_attribute* p_attr = node.try_get_attribute("defaultValue"))
        set_value(p_attr->get_value<float>());
    if (const layout_attribute* p_attr = node.try_get_attribute("drawLayer"))
        set_bar_draw_layer(p_attr->get_value<std::string>());

    if (const layout_attribute* p_attr = node.try_get_attribute("orientation")) {
        std::string orientation = p_attr->get_value<std::string>();
        if (orientation == "HORIZONTAL")
            set_orientation(orientation::horizontal);
        else if (orientation == "VERTICAL")
            set_orientation(orientation::vertical);
        else {
            gui::out << gui::warning << node.get_location()
                     << " : "
                        "Unknown StatusBar orientation : \"" +
                            orientation +
                            "\". Expecting either :\n"
                            "\"HORIZONTAL\" or \"VERTICAL\". Attribute ignored."
                     << std::endl;
        }
    }

    if (const layout_attribute* p_attr = node.try_get_attribute("reversed"))
        set_reversed(p_attr->get_value<bool>());
}

void status_bar::parse_all_nodes_before_children_(const layout_node& node) {
    frame::parse_all_nodes_before_children_(node);

    const layout_node* p_texture_node = node.try_get_child("BarTexture");
    const layout_node* p_color_node   = node.try_get_child("BarColor");
    if (p_color_node && p_texture_node) {
        gui::out << gui::warning << node.get_location()
                 << " : "
                    "StatusBar can only contain one of BarTexture or BarColor, but not both. "
                    "BarColor ignored."
                 << std::endl;
    }

    if (p_texture_node) {
        layout_node defaulted = *p_texture_node;
        defaulted.get_or_set_attribute_value("name", "$parentBarTexture");

        auto p_bar_texture = parse_region_(defaulted, "ARTWORK", "Texture");
        if (p_bar_texture) {
            p_bar_texture->set_special();
            set_bar_texture(utils::static_pointer_cast<texture>(p_bar_texture));
        }

        warn_for_not_accessed_node(defaulted);
        p_texture_node->bypass_access_check();
    } else if (p_color_node) {
        set_bar_color(parse_color_node_(*p_color_node));
    }
}

} // namespace lxgui::gui
