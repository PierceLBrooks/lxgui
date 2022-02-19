#include "lxgui/impl/gui_sdl_font.hpp"

#include "lxgui/gui_exception.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/impl/gui_sdl_material.hpp"
#include "lxgui/utils_string.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

namespace lxgui::gui::sdl {

font::font(
    SDL_Renderer*                        p_renderer,
    const std::string&                   font_file,
    std::size_t                          ui_size,
    std::size_t                          ui_outline,
    const std::vector<code_point_range>& code_points,
    char32_t                             ui_default_code_point,
    bool                                 pre_multiplied_alpha_supported) :
    ui_size_(ui_size), ui_default_code_point_(ui_default_code_point) {
    if (!TTF_WasInit() && TTF_Init() != 0) {
        throw gui::exception(
            "gui::sdl::font", "Could not initialise SDL_ttf: " + std::string(TTF_GetError()));
    }

    TTF_Font* p_font = TTF_OpenFont(font_file.c_str(), ui_size_);
    if (!p_font) {
        throw gui::exception(
            "gui::sdl::font", "Could not load font file '" + font_file + "' at size " +
                                  utils::to_string(ui_size) + ": " + std::string(TTF_GetError()) +
                                  ".");
    }

    if (ui_outline > 0)
        TTF_SetFontOutline(p_font, ui_outline);

    // Add some space between letters to prevent artifacts
    std::size_t ui_spacing = 1;

    int max_height = 0, max_width = 0;

    // Calculate maximum width and height
    std::size_t ui_num_char = 0;
    for (const code_point_range& range : code_points) {
        for (char32_t ui_code_point = range.ui_first; ui_code_point <= range.ui_last;
             ++ui_code_point) {
            if (ui_code_point > std::numeric_limits<Uint16>::max())
                break;

            const Uint16 ui_alt_char = static_cast<Uint16>(ui_code_point);

            int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
            if (TTF_GlyphMetrics(p_font, ui_alt_char, &min_x, &max_x, &min_y, &max_y, &advance) !=
                0)
                continue;

            int char_height = max_y - min_y;
            if (char_height > max_height)
                max_height = char_height;

            int char_width = max_x - min_x;
            if (char_width > max_width)
                max_width = char_width;

            ++ui_num_char;
        }
    }

    max_height = max_height + 2 * ui_outline;
    max_width  = max_width + 2 * ui_outline;

    // Calculate the size of the texture
    std::size_t ui_tex_size = (max_width + ui_spacing) * (max_height + ui_spacing) * ui_num_char;
    std::size_t ui_tex_side = static_cast<std::size_t>(std::sqrt((float)ui_tex_size));
    ui_tex_side += std::max(max_width, max_height);

    // Round up to nearest power of two
    {
        std::size_t i = 1;
        while (ui_tex_side > i)
            i *= 2;
        ui_tex_side = i;
    }

    std::size_t ui_final_width, ui_final_height;
    if (ui_tex_side * ui_tex_side / 2 >= ui_tex_size)
        ui_final_height = ui_tex_side / 2;
    else
        ui_final_height = ui_tex_side;

    ui_final_width = ui_tex_side;

    p_texture_ =
        std::make_shared<sdl::material>(p_renderer, vector2ui(ui_final_width, ui_final_height));

    vector2ui canvas_dimensions       = p_texture_->get_canvas_dimensions();
    vector2f  canvas_dimensions_float = vector2f(canvas_dimensions);

    std::size_t ui_pitch         = 0;
    ub32color*  p_texture_pixels = p_texture_->lock_pointer(&ui_pitch);
    std::fill(
        p_texture_pixels, p_texture_pixels + ui_pitch * canvas_dimensions.y, ub32color(0, 0, 0, 0));

    std::size_t x = 0, y = 0;
    std::size_t ui_line_max_height = max_height;

    const SDL_Color color = {255, 255, 255, 255};

    const float y_offset = TTF_FontDescent(p_font);

    for (const code_point_range& range : code_points) {
        range_info info;
        info.range = range;
        info.data.resize(range.ui_last - range.ui_first + 1);

        for (char32_t ui_code_point = range.ui_first; ui_code_point <= range.ui_last;
             ++ui_code_point) {
            character_info& ci = info.data[ui_code_point - range.ui_first];
            ci.ui_code_point   = ui_code_point;

            if (ui_code_point > std::numeric_limits<Uint16>::max()) {
                gui::out << gui::warning << "gui::sdl::font : Cannot load character "
                         << ui_code_point << " because SDL_ttf only accepts 16bit code points."
                         << std::endl;
                break;
            }

            const Uint16 ui_alt_char = static_cast<Uint16>(ui_code_point);

            int min_x = 0, max_x = 0, min_y = 0, max_y = 0, advance = 0;
            if (TTF_GlyphMetrics(p_font, ui_alt_char, &min_x, &max_x, &min_y, &max_y, &advance) !=
                0) {
                gui::out << gui::warning << "gui::sdl::font : Cannot load character "
                         << ui_code_point << " in font \"" << font_file << "\"." << std::endl;
                continue;
            }

            SDL_Surface* p_glyph_surface = TTF_RenderGlyph_Blended(p_font, ui_alt_char, color);
            if (!p_glyph_surface) {
                gui::out << gui::warning << "gui::sdl::font : Cannot draw character "
                         << ui_code_point << " in font \"" << font_file << "\"." << std::endl;
                continue;
            }

            if (p_glyph_surface->format->format != SDL_PIXELFORMAT_ARGB8888) {
                throw gui::exception(
                    "gui::sdl::font", "SDL_ttf output format is not ARGB8888 (got " +
                                          utils::to_string(p_glyph_surface->format->format) + ")");
            }

            const std::size_t ui_glyph_width  = p_glyph_surface->w;
            const std::size_t ui_glyph_height = p_glyph_surface->h;

            ui_line_max_height = std::max(ui_line_max_height, ui_glyph_height);

            // If at end of row, jump to next line
            if (x + ui_glyph_width > static_cast<std::size_t>(canvas_dimensions.x) - 1) {
                y += ui_line_max_height + ui_spacing;
                x = 0;
            }

            // SDL_ttf outputs glyphs in BGRA (little-endian) and we use RGBA;
            // this is fine because we always render glyphs in white, and don't care about
            // the color information.
            ub32color*  p_glyph_pixels = reinterpret_cast<ub32color*>(p_glyph_surface->pixels);
            std::size_t glyph_pitch    = p_glyph_surface->pitch / sizeof(ub32color);
            for (std::size_t j = 0; j < ui_glyph_height; ++j)
                for (std::size_t i = 0; i < ui_glyph_width; ++i)
                    p_texture_pixels[x + i + (y + j) * ui_pitch] =
                        p_glyph_pixels[i + j * glyph_pitch];

            SDL_FreeSurface(p_glyph_surface);

            ci.uvs.left   = x / canvas_dimensions_float.x;
            ci.uvs.top    = y / canvas_dimensions_float.y;
            ci.uvs.right  = (x + ui_glyph_width) / canvas_dimensions_float.x;
            ci.uvs.bottom = (y + ui_glyph_height) / canvas_dimensions_float.y;

            // NB: do not use iMinX etc here; SDL_ttf has already applied them to the rendered glyph
            ci.rect.left   = -static_cast<float>(ui_outline);
            ci.rect.right  = ci.rect.left + ui_glyph_width;
            ci.rect.top    = y_offset - static_cast<float>(ui_outline);
            ci.rect.bottom = ci.rect.top + ui_glyph_height;

            ci.advance = advance;

            // Advance a column
            x += ui_glyph_width + ui_spacing;
        }

        range_list_.push_back(std::move(info));
    }

    TTF_CloseFont(p_font);

    // Pre-multiply alpha
    if (pre_multiplied_alpha_supported) {
        const std::size_t ui_area = canvas_dimensions.x * canvas_dimensions.y;
        for (std::size_t i = 0; i < ui_area; ++i) {
            float a = p_texture_pixels[i].a / 255.0f;
            p_texture_pixels[i].r *= a;
            p_texture_pixels[i].g *= a;
            p_texture_pixels[i].b *= a;
        }
    }

    p_texture_->unlock_pointer();
}

std::size_t font::get_size() const {
    return ui_size_;
}

const font::character_info* font::get_character_(char32_t c) const {
    for (const auto& info : range_list_) {
        if (c < info.range.ui_first || c > info.range.ui_last)
            continue;

        return &info.data[c - info.range.ui_first];
    }

    if (c != ui_default_code_point_)
        return get_character_(ui_default_code_point_);
    else
        return nullptr;
}

bounds2f font::get_character_uvs(char32_t c) const {
    const character_info* p_char = get_character_(c);
    if (!p_char)
        return bounds2f{};

    vector2f top_left     = p_texture_->get_canvas_uv(p_char->uvs.top_left(), true);
    vector2f bottom_right = p_texture_->get_canvas_uv(p_char->uvs.bottom_right(), true);
    return bounds2f(top_left.x, bottom_right.x, top_left.y, bottom_right.y);
}

bounds2f font::get_character_bounds(char32_t c) const {
    const character_info* p_char = get_character_(c);
    if (!p_char)
        return bounds2f{};

    return p_char->rect;
}

float font::get_character_width(char32_t c) const {
    const character_info* p_char = get_character_(c);
    if (!p_char)
        return 0.0f;

    return p_char->advance;
}

float font::get_character_height(char32_t c) const {
    const character_info* p_char = get_character_(c);
    if (!p_char)
        return 0.0f;

    return p_char->rect.height();
}

float font::get_character_kerning(char32_t, char32_t) const {
    // Note: SDL_ttf does not expose kerning
    return 0.0f;
}

std::weak_ptr<gui::material> font::get_texture() const {
    return p_texture_;
}

void font::update_texture(std::shared_ptr<gui::material> p_mat) {
    p_texture_ = std::static_pointer_cast<sdl::material>(p_mat);
}

} // namespace lxgui::gui::sdl
