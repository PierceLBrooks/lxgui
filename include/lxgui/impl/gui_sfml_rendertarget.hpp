#ifndef LXGUI_GUI_SFML_RENDERTARGET_HPP
#define LXGUI_GUI_SFML_RENDERTARGET_HPP

#include "lxgui/gui_rendertarget.hpp"
#include "lxgui/impl/gui_sfml_material.hpp"
#include "lxgui/utils.hpp"

#include <memory>

/** \cond INCLUDE_INTERNALS_IN_DOC
 */
namespace sf {

class RenderTexture;

}
/** \endcond
 */

namespace lxgui::gui::sfml {

/// A place to render things (the screen, a texture, ...)
class render_target final : public gui::render_target {
public:
    /// Constructor.
    /** \param mDimensions The dimensions of the render_target
     *   \param mFilter     The filtering to apply to the target texture when displayed
     */
    render_target(const vector2ui& m_dimensions, material::filter m_filter = material::filter::none);

    /// Begins rendering on this target.
    void begin() override;

    /// Ends rendering on this target.
    void end() override;

    /// Clears the content of this render_target.
    /** \param mColor The color to use as background
     */
    void clear(const color& m_color) override;

    /// Returns this render target's pixel rect.
    /** \return This render target's pixel rect
     */
    bounds2f get_rect() const override;

    /// Sets this render target's dimensions.
    /** \param mDimensions The new dimensions (in pixels)
     *   \return 'true' if the function had to re-create a
     *           new render target
     */
    bool set_dimensions(const vector2ui& m_dimensions) override;

    /// Returns this render target's canvas dimension.
    /** \return This render target's canvas dimension
     *   \note This is the physical size of the render target.
     *         On some systems, abitrary dimensions are not supported:
     *         they can be promoted to the nearest power of two from
     *         for example.
     */
    vector2ui get_canvas_dimensions() const override;

    /// Returns the associated texture for rendering.
    /** \return The underlying pixel buffer, that you can use to render its content
     */
    std::weak_ptr<sfml::material> get_material();

    /// Returns the underlying SFML render texture object.
    /** return The underlying SFML render texture object
     */
    sf::RenderTexture* get_render_texture();

private:
    void update_view_matrix_() const;

    std::shared_ptr<sfml::material> p_texture_;
    sf::RenderTexture*              p_render_texture_ = nullptr;
};

} // namespace lxgui::gui::sfml

#endif
