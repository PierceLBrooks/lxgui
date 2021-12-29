#ifndef LXGUI_GUI_MANAGER_HPP
#define LXGUI_GUI_MANAGER_HPP

#include <lxgui/lxgui.hpp>
#include "lxgui/gui_eventmanager.hpp"
#include "lxgui/gui_eventreceiver.hpp"
#include "lxgui/gui_addon.hpp"
#include "lxgui/gui_anchor.hpp"
#include "lxgui/gui_uiobject.hpp"
#include "lxgui/gui_quad.hpp"
#include "lxgui/input_keys.hpp"

#include <lxgui/utils_exception.hpp>
#include <lxgui/utils_view.hpp>
#include <lxgui/utils_observer.hpp>

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <tuple>
#include <array>
#include <functional>
#include <memory>

namespace sol {
    class state;
}

namespace lxgui {

namespace input {
    class source;
    class manager;
}

namespace gui
{
    class frame;
    class focus_frame;
    class renderer;
    class localizer;
    class factory;
    class uiroot;
    class virtual_uiroot;
    class addon_registry;

    /// Manages the user interface
    class manager : private event_manager, public event_receiver
    {
    public :

        /// Constructor.
        /** \param pInputSource The input source to use
        *   \param pRenderer    The renderer implementation
        */
        manager(std::unique_ptr<input::source> pInputSource,
                std::unique_ptr<renderer> pRenderer);

        /// Destructor.
        ~manager() override;

        manager(const manager& mMgr) = delete;
        manager(manager&& mMgr) = delete;
        manager& operator = (const manager& mMgr) = delete;
        manager& operator = (manager&& mMgr) = delete;

        /// Sets the global UI scaling factor.
        /** \param fScalingFactor The factor to use for rescaling (1: no rescaling, default)
        *   \note This value determines how to convert sizing units or position coordinates
        *         into actual number of pixels. By default, units specified for sizes and
        *         positions are 1:1 mapping with pixels on the screen. If designing the UI
        *         on a "traditional" display (say, 1080p resolution monitor), the UI will not
        *         scale correctly when running on high-DPI displays unless the scaling factor is
        *         adjusted accordingly. The value of the scaling factor should be the ratio
        *         DPI_target/DPI_dev, where DPI_dev is the DPI of the display used for
        *         development, and DPI_target is the DPI of the display used to run the program.
        *         In addition, the scaling factor can also be used to improve accessibility of
        *         the interface to users with poorer eye sight, which would benefit from larger
        *         font sizes and larger icons.
        */
        void set_interface_scaling_factor(float fScalingFactor);

        /// Returns the current UI scaling factor.
        /** \return The current UI scaling factor
        *   \see set_interface_scaling_factor()
        */
        float get_interface_scaling_factor() const;

        /// Enables or disables interface caching.
        /** \param bEnableCaching 'true' to enable, 'false' to disable
        *   \see toggle_caching()
        */
        void enable_caching(bool bEnableCaching);

        /// Toggles interface caching.
        /** \note Disabled by default. Enabling this will most likely improve performances,
        *         at the expense of higher GPU memory usage. The UI will be cached into
        *         large render targets, which are only redrawn when the UI changes, rather
        *         than redrawn on each frame.
        */
        void toggle_caching();

        /// Checks if interface caching is enabled.
        /** \return 'true' if interface caching is enabled
        *   \see toggle_caching()
        */
        bool is_caching_enabled() const;

        /// Adds a new directory to be parsed for UI addons.
        /** \param sDirectory The new directory
        */
        void add_addon_directory(const std::string& sDirectory);

        /// Clears the addon directory list.
        /** \note This is usefull whenever you need to reload a
        *         completely different UI (for example, when switching
        *         from your game's main menu to the real game).
        */
        void clear_addon_directory_list();

        /// Prints in the log several performance statistics.
        void print_statistics();

        /// Prints debug informations in the log file.
        /** \note Calls uiobject::serialize().
        */
        std::string print_ui() const;

        /// Binds some Lua code to a key.
        /** \param uiKey      The key to bind
        *   \param sLuaString The Lua code that will be executed
        */
        void set_key_binding(input::key uiKey, const std::string& sLuaString);

        /// Binds some Lua code to a key.
        /** \param uiKey      The key to bind
        *   \param uiModifier The modifier key (shift, ctrl, ...)
        *   \param sLuaString The Lua code that will be executed
        */
        void set_key_binding(input::key uiKey, input::key uiModifier, const std::string& sLuaString);

        /// Binds some Lua code to a key.
        /** \param uiKey       The key to bind
        *   \param uiModifier1 The first modifier key (shift, ctrl, ...)
        *   \param uiModifier2 The second modifier key (shift, ctrl, ...)
        *   \param sLuaString  The Lua code that will be executed
        */
        void set_key_binding(
            input::key uiKey, input::key uiModifier1, input::key uiModifier2,
            const std::string& sLuaString
        );

        /// Unbinds a key.
        /** \param uiKey       The key to unbind
        *   \param uiModifier1 The first modifier key (shift, ctrl, ...), default is no modifier
        *   \param uiModifier2 The second modifier key (shift, ctrl, ...), default is no modified
        */
        void remove_key_binding(
            input::key uiKey, input::key uiModifier1 = input::key::K_UNASSIGNED,
            input::key uiModifier2 = input::key::K_UNASSIGNED
        );

        /// Returns the GUI Lua state (sol wrapper).
        /** \return The GUI Lua state
        */
        sol::state& get_lua();

        /// Returns the GUI Lua state (sol wrapper).
        /** \return The GUI Lua state
        */
        const sol::state& get_lua() const;

        /// Set the code to be executed on each fresh Lua state.
        /** \param pLuaRegs Some code that will get executed immediately after the Lua
        *                   state is created
        *   \note This function is usefull if you need to create additionnal
        *         resources on the Lua state before the GUI files are loaded.
        *         The argument to this function will be stored and reused, each time
        *         the Lua state is created (e.g., when the GUI is re-loaded, see reload_ui()).
        *   \warning Do not call this function while the manager is running update()
        *           (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void register_lua_glues(std::function<void(gui::manager&)> pLuaRegs);

        /// Loads the UI.
        /** \note Creates the Lua state and loads addon files (if any).
        *   \warning Do not call this function while the manager is running update()
        *            (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void load_ui();

        /// Closes the UI, deletes widgets.
        /** \warning Do not call this function while the manager is running update()
        *            (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void close_ui();

        /// Closes the UI and load it again (at the end of the current or next update()).
        /** \note Because the actual re-loading is deferred to the end of the current update() call,
        *         or to the end of the next update() call, it is safe to call this function at any
        *         time. If you need to reload the UI without delay, use reload_ui_now().
        */
        void reload_ui();

        /// Closes the UI and load it again (immediately).
        /** \note Calls close_ui() then load_ui().
        *   \warning Do not call this function while the manager is running update()
        *            (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void reload_ui_now();

        /// Tells the rendering back-end to start rendering into a new target.
        /** \param pTarget The target to render to (nullptr to render to the screen)
        */
        void begin(std::shared_ptr<render_target> pTarget = nullptr) const;

        /// Tells the rendering back-end we are done rendering on the current target.
        /** \note For most back-ends, this is when the rendering is actually
        *         done, so do not forget to call it even if it appears to do nothing.
        */
        void end() const;

        /// Renders the UI into the current render target.
        void render_ui() const;

        /// Checks if the UI has been loaded.
        /** \return 'true' if the UI has being loaded
        */
        bool is_loaded() const;

        /// Ask this manager for movement management.
        /** \param pObj        The object to move
        *   \param pAnchor     The reference anchor
        *   \param mConstraint The constraint axis if any
        *   \param mApplyConstraintFunc Optional function to implement further constraints
        *   \note Movement is handled by the manager itself, you don't
        *         need to do anything.
        */
        void start_moving(
            utils::observer_ptr<uiobject> pObj, anchor* pAnchor = nullptr,
            constraint mConstraint = constraint::NONE,
            std::function<void()> mApplyConstraintFunc = nullptr
        );

        /// Stops movement for the given object.
        /** \param mObj The object to stop moving
        */
        void stop_moving(const uiobject& mObj);

        /// Checks if the given object is allowed to move.
        /** \param mObj The object to check
        *   \return 'true' if the given object is allowed to move
        */
        bool is_moving(const uiobject& mObj) const;

        /// Starts resizing a widget.
        /** \param pObj   The object to resize
        *   \param mPoint The sizing point
        *   \note Resizing is handled by the manager itself, you don't
        *         need to do anything.
        */
        void start_sizing(utils::observer_ptr<uiobject> pObj, anchor_point mPoint);

        /// Stops sizing for the given object.
        /** \param mObj The object to stop sizing
        */
        void stop_sizing(const uiobject& mObj);

        /// Checks if the given object is allowed to be resized.
        /** \param mObj The object to check
        *   \return 'true' if the given object is allowed to be resized
        */
        bool is_sizing(const uiobject& mObj) const;

        /// Returns the accumulated mouse movement.
        /** \return The accumulated mouse movement
        *   \note This vector is reset to zero whenever start_moving() or
        *         start_sizing() is called.
        */
        const vector2f& get_movement() const;

        /// Tells this manager an object has moved.
        void notify_object_moved();

        /// Enables/disables input response for all widgets.
        /** \param bEnable 'true' to enable input
        *   \note See toggle_input() and is_input_enabled().
        */
        void enable_input(bool bEnable);

        /// Toggles input response for all widgets.
        /** \note Enabled by default.
        *   \note See is_input_enabled().
        */
        void toggle_input();

        /// Checks if input response is enabled for all widgets.
        /** \return 'true' if input response is enabled
        *   \note All widgets must call this function and check
        *         its return value before reacting to input events.
        *   \note See toggle_input().
        */
        bool is_input_enabled() const;

        /// Returns the frame under the mouse.
        /** \return The frame under the mouse (nullptr if none)
        */
        const utils::observer_ptr<frame>& get_hovered_frame();

        /// Notifies this manager that it should update the hovered frame.
        void notify_hovered_frame_dirty();

        /// Asks this manager for focus.
        /** \param pFocusFrame The focus_frame requesting focus
        */
        void request_focus(utils::observer_ptr<focus_frame> pFocusFrame);

        /// Updates this manager and its widgets.
        /** \param fDelta The time elapsed since the last call
        */
        void update_ui(float fDelta);

        /// Called whenever an Event occurs.
        /** \param mEvent The Event which has occured
        */
        void on_event(const event& mEvent) override;

        /// Returns the renderer implementation.
        /** \return The renderer implementation
        */
        const renderer& get_renderer() const { return *pRenderer_; }

        /// Returns the renderer implementation.
        /** \return The renderer implementation
        */
        renderer& get_renderer() { return *pRenderer_; }

        /// Returns the gui event manager.
        /** \return The gui event manager
        */
        const event_manager& get_event_manager() const { return *this; }

        /// Returns the gui event manager.
        /** \return The gui event manager
        */
        event_manager& get_event_manager() { return *this; }

        /// Returns the input manager associated to this gui.
        /** \return The input manager associated to this gui
        */
        const input::manager& get_input_manager() const { return *pInputManager_; }

        /// Returns the input manager associated to this gui.
        /** \return The input manager associated to this gui
        */
        input::manager& get_input_manager() { return *pInputManager_; }

        /// Returns the object used for localizing strings.
        /** \return The current localizer
        */
        localizer& get_localizer() { return *pLocalizer_; }

        /// Returns the object used for localizing strings.
        /** \return The current localizer
        */
        const localizer& get_localizer() const { return *pLocalizer_; }

        /// Returns the UI root object, which contains root frames.
        /** \return The root object
        */
        uiroot& get_root() { return *pRoot_; }

        /// Returns the UI root object, which contains root frames.
        /** \return The root object
        */
        const uiroot& get_root() const { return *pRoot_; }

        /// Returns the UI root object, which contains root frames.
        /** \return The root object
        */
        virtual_uiroot& get_virtual_root() { return *pVirtualRoot_; }

        /// Returns the UI root object, which contains root frames.
        /** \return The root object
        */
        const virtual_uiroot& get_virtual_root() const { return *pVirtualRoot_; }

        /// Returns the UI object factory, which is used to create new objects.
        /** \return The factory object
        */
        factory& get_factory() { return *pFactory_; }

        /// Returns the UI object factory, which is used to create new objects.
        /** \return The factory object
        */
        const factory& get_factory() const { return *pFactory_; }

        /// Returns the addon registry, which keeps track of loaded addons.
        /** \return The registry object
        */
        addon_registry* get_addon_registry() { return pAddOnRegistry_.get(); }

        /// Returns the addon registry, which keeps track of loaded addons.
        /** \return The registry object
        */
        const addon_registry* get_addon_registry() const { return pAddOnRegistry_.get(); }

        /// Return an observer pointer to 'this'.
        /** \return A new observer pointer pointing to 'this'.
        */
        utils::observer_ptr<const manager> observer_from_this() const
        {
            return utils::static_pointer_cast<const manager>(event_receiver::observer_from_this());
        }

        /// Return an observer pointer to 'this'.
        /** \return A new observer pointer pointing to 'this'.
        */
        utils::observer_ptr<manager> observer_from_this()
        {
            return utils::static_pointer_cast<manager>(event_receiver::observer_from_this());
        }

    private :

        /// Creates the lua::state that will be used to communicate with the GUI.
        /**
        *   \warning Do not call this function while the manager is running update()
        *           (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void create_lua_();

        /// Reads GUI files in the directory list.
        /** \note See add_addon_directory().
        *   \note See load_ui().
        *   \note See create_lua().
        *   \warning Do not call this function while the manager is running update()
        *            (i.e., do not call this directly from a frame's callback, C++ or Lua).
        */
        void read_files_();

        void load_addon_toc_(const std::string& sAddOnName, const std::string& sAddOnDirectory);
        void load_addon_files_(addon* pAddOn);
        void load_addon_directory_(const std::string& sDirectory);

        void save_variables_(const addon* pAddOn);
        std::string serialize_global_(const std::string& sVariable) const;

        void clear_hovered_frame_();
        void update_hovered_frame_();
        void set_hovered_frame_(utils::observer_ptr<frame> pFrame,
            const vector2f& mMousePos = vector2f::ZERO);

        void parse_layout_file_(const std::string& sFile, addon* pAddOn);

        float fScalingFactor_ = 1.0f;
        float fBaseScalingFactor_ = 1.0f;
        bool  bEnableCaching_ = false;

        std::unique_ptr<sol::state>        pLua_;
        std::function<void(gui::manager&)> pLuaRegs_;

        bool bClosed_ = true;
        bool bLoadingUI_ = false;
        bool bReloadUI_ = false;
        bool bFirstIteration_ = true;
        bool bUpdating_ = false;

        bool                            bInputEnabled_ = true;
        std::unique_ptr<input::manager> pInputManager_;

        template<typename T>
        using key_map = std::unordered_map<input::key,T>;
        template<typename T>
        using string_map = std::unordered_map<std::string,T>;

        key_map<key_map<key_map<std::string>>> lKeyBindingList_;

        utils::owner_ptr<uiroot>         pRoot_;
        utils::owner_ptr<virtual_uiroot> pVirtualRoot_;

        std::vector<std::string>        lGUIDirectoryList_;
        std::unique_ptr<addon_registry> pAddOnRegistry_;

        bool                             bObjectMoved_ = false;
        utils::observer_ptr<frame>       pHoveredFrame_ = nullptr;
        bool                             bUpdateHoveredFrame_ = false;
        utils::observer_ptr<focus_frame> pFocusedFrame_ = nullptr;

        utils::observer_ptr<uiobject> pMovedObject_ = nullptr;
        utils::observer_ptr<uiobject> pSizedObject_ = nullptr;
        vector2f                      mMouseMovement_;

        anchor*    pMovedAnchor_ = nullptr;
        vector2f   mMovementStartPosition_;
        constraint mConstraint_ = constraint::NONE;
        std::function<void()> mApplyConstraintFunc_;

        vector2f mResizeStart_;
        bool bResizeWidth_ = false;
        bool bResizeHeight_ = false;
        bool bResizeFromRight_ = false;
        bool bResizeFromBottom_ = false;

        std::unique_ptr<factory> pFactory_;

        std::unique_ptr<localizer> pLocalizer_;
        std::unique_ptr<renderer>  pRenderer_;
    };
}
}


#endif
