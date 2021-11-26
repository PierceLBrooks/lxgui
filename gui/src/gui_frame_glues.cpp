#include "lxgui/gui_frame.hpp"

#include "lxgui/gui_backdrop.hpp"
#include "lxgui/gui_region.hpp"
#include "lxgui/gui_fontstring.hpp"
#include "lxgui/gui_texture.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_event.hpp"
#include "lxgui/gui_uiobject_tpl.hpp"

#include <sol/state.hpp>
#include <sol/variadic_args.hpp>

/** A @{UIObject} that can contain other objects and react to events.
*   This class, which is at the core of the UI design, can contain
*   other @{Frame}s as "children", and @{LayeredRegion}s sorted by layers
*   (text, images, ...). A frame can also react to events, and register
*   callbacks to be executed on particular events (key presses, etc.)
*   or on every tick.
*
*   Each frame has an optional "title region", which can be used to
*   define and draw a title bar. This title bar can then be used to
*   move the frame around the screen using mouse click and drag.
*   Furthermore, frames have optional support for resizing by click
*   and drag on corners or edges (opt in).
*
*   Frames can either move freely on the screen, or be "clamped" to the
*   screen so they cannot be partly outside of their render area.
*
*   __Rendering.__ Frames are grouped into different "strata", which are
*   rendered sequentially. Frames in a high strata will always be rendered
*   above frames in a low strata. Then, within a strata, frames are further
*   sorted by "level"; within this particular strata, a frame with a high
*   level will always be rendered above all frames with a lower level, but
*   it will still remain below other frames in a higher strata. The level
*   of a frame is automatically set to the maximum level inside the strata
*   when the frame is clicked, which effectively brings the frame to the
*   front.
*
*   __Children and regions.__ When a frame is hidden, all its children
*   and regions will also be hidden. Likewise, deleting a frame will
*   automatically delete all its children and regions, unless they are
*   detached first. Other than this, children and regions do not need to
*   be located inside the frame; this is controlled purely by their anchors.
*   Therefore, if a child is not anchored to its parent, moving the parent
*   will not automatically move the child.
*
*   __Events.__ Frames can react to events. For this to happen, a callback
*   function must be registered to handle the corresponding event. There are
*   two types of events. First: hard-coded UI events such as `OnKeyPress`
*   or `OnUpdate`, which are automatically triggered by lxgui. Second:
*   generic events, which can be triggered from various sources and all
*   forwarded to the `OnEvent` callback. Generic events are typically
*   generated by whatever application is being driven by lxgui (i.e., your
*   game), and they enable application-specific behavior (for example:
*   changing the UI when the player is under attack will likely require an
*   `"UNDER_ATTACK"` event).
*
*   To use the first type of events (hard-coded events), all you have to
*   do in general is register a callback function using @{Frame:add_script}
*   or @{Frame:set_script}. However, some hard-coded events require explicit
*   enabling. In particular:
*
*   - Events related to keyboard input (`OnKeyDown`, `OnKeyUp`) require
*   @{Frame:enable_keyboard}.
*   - Events related to mouse input (`OnDragStart`, `OnDragStop`,`OnEnter`,
*   `OnLeave`, `OnMouseUp`, `OnMouseDown`, `OnMouseWheel`, `OnReceiveDrag`)
*   require @{Frame:enable_mouse}.
*
*   To use the second type of events (generic events), you have to register
*   a callback for `OnEvent` _and_ register the frame for each generic event
*   you wish to listen to. This is done with @{Frame:register_event}.
*
*   Some events provide arguments to the registered callback function. For
*   example, the application can fire a `"UNIT_ATTACKED"` event when a unit
*   is under attack, and pass the ID of the attacked unit as a first argument,
*   and the ID of the attacker as a second argument. If a callback
*   function is registered using @{Frame:add_script} or @{Frame:set_script},
*   these arguments can be handled and named like regular function parameters.
*   In XML callback handlers, they can be accessed with the hard-coded generic
*   names `arg1`, `arg2`, etc.
*
*   Hard-coded events available to all @{Frame}s:
*
*   - `OnDragStart`: Triggered when one of the mouse button registered for
*   dragging (see @{Frame:register_for_drag}) has been pressed inside the
*   area of the screen occupied by the frame, and a mouse movement is first
*   recorded.
*   - `OnEnter`: Triggered when the mouse pointer enters into the area of
*   the screen occupied by the frame. Note: this only takes into account the
*   position and size of the frame and its title region, but not the space
*   occupied by its children or layered regions. Will not trigger if the
*   frame is hidden.
*   - `OnEvent`: Triggered when a registered generic event occurs. See
*   @{Frame:register_event}. To allow distinguishing which event has just
*   been fired, the registered callback function is always provided with a
*   first argument that is set to a string matching the event name. Further
*   arguments can be passed to the callback and are handled as for other events.
*   - `OnHide`: Triggered when @{UIObject:hide} is called, or when the frame
*   is hidden indirectly (for example if its parent is itself hidden). This
*   will only fire if the frame was previously shown.
*   - `OnKeyDown`: Triggered when any keyboard key is pressed. Will not
*   trigger if the frame is hidden. This event provides two arguments to
*   the registered callback: a number identifying the key, and the
*   human-readable name of the key.
*   - `OnKeyUp`: Triggered when any keyboard key is released. Will not
*   trigger if the frame is hidden. This event provides two arguments to
*   the registered callback: a number identifying the key, and the
*   human-readable name of the key.
*   - `OnLeave`: Triggered when the mouse pointer leaves the area of the
*   screen occupied by the frame. Note: this only takes into account the
*   position and size of the frame and its title region, but not the space
*   occupied by its children or layered regions. Will not trigger if the
*   frame is hidden, unless the frame was just hidden with the mouse
*   previously inside the frame.
*   - `OnLoad`: Triggered just after the frame is created. This is where
*   you would normally register for events and specific inputs, set up
*   initial states for extra logic, or do localization. When this event is
*   triggered, you can assume that all the frame's regions and children
*   have already been loaded. The same is true for other frames and regions
*   that are defined *earlier* in the same XML file, and those that are
*   defined in an addon listed *earlier* than the current addon in the
*   'addons.txt' file. In all other cases, frames or regions will not yet
*   be loaded when `OnLoad` is called, hence they cannot be refered to
*   (directly or indirectly).
*   - `OnMouseDown`: Triggered when any mouse button is pressed. Will not
*   trigger if the frame is hidden. This event provides one argument to
*   the registered callback: a string identifying the mouse button
*   (`"LeftButton"`, `"RightButton"`, or `"MiddleButton"`).
*   - `OnMouseUp`: Triggered when any mouse button is released. Will not
*   trigger if the frame is hidden. This event provides one argument to
*   the registered callback: a string identifying the mouse button
*   (`"LeftButton"`, `"RightButton"`, or `"MiddleButton"`).
*   - `OnMouseWheel`: Triggered when the mouse wheel is moved. This event
*   provides one argument to the registered callback: a number indicating by
*   how many "notches" the wheel has turned in this event. A positive value
*   means the wheel has been moved "away" from the user (this would normally
*   scroll *up* in a document).
*   - `OnReceiveDrag`: Triggered when the mouse pointer was previously
*   dragged onto the frame, and when one of the mouse button registered for
*   dragging (see @{Frame:register_for_drag}) is released. This enables
*   the "drop" in "drag and drop" operations.
*   - `OnShow`: Triggered when @{UIObject:show} is called, or when the frame
*   is shown indirectly (for example if its parent is itself shown). This
*   will only fire if the frame was previously hidden.
*   - `OnSizeChanged`: Triggered whenever the size of the frame changes, either
*   directly or indirectly. Be very careful not to call any function that could
*   change the size of the frame inside this callback, as this would generate
*   an infinite loop.
*   - `OnUpdate`: Triggered on every tick of the game loop. This event provides
*   one argument to the registered callback: a floating point number indicating
*   how much time has passed since the last call to `OnUpdate` (in seconds).
*   For optimal performance, prefer using other events types whenever possible.
*   `OnUpdate` callbacks will be executed over and over again, and can quickly
*   consume a lot of resources if user unreasonably. If you have to use
*   `OnUpdate`, you can mitigate performance problems by artificially reducing
*   the update rate: let the callback function only accumulate the time passed,
*   and wait until enough time has passed (say, half a second) to execute any
*   expensive operation. Then reset the accumulated time, and wait again.
*
*   Generic events fired natively by lxgui:
*
*   - `"LUA_ERROR"`: Triggered whenever a callback function or an addon script
*   file generates a Lua error. This event provides one argument to the
*   registered callback: a string containing the error message.
*   - `"ADDON_LOADED"`: Triggered when an addon is fully loaded. This event
*   provides one argument to the registered callback: a string containing the
*   name of the loaded addon.
*   - `"ENTERING_WORLD"`: Triggered once at the start of the program, at the
*   end of the first update tick.
*
*   __Virtual frames.__ Virtual frames are not displayed on the screen,
*   and technically are not part of the interface. They are only available
*   as "templates" that can be reused by other (virtual or non-virtual)
*   frames. This is useful for defining a few frame templates with a
*   particular style, and then reuse these templates across the interface
*   to ensure a consistent look. When inheriting from a virtual frame,
*   the inheriting frame will copy all the registered callbacks, all the
*   child frames, and all the layered regions of the virtual frame.
*
*   This inheritance mechanism can be chained: a virtual frame itself can
*   inherit from another virtual frame. It is also possible to inherit from
*   multiple virtual frames at once, which will copy their respective content
*   in the order they are specified.
*
*   Inherits all methods from: @{UIObject}.
*
*   Child classes: @{Button}, @{CheckButton}, @{FocusFrame}, @{EditBox},
*   @{ScrollFrame}, @{Slider}, @{StatusBar}.
*   @classmod Frame
*/

namespace lxgui {
namespace gui
{

void frame::register_on_lua(sol::state& mLua)
{
    auto mClass = mLua.new_usertype<frame>("Frame",
        sol::base_classes, sol::bases<uiobject>(),
        sol::meta_function::index,
        &frame::set_lua_member_,
        sol::meta_function::new_index,
        &frame::get_lua_member_);

    /** @function add_script
    */
    mClass.set_function("add_script", [](frame& mSelf, const std::string& sName,
        sol::protected_function mFunc)
    {
        mSelf.add_script(sName, std::move(mFunc));
    });

    /** @function create_font_string
    */
    mClass.set_function("create_font_string", [](frame& mSelf, const std::string& sName,
        sol::optional<std::string> sLayer, sol::optional<std::string> sInheritance)
    {
        layer_type mLayer = layer_type::ARTWORK;
        if (sLayer.has_value())
            mLayer = layer::get_layer_type(sLayer.value());

        return mSelf.create_region<font_string>(
            mLayer, sName,
            mSelf.get_manager().get_virtual_uiobject_list(sInheritance.value_or(""))
        );
    });

    /** @function create_texture
    */
    mClass.set_function("create_texture", [](frame& mSelf, const std::string& sName,
        sol::optional<std::string> sLayer, sol::optional<std::string> sInheritance)
    {
        layer_type mLayer = layer_type::ARTWORK;
        if (sLayer.has_value())
            mLayer = layer::get_layer_type(sLayer.value());

        return mSelf.create_region<texture>(
            mLayer, sName,
            mSelf.get_manager().get_virtual_uiobject_list(sInheritance.value_or(""))
        );
    });

    /** @function create_title_region
    */
    mClass.set_function("create_title_region", member_function<&frame::create_title_region>());

    /** @function disable_draw_layer
    */
    mClass.set_function("disable_draw_layer", [](frame& mSelf, const std::string& sLayer)
    {
        mSelf.disable_draw_layer(layer::get_layer_type(sLayer));
    });

    /** @function enable_draw_layer
    */
    mClass.set_function("enable_draw_layer", [](frame& mSelf, const std::string& sLayer)
    {
        mSelf.enable_draw_layer(layer::get_layer_type(sLayer));
    });

    /** @function enable_keyboard
    */
    mClass.set_function("enable_keyboard", member_function<&frame::enable_keyboard>());

    /** @function enable_mouse
    */
    mClass.set_function("enable_mouse", [](frame& mSelf, bool bEnable,
        sol::optional<bool> bWorldAllowed)
    {
        mSelf.enable_mouse(bEnable, bWorldAllowed.value_or(false));
    });

    /** @function enable_mouse_wheel
    */
    mClass.set_function("enable_mouse_wheel", member_function<&frame::enable_mouse_wheel>());

    /** @function get_backdrop
    */
    mClass.set_function("get_backdrop", [](sol::this_state mLua, const frame& mSelf) -> sol::optional<sol::table>
    {
        const backdrop* pBackdrop = mSelf.get_backdrop();
        if (!pBackdrop)
            return sol::nullopt;

        sol::table mReturn = sol::state_view(mLua).create_table();

        mReturn["bgFile"] = pBackdrop->get_background_file();
        mReturn["edgeFile"] = pBackdrop->get_edge_file();
        mReturn["tile"] = pBackdrop->is_background_tilling();

        mReturn["tileSize"] = pBackdrop->get_tile_size();
        mReturn["edgeSize"] = pBackdrop->get_edge_size();

        const auto& lInsets = pBackdrop->get_background_insets();
        mReturn["insets"]["left"] = lInsets.left;
        mReturn["insets"]["right"] = lInsets.right;
        mReturn["insets"]["top"] = lInsets.top;
        mReturn["insets"]["bottom"] = lInsets.bottom;

        return std::move(mReturn);
    });

    /** @function get_backdrop_border_color
    */
    mClass.set_function("get_backdrop_border_color", [](const frame& mSelf)
        -> sol::optional<std::tuple<float, float, float, float>>
    {
        if (!mSelf.get_backdrop())
            return sol::nullopt;

        const color& mColor = mSelf.get_backdrop()->get_edge_color();
        return std::make_tuple(mColor.r, mColor.g, mColor.b, mColor.a);
    });

    /** @function get_backdrop_color
    */
    mClass.set_function("get_backdrop_color", [](const frame& mSelf)
        -> sol::optional<std::tuple<float, float, float, float>>
    {
        if (!mSelf.get_backdrop())
            return sol::nullopt;

        const color& mColor = mSelf.get_backdrop()->get_background_color();
        return std::make_tuple(mColor.r, mColor.g, mColor.b, mColor.a);
    });

    /** @function get_children
    */
    mClass.set_function("get_children", [](const frame& mSelf)
    {
        std::vector<utils::observer_ptr<frame>> lChildren;
        lChildren.reserve(mSelf.get_rough_num_children());

        for (auto& mChild : mSelf.get_children())
        {
            lChildren.push_back(observer_from(&mChild));
        }

        return sol::as_table(std::move(lChildren));
    });

    /** @function get_effective_alpha
    */
    mClass.set_function("get_effective_alpha", member_function<&frame::get_effective_alpha>());

    /** @function get_effective_scale
    */
    mClass.set_function("get_effective_scale", member_function<&frame::get_effective_scale>());

    /** @function get_frame_level
    */
    mClass.set_function("get_frame_level", member_function<&frame::get_level>());

    /** @function get_frame_strata
    */
    mClass.set_function("get_frame_strata", [](const frame& mSelf)
    {
        frame_strata mStrata = mSelf.get_frame_strata();
        std::string sStrata;

        if (mStrata == frame_strata::BACKGROUND)
            sStrata = "BACKGROUND";
        else if (mStrata == frame_strata::LOW)
            sStrata = "LOW";
        else if (mStrata == frame_strata::MEDIUM)
            sStrata = "MEDIUM";
        else if (mStrata == frame_strata::HIGH)
            sStrata = "HIGH";
        else if (mStrata == frame_strata::DIALOG)
            sStrata = "DIALOG";
        else if (mStrata == frame_strata::FULLSCREEN)
            sStrata = "FULLSCREEN";
        else if (mStrata == frame_strata::FULLSCREEN_DIALOG)
            sStrata = "FULLSCREEN_DIALOG";
        else if (mStrata == frame_strata::TOOLTIP)
            sStrata = "TOOLTIP";

        return sStrata;
    });

    /** @function get_frame_type
    */
    mClass.set_function("get_frame_type", member_function<&frame::get_frame_type>());

    /** @function get_hit_rect_insets
    */
    mClass.set_function("get_hit_rect_insets", [](const frame& mSelf)
    {
        const bounds2f& lInsets = mSelf.get_abs_hit_rect_insets();
        return std::make_tuple(lInsets.left, lInsets.right, lInsets.top, lInsets.bottom);
    });

    /** @function get_id
    */
    mClass.set_function("get_id", member_function<&frame::get_id>());

    /** @function get_max_resize
    */
    mClass.set_function("get_max_resize", [](const frame& mSelf)
    {
        const vector2f& lMax = mSelf.get_max_resize();
        return std::make_tuple(lMax.x, lMax.y);
    });

    /** @function get_min_resize
    */
    mClass.set_function("get_min_resize", [](const frame& mSelf)
    {
        const vector2f& lMin = mSelf.get_min_resize();
        return std::make_tuple(lMin.x, lMin.y);
    });

    /** @function get_num_children
    */
    mClass.set_function("get_num_children", member_function<&frame::get_num_children>());

    /** @function get_num_regions
    */
    mClass.set_function("get_num_regions", member_function<&frame::get_num_regions>());

    /** @function get_scale
    */
    mClass.set_function("get_scale", member_function<&frame::get_scale>());

    /** @function get_script
    */
    mClass.set_function("get_script", [](const frame& mSelf, const std::string& sScriptName) -> sol::object
    {
        if (!mSelf.has_script(sScriptName))
            return sol::lua_nil;

        std::string sAdjustedName = get_adjusted_script_name(sScriptName);
        return mSelf.get_manager().get_lua()[mSelf.get_lua_name()][sAdjustedName];
    });

    /** @function get_title_region
    */
    mClass.set_function("get_title_region", member_function< // select the right overload for Lua
        static_cast<utils::observer_ptr<region> (frame::*)()>(&frame::get_title_region)>());

    /** @function has_script
    */
    mClass.set_function("has_script", member_function<&frame::has_script>());

    /** @function is_clamped_to_screen
    */
    mClass.set_function("is_clamped_to_screen", member_function<&frame::is_clamped_to_screen>());

    /** @function is_frame_type
    */
    mClass.set_function("is_frame_type", [](const frame& mSelf, const std::string& sType)
    {
        return mSelf.get_frame_type() == sType;
    });

    /** @function is_keyboard_enabled
    */
    mClass.set_function("is_keyboard_enabled", member_function<&frame::is_keyboard_enabled>());

    /** @function is_mouse_enabled
    */
    mClass.set_function("is_mouse_enabled", member_function<&frame::is_mouse_enabled>());

    /** @function is_mouse_wheel_enabled
    */
    mClass.set_function("is_mouse_wheel_enabled", member_function<&frame::is_mouse_wheel_enabled>());

    /** @function is_movable
    */
    mClass.set_function("is_movable", member_function<&frame::is_movable>());

    /** @function is_resizable
    */
    mClass.set_function("is_resizable", member_function<&frame::is_resizable>());

    /** @function is_top_level
    */
    mClass.set_function("is_top_level", member_function<&frame::is_top_level>());

    /** @function is_user_placed
    */
    mClass.set_function("is_user_placed", member_function<&frame::is_user_placed>());

    /** @function raise
    */
    mClass.set_function("raise", member_function<&frame::raise>());

    /** @function register_all_events
    */
    mClass.set_function("register_all_events", member_function<&frame::register_all_events>());

    /** @function register_event
    */
    mClass.set_function("register_event", member_function<&frame::register_event>());

    /** @function register_for_drag
    */
    mClass.set_function("register_for_drag", [](frame& mSelf, sol::optional<std::string> sButton1,
        sol::optional<std::string> sButton2, sol::optional<std::string> sButton3)
    {
        std::vector<std::string> lButtonList;
        if (sButton1.has_value())
            lButtonList.push_back(sButton1.value());
        if (sButton2.has_value())
            lButtonList.push_back(sButton2.value());
        if (sButton3.has_value())
            lButtonList.push_back(sButton3.value());

        mSelf.register_for_drag(lButtonList);
    });

    /** @function set_backdrop
    */
    mClass.set_function("set_backdrop", [](frame& mSelf, sol::optional<sol::table> mTableOpt)
    {
        if (!mTableOpt.has_value())
        {
            mSelf.set_backdrop(nullptr);
            return;
        }

        std::unique_ptr<backdrop> pBackdrop(new backdrop(mSelf));

        sol::table& mTable = mTableOpt.value();
        manager& mManager = mSelf.get_manager();

        pBackdrop->set_background(mManager.parse_file_name(mTable["bgFile"].get_or<std::string>("")));
        pBackdrop->set_edge(mManager.parse_file_name(mTable["edgeFile"].get_or<std::string>("")));
        pBackdrop->set_background_tilling(mTable["tile"].get_or(false));

        float fTileSize = mTable["tileSize"].get_or<float>(0.0);
        if (fTileSize != 0)
            pBackdrop->set_tile_size(fTileSize);

        float fEdgeSize = mTable["edgeSize"].get_or<float>(0.0);
        if (fEdgeSize != 0)
            pBackdrop->set_edge_size(fEdgeSize);

        if (mTable["insets"])
        {
            pBackdrop->set_background_insets(bounds2f(
                mTable["insets"]["left"].get_or<float>(0),
                mTable["insets"]["right"].get_or<float>(0),
                mTable["insets"]["top"].get_or<float>(0),
                mTable["insets"]["bottom"].get_or<float>(0)
            ));
        }

        mSelf.set_backdrop(std::move(pBackdrop));
    });

    /** @function set_backdrop_border_color
    */
    mClass.set_function("set_backdrop_border_color", sol::overload(
    [](frame& mSelf, float fR, float fG, float fB, sol::optional<float> fA)
    {
        mSelf.get_or_create_backdrop().set_edge_color(color(fR, fG, fB, fA.value_or(1.0f)));
    },
    [](frame& mSelf, const std::string& sColor)
    {
        mSelf.get_or_create_backdrop().set_edge_color(color(sColor));
    }));

    /** @function set_backdrop_color
    */
    mClass.set_function("set_backdrop_color", sol::overload(
    [](frame& mSelf, float fR, float fG, float fB, sol::optional<float> fA)
    {
        mSelf.get_or_create_backdrop().set_background_color(color(fR, fG, fB, fA.value_or(1.0f)));
    },
    [](frame& mSelf, const std::string& sColor)
    {
        mSelf.get_or_create_backdrop().set_background_color(color(sColor));
    }));

    /** @function set_clamped_to_screen
    */
    mClass.set_function("set_clamped_to_screen", member_function<&frame::set_clamped_to_screen>());

    /** @function set_frame_level
    */
    mClass.set_function("set_frame_level", member_function<&frame::set_level>());

    /** @function set_frame_strata
    */
    mClass.set_function("set_frame_strata", member_function< // select the right overload for Lua
        static_cast<void (frame::*)(const std::string&)>(&frame::set_frame_strata)>());

    /** @function set_hit_rect_insets
    */
    mClass.set_function("set_hit_rect_insets", member_function< // select the right overload for Lua
        static_cast<void (frame::*)(float, float, float, float)>(&frame::set_abs_hit_rect_insets)>());

    /** @function set_max_resize
    */
    mClass.set_function("set_max_resize", member_function< // select the right overload for Lua
        static_cast<void (frame::*)(float, float)>(&frame::set_max_resize)>());

    /** @function set_min_resize
    */
    mClass.set_function("set_min_resize", member_function< // select the right overload for Lua
        static_cast<void (frame::*)(float, float)>(&frame::set_min_resize)>());

    /** @function set_max_width
    */
    mClass.set_function("set_max_width", member_function<&frame::set_max_width>());

    /** @function set_max_height
    */
    mClass.set_function("set_max_height", member_function<&frame::set_max_height>());

    /** @function set_min_width
    */
    mClass.set_function("set_min_width", member_function<&frame::set_min_width>());

    /** @function set_min_height
    */
    mClass.set_function("set_min_height", member_function<&frame::set_min_height>());

    /** @function set_movable
    */
    mClass.set_function("set_movable", member_function<&frame::set_movable>());

    /** @function set_resizable
    */
    mClass.set_function("set_resizable", member_function<&frame::set_resizable>());

    /** @function set_scale
    */
    mClass.set_function("set_scale", member_function<&frame::set_scale>());

    /** @function set_script
    */
    mClass.set_function("set_script", [](frame& mSelf, const std::string& sScriptName,
        sol::optional<sol::protected_function> mScript)
    {
        if (!mSelf.can_use_script(sScriptName))
        {
            gui::out << gui::error << mSelf.get_frame_type() << " : "
                << "\"" << mSelf.get_name() << "\" cannot use script \""
                << sScriptName << "\"." << std::endl;
            return;
        }

        if (mScript.has_value())
            mSelf.set_script(sScriptName, mScript.value());
        else
            mSelf.remove_script(sScriptName);
    });

    /** @function set_top_level
    */
    mClass.set_function("set_top_level", member_function<&frame::set_top_level>());

    /** @function set_user_placed
    */
    mClass.set_function("set_user_placed", member_function<&frame::set_user_placed>());

    /** @function start_moving
    */
    mClass.set_function("start_moving", member_function<&frame::start_moving>());

    /** @function start_sizing
    */
    mClass.set_function("start_sizing", [](frame& mSelf, const std::string& sPoint)
    {
        mSelf.start_sizing(anchor::get_anchor_point(sPoint));
    });

    /** @function stop_moving_or_sizing
    */
    mClass.set_function("stop_moving_or_sizing", [](frame& mSelf)
    {
        mSelf.stop_moving();
        mSelf.stop_sizing();
    });

    /** @function unregister_all_events
    */
    mClass.set_function("unregister_all_events", member_function<&frame::unregister_all_events>());

    /** @function unregister_event
    */
    mClass.set_function("unregister_event", member_function<&frame::unregister_event>());
}

}
}
