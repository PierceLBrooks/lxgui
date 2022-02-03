#ifndef LXGUI_GUI_LAYEREDREGION_HPP
#define LXGUI_GUI_LAYEREDREGION_HPP

#include "lxgui/gui_region.hpp"
#include "lxgui/lxgui.hpp"
#include "lxgui/utils.hpp"

namespace lxgui { namespace gui {

/// ID of a layer for rendering inside a frame.
enum class layer {
    BACKGROUND  = 0,
    BORDER      = 1,
    ARTWORK     = 2,
    OVERLAY     = 3,
    HIGHLIGHT   = 4,
    SPECIALHIGH = 5,
    ENUM_SIZE
};

/// Converts a string representation of a layer into the corresponding enumerator
/** \param sLayer The layer string (e.g., "ARTWORK")
 *   \return The corresponding enumerator, or "ARTWORK" if parsing failed
 */
layer parse_layer_type(const std::string& sLayer);

/// A #region that can be rendered in a layer.
/** Layered regions can display content on the screen (texture,
 *   texts, 3D models, ...) and must be contained inside a layer,
 *   within a #lxgui::gui::frame object. The frame will then render all
 *   its layered regions, sorted by layers.
 *
 *   Layered regions cannot themselves react to events; this
 *   must be taken care of by the parent frame.
 */
class layered_region : public region {
    using base = region;

public:
    /// Constructor.
    explicit layered_region(utils::control_block& mBlock, manager& mManager);

    /// Prints all relevant information about this region in a string.
    /** \param sTab The offset to give to all lines
     *   \return All relevant information about this region
     */
    std::string serialize(const std::string& sTab) const override;

    /// Creates the associated Lua glue.
    void create_glue() override;

    /// Removes this region from its parent and return an owning pointer.
    /** \return An owning pointer to this region
     */
    utils::owner_ptr<region> release_from_parent() override;

    /// shows this region.
    /** \note Its parent must be shown for it to appear on
     *         the screen.
     */
    void show() override;

    /// hides this region.
    /** \note All its children won't be visible on the screen
     *   anymore, even if they are still marked as shown.
     */
    void hide() override;

    /// Checks if this region can be seen on the screen.
    /** \return 'true' if this region can be seen on the screen
     */
    bool is_visible() const override;

    /// Returns this layered_region's draw layer.
    /** \return this layered_region's draw layer
     */
    layer get_draw_layer() const;

    /// Sets this layered_region's draw layer.
    /** \param mLayer The new layer
     */
    virtual void set_draw_layer(layer mLayer);

    /// Sets this layered_region's draw layer.
    /** \param sLayer The new layer
     */
    virtual void set_draw_layer(const std::string& sLayer);

    /// Notifies the renderer of this region that it needs to be redrawn.
    /** \note Automatically called by any shape changing function.
     */
    void notify_renderer_need_redraw() override;

    /// Parses data from a layout_node.
    /** \param mNode The layout node
     */
    void parse_layout(const layout_node& mNode) override;

    /// Registers this region class to the provided Lua state
    static void register_on_lua(sol::state& mLua);

    static constexpr const char* CLASS_NAME = "LayeredRegion";

protected:
    void parse_attributes_(const layout_node& mNode) override;

    layer mLayer_ = layer::ARTWORK;
};

}} // namespace lxgui::gui

#endif
