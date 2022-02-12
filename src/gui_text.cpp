#include "lxgui/gui_text.hpp"

#include "lxgui/gui_exception.hpp"
#include "lxgui/gui_font.hpp"
#include "lxgui/gui_material.hpp"
#include "lxgui/gui_matrix4.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_quad.hpp"
#include "lxgui/gui_renderer.hpp"
#include "lxgui/gui_vertexcache.hpp"
#include "lxgui/utils.hpp"
#include "lxgui/utils_range.hpp"

#include <map>

// #define DEBUG_LOG(msg) gui::out << (msg) << std::endl
#define DEBUG_LOG(msg)

namespace lxgui::gui {

/** \cond INCLUDE_INTERNALS_IN_DOC
 */
namespace parser {
enum class color_action { none, set, reset };

struct format {
    color        m_color        = color::white;
    color_action m_color_action = color_action::none;
};

struct texture {
    std::string               s_file_name;
    float                     f_width  = 0.0f;
    float                     f_height = 0.0f;
    std::shared_ptr<material> p_material;
};

using item = std::variant<char32_t, format, texture>;

struct line {
    std::vector<item> l_content;
    float             f_width = 0.0f;
};

std::vector<item>
parse_string(renderer& m_renderer, const utils::ustring_view& sCaption, bool b_formatting_enabled) {
    std::vector<item> l_content;
    for (auto iter_char = sCaption.begin(); iter_char != sCaption.end(); ++iter_char) {
        // Read format tags
        if (*iter_char == U'|' && b_formatting_enabled) {
            ++iter_char;
            if (iter_char == sCaption.end())
                break;

            if (*iter_char != U'|') {
                if (*iter_char == U'r') {
                    format m_format;
                    m_format.m_color_action = color_action::reset;
                    l_content.push_back(m_format);
                } else if (*iter_char == U'c') {
                    format m_format;
                    m_format.m_color_action = color_action::set;

                    auto m_read_two = [&](float& f_out) {
                        ++iter_char;
                        if (iter_char == sCaption.end())
                            return false;
                        utils::ustring s_color_part(2, U'0');
                        s_color_part[0] = *iter_char;
                        ++iter_char;
                        if (iter_char == sCaption.end())
                            return false;
                        s_color_part[1] = *iter_char;
                        f_out = utils::hex_to_uint(utils::unicode_to_utf8(s_color_part)) / 255.0f;
                        return true;
                    };

                    if (!m_read_two(m_format.m_color.a))
                        break;
                    if (!m_read_two(m_format.m_color.r))
                        break;
                    if (!m_read_two(m_format.m_color.g))
                        break;
                    if (!m_read_two(m_format.m_color.b))
                        break;

                    l_content.push_back(m_format);
                } else if (*iter_char == U'T') {
                    ++iter_char;

                    const auto ui_begin = iter_char - sCaption.begin();
                    const auto ui_pos   = sCaption.find(U"|t", ui_begin);
                    if (ui_pos == sCaption.npos)
                        break;

                    const std::string s_extracted =
                        utils::unicode_to_utf8(sCaption.substr(ui_begin, ui_pos - ui_begin));

                    const auto l_words = utils::cut(s_extracted, ":");
                    if (!l_words.empty()) {
                        texture m_texture;
                        m_texture.p_material = m_renderer.create_material(std::string{l_words[0]});
                        m_texture.f_width    = m_texture.f_height =
                            std::numeric_limits<float>::quiet_NaN();

                        if (l_words.size() == 2) {
                            utils::from_string(l_words[1], m_texture.f_width);
                            m_texture.f_height = m_texture.f_width;
                        } else if (l_words.size() > 2) {
                            utils::from_string(l_words[1], m_texture.f_width);
                            utils::from_string(l_words[2], m_texture.f_height);
                        }

                        l_content.push_back(m_texture);
                    }

                    iter_char += s_extracted.size() + 1;
                }

                continue;
            }
        }

        // Add characters
        l_content.push_back(*iter_char);
    }

    return l_content;
}

bool is_whitespace(const item& m_item) {
    return std::visit(
        [](const auto& m_value) {
            using type = std::decay_t<decltype(m_value)>;
            if constexpr (std::is_same_v<type, char32_t>) {
                return utils::is_whitespace(m_value);
            } else {
                return false;
            }
        },
        m_item);
}

bool is_word(const item& m_item) {
    return std::visit(
        [](const auto& m_value) {
            using type = std::decay_t<decltype(m_value)>;
            if constexpr (std::is_same_v<type, char32_t>) {
                return !utils::is_whitespace(m_value);
            } else {
                return false;
            }
        },
        m_item);
}

bool is_character(const item& m_item) {
    return m_item.index() == 0u;
}

bool is_format(const item& m_item) {
    return m_item.index() == 1u;
}

bool is_character(const item& m_item, char32_t ui_char) {
    return m_item.index() == 0u && std::get<char32_t>(m_item) == ui_char;
}

float get_width(const text& m_text, const item& m_item) {
    return std::visit(
        [&](const auto& m_value) {
            using type = std::decay_t<decltype(m_value)>;
            if constexpr (std::is_same_v<type, char32_t>) {
                return m_text.get_character_width(m_value);
            } else if constexpr (std::is_same_v<type, texture>) {
                if (std::isnan(m_value.f_width))
                    return m_text.get_line_height();
                else
                    return m_value.f_width * m_text.get_scaling_factor();
            } else {
                return 0.0f;
            }
        },
        m_item);
}

float get_kerning(const text& mText, const item& m_item1, const item& m_item2) {
    return std::visit(
        [&](const auto& m_value1) {
            using type1 = std::decay_t<decltype(m_value1)>;
            if constexpr (std::is_same_v<type1, char32_t>) {
                return std::visit(
                    [&](const auto& m_value2) {
                        using type2 = std::decay_t<decltype(m_value2)>;
                        if constexpr (std::is_same_v<type2, char32_t>) {
                            return mText.get_character_kerning(m_value1, m_value2);
                        } else {
                            return 0.0f;
                        }
                    },
                    m_item2);
            } else {
                return 0.0f;
            }
        },
        m_item1);
}

float get_tracking(const text& m_text, const item& m_item) {
    return std::visit(
        [&](const auto& m_value) {
            using type = std::decay_t<decltype(m_value)>;
            if constexpr (std::is_same_v<type, char32_t>) {
                if (m_value != U'\n')
                    return m_text.get_tracking();
                else
                    return 0.0f;
            } else {
                return 0.0f;
            }
        },
        m_item);
}

std::pair<float, float> get_advance(
    const text&                       m_text,
    std::vector<item>::const_iterator iter_char,
    std::vector<item>::const_iterator iter_begin) {
    float f_advance = parser::get_width(m_text, *iter_char);
    float f_kerning = 0.0f;

    auto iter_prev = iter_char;
    while (iter_prev != iter_begin) {
        --iter_prev;
        if (parser::is_format(*iter_prev))
            continue;

        f_kerning = parser::get_tracking(m_text, *iter_char);

        if (!parser::is_whitespace(*iter_char) && !parser::is_whitespace(*iter_prev))
            f_kerning += parser::get_kerning(m_text, *iter_prev, *iter_char);

        break;
    }

    return std::make_pair(f_kerning, f_advance);
}

float get_full_advance(
    const text&                       m_text,
    std::vector<item>::const_iterator iter_char,
    std::vector<item>::const_iterator iter_begin) {
    const auto m_advance = get_advance(m_text, iter_char, iter_begin);
    return m_advance.first + m_advance.second;
}

float get_string_width(const text& m_text, const std::vector<item>& l_content) {
    float f_width     = 0.0f;
    float f_max_width = 0.0f;

    for (auto iter_char : utils::range::iterator(l_content)) {
        if (parser::is_character(*iter_char, U'\n')) {
            if (f_width > f_max_width)
                f_max_width = f_width;

            f_width = 0.0f;
        } else {
            f_width += parser::get_full_advance(m_text, iter_char, l_content.begin());
        }
    }

    if (f_width > f_max_width)
        f_max_width = f_width;

    return f_max_width;
}
} // namespace parser
/** \endcond
 */

text::text(
    renderer&                  m_renderer,
    std::shared_ptr<gui::font> p_font,
    std::shared_ptr<gui::font> p_outline_font) :
    m_renderer_(m_renderer),
    p_font_(std::move(p_font)),
    p_outline_font_(std::move(p_outline_font)) {
    if (!p_font_)
        return;

    b_ready_ = true;
}

float text::get_line_height() const {
    if (p_font_)
        return p_font_->get_size() * f_scaling_factor_;
    else
        return 0.0;
}

void text::set_scaling_factor(float f_scaling_factor) {
    if (f_scaling_factor_ == f_scaling_factor)
        return;

    f_scaling_factor_ = f_scaling_factor;
    notify_cache_dirty_();
}

float text::get_scaling_factor() const {
    return f_scaling_factor_;
}

void text::set_text(const utils::ustring& s_text) {
    if (s_unicode_text_ != s_text) {
        s_unicode_text_ = s_text;
        notify_cache_dirty_();
    }
}

const utils::ustring& text::get_text() const {
    return s_unicode_text_;
}

void text::set_color(const color& m_color, bool b_force_color) {
    if (m_color_ != m_color || b_force_color_ != b_force_color) {
        m_color_       = m_color;
        b_force_color_ = b_force_color;
        if (m_renderer_.is_vertex_cache_enabled())
            notify_cache_dirty_();
    }
}

const color& text::get_color() const {
    return m_color_;
}

void text::set_alpha(float f_alpha) {
    if (f_alpha == f_alpha_)
        return;

    f_alpha_ = f_alpha;
    if (m_renderer_.is_vertex_cache_enabled())
        notify_cache_dirty_();
}

float text::get_alpha() const {
    return f_alpha_;
}

void text::set_dimensions(float f_w, float f_h) {
    if (f_box_w_ != f_w || f_box_h_ != f_h) {
        f_box_w_ = f_w;
        f_box_h_ = f_h;
        notify_cache_dirty_();
    }
}

void text::set_box_width(float f_box_w) {
    if (f_box_w_ != f_box_w) {
        f_box_w_ = f_box_w;
        notify_cache_dirty_();
    }
}

void text::set_box_height(float f_box_h) {
    if (f_box_h_ != f_box_h) {
        f_box_h_ = f_box_h;
        notify_cache_dirty_();
    }
}

float text::get_width() const {
    update_();

    return f_w_;
}

float text::get_height() const {
    update_();

    return f_h_;
}

float text::get_box_width() const {
    return f_box_w_;
}

float text::get_box_height() const {
    return f_box_h_;
}

float text::get_text_width() const {
    return get_string_width(s_unicode_text_);
}

float text::get_text_height() const {
    if (!b_ready_)
        return 0.0f;

    std::size_t count    = std::count(s_unicode_text_.begin(), s_unicode_text_.end(), U'\n');
    float       f_height = (1.0f + count * f_line_spacing_) * get_line_height();

    return f_height;
}

std::size_t text::get_num_lines() const {
    update_();
    return ui_num_lines_;
}

float text::get_string_width(const std::string& s_string) const {
    return get_string_width(utils::utf8_to_unicode(s_string));
}

float text::get_string_width(const utils::ustring& s_string) const {
    if (!b_ready_)
        return 0.0f;

    return parser::get_string_width(
        *this, parser::parse_string(m_renderer_, s_string, b_formatting_enabled_));
}

float text::get_character_width(char32_t ui_char) const {
    if (b_ready_) {
        if (ui_char == U'\t')
            return 4.0f * p_font_->get_character_width(U' ') * f_scaling_factor_;
        else
            return p_font_->get_character_width(ui_char) * f_scaling_factor_;
    } else
        return 0.0f;
}

float text::get_character_kerning(char32_t ui_char1, char32_t ui_char2) const {
    return p_font_->get_character_kerning(ui_char1, ui_char2) * f_scaling_factor_;
}

void text::set_alignment_x(const alignment_x& m_align_x) {
    if (m_align_x_ != m_align_x) {
        m_align_x_ = m_align_x;
        notify_cache_dirty_();
    }
}

void text::set_alignment_y(const alignment_y& m_align_y) {
    if (m_align_y_ != m_align_y) {
        m_align_y_ = m_align_y;
        notify_cache_dirty_();
    }
}

const alignment_x& text::get_alignment_x() const {
    return m_align_x_;
}

const alignment_y& text::get_alignment_y() const {
    return m_align_y_;
}

void text::set_tracking(float f_tracking) {
    if (f_tracking_ != f_tracking) {
        f_tracking_ = f_tracking;
        notify_cache_dirty_();
    }
}

float text::get_tracking() const {
    return f_tracking_;
}

void text::set_line_spacing(float f_line_spacing) {
    if (f_line_spacing_ != f_line_spacing) {
        f_line_spacing_ = f_line_spacing;
        notify_cache_dirty_();
    }
}

float text::get_line_spacing() const {
    return f_line_spacing_;
}

void text::set_remove_starting_spaces(bool b_remove_starting_spaces) {
    if (b_remove_starting_spaces_ != b_remove_starting_spaces) {
        b_remove_starting_spaces_ = b_remove_starting_spaces;
        notify_cache_dirty_();
    }
}

bool text::get_remove_starting_spaces() const {
    return b_remove_starting_spaces_;
}

void text::enable_word_wrap(bool b_wrap, bool b_add_ellipsis) {
    if (b_word_wrap_ != b_wrap || b_add_ellipsis_ != b_add_ellipsis) {
        b_word_wrap_    = b_wrap;
        b_add_ellipsis_ = b_add_ellipsis;
        notify_cache_dirty_();
    }
}

bool text::is_word_wrap_enabled() const {
    return b_word_wrap_;
}

void text::enable_formatting(bool b_formatting) {
    if (b_formatting != b_formatting_enabled_) {
        b_formatting_enabled_ = b_formatting;
        if (m_renderer_.is_vertex_cache_enabled())
            notify_cache_dirty_();
    }
}

void text::render(const matrix4f& m_transform) const {
    if (!b_ready_ || s_unicode_text_.empty())
        return;

    bool b_use_vertex_cache =
        m_renderer_.is_vertex_cache_enabled() && !m_renderer_.is_quad_batching_enabled();

    if ((b_use_vertex_cache && !p_vertex_cache_) || (b_use_vertex_cache && l_quad_list_.empty()))
        b_update_cache_ = true;

    update_();

    if (p_outline_font_) {
        if (const auto p_mat = p_outline_font_->get_texture().lock()) {
            if (b_use_vertex_cache && p_outline_vertex_cache_) {
                m_renderer_.render_cache(p_mat.get(), *p_outline_vertex_cache_, m_transform);
            } else {
                std::vector<std::array<vertex, 4>> l_quads_copy = l_outline_quad_list_;
                for (auto& m_quad : l_quads_copy)
                    for (std::size_t i = 0; i < 4; ++i) {
                        m_quad[i].pos = m_quad[i].pos * m_transform;
                        m_quad[i].col.a *= f_alpha_;
                    }

                m_renderer_.render_quads(p_mat.get(), l_quads_copy);
            }
        }
    }

    if (const auto p_mat = p_font_->get_texture().lock()) {
        if (b_use_vertex_cache && p_vertex_cache_) {
            m_renderer_.render_cache(p_mat.get(), *p_vertex_cache_, m_transform);
        } else {
            std::vector<std::array<vertex, 4>> l_quads_copy = l_quad_list_;
            for (auto& m_quad : l_quads_copy)
                for (std::size_t i = 0; i < 4; ++i) {
                    m_quad[i].pos = m_quad[i].pos * m_transform;

                    if (!b_formatting_enabled_ || b_force_color_ || m_quad[i].col == color::empty) {
                        m_quad[i].col = m_color_;
                    }

                    m_quad[i].col.a *= f_alpha_;
                }

            m_renderer_.render_quads(p_mat.get(), l_quads_copy);
        }

        for (auto m_quad : l_icons_list_) {
            for (std::size_t i = 0; i < 4; ++i) {
                m_quad.v[i].pos = m_quad.v[i].pos * m_transform;
                m_quad.v[i].col.a *= f_alpha_;
            }

            m_renderer_.render_quad(m_quad);
        }
    }
}

void text::notify_cache_dirty_() const {
    b_update_cache_ = true;
}

float text::round_to_pixel_(float f_value, utils::rounding_method m_method) const {
    return utils::round(f_value, f_scaling_factor_, m_method);
}

void text::update_() const {
    if (!b_ready_ || !b_update_cache_)
        return;

    // Update the line list, read format tags, do word wrapping, ...
    std::vector<parser::line> l_line_list;

    DEBUG_LOG("     Get max line nbr");
    std::size_t ui_max_line_nbr = 0;
    if (f_box_h_ != 0.0f && !std::isinf(f_box_h_)) {
        if (f_box_h_ < get_line_height()) {
            ui_max_line_nbr = 0;
        } else {
            float f_remaining = f_box_h_ - get_line_height();
            ui_max_line_nbr   = 1 + static_cast<std::size_t>(std::floor(
                                      f_remaining / (get_line_height() * f_line_spacing_)));
        }
    } else
        ui_max_line_nbr = std::numeric_limits<std::size_t>::max();

    if (ui_max_line_nbr != 0) {
        auto l_manual_line_list = utils::cut_each(s_unicode_text_, U"\n");
        for (auto iter_manual : utils::range::iterator(l_manual_line_list)) {
            DEBUG_LOG("     Line : '" + utils::unicode_to_utf8(*iterManual) + "'");

            // Parse the line
            std::vector<parser::item> l_parsed_content =
                parser::parse_string(m_renderer_, *iter_manual, b_formatting_enabled_);

            // Make a temporary line array
            std::vector<parser::line> l_lines;

            auto         iter_line_begin = l_parsed_content.begin();
            parser::line m_line;
            m_line.f_width = 0.0f;

            bool b_done = false;
            for (auto iter_char1 = l_parsed_content.begin(); iter_char1 != l_parsed_content.end();
                 ++iter_char1) {
                DEBUG_LOG("      Get width");
                m_line.f_width += parser::get_full_advance(*this, iter_char1, iter_line_begin);
                m_line.l_content.push_back(*iter_char1);

                if (round_to_pixel_(m_line.f_width - f_box_w_) > 0) {
                    DEBUG_LOG(
                        "      Box break " + utils::to_string(mLine.fWidth) + " > " +
                        utils::to_string(fBoxW_));

                    // Whoops, the line is too long...
                    auto m_iter_space = std::find_if(
                        m_line.l_content.begin(), m_line.l_content.end(), &parser::is_whitespace);

                    if (m_iter_space != m_line.l_content.end() && b_word_wrap_) {
                        DEBUG_LOG("       Spaced");
                        // There are several words on this line, we'll
                        // be able to put the last one on the next line
                        auto                      iter_char2 = iter_char1 + 1;
                        std::vector<parser::item> l_erased_content;
                        std::size_t               ui_char_to_erase  = 0;
                        float                     f_last_word_width = 0.0f;
                        bool                      b_last_was_word   = false;
                        while (m_line.f_width > f_box_w_ && iter_char2 != iter_line_begin) {
                            --iter_char2;

                            if (parser::is_whitespace(*iter_char2)) {
                                if (!b_last_was_word || b_remove_starting_spaces_ ||
                                    m_line.f_width - f_last_word_width > f_box_w_) {
                                    f_last_word_width += parser::get_full_advance(
                                        *this, iter_char2, iter_line_begin);
                                    l_erased_content.insert(l_erased_content.begin(), *iter_char2);
                                    ++ui_char_to_erase;

                                    m_line.f_width -= f_last_word_width;
                                    f_last_word_width = 0.0f;
                                } else
                                    break;
                            } else {
                                f_last_word_width +=
                                    parser::get_full_advance(*this, iter_char2, iter_line_begin);
                                l_erased_content.insert(l_erased_content.begin(), *iter_char2);
                                ++ui_char_to_erase;

                                b_last_was_word = true;
                            }
                        }

                        if (b_remove_starting_spaces_) {
                            while (iter_char2 != iter_char1 + 1 &&
                                   parser::is_whitespace(*iter_char2)) {
                                --ui_char_to_erase;
                                l_erased_content.erase(l_erased_content.begin());
                                ++iter_char2;
                            }
                        }

                        m_line.f_width -= f_last_word_width;
                        m_line.l_content.erase(
                            m_line.l_content.end() - ui_char_to_erase, m_line.l_content.end());
                        l_lines.push_back(m_line);

                        m_line.f_width   = parser::get_string_width(*this, l_erased_content);
                        m_line.l_content = l_erased_content;
                        iter_line_begin  = iter_char1 - (m_line.l_content.size() - 1u);
                    } else {
                        DEBUG_LOG("       Single word");
                        // There is only one word on this line, or word
                        // wrap is disabled. Anyway, this line is just
                        // too long for the text box : our only option
                        // is to truncate it.
                        if (b_add_ellipsis_) {
                            DEBUG_LOG("       Ellipsis");
                            // FIXME: this doesn't account for kerning between the "..." and prev
                            // char
                            float       f_word_width     = get_string_width(U"...");
                            auto        iter_char2       = iter_char1 + 1;
                            std::size_t ui_char_to_erase = 0;
                            while (m_line.f_width + f_word_width > f_box_w_ &&
                                   iter_char2 != iter_line_begin) {
                                --iter_char2;
                                m_line.f_width -=
                                    parser::get_full_advance(*this, iter_char2, iter_line_begin);
                                ++ui_char_to_erase;
                            }

                            DEBUG_LOG(
                                "       Char to erase : " + utils::to_string(uiCharToErase) +
                                " / " + utils::to_string(mLine.lContent.size()));

                            m_line.l_content.erase(
                                m_line.l_content.end() - ui_char_to_erase, m_line.l_content.end());
                            m_line.l_content.push_back(U'.');
                            m_line.l_content.push_back(U'.');
                            m_line.l_content.push_back(U'.');
                            m_line.f_width += f_word_width;
                        } else {
                            DEBUG_LOG("       Truncate");
                            auto        iter_char2       = iter_char1 + 1;
                            std::size_t ui_char_to_erase = 0;
                            while (m_line.f_width > f_box_w_ && iter_char2 != iter_line_begin) {
                                --iter_char2;
                                m_line.f_width -=
                                    parser::get_full_advance(*this, iter_char2, iter_line_begin);
                                ++ui_char_to_erase;
                            }

                            m_line.l_content.erase(
                                m_line.l_content.end() - ui_char_to_erase, m_line.l_content.end());
                        }

                        if (!b_word_wrap_) {
                            DEBUG_LOG("       Display single line");
                            // Word wrap is disabled, so we can only display one line anyway.
                            l_line_list.push_back(m_line);
                            b_done = true;
                            break;
                        }

                        // Add the line
                        l_lines.push_back(m_line);
                        m_line.f_width = 0.0f;
                        m_line.l_content.clear();

                        DEBUG_LOG("       Continue");

                        // Skip all following content (which we cannot display) until next
                        // whitespace
                        auto iter_temp = iter_char1;
                        iter_char1     = std::find_if(
                            iter_char1, l_parsed_content.end(), &parser::is_whitespace);

                        if (iter_char1 == l_parsed_content.end())
                            break;

                        // Apply the format tags that were cut
                        for (; iter_temp != iter_char1; ++iter_temp) {
                            std::visit(
                                [&](const auto& m_value) {
                                    using type = std::decay_t<decltype(m_value)>;
                                    if constexpr (std::is_same_v<type, parser::format>) {
                                        m_line.l_content.push_back(m_value);
                                    }
                                },
                                *iter_temp);
                        }

                        // Look for the next word
                        iter_char1 =
                            std::find_if(iter_char1, l_parsed_content.end(), &parser::is_word);
                        if (iter_char1 != l_parsed_content.end())
                            break;

                        --iter_char1;
                        iter_line_begin = iter_char1;
                    }
                }
            }

            if (b_done)
                break;

            DEBUG_LOG("     End");

            l_lines.push_back(m_line);

            // Add the maximum number of line to this text
            for (auto& s_line : l_lines) {
                l_line_list.push_back(std::move(s_line));
                if (l_line_list.size() == ui_max_line_nbr) {
                    b_done = true;
                    break;
                }
            }

            if (b_done)
                break;
            DEBUG_LOG("     .");
        }
    }

    ui_num_lines_ = l_line_list.size();

    l_quad_list_.clear();
    l_outline_quad_list_.clear();
    l_icons_list_.clear();

    if (!l_line_list.empty()) {
        if (f_box_w_ == 0.0f || std::isinf(f_box_w_)) {
            f_w_ = 0.0f;
            for (const auto& m_line : l_line_list)
                f_w_ = std::max(f_w_, m_line.f_width);
        } else
            f_w_ = f_box_w_;

        f_h_ = (1.0f + static_cast<float>(l_line_list.size() - 1) * f_line_spacing_) *
               get_line_height();

        float fY   = 0.0f;
        float f_x0 = 0.0f;

        if (f_box_w_ != 0.0f && !std::isinf(f_box_w_)) {
            switch (m_align_x_) {
            case alignment_x::left: f_x0 = 0.0f; break;
            case alignment_x::center: f_x0 = f_box_w_ * 0.5f; break;
            case alignment_x::right: f_x0 = f_box_w_; break;
            }
        } else
            f_x0 = 0.0f;

        if (!std::isinf(f_box_h_)) {
            switch (m_align_y_) {
            case alignment_y::top: fY = 0.0f; break;
            case alignment_y::middle: fY = (f_box_h_ - f_h_) * 0.5f; break;
            case alignment_y::bottom: fY = (f_box_h_ - f_h_); break;
            }
        } else {
            switch (m_align_y_) {
            case alignment_y::top: fY = 0.0f; break;
            case alignment_y::middle: fY = -f_h_ * 0.5f; break;
            case alignment_y::bottom: fY = -f_h_; break;
            }
        }

        f_x0 = round_to_pixel_(f_x0);
        fY   = round_to_pixel_(fY);

        std::vector<color> l_color_stack;

        for (const auto& m_line : l_line_list) {
            float fX = 0.0f;
            switch (m_align_x_) {
            case alignment_x::left: fX = 0.0f; break;
            case alignment_x::center: fX = -m_line.f_width * 0.5f; break;
            case alignment_x::right: fX = -m_line.f_width; break;
            }

            fX = round_to_pixel_(fX) + f_x0;

            for (auto iter_char : utils::range::iterator(m_line.l_content)) {
                const auto m_advance =
                    parser::get_advance(*this, iter_char, m_line.l_content.begin());

                fX += m_advance.first;

                std::visit(
                    [&](const auto& m_value) {
                        using type = std::decay_t<decltype(m_value)>;
                        if constexpr (std::is_same_v<type, parser::format>) {
                            switch (m_value.m_color_action) {
                            case parser::color_action::set:
                                l_color_stack.push_back(m_value.m_color);
                                break;
                            case parser::color_action::reset: l_color_stack.pop_back(); break;
                            default: break;
                            }
                        } else if constexpr (std::is_same_v<type, parser::texture>) {
                            float f_tex_width = 0.0f, f_tex_height = 0.0f;
                            if (std::isnan(m_value.f_width)) {
                                f_tex_width  = get_line_height();
                                f_tex_height = get_line_height();
                            } else {
                                f_tex_width  = m_value.f_width * get_scaling_factor();
                                f_tex_height = m_value.f_height * get_scaling_factor();
                            }

                            f_tex_width  = round_to_pixel_(f_tex_width);
                            f_tex_height = round_to_pixel_(f_tex_height);

                            quad m_icon;
                            m_icon.mat      = m_value.p_material;
                            m_icon.v[0].pos = vector2f(0.0f, 0.0f);
                            m_icon.v[1].pos = vector2f(f_tex_width, 0.0f);
                            m_icon.v[2].pos = vector2f(f_tex_width, f_tex_height);
                            m_icon.v[3].pos = vector2f(0.0f, f_tex_height);
                            if (m_icon.mat) {
                                m_icon.v[0].uvs =
                                    m_icon.mat->get_canvas_uv(vector2f(0.0f, 0.0f), true);
                                m_icon.v[1].uvs =
                                    m_icon.mat->get_canvas_uv(vector2f(1.0f, 0.0f), true);
                                m_icon.v[2].uvs =
                                    m_icon.mat->get_canvas_uv(vector2f(1.0f, 1.0f), true);
                                m_icon.v[3].uvs =
                                    m_icon.mat->get_canvas_uv(vector2f(0.0f, 1.0f), true);
                            }

                            for (std::size_t i = 0; i < 4; ++i) {
                                m_icon.v[i].pos +=
                                    vector2f(round_to_pixel_(fX), round_to_pixel_(fY));
                            }

                            l_icons_list_.push_back(m_icon);
                        } else if constexpr (std::is_same_v<type, char32_t>) {
                            if (p_outline_font_) {
                                std::array<vertex, 4> l_vertex_list =
                                    create_outline_letter_quad_(m_value);
                                for (std::size_t i = 0; i < 4; ++i) {
                                    l_vertex_list[i].pos +=
                                        vector2f(round_to_pixel_(fX), round_to_pixel_(fY));
                                    l_vertex_list[i].col = color::black;
                                }

                                l_outline_quad_list_.push_back(l_vertex_list);
                            }

                            std::array<vertex, 4> l_vertex_list = create_letter_quad_(m_value);
                            for (std::size_t i = 0; i < 4; ++i) {
                                l_vertex_list[i].pos +=
                                    vector2f(round_to_pixel_(fX), round_to_pixel_(fY));
                                l_vertex_list[i].col =
                                    l_color_stack.empty() ? color::empty : l_color_stack.back();
                            }

                            l_quad_list_.push_back(l_vertex_list);
                        }
                    },
                    *iter_char);

                fX += m_advance.second;
            }

            fY += get_line_height() * f_line_spacing_;
        }
    } else {
        f_w_ = 0.0f;
        f_h_ = 0.0f;
    }

    if (m_renderer_.is_vertex_cache_enabled() && !m_renderer_.is_quad_batching_enabled()) {
        if (!p_outline_vertex_cache_)
            p_outline_vertex_cache_ = m_renderer_.create_vertex_cache(vertex_cache::type::quads);

        p_outline_vertex_cache_->update(
            l_outline_quad_list_[0].data(), l_outline_quad_list_.size() * 4);

        if (!p_vertex_cache_)
            p_vertex_cache_ = m_renderer_.create_vertex_cache(vertex_cache::type::quads);

        std::vector<std::array<vertex, 4>> l_quads_copy = l_quad_list_;
        for (auto& m_quad : l_quads_copy)
            for (std::size_t i = 0; i < 4; ++i) {
                if (!b_formatting_enabled_ || b_force_color_ || m_quad[i].col == color::empty) {
                    m_quad[i].col = m_color_;
                }

                m_quad[i].col.a *= f_alpha_;
            }

        p_vertex_cache_->update(l_quads_copy[0].data(), l_quads_copy.size() * 4);
    }

    b_update_cache_ = false;
}

std::array<vertex, 4> text::create_letter_quad_(gui::font& m_font, char32_t ui_char) const {
    bounds2f m_quad = m_font.get_character_bounds(ui_char) * f_scaling_factor_;

    std::array<vertex, 4> l_vertex_list;
    l_vertex_list[0].pos = m_quad.top_left();
    l_vertex_list[1].pos = m_quad.top_right();
    l_vertex_list[2].pos = m_quad.bottom_right();
    l_vertex_list[3].pos = m_quad.bottom_left();

    bounds2f m_u_vs      = m_font.get_character_uvs(ui_char);
    l_vertex_list[0].uvs = m_u_vs.top_left();
    l_vertex_list[1].uvs = m_u_vs.top_right();
    l_vertex_list[2].uvs = m_u_vs.bottom_right();
    l_vertex_list[3].uvs = m_u_vs.bottom_left();

    return l_vertex_list;
}

std::array<vertex, 4> text::create_letter_quad_(char32_t ui_char) const {
    return create_letter_quad_(*p_font_, ui_char);
}

std::array<vertex, 4> text::create_outline_letter_quad_(char32_t ui_char) const {
    return create_letter_quad_(*p_outline_font_, ui_char);
}

quad text::create_letter_quad(char32_t ui_char) const {
    quad m_output;
    m_output.mat = p_font_->get_texture().lock();
    m_output.v   = create_letter_quad_(ui_char);

    return m_output;
}

std::size_t text::get_num_letters() const {
    update_();
    return l_quad_list_.size();
}

const std::array<vertex, 4>& text::get_letter_quad(std::size_t ui_index) const {
    update_();

    if (ui_index >= l_quad_list_.size())
        throw gui::exception("text", "Trying to access letter at invalid index.");

    return l_quad_list_[ui_index];
}

} // namespace lxgui::gui
