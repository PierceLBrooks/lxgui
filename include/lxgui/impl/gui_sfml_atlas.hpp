#ifndef LXGUI_GUI_SFML_ATLAS_HPP
#define LXGUI_GUI_SFML_ATLAS_HPP

#include "lxgui/gui_atlas.hpp"
#include "lxgui/gui_material.hpp"
#include "lxgui/utils.hpp"

#include <SFML/Graphics/Texture.hpp>

namespace lxgui::gui::sfml {

class renderer;

/// A single texture holding multiple materials for efficient rendering
/** This is an abstract class that must be implemented
 *   and created by the corresponding gui::renderer.
 */
class atlas_page final : public gui::atlas_page {
public:
    /// Constructor.
    explicit atlas_page(gui::renderer& rdr, material::filter filt);

protected:
    /// Adds a new material to this page, at the provided location
    /** \param mat      The material to add
     *   \param location The position at which to insert this material
     *   \return A new material pointing to inside this page
     */
    std::shared_ptr<gui::material>
    add_material_(const gui::material& mat, const bounds2f& location) override;

    /// Return the width of this page (in pixels).
    /** \return The width of this page (in pixels)
     */
    float get_width_() const override;

    /// Return the height of this page (in pixels).
    /** \return The height of this page (in pixels)
     */
    float get_height_() const override;

private:
    sf::Texture texture_;
};

/// A class that holds rendering data
/** This implementation can contain either a plain color
 *   or a real sf::Texture. It is also used by the
 *   gui::sfml::render_target class to store the output data.
 */
class atlas final : public gui::atlas {
public:
    /// Constructor for textures.
    /** \param mRenderer The renderer with witch to create this atlas
     *   \param filt   Use texture filtering or not (see set_filter())
     */
    explicit atlas(renderer& rdr, material::filter filt);

    atlas(const atlas& tex) = delete;
    atlas(atlas&& tex)      = delete;
    atlas& operator=(const atlas& tex) = delete;
    atlas& operator=(atlas&& tex) = delete;

protected:
    /// Create a new page in this atlas.
    /** \return The new page, added at the back of the page list
     */
    std::unique_ptr<gui::atlas_page> create_page_() override;
};

} // namespace lxgui::gui::sfml

#endif
