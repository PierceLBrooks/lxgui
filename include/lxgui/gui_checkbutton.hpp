#ifndef LXGUI_GUI_CHECKBUTTON_HPP
#define LXGUI_GUI_CHECKBUTTON_HPP

#include "lxgui/gui_button.hpp"
#include "lxgui/lxgui.hpp"
#include "lxgui/utils.hpp"

namespace lxgui::gui {

/// A #button with two additional states: checked and unchecked.
/** This region works exactly like a classic #button, but is has two
 *   additional special textures for the check sign.
 */
class check_button : public button {
    using base = button;

public:
    /// Constructor.
    explicit check_button(utils::control_block& m_block, manager& m_manager);

    /// Prints all relevant information about this region in a string.
    /** \param tab The offset to give to all lines
     *   \return All relevant information about this region
     */
    std::string serialize(const std::string& tab) const override;

    /// Copies a region's parameters into this check button (inheritance).
    /** \param mObj The region to copy
     */
    void copy_from(const region& m_obj) override;

    /// Checks this button.
    virtual void check();

    /// UnChecks this button.
    virtual void uncheck();

    /// Disables this check button.
    /** \note A disabled button doesn't receive any input.
     */
    void disable() override;

    /// Enables this check button.
    void enable() override;

    /// Releases this check button.
    /** \note This function only has a visual impact:
     *         the OnClick() handler is not called.
     */
    void release() override;

    /// Checks if this check button is checked.
    /** \return 'true' if checked, 'false' otherwise
     */
    bool is_checked() const;

    /// Returns this button's checked texture.
    /** \return This button's checked texture
     */
    const utils::observer_ptr<texture>& get_checked_texture() {
        return p_checked_texture_;
    }

    /// Returns this button's checked texture.
    /** \return This button's checked texture
     */
    utils::observer_ptr<const texture> get_checked_texture() const {
        return p_checked_texture_;
    }

    /// Returns this button's disabled checked texture.
    /** \return This button's disabled checked texture
     */
    const utils::observer_ptr<texture>& get_disabled_checked_texture() {
        return p_disabled_checked_texture_;
    }

    /// Returns this button's disabled checked texture.
    /** \return This button's disabled checked texture
     */
    utils::observer_ptr<const texture> get_disabled_checked_texture() const {
        return p_disabled_checked_texture_;
    }

    /// Sets this button's checked texture.
    /** \param pTexture The new texture
     */
    void set_checked_texture(utils::observer_ptr<texture> p_texture);

    /// Sets this button's disabled checked texture.
    /** \param pTexture The new texture
     */
    void set_disabled_checked_texture(utils::observer_ptr<texture> p_texture);

    /// Returns this region's Lua glue.
    void create_glue() override;

    /// Registers this region class to the provided Lua state
    static void register_on_lua(sol::state& m_lua);

    static constexpr const char* class_name = "CheckButton";

protected:
    void parse_all_nodes_before_children_(const layout_node& m_node) override;

    bool is_checked_ = false;

    utils::observer_ptr<texture> p_checked_texture_          = nullptr;
    utils::observer_ptr<texture> p_disabled_checked_texture_ = nullptr;
};

} // namespace lxgui::gui

#endif
