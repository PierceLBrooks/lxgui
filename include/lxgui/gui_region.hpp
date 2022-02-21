#ifndef LXGUI_GUI_REGION_HPP
#define LXGUI_GUI_REGION_HPP

#include "lxgui/gui_anchor.hpp"
#include "lxgui/gui_bounds2.hpp"
#include "lxgui/gui_color.hpp"
#include "lxgui/gui_exception.hpp"
#include "lxgui/gui_strata.hpp"
#include "lxgui/gui_vector2.hpp"
#include "lxgui/lxgui.hpp"
#include "lxgui/utils.hpp"
#include "lxgui/utils_maths.hpp"
#include "lxgui/utils_observer.hpp"

#include <array>
#include <lxgui/extern_sol2_object.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

/** \cond INCLUDE_INTERNALS_IN_DOC
 */
namespace sol {

class state;

}
/** \endcond
 */

namespace lxgui::gui {

struct addon;
class manager;
class registry;
class factory;
class layout_node;

class frame;
class frame_renderer;

/// The base class of all elements in the GUI.
/** Objects of this class offers core functionalities needed by every element
 *   of the interface. They have a name, and a corresponding variable created
 *   in Lua to access them. They can have a parent #lxgui::gui::frame. They can be
 *   placed on the screen at an absolute position, or relative to other regions.
 *   They can be shown or hidden.
 *
 *   Apart form this, a region does not contain anything, nor can it display
 *   anything on the screen. Any functionality beyond the list above is implemented
 *   in specialized subclasses (see the full list below).
 *
 *   __Interaction between C++, Lua, and layout files.__ When a region is created,
 *   it must be given a name, for example `"PlayerHealthBar"`. For as long as the
 *   object lives, this name will be used to refer to it. In particular, as soon
 *   as the object is created, regardless of whether this was done in C++, layout
 *   files, or Lua, a new variable will be created in the Lua state with the exact
 *   same name, `PlayerHealthBar`. This variable is a reference to the region, and
 *   can be used to interact with it dynamically. Because of this, each object must
 *   have a unique name, otherwise it could not be accessible from Lua.
 *
 *   Note: Although you can destroy this Lua variable by setting it to nil, this is
 *   not recommended: the object will _not_ be destroyed (nor garbage-collected)
 *   because it still exists in the C++ memory space. The only way to truly destroy
 *   an object from Lua is to call `delete_frame` (for frames only). Destroying and
 *   creating objects has a cost however. If the object is likely to reappear later
 *   with the same content, simply hide it and show it again later on. If the
 *   content may change, you can also recycle the object, i.e., keep it alive and
 *   simply change its content when it later reappears.
 *
 *   Deleting an object from C++ is done using region::destroy.
 *   This will automatically delete all references to this object in Lua as well.
 *
 *   Finally, note that objects do not need to be explicitly destroyed: they will
 *   automatically be destroyed when their parent is itself destroyed (see below).
 *   Only use explicit destruction when absolutely necessary.
 *
 *   __Parent-child relationship.__ Parents of regions are frames. See
 *   the #lxgui::gui::frame class documentation for more information. One important
 *   aspect of the parent-child relationship is related to the object name. If a
 *   region has a parent, it can be given a name starting with `"$parent"`.
 *   The name of the parent will automatically replace the `"$parent"` string.
 *   For example, if an object is named `"$parentButton"` and its parent is named
 *   `"ErrorMessage"`, the final name of the object will be `"ErrorMessageButton"`.
 *   It can be accessed from the Lua state as `ErrorMessageButton`, or as
 *   `ErrorMessage.Button`. Note that this is totally dynamic: if you later change
 *   the parent of this button to be another frame, for example `"ExitDialog"`
 *   its name will naturally change to `"ExitDialogButton"`, and it can be accessed
 *   from Lua as `ExitDialogButton`, or as `ExitDialog.Button`. This is particularly
 *   powerful for writing generic code which does not rely on the full names of
 *   objects, only on their child-parent relationship.
 *
 *   __Positioning.__ regions have a position on the screen, but this is
 *   not parametrized as a simple pair of X and Y coordinates. Instead, objects
 *   are positioned based on a list of "anchors". Anchors are links between
 *   objects, which force one edge or one corner of a given object to match with
 *   the edge or corner of another object. For example, given two objects A and B,
 *   you can create an anchor that links the top-left corner of A to the top-left
 *   corner of B. The position of A will automatically be linked to the position of
 *   B, hence if B moves, A will follow. To further refine this positioning, you
 *   can specify anchor offsets: for example, you may want A's top-left corner to
 *   be shifted from B's top-left corner by two pixels in the X direction, and
 *   five in the Y direction. This offset can be defined either as an absolute
 *   number of pixels, or as a relative fraction of the size of the object being
 *   anchored to. For example, you can specify that A's top-left corner links to
 *   B's top-left corner, with an horizontal offset equal to 30% of B's width.
 *   Read the "Anchors" section below for more information.
 *
 *   An object which has no anchor will be considered "invalid" and will not be
 *   displayed.
 *
 *   __Sizing.__ There are two ways to specify the size of a region. The
 *   first and most straightforward approach is to directly set its width and/or
 *   height. This must be specified as an absolute number of pixels. The second
 *   and more versatile method is to use more than one anchor for opposite sides
 *   of the object, for example an anchor for the "left" and another for the
 *   "right" edge. This will implicitly give a width to the object, depending on
 *   the position of the other objects to which it is anchored. Anchors will always
 *   override the absolute width and height of an object if they provide any
 *   constraint on the extents of the object in a given dimension.
 *
 *   An object which has neither a fixed absolute size, nor has it size implicitly
 *   constrained by anchors, is considered "invalid" and will not be displayed.
 *
 *   __Anchors.__ There are nine available anchor points:
 *
 *   - `TOP_LEFT`: constrains the max Y and min X.
 *   - `TOP_RIGHT`: constrains the max Y and max X.
 *   - `BOTTOM_LEFT`: constrains the min Y and min X.
 *   - `BOTTOMRIGH`: constrains the min Y and max X.
 *   - `LEFT`: constrains the min X and the midpoint in Y.
 *   - `RIGHT`: constrains the max X and the midpoint in Y.
 *   - `TOP`: constrains the max Y and the midpoint in X.
 *   - `BOTTOM`: constrains the min Y and the midpoint in X.
 *   - `CENTER`: constrains the midpoint in X and Y.
 *
 *   If you specify two constraints on the same point (for example: `TOP_LEFT`
 *   and `BOTTOM_LEFT` both constrain the min X coordinate), the most stringent
 *   constraint always wins. Constraints on the midpoints are more subtle however,
 *   as they will always be discarded when both the min and max are constrained.
 *   For example, consider an object `A` of fixed size 30x30 and some other object
 *   `B` of fixed size 40x40. If we anchor the `RIGHT` of `A` to the `LEFT` of `B`,
 *   `A`'s _vertical_ center will be automatically aligned with `B`'s vertical center.
 *   This is the effect of the midpoint constraint. Now, if we further anchor the
 *   `TOP` of `A` to the `TOP` of `B`, we have more than one anchor constraining
 *   the vertical extents of `A` (see "Sizing" above), therefore `A`'s fixed height
 *   of 30 pixels will be ignored from now on. It will shrink to a height of 20
 *   pixels, i.e., the distance between `B`'s top edge and its vertical center.
 *   Finally, if we further anchor the `BOTTOM` of `A` to the `BOTTOM` of `B`, the
 *   constraint on `A`'s midpoint will be ignored: `A` will be enlarged to a height
 *   of 40 pixels, i.e., the distance between `B`'s top and bottom edges.
 */
class region : public utils::enable_observer_from_this<region> {
    friend factory;
    friend frame;

public:
    /// Contructor.
    explicit region(utils::control_block& block, manager& mgr);

    /// Destructor.
    ~region() override;

    /// Non-copiable
    region(const region&) = delete;

    /// Non-movable
    region(region&&) = delete;

    /// Non-copiable
    region& operator=(const region&) = delete;

    /// Non-movable
    region& operator=(region&&) = delete;

    /// Renders this region on the current render target.
    virtual void render() const;

    /// Updates this region's logic.
    /** \param delta Time spent since last update
     */
    virtual void update(float delta);

    /// Prints all relevant information about this region in a string.
    /** \param tab The offset to give to all lines
     *   \return All relevant information about this region
     */
    virtual std::string serialize(const std::string& tab) const;

    /// Copies a region's parameters into this region (inheritance).
    /** \param obj The region to copy
     */
    virtual void copy_from(const region& obj);

    /// Tells this region that its borders need updating.
    virtual void notify_borders_need_update();

    /// Tells this region that the global interface scaling factor has changed.
    virtual void notify_scaling_factor_updated();

    /// Returns this region's name.
    /** \return This region's name
     */
    const std::string& get_name() const;

    /// Returns this region's Lua name.
    /** \return This region's Lua name
     */
    const std::string& get_lua_name() const;

    /// Returns this region's raw name.
    /** \return This region's raw name
     *   \note This is the name of the region before "$parent"
     *         has been replaced by its parent's name.
     */
    const std::string& get_raw_name() const;

    /// Returns this region's parent.
    /** \return This region's parent
     */
    utils::observer_ptr<const frame> get_parent() const {
        return parent_;
    }

    /// Returns this region's parent.
    /** \return This region's parent
     */
    const utils::observer_ptr<frame>& get_parent() {
        return parent_;
    }

    /// Removes this region from its parent and return an owning pointer.
    /** \return An owning pointer to this region
     */
    virtual utils::owner_ptr<region> release_from_parent();

    /// Forcefully removes this region from the GUI.
    /** \warning After calling this function, any pointer to the object is invalidated!
     *            Only call this function if you need the object to be destroyed early,
     *            before its parent (if any) would itself be destroyed.
     */
    void destroy();

    /// Changes this region's alpha (opacity).
    /** \param alpha The new alpha value
     *   \note Default is 1.0f.
     */
    void set_alpha(float alpha);

    /// Returns this region's alpha (opacity).
    /** \return This region's alpha (opacity).
     */
    float get_alpha() const;

    /// Returns this region's effective alpha (opacity).
    /** \return This region's effective alpha (opacity).
     *   \note This includes the region's parent alpha.
     */
    float get_effective_alpha() const;

    /// shows this region.
    /** \note Its parent must be shown for it to appear on
     *         the screen.
     */
    virtual void show();

    /// hides this region.
    /** \note All its children won't be visible on the screen
     *         anymore, even if they are still marked as shown.
     */
    virtual void hide();

    /// shows/hides this region.
    /** \param is_shown 'true' if you want to show this region
     *   \note See show() and hide() for more infos.
     */
    void set_shown(bool is_shown);

    /// Checks if this region is shown.
    /** \return 'true' if this region is shown
     */
    bool is_shown() const;

    /// Checks if this region can be seen on the screen.
    /** \return 'true' if this region can be seen on the screen
     */
    virtual bool is_visible() const;

    /// Changes this region's absolute dimensions (in pixels).
    /** \param dimensions The new dimensions
     */
    virtual void set_dimensions(const vector2f& dimensions);

    /// Changes this region's absolute width (in pixels).
    /** \param abs_width The new width
     */
    virtual void set_width(float abs_width);

    /// Changes this region's absolute height (in pixels).
    /** \param abs_height The new height
     */
    virtual void set_height(float abs_height);

    /// Changes this region's dimensions (relative to its parent).
    /** \param dimensions The new dimensions (relative)
     */
    void set_relative_dimensions(const vector2f& dimensions);

    /// Changes this region's width (relative to its parent).
    /** \param rel_width The new width
     */
    void set_relative_width(float rel_width);

    /// Changes this region's height (relative to its parent).
    /** \param rel_height The new height
     */
    void set_relative_height(float rel_height);

    /// Returns this region's explicitly-defined width and height (in pixels).
    /** \return This region's explicitly-defined width and height (in pixels)
     *   \note If you need to get the actual size of a region on the screen,
     *         use get_apparent_dimensions(), as some regions may not have
     *         their dimensions explicitly defined, and instead get their
     *         extents from anchors. If a dimension is not explicitly defined,
     *         it will be returned as zero.
     */
    const vector2f& get_dimensions() const;

    /// Returns this region's appearent width and height (in pixels).
    /** \return This region's appearent width and height (in pixels)
     *   \note If you need to get the actual size of a region on the screen,
     *         use this function instead of get_dimensions(), as some regions
     *         may not have their dimensions explicitly defined, and instead
     *         get their extents from anchors.
     */
    vector2f get_apparent_dimensions() const;

    /// Checks if this region's apparent width is defined.
    /** \return 'true' if defined, 'false' otherwise
     *   \note The apparent width is defined if either the region's absolute
     *         or relative width is explicitly specified (from set_width(),
     *         set_relative_width(), set_dimensions(), or set_relative_dimensions()),
     *         or if its left and right borders are anchored. A region with an undefined
     *         apparent width will not be rendered on the screen until its width is defined.
     */
    bool is_apparent_width_defined() const;

    /// Checks if this region's apparent height is defined.
    /** \return 'true' if defined, 'false' otherwise
     *   \note The apparent height is defined if either the region's absolute
     *         or relative height is explicitly specified (from set_height(),
     *         set_relative_height(), set_dimensions(), or set_relative_dimensions()),
     *         or if its left and right borders are anchored. A region with an undefined
     *         apparent height will not be rendered on the screen until its height is defined.
     */
    bool is_apparent_height_defined() const;

    /// Checks if the provided coordinates are inside this region.
    /** \param position The coordinates to test
     *   \return 'true' if the provided coordinates are inside this region
     */
    virtual bool is_in_region(const vector2f& position) const;

    /// Returns the type of this region.
    /** \return The type of this region
     */
    const std::string& get_object_type() const;

    /// Checks if this region is of the provided type.
    /** \param type_name The type to test
     *   \return 'true' if this region is of the provided type
     */
    bool is_object_type(const std::string& type_name) const;

    /// Checks if this region is of the provided type.
    /** \return 'true' if this region is of the provided type
     */
    template<typename ObjectType>
    bool is_object_type() const {
        return is_object_type(ObjectType::class_name);
    }

    /// Returns an array containing all the types of this region.
    /** \return An array containing all the types of this region
     */
    const std::vector<std::string>& get_object_type_list() const;

    /// Returns the vertical position of this region's bottom border.
    /** \return The vertical position of this region's bottom border
     */
    float get_bottom() const;

    /// Returns the position of this region's center.
    /** \return The position of this region's center
     */
    vector2f get_center() const;

    /// Returns the horizontal position of this region's left border.
    /** \return The horizontal position of this region's left border
     */
    float get_left() const;

    /// Returns the horizontal position of this region's right border.
    /** \return The horizontal position of this region's right border
     */
    float get_right() const;

    /// Returns the vertical position of this region's top border.
    /** \return The vertical position of this region's top border
     */
    float get_top() const;

    /// Returns this region's borders.
    /** \return This region's borders
     */
    const bounds2f& get_borders() const;

    /// Removes all anchors.
    /** \note This region and its children won't be visible until you
     *         define at least one anchor.
     */
    void clear_all_points();

    /// Adjusts this regions anchors to fit the provided region.
    /** \param obj A pointer to the object you want to wrap
     *   \note Removes all anchors and defines two new ones.
     */
    void set_all_points(const utils::observer_ptr<region>& obj);

    /// Adjusts this regions anchors to fit the provided region.
    /** \param obj_name The name of the object to fit to
     *   \note Removes all anchors and defines two new ones.
     */
    void set_all_points(const std::string& obj_name);

    /// Adds/replaces an anchor.
    /** \param anchor The anchor to add
     */
    void set_point(const anchor_data& anchor);

    /// Adds/replaces an anchor.
    /** \param args Argument to construct a new anchor_data
     */
    template<typename... Args>
    void set_point(Args&&... args) {
        constexpr auto set_point_overload =
            static_cast<void (region::*)(const anchor_data&)>(&region::set_point);
        (this->*set_point_overload)(anchor_data{std::forward<Args>(args)...});
    }

    /// Checks if this region depends on another.
    /** \param obj The region to test
     *   \note Usefull to detect circular refences.
     */
    bool depends_on(const region& obj) const;

    /// Returns the number of defined anchors.
    /** \return The number of defined anchors
     */
    std::size_t get_num_point() const;

    /// Returns one of this region's anchor to modify it.
    /** \param point The anchor point
     *   \return A pointer to the anchor, nullptr if none
     */
    anchor& modify_point(anchor_point point);

    /// Returns one of this region's anchor.
    /** \param point The anchor point
     *   \return A pointer to the anchor, nullptr if none
     */
    const anchor& get_point(anchor_point point) const;

    /// Returns all of this regions's anchors.
    /** \return All of this regions's anchors
     */
    const std::array<std::optional<anchor>, 9>& get_point_list() const;

    /// Round an absolute position on screen to the nearest physical pixel.
    /** \param value The input absolute position (can be fractional)
     *   \param method   The rounding method
     *   \return The position of the nearest physical pixel
     */
    float round_to_pixel(
        float value, utils::rounding_method method = utils::rounding_method::nearest) const;

    /// Round an absolute position on screen to the nearest physical pixel.
    /** \param position The input absolute position (can be fractional)
     *   \param method   The rounding method
     *   \return The position of the nearest physical pixel
     */
    vector2f round_to_pixel(
        const vector2f&        position,
        utils::rounding_method method = utils::rounding_method::nearest) const;

    /// Notifies this region that another one is anchored to it.
    /** \param obj The anchored region
     *   \note Anchored objects get their borders automatically updated
     *         whenever this objet's borders are updated.
     */
    void add_anchored_object(region& obj);

    /// Notifies this region that another one is no longer anchored to it.
    /** \param obj The region no longer anchored
     *   \see add_anchored_object()
     */
    void remove_anchored_object(region& obj);

    /// Checks if this region is virtual.
    /** \return 'true' if this region is virtual
     *   \note A virtual region will not be displayed on the screen, but can serve as a
     *         template to create new GUI elements (it is then "inherited", although note
     *         that this has no connection to C++ inheritance).
     */
    bool is_virtual() const;

    /// Makes this region virtual.
    /** \note See is_virtual().
     */
    void set_virtual();

    /// Flags this object as "special".
    /** \note Special objects are not automatically copied
     *         in the frame inheritance process. They must be
     *         explicitely copied by the derived class
     *         (example: Button will have to copy its button
     *         textures itself).
     */
    void set_special();

    /// Checks if this object is special.
    /** \return 'true' if this objet is special
     *   \note For more informations, see set_special().
     */
    bool is_special() const;

    /// Returns the renderer of this object or its parents.
    /** \return The renderer of this object or its parents
     *   \note For more informations, see frame::set_renderer().
     */
    virtual utils::observer_ptr<const frame_renderer> get_top_level_renderer() const;

    /// Returns the renderer of this object or its parents, nullptr if none.
    /** \return The renderer of this object or its parents, nullptr if none
     *   \note For more informations, see set_renderer().
     */
    utils::observer_ptr<frame_renderer> get_top_level_renderer() {
        return utils::const_pointer_cast<frame_renderer>(
            const_cast<const region*>(this)->get_top_level_renderer());
    }

    /// Notifies the renderer of this region that it needs to be redrawn.
    /** \note Automatically called by any shape-changing function.
     */
    virtual void notify_renderer_need_redraw();

    /// Returns the list of all objects that are anchored to this one.
    /** \return The list of all objects that are anchored to this one
     */
    const std::vector<utils::observer_ptr<region>>& get_anchored_objects() const;

    /// Notifies this region that it has been fully loaded.
    /** \see is_loaded()
     */
    virtual void notify_loaded();

    /// Checks if this region has been fully loaded.
    /** \note A region that is not fully loaded still has all its core attributes
     *         set, hence can be considered as "fully constructed" from a C++ point
     *         of view. However, semantically, the object may need further steps to
     *         be complete, as designed by the UI designer. Therefore, form the UI's
     *         point of view, a region is considered "complete" only if is_loaded()
     *         returns 'true' (see notifu_loaded()). Only then can the region, e.g.,
     *         react to or generate events.
     */
    bool is_loaded() const;

    /// Notifies this region that it is now visible on screen.
    /** \note Automatically called by show()/hide().
     */
    virtual void notify_visible();

    /// Notifies this region that it is no longer visible on screen.
    /** \note Automatically called by show()/hide().
     */
    virtual void notify_invisible();

    /// Sets the addon this frame belongs to.
    /** \param a The addon this frame belongs to
     */
    void set_addon(const addon* a);

    /// Returns this frame's addon.
    /** \return This frame's addon
     *   \note Returns "nullptr" if the frame has been created
     *         by Lua code and wasn't assigned a parent.
     */
    const addon* get_addon() const;

    /// Convert an addon-relative file path to a application-relative path
    /** \param file_name The raw file name
     *   \return The modified file name
     *   \note All file names must be relative to the current working directory
     *         (typically, the application's executable path),
     *         but sometimes it is more convenient and maintainable to specify a path that
     *         is relative to the addon directory. This can be achieved by simply putting
     *         "|" in front of a file name, which will then be interpreted as relative
     *         to the addon directory. This function takes care of this transformation.
     */
    std::string parse_file_name(const std::string& file_name) const;

    /// Returns this region's manager.
    /** \return This region's manager
     */
    manager& get_manager() {
        return manager_;
    }

    /// Returns this region's manager.
    /** \return This region's manager
     */
    const manager& get_manager() const {
        return manager_;
    }

    /// Returns the UI object registry, which keeps track of all objects in the UI.
    /** \return The registry object
     */
    registry& get_registry();

    /// Returns the UI object registry, which keeps track of all objects in the UI.
    /** \return The registry object
     */
    const registry& get_registry() const;

    /// Creates the associated Lua glue.
    virtual void create_glue();

    /// Removes the Lua glue.
    void remove_glue();

    /// Parses data from a layout_node.
    /** \param node The layout node
     */
    virtual void parse_layout(const layout_node& node);

    /// Registers this region class to the provided Lua state
    static void register_on_lua(sol::state& lua);

    template<typename ObjectType>
    friend const ObjectType* down_cast(const region* self);

    template<typename ObjectType>
    friend ObjectType* down_cast(region* self);

    static constexpr const char* class_name = "Region";

protected:
    // Layout parsing
    virtual void parse_attributes_(const layout_node& node);
    virtual void parse_size_node_(const layout_node& node);
    virtual void parse_anchor_node_(const layout_node& node);
    color        parse_color_node_(const layout_node& node);
    std::pair<anchor_type, vector2<std::optional<float>>> parse_dimension_(const layout_node& node);

    void read_anchors_(
        float& left, float& right, float& top, float& bottom, float& x_center, float& y_center)
        const;

    bool make_borders_(float& min, float& max, float center, float size) const;

    virtual void update_borders_();
    virtual void update_anchors_();

    sol::state& get_lua_();

    template<typename T>
    void create_glue_(T* self);

    void        set_lua_member_(std::string key, sol::stack_object value);
    sol::object get_lua_member_(const std::string& key) const;

    /// Sets this region's name.
    /** \param name This region's name
     *   \note Can only be called once. If you need to set both the name and the parent
     *         at the same time (typically, at creation), use set_name_and_parent_().
     */
    void set_name_(const std::string& name);

    /// Changes this region's parent.
    /** \param parent The new parent
     *   \note Default is nullptr.
     */
    void set_parent_(utils::observer_ptr<frame> parent);

    /// Sets this region's name and parent at once.
    /** \param name This region's name
     *   \param parent The new parent
     *   \note The name can only be set once. If you need to just change the
     *         parent, call set_parent_().
     */
    void set_name_and_parent_(const std::string& name, utils::observer_ptr<frame> parent);

    manager& manager_;

    const addon* addon_ = nullptr;

    std::string name_;
    std::string raw_name_;
    std::string lua_name_;
    std::size_t id_ = std::numeric_limits<std::size_t>::max();

    utils::observer_ptr<frame> parent_ = nullptr;

    bool is_special_ = false;
    bool is_virtual_ = false;
    bool is_loaded_  = false;
    bool is_ready_   = true;

    std::vector<std::string> type_;

    std::array<std::optional<anchor>, 9>     anchor_list_;
    std::vector<utils::observer_ptr<region>> previous_anchor_parent_list_;
    bounds2<bool>                            defined_border_list_;
    bounds2f                                 border_list_;

    float alpha_      = 1.0f;
    bool  is_shown_   = true;
    bool  is_visible_ = true;

    vector2f dimensions_;

    std::vector<utils::observer_ptr<region>> anchored_object_list_;

    std::unordered_map<std::string, sol::object> lua_members_;
};

/// Obtain a pointer to a derived class.
/** \param self The pointer to down cast
 *   \return A pointer to a derived class
 *   \note Like dynamic_cast(), this will return nullptr if this region
 *         is not of the requested type. However, it will throw if the cast
 *         failed because the derived class destructor has already been
 *         called. This indicates a programming error.
 */
template<typename ObjectType>
const ObjectType* down_cast(const region* self) {
    const ObjectType* object = dynamic_cast<const ObjectType*>(self);
    if (self && !object && self->is_object_type(ObjectType::class_name)) {
        throw gui::exception(
            self->type_.back(), "cannot use down_cast() to " + std::string(ObjectType::class_name) +
                                    " as object is being destroyed");
    }
    return object;
}

/// Obtain a pointer to a derived class.
/** \param self The pointer to down cast
 *   \return A pointer to a derived class
 *   \note Like dynamic_cast(), this will return nullptr if this region
 *         is not of the requested type. However, it will throw if the cast
 *         failed because the derived class destructor has already been
 *         called. This indicates a programming error.
 */
template<typename ObjectType>
ObjectType* down_cast(region* self) {
    return const_cast<ObjectType*>(down_cast<ObjectType>(const_cast<const region*>(self)));
}

/// Perform a down cast on an owning pointer.
/** \param object The owning pointer to down cast
 *   \return The down casted pointer.
 *   \note See down_cast(const region*) for more information.
 */
template<typename ObjectType>
utils::owner_ptr<ObjectType> down_cast(utils::owner_ptr<region>&& object) {
    return utils::owner_ptr<ObjectType>(std::move(object), down_cast<ObjectType>(object.get()));
}

/// Perform a down cast on an observer pointer.
/** \param object The observer pointer to down cast
 *   \return The down casted pointer.
 *   \note See down_cast(const region*) for more information.
 */
template<typename ObjectType>
utils::observer_ptr<ObjectType> down_cast(const utils::observer_ptr<region>& object) {
    return utils::observer_ptr<ObjectType>(object, down_cast<ObjectType>(object.get()));
}

/// Perform a down cast on an observer pointer.
/** \param object The observer pointer to down cast
 *   \return The down casted pointer.
 *   \note See down_cast(const region*) for more information.
 */
template<typename ObjectType>
utils::observer_ptr<ObjectType> down_cast(utils::observer_ptr<region>&& object) {
    return utils::observer_ptr<ObjectType>(std::move(object), down_cast<ObjectType>(object.get()));
}

/// Obtain an observer pointer from a raw pointer (typically 'this')
/** \param self The raw pointer to get an observer from
 *   \return The observer pointer.
 *   \note This returns the same things as self->observer_from_this(),
 *         but returning a pointer to the most-derived type known form the
 *         input pointer.
 */
template<typename ObjectType>
utils::observer_ptr<ObjectType> observer_from(ObjectType* self) {
    if (self)
        return utils::static_pointer_cast<ObjectType>(self->region::observer_from_this());
    else
        return nullptr;
}

} // namespace lxgui::gui

#endif
