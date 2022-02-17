#include "lxgui/gui_fontstring.hpp"

#include "lxgui/gui_layeredregion.hpp"
#include "lxgui/gui_localizer.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_region_tpl.hpp"
#include "lxgui/gui_renderer.hpp"

#include <sstream>

namespace lxgui::gui {

font_string::font_string(utils::control_block& m_block, manager& m_manager) :
    layered_region(m_block, m_manager) {
    type_.push_back(class_name);
}

void font_string::render() const {
    if (!p_text_ || !is_ready_ || !is_visible())
        return;

    float f_x = 0.0f, f_y = 0.0f;

    if (std::isinf(p_text_->get_box_width())) {
        switch (m_align_x_) {
        case alignment_x::left: f_x = border_list_.left; break;
        case alignment_x::center: f_x = (border_list_.left + border_list_.right) / 2; break;
        case alignment_x::right: f_x = border_list_.right; break;
        }
    } else {
        f_x = border_list_.left;
    }

    if (std::isinf(p_text_->get_box_height())) {
        switch (m_align_y_) {
        case alignment_y::top: f_y = border_list_.top; break;
        case alignment_y::middle: f_y = (border_list_.top + border_list_.bottom) / 2; break;
        case alignment_y::bottom: f_y = border_list_.bottom; break;
        }
    } else {
        f_y = border_list_.top;
    }

    f_x += m_offset_.x;
    f_y += m_offset_.y;

    p_text_->set_alpha(get_effective_alpha());

    if (has_shadow_) {
        p_text_->set_color(m_shadow_color_, true);
        p_text_->render(
            matrix4f::translation(round_to_pixel(vector2f(f_x, f_y) + m_shadow_offset_)));
    }

    p_text_->set_color(m_text_color_);
    p_text_->render(matrix4f::translation(round_to_pixel(vector2f(f_x, f_y))));
}

std::string font_string::serialize(const std::string& tab) const {
    std::ostringstream str;

    str << base::serialize(tab);

    str << tab << "  # Font name   : " << font_name_ << "\n";
    str << tab << "  # Font height : " << f_height_ << "\n";
    str << tab << "  # Text ready  : " << (p_text_ != nullptr) << "\n";
    str << tab << "  # Text        : \"" << utils::unicode_to_utf8(content_) << "\"\n";
    str << tab << "  # Outlined    : " << is_outlined_ << "\n";
    str << tab << "  # Text color  : " << m_text_color_ << "\n";
    str << tab << "  # Spacing     : " << f_spacing_ << "\n";
    str << tab << "  # Justify     :\n";
    str << tab << "  #-###\n";
    str << tab << "  |   # horizontal : ";
    switch (m_align_x_) {
    case alignment_x::left: str << "LEFT\n"; break;
    case alignment_x::center: str << "CENTER\n"; break;
    case alignment_x::right: str << "RIGHT\n"; break;
    default: str << "<error>\n"; break;
    }
    str << tab << "  |   # vertical   : ";
    switch (m_align_y_) {
    case alignment_y::top: str << "TOP\n"; break;
    case alignment_y::middle: str << "MIDDLE\n"; break;
    case alignment_y::bottom: str << "BOTTOM\n"; break;
    default: str << "<error>\n"; break;
    }
    str << tab << "  #-###\n";
    str << tab << "  # NonSpaceW.  : " << non_space_wrap_enabled_ << "\n";
    if (has_shadow_) {
        str << tab << "  # Shadow off. : (" << m_shadow_offset_.x << ", " << m_shadow_offset_.y
            << ")\n";
        str << tab << "  # Shadow col. : " << m_shadow_color_ << "\n";
    }

    return str.str();
}

void font_string::create_glue() {
    create_glue_(this);
}

void font_string::copy_from(const region& m_obj) {
    base::copy_from(m_obj);

    const font_string* p_font_string = down_cast<font_string>(&m_obj);
    if (!p_font_string)
        return;

    std::string font_name = p_font_string->get_font_name();
    float       f_height  = p_font_string->get_font_height();
    if (!font_name.empty() && f_height != 0)
        this->set_font(font_name, f_height);

    this->set_alignment_x(p_font_string->get_alignment_x());
    this->set_alignment_y(p_font_string->get_alignment_y());
    this->set_spacing(p_font_string->get_spacing());
    this->set_line_spacing(p_font_string->get_line_spacing());
    this->set_text(p_font_string->get_text());
    this->set_outlined(p_font_string->is_outlined());
    if (p_font_string->has_shadow()) {
        this->set_shadow(true);
        this->set_shadow_color(p_font_string->get_shadow_color());
        this->set_shadow_offset(p_font_string->get_shadow_offset());
    }
    this->set_text_color(p_font_string->get_text_color());
    this->set_non_space_wrap(p_font_string->can_non_space_wrap());
}

const std::string& font_string::get_font_name() const {
    return font_name_;
}

float font_string::get_font_height() const {
    return f_height_;
}

void font_string::set_outlined(bool is_outlined) {
    if (is_outlined_ == is_outlined)
        return;

    is_outlined_ = is_outlined;

    create_text_object_();

    notify_renderer_need_redraw();
}

bool font_string::is_outlined() const {
    return is_outlined_;
}

alignment_x font_string::get_alignment_x() const {
    return m_align_x_;
}

alignment_y font_string::get_alignment_y() const {
    return m_align_y_;
}

const color& font_string::get_shadow_color() const {
    return m_shadow_color_;
}

const vector2f& font_string::get_shadow_offset() const {
    return m_shadow_offset_;
}

const vector2f& font_string::get_offset() const {
    return m_offset_;
}

float font_string::get_spacing() const {
    return f_spacing_;
}

float font_string::get_line_spacing() const {
    return f_line_spacing_;
}

const color& font_string::get_text_color() const {
    return m_text_color_;
}

void font_string::notify_scaling_factor_updated() {
    base::notify_scaling_factor_updated();

    if (p_text_)
        set_font(font_name_, f_height_);
}

void font_string::create_text_object_() {
    if (font_name_.empty())
        return;

    std::size_t ui_pixel_height = static_cast<std::size_t>(
        std::round(get_manager().get_interface_scaling_factor() * f_height_));

    auto&       m_renderer  = get_manager().get_renderer();
    const auto& m_localizer = get_manager().get_localizer();

    const auto&    code_points           = m_localizer.get_allowed_code_points();
    const char32_t ui_default_code_point = m_localizer.get_fallback_code_point();

    std::shared_ptr<gui::font> p_outline_font;
    if (is_outlined_) {
        p_outline_font = m_renderer.create_atlas_font(
            "GUI", font_name_, ui_pixel_height,
            std::min<std::size_t>(2u, static_cast<std::size_t>(std::round(0.2 * ui_pixel_height))),
            code_points, ui_default_code_point);
    }

    auto p_font = m_renderer.create_atlas_font(
        "GUI", font_name_, ui_pixel_height, 0u, code_points, ui_default_code_point);

    p_text_ = std::unique_ptr<text>(new text(m_renderer, p_font, p_outline_font));

    p_text_->set_scaling_factor(1.0f / get_manager().get_interface_scaling_factor());
    p_text_->set_remove_starting_spaces(true);
    p_text_->set_text(content_);
    p_text_->set_alignment_x(m_align_x_);
    p_text_->set_alignment_y(m_align_y_);
    p_text_->set_tracking(f_spacing_);
    p_text_->enable_word_wrap(word_wrap_enabled_, ellipsis_enabled_);
    p_text_->enable_formatting(formatting_enabled_);
}

void font_string::set_font(const std::string& font_name, float f_height) {
    font_name_ = parse_file_name(font_name);
    f_height_  = f_height;

    create_text_object_();

    if (!is_virtual_) {
        notify_borders_need_update();
        notify_renderer_need_redraw();
    }
}

void font_string::set_alignment_x(alignment_x m_justify_h) {
    if (m_align_x_ == m_justify_h)
        return;

    m_align_x_ = m_justify_h;
    if (p_text_) {
        p_text_->set_alignment_x(m_align_x_);

        if (!is_virtual_)
            notify_renderer_need_redraw();
    }
}

void font_string::set_alignment_y(alignment_y m_justify_v) {
    if (m_align_y_ == m_justify_v)
        return;

    m_align_y_ = m_justify_v;
    if (p_text_) {
        p_text_->set_alignment_y(m_align_y_);

        if (!is_virtual_)
            notify_renderer_need_redraw();
    }
}

void font_string::set_shadow_color(const color& m_shadow_color) {
    if (m_shadow_color_ == m_shadow_color)
        return;

    m_shadow_color_ = m_shadow_color;
    if (has_shadow_ && !is_virtual_)
        notify_renderer_need_redraw();
}

void font_string::set_shadow_offset(const vector2f& m_shadow_offset) {
    if (m_shadow_offset_ == m_shadow_offset)
        return;

    m_shadow_offset_ = m_shadow_offset;
    if (has_shadow_ && !is_virtual_)
        notify_renderer_need_redraw();
}

void font_string::set_offset(const vector2f& m_offset) {
    if (m_offset_ == m_offset)
        return;

    m_offset_ = m_offset;
    if (!is_virtual_)
        notify_renderer_need_redraw();
}

void font_string::set_spacing(float f_spacing) {
    if (f_spacing_ == f_spacing)
        return;

    f_spacing_ = f_spacing;
    if (p_text_) {
        p_text_->set_tracking(f_spacing_);
        if (!is_virtual_)
            notify_renderer_need_redraw();
    }
}

void font_string::set_line_spacing(float f_line_spacing) {
    if (f_line_spacing_ == f_line_spacing)
        return;

    f_line_spacing_ = f_line_spacing;
    if (p_text_) {
        p_text_->set_line_spacing(f_line_spacing_);
        if (!is_virtual_)
            notify_renderer_need_redraw();
    }
}

void font_string::set_text_color(const color& m_text_color) {
    if (m_text_color_ == m_text_color)
        return;

    m_text_color_ = m_text_color;
    if (!is_virtual_)
        notify_renderer_need_redraw();
}

bool font_string::can_non_space_wrap() const {
    return non_space_wrap_enabled_;
}

float font_string::get_string_height() const {
    if (p_text_)
        return p_text_->get_text_height();
    else
        return 0.0f;
}

float font_string::get_string_width() const {
    if (p_text_)
        return p_text_->get_text_width();
    else
        return 0.0f;
}

float font_string::get_string_width(const utils::ustring& content) const {
    if (p_text_)
        return p_text_->get_string_width(content);
    else
        return 0.0f;
}

const utils::ustring& font_string::get_text() const {
    return content_;
}

void font_string::set_non_space_wrap(bool can_non_space_wrap) {
    if (non_space_wrap_enabled_ == can_non_space_wrap)
        return;

    non_space_wrap_enabled_ = can_non_space_wrap;
    if (!is_virtual_)
        notify_renderer_need_redraw();
}

bool font_string::has_shadow() const {
    return has_shadow_;
}

void font_string::set_shadow(bool has_shadow) {
    if (has_shadow_ == has_shadow)
        return;

    has_shadow_ = has_shadow;
    if (!is_virtual_)
        notify_renderer_need_redraw();
}

void font_string::set_word_wrap(bool can_word_wrap, bool add_ellipsis) {
    word_wrap_enabled_ = can_word_wrap;
    ellipsis_enabled_  = add_ellipsis;
    if (p_text_)
        p_text_->enable_word_wrap(word_wrap_enabled_, ellipsis_enabled_);
}

bool font_string::can_word_wrap() const {
    return word_wrap_enabled_;
}

void font_string::enable_formatting(bool formatting) {
    formatting_enabled_ = formatting;
    if (p_text_)
        p_text_->enable_formatting(formatting_enabled_);
}

bool font_string::is_formatting_enabled() const {
    return formatting_enabled_;
}

void font_string::set_text(const utils::ustring& content) {
    if (content_ == content)
        return;

    content_ = content;
    if (p_text_) {
        p_text_->set_text(content_);
        if (!is_virtual_)
            notify_borders_need_update();
    }
}

text* font_string::get_text_object() {
    return p_text_.get();
}

const text* font_string::get_text_object() const {
    return p_text_.get();
}

void font_string::update_borders_() {
    if (!p_text_)
        return base::update_borders_();

//#define DEBUG_LOG(msg) gui::out << (msg) << std::endl
#define DEBUG_LOG(msg)

    const bool old_ready       = is_ready_;
    const auto old_border_list = border_list_;
    is_ready_                  = true;

    if (!anchor_list_.empty()) {
        float f_left = 0.0f, f_right = 0.0f, f_top = 0.0f, f_bottom = 0.0f;
        float f_x_center = 0.0f, f_y_center = 0.0f;

        DEBUG_LOG("  Read anchors");
        read_anchors_(f_left, f_right, f_top, f_bottom, f_x_center, f_y_center);

        float f_box_width = std::numeric_limits<float>::infinity();
        if (get_dimensions().x != 0.0f)
            f_box_width = get_dimensions().x;
        else if (defined_border_list_.left && defined_border_list_.right)
            f_box_width = f_right - f_left;

        float f_box_height = std::numeric_limits<float>::infinity();
        if (get_dimensions().y != 0.0f)
            f_box_height = get_dimensions().y;
        else if (defined_border_list_.top && defined_border_list_.bottom)
            f_box_height = f_bottom - f_top;

        f_box_width  = round_to_pixel(f_box_width, utils::rounding_method::nearest_not_zero);
        f_box_height = round_to_pixel(f_box_height, utils::rounding_method::nearest_not_zero);

        p_text_->set_dimensions(f_box_width, f_box_height);

        DEBUG_LOG("  Make borders");
        if (std::isinf(f_box_height))
            f_box_height = p_text_->get_height();
        if (std::isinf(f_box_width))
            f_box_width = p_text_->get_width();

        if (!make_borders_(f_top, f_bottom, f_y_center, f_box_height))
            is_ready_ = false;
        if (!make_borders_(f_left, f_right, f_x_center, f_box_width))
            is_ready_ = false;

        if (is_ready_) {
            if (f_right < f_left)
                f_right = f_left + 1.0f;
            if (f_bottom < f_top)
                f_bottom = f_top + 1.0f;

            border_list_.left   = f_left;
            border_list_.right  = f_right;
            border_list_.top    = f_top;
            border_list_.bottom = f_bottom;
        } else
            border_list_ = bounds2f::zero;
    } else {
        float f_box_width = get_dimensions().x;
        if (f_box_width == 0.0)
            f_box_width = p_text_->get_width();

        float f_box_height = get_dimensions().y;
        if (f_box_height == 0.0)
            f_box_height = p_text_->get_height();

        border_list_ = bounds2f(0.0, 0.0, f_box_width, f_box_height);
        is_ready_    = false;
    }

    border_list_.left   = round_to_pixel(border_list_.left);
    border_list_.right  = round_to_pixel(border_list_.right);
    border_list_.top    = round_to_pixel(border_list_.top);
    border_list_.bottom = round_to_pixel(border_list_.bottom);

    if (border_list_ != old_border_list || is_ready_ != old_ready) {
        DEBUG_LOG("  Fire redraw");
        notify_renderer_need_redraw();
    }
    DEBUG_LOG("  @");
}

} // namespace lxgui::gui
