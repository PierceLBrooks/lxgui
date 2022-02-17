#include "lxgui/gui_slider.hpp"

#include "lxgui/gui_alive_checker.hpp"
#include "lxgui/gui_event.hpp"
#include "lxgui/gui_frame.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_region_tpl.hpp"
#include "lxgui/gui_texture.hpp"

#include <algorithm>
#include <sstream>

namespace lxgui::gui {

void step_value(float& value, float step) {
    // Makes the value a multiple of the step :
    // fValue = N*fStep, where N is an integer.
    if (step != 0.0f)
        value = std::round(value / step) * step;
}

slider::slider(utils::control_block& m_block, manager& m_manager) : frame(m_block, m_manager) {
    type_.push_back(class_name);
    enable_mouse(true);
    register_for_drag({"LeftButton"});
}

std::string slider::serialize(const std::string& tab) const {
    std::ostringstream str;

    str << base::serialize(tab);
    str << tab << "  # Orientation: ";
    switch (m_orientation_) {
    case orientation::horizontal: str << "HORIZONTAL"; break;
    case orientation::vertical: str << "VERTICAL"; break;
    }
    str << "\n";
    str << tab << "  # Value      : " << value_ << "\n";
    str << tab << "  # Min value  : " << min_value_ << "\n";
    str << tab << "  # Max value  : " << max_value_ << "\n";
    str << tab << "  # Step       : " << value_step_ << "\n";
    str << tab << "  # Click out  : " << allow_clicks_outside_thumb_ << "\n";

    return str.str();
}

bool slider::can_use_script(const std::string& script_name) const {
    if (base::can_use_script(script_name))
        return true;
    else if (script_name == "OnValueChanged")
        return true;
    else
        return false;
}

void slider::fire_script(const std::string& script_name, const event_data& m_data) {
    alive_checker m_checker(*this);
    base::fire_script(script_name, m_data);
    if (!m_checker.is_alive())
        return;

    if (script_name == "OnDragStart") {
        if (p_thumb_texture_ &&
            p_thumb_texture_->is_in_region({m_data.get<float>(1), m_data.get<float>(2)})) {
            anchor& m_anchor = p_thumb_texture_->modify_point(anchor_point::center);

            get_manager().get_root().start_moving(
                p_thumb_texture_, &m_anchor,
                m_orientation_ == orientation::horizontal ? constraint::x : constraint::y,
                [&]() { constrain_thumb_(); });

            is_thumb_dragged_ = true;
        }
    } else if (script_name == "OnDragStop") {
        if (p_thumb_texture_) {
            if (get_manager().get_root().is_moving(*p_thumb_texture_))
                get_manager().get_root().stop_moving();

            is_thumb_dragged_ = false;
        }
    } else if (script_name == "OnMouseDown") {
        if (allow_clicks_outside_thumb_) {
            const vector2f m_apparent_size = get_apparent_dimensions();

            float value;
            if (m_orientation_ == orientation::horizontal) {
                float offset = m_data.get<float>(1) - border_list_.left;
                value        = offset / m_apparent_size.x;
                set_value(value * (max_value_ - min_value_) + min_value_);
            } else {
                float offset = m_data.get<float>(2) - border_list_.top;
                value        = offset / m_apparent_size.y;
                set_value(value * (max_value_ - min_value_) + min_value_);
            }
        }
    }
}

void slider::copy_from(const region& m_obj) {
    base::copy_from(m_obj);

    const slider* p_slider = down_cast<slider>(&m_obj);
    if (!p_slider)
        return;

    this->set_value_step(p_slider->get_value_step());
    this->set_min_value(p_slider->get_min_value());
    this->set_max_value(p_slider->get_max_value());
    this->set_value(p_slider->get_value());
    this->set_thumb_draw_layer(p_slider->get_thumb_draw_layer());
    this->set_orientation(p_slider->get_orientation());
    this->set_allow_clicks_outside_thumb(p_slider->are_clicks_outside_thumb_allowed());

    if (const texture* p_thumb = p_slider->get_thumb_texture().get()) {
        region_core_attributes m_attr;
        m_attr.name        = p_thumb->get_name();
        m_attr.inheritance = {p_slider->get_thumb_texture()};

        auto p_texture =
            this->create_layered_region<texture>(p_thumb->get_draw_layer(), std::move(m_attr));

        if (p_texture) {
            p_texture->set_special();
            p_texture->notify_loaded();
            this->set_thumb_texture(p_texture);
        }
    }
}

void slider::constrain_thumb_() {
    if (max_value_ == min_value_)
        return;

    const vector2f m_apparent_size = get_apparent_dimensions();

    if ((m_orientation_ == orientation::horizontal && m_apparent_size.x <= 0) ||
        (m_orientation_ == orientation::vertical && m_apparent_size.y <= 0))
        return;

    float old_value = value_;

    if (is_thumb_dragged_) {
        if (m_orientation_ == orientation::horizontal)
            value_ =
                p_thumb_texture_->get_point(anchor_point::center).m_offset.x / m_apparent_size.x;
        else
            value_ =
                p_thumb_texture_->get_point(anchor_point::center).m_offset.y / m_apparent_size.y;

        value_ = value_ * (max_value_ - min_value_) + min_value_;
        value_ = std::clamp(value_, min_value_, max_value_);
        step_value(value_, value_step_);
    }

    float coef = (value_ - min_value_) / (max_value_ - min_value_);

    anchor& m_anchor = p_thumb_texture_->modify_point(anchor_point::center);

    vector2f m_new_offset;
    if (m_orientation_ == orientation::horizontal)
        m_new_offset = vector2f(m_apparent_size.x * coef, 0);
    else
        m_new_offset = vector2f(0, m_apparent_size.y * coef);

    if (m_new_offset != m_anchor.m_offset) {
        m_anchor.m_offset = m_new_offset;
        p_thumb_texture_->notify_borders_need_update();
    }

    if (value_ != old_value)
        fire_script("OnValueChanged");
}

void slider::set_min_value(float min_value) {
    if (min_value != min_value_) {
        min_value_ = min_value;
        if (min_value_ > max_value_)
            min_value_ = max_value_;
        else
            step_value(min_value_, value_step_);

        if (value_ < min_value_) {
            value_ = min_value_;
            fire_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_max_value(float max_value) {
    if (max_value != max_value_) {
        max_value_ = max_value;
        if (max_value_ < min_value_)
            max_value_ = min_value_;
        else
            step_value(max_value_, value_step_);

        if (value_ > max_value_) {
            value_ = max_value_;
            fire_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_min_max_values(float min_value, float max_value) {
    if (min_value != min_value_ || max_value != max_value_) {
        min_value_ = std::min(min_value, max_value);
        max_value_ = std::max(min_value, max_value);
        step_value(min_value_, value_step_);
        step_value(max_value_, value_step_);

        if (value_ > max_value_ || value_ < min_value_) {
            value_ = std::clamp(value_, min_value_, max_value_);
            fire_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_value(float value, bool silent) {
    value = std::clamp(value, min_value_, max_value_);
    step_value(value, value_step_);

    if (value != value_) {
        value_ = value;

        if (!silent)
            fire_script("OnValueChanged");

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_value_step(float value_step) {
    if (value_step_ != value_step) {
        value_step_ = value_step;

        step_value(min_value_, value_step_);
        step_value(max_value_, value_step_);

        float old_value = value_;
        value_          = std::clamp(value_, min_value_, max_value_);
        step_value(value_, value_step_);

        if (value_ != old_value)
            fire_script("OnValueChanged");

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_thumb_texture(utils::observer_ptr<texture> p_texture) {
    p_thumb_texture_ = std::move(p_texture);
    if (!p_thumb_texture_)
        return;

    p_thumb_texture_->set_draw_layer(m_thumb_layer_);
    p_thumb_texture_->clear_all_points();
    p_thumb_texture_->set_point(
        anchor_point::center, p_thumb_texture_->get_parent().get() == this ? "$parent" : name_,
        m_orientation_ == orientation::horizontal ? anchor_point::left : anchor_point::top);

    notify_thumb_texture_needs_update_();
}

void slider::set_orientation(orientation m_orientation) {
    if (m_orientation != m_orientation_) {
        m_orientation_ = m_orientation;
        if (p_thumb_texture_) {
            p_thumb_texture_->set_point(
                anchor_point::center, name_,
                m_orientation_ == orientation::horizontal ? anchor_point::left : anchor_point::top);
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_orientation(const std::string& orientation_name) {
    orientation m_orientation = orientation::horizontal;
    if (orientation_name == "VERTICAL")
        m_orientation = orientation::vertical;
    else if (orientation_name == "HORIZONTAL")
        m_orientation = orientation::horizontal;
    else {
        gui::out << gui::warning << "gui::" << type_.back() << " : Unknown orientation : \""
                 << orientation_name << "\". Using \"HORIZONTAL\"." << std::endl;
    }

    set_orientation(m_orientation);
}

void slider::set_thumb_draw_layer(layer m_thumb_layer) {
    m_thumb_layer_ = m_thumb_layer;
    if (p_thumb_texture_)
        p_thumb_texture_->set_draw_layer(m_thumb_layer_);
}

void slider::set_thumb_draw_layer(const std::string& thumb_layer_name) {
    if (thumb_layer_name == "ARTWORK")
        m_thumb_layer_ = layer::artwork;
    else if (thumb_layer_name == "BACKGROUND")
        m_thumb_layer_ = layer::background;
    else if (thumb_layer_name == "BORDER")
        m_thumb_layer_ = layer::border;
    else if (thumb_layer_name == "HIGHLIGHT")
        m_thumb_layer_ = layer::highlight;
    else if (thumb_layer_name == "OVERLAY")
        m_thumb_layer_ = layer::overlay;
    else {
        gui::out << gui::warning << "gui::" << type_.back()
                 << " : "
                    "Unknown layer type : \"" +
                        thumb_layer_name + "\". Using \"OVERLAY\"."
                 << std::endl;
        m_thumb_layer_ = layer::overlay;
    }

    if (p_thumb_texture_)
        p_thumb_texture_->set_draw_layer(m_thumb_layer_);
}

float slider::get_min_value() const {
    return min_value_;
}

float slider::get_max_value() const {
    return max_value_;
}

float slider::get_value() const {
    return value_;
}

float slider::get_value_step() const {
    return value_step_;
}

slider::orientation slider::get_orientation() const {
    return m_orientation_;
}

layer slider::get_thumb_draw_layer() const {
    return m_thumb_layer_;
}

void slider::set_allow_clicks_outside_thumb(bool allow) {
    allow_clicks_outside_thumb_ = allow;
}

bool slider::are_clicks_outside_thumb_allowed() const {
    return allow_clicks_outside_thumb_;
}

bool slider::is_in_region(const vector2f& m_position) const {
    if (allow_clicks_outside_thumb_) {
        if (base::is_in_region(m_position))
            return true;
    }

    return p_thumb_texture_ && p_thumb_texture_->is_in_region(m_position);
}

void slider::update_thumb_texture_() {
    if (!p_thumb_texture_)
        return;

    if (max_value_ == min_value_) {
        p_thumb_texture_->hide();
        return;
    } else
        p_thumb_texture_->show();

    constrain_thumb_();
}

void slider::notify_borders_need_update() {
    base::notify_borders_need_update();
    notify_thumb_texture_needs_update_();
}

void slider::create_glue() {
    create_glue_(this);
}

void slider::notify_thumb_texture_needs_update_() {
    update_thumb_texture_();
}

} // namespace lxgui::gui
