#include "lxgui/gui_button.hpp"
#include "lxgui/gui_fontstring.hpp"
#include "lxgui/gui_layoutnode.hpp"
#include "lxgui/gui_localizer.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_parser_common.hpp"
#include "lxgui/gui_texture.hpp"

namespace lxgui::gui {
void button::parse_attributes_(const layout_node& node) {
    frame::parse_attributes_(node);

    if (const layout_attribute* p_attr = node.try_get_attribute("text")) {
        set_text(utils::utf8_to_unicode(
            get_manager().get_localizer().localize(p_attr->get_value<std::string>())));
    }
}

void button::parse_all_nodes_before_children_(const layout_node& node) {
    frame::parse_all_nodes_before_children_(node);

    if (const layout_node* p_special_node = node.try_get_child("NormalTexture")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "ARTWORK");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentNormalTexture");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_texture = parse_region_(defaulted, layer, "Texture");
        if (p_texture) {
            p_texture->set_special();
            set_normal_texture(utils::static_pointer_cast<texture>(p_texture));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("PushedTexture")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "BORDER");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentPushedTexture");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_texture = parse_region_(defaulted, layer, "Texture");
        if (p_texture) {
            p_texture->set_special();
            set_pushed_texture(utils::static_pointer_cast<texture>(p_texture));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("DisabledTexture")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "BORDER");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentDisabledTexture");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_texture = parse_region_(defaulted, layer, "Texture");
        if (p_texture) {
            p_texture->set_special();
            set_disabled_texture(utils::static_pointer_cast<texture>(p_texture));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("HighlightTexture")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "HIGHLIGHT");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentHighlightTexture");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_texture = parse_region_(defaulted, layer, "Texture");
        if (p_texture) {
            p_texture->set_special();
            set_highlight_texture(utils::static_pointer_cast<texture>(p_texture));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("NormalText")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "ARTWORK");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentNormalText");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_font_string = parse_region_(defaulted, layer, "FontString");
        if (p_font_string) {
            p_font_string->set_special();
            set_normal_text(utils::static_pointer_cast<font_string>(p_font_string));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("HighlightText")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "HIGHLIGHT");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentHighlightText");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_font_string = parse_region_(defaulted, layer, "FontString");
        if (p_font_string) {
            p_font_string->set_special();
            set_highlight_text(utils::static_pointer_cast<font_string>(p_font_string));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_special_node = node.try_get_child("DisabledText")) {
        auto layer = p_special_node->get_attribute_value_or<std::string>("layer", "BORDER");

        layout_node defaulted = *p_special_node;
        defaulted.get_or_set_attribute_value("name", "$parentDisabledText");
        defaulted.get_or_set_attribute_value("setAllPoints", "true");

        auto p_font_string = parse_region_(defaulted, layer, "FontString");
        if (p_font_string) {
            p_font_string->set_special();
            set_disabled_text(utils::static_pointer_cast<font_string>(p_font_string));
        }

        warn_for_not_accessed_node(defaulted);
        p_special_node->bypass_access_check();
    }

    if (const layout_node* p_offset_block = node.try_get_child("PushedTextOffset")) {
        auto dimensions = parse_dimension_(*p_offset_block);
        if (dimensions.first == anchor_type::abs) {
            set_pushed_text_offset(
                vector2f(dimensions.second.x.value_or(0.0f), dimensions.second.y.value_or(0.0f)));
        } else {
            gui::out << gui::warning << p_offset_block->get_location() << " : "
                     << "RelDimension for Button:PushedTextOffset is not yet supported. Skipped."
                     << std::endl;
        }
    }
}
} // namespace lxgui::gui
