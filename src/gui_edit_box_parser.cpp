#include "lxgui/gui_edit_box.hpp"
#include "lxgui/gui_font_string.hpp"
#include "lxgui/gui_layout_node.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_parser_common.hpp"

namespace lxgui::gui {
void edit_box::parse_attributes_(const layout_node& node) {
    base::parse_attributes_(node);

    if (const layout_attribute* attr = node.try_get_attribute("letters"))
        set_max_letters(attr->get_value<std::size_t>());

    if (const layout_attribute* attr = node.try_get_attribute("blinkPeriod"))
        set_blink_period(attr->get_value<float>());

    if (const layout_attribute* attr = node.try_get_attribute("numeric"))
        set_numeric_only(attr->get_value<bool>());

    if (const layout_attribute* attr = node.try_get_attribute("positive"))
        set_positive_only(attr->get_value<bool>());

    if (const layout_attribute* attr = node.try_get_attribute("integer"))
        set_integer_only(attr->get_value<bool>());

    if (const layout_attribute* attr = node.try_get_attribute("password"))
        enable_password_mode(attr->get_value<bool>());

    if (const layout_attribute* attr = node.try_get_attribute("multiLine"))
        set_multi_line(attr->get_value<bool>());

    if (const layout_attribute* attr = node.try_get_attribute("historyLines"))
        set_max_history_lines(attr->get_value<std::size_t>());

    if (const layout_attribute* attr = node.try_get_attribute("ignoreArrows"))
        set_arrows_ignored(attr->get_value<bool>());
}

void edit_box::parse_all_nodes_before_children_(const layout_node& node) {
    base::parse_all_nodes_before_children_(node);

    parse_text_insets_node_(node);
    parse_font_string_node_(node);

    if (const layout_node* highlight_node = node.try_get_child("HighlightColor"))
        set_highlight_color(parse_color_node_(*highlight_node));
}

void edit_box::parse_font_string_node_(const layout_node& node) {
    if (const layout_node* font_string_node = node.try_get_child("FontString")) {
        layout_node defaulted = *font_string_node;
        defaulted.get_or_set_attribute_value("name", "$parentFontString");

        auto fstr = parse_region_(defaulted, "ARTWORK", "FontString");
        if (fstr) {
            fstr->set_manually_inherited(true);
            set_font_string(utils::static_pointer_cast<font_string>(fstr));
        }

        if (const layout_node* error_node = defaulted.try_get_child("Anchors")) {
            gui::out << gui::warning << error_node->get_location() << ": "
                     << "edit_box: font_string's anchors will be ignored." << std::endl;
        }

        if (const layout_node* error_node = defaulted.try_get_child("Size")) {
            gui::out << gui::warning << error_node->get_location() << ": "
                     << "edit_box: font_string's Size will be ignored." << std::endl;
        }

        warn_for_not_accessed_node(defaulted);
        font_string_node->bypass_access_check();
    }
}

void edit_box::parse_text_insets_node_(const layout_node& node) {
    if (const layout_node* text_insets_node = node.try_get_child("TextInsets")) {
        set_text_insets(bounds2f(
            text_insets_node->get_attribute_value_or<float>("left", 0.0f),
            text_insets_node->get_attribute_value_or<float>("right", 0.0f),
            text_insets_node->get_attribute_value_or<float>("top", 0.0f),
            text_insets_node->get_attribute_value_or<float>("bottom", 0.0f)));
    }
}
} // namespace lxgui::gui
