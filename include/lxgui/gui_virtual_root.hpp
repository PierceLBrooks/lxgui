#ifndef LXGUI_GUI_VIRTUAL_UIROOT_HPP
#define LXGUI_GUI_VIRTUAL_UIROOT_HPP

#include "lxgui/gui_frame_container.hpp"
#include "lxgui/gui_virtual_registry.hpp"
#include "lxgui/lxgui.hpp"
#include "lxgui/utils_observer.hpp"

#include <list>
#include <memory>

namespace lxgui { namespace gui {

class registry;
class manager;
class renderer;

/// Root of the virtual UI object hierarchy.
/** This class contains and owns all virtual "root" frames (frames with no parents)
 *   and is responsible for their lifetime.
 */
class virtual_root : public frame_container {
public:
    /// Constructor.
    /** \param mManager            The GUI manager
     *   \param mNonVirtualRegistry The registry for non-virtual objects (for error messages)
     */
    explicit virtual_root(manager& mManager, registry& mNonVirtualRegistry);

    /// Destructor.
    ~virtual_root();

    virtual_root(const virtual_root&) = delete;
    virtual_root(virtual_root&&)      = delete;
    virtual_root& operator=(const virtual_root&) = delete;
    virtual_root& operator=(virtual_root&&) = delete;

    /// Returns the manager instance associated with this root.
    /** \return The manager instance associated with this root
     */
    manager& get_manager() {
        return mManager_;
    }

    /// Returns the manager instance associated with this root.
    /** \return The manager instance associated with this root
     */
    const manager& get_manager() const {
        return mManager_;
    }

    /// Returns the UI object registry, which keeps track of all objects in the UI.
    /** \return The registry object
     */
    virtual_registry& get_registry() {
        return mObjectRegistry_;
    }

    /// Returns the UI object registry, which keeps track of all objects in the UI.
    /** \return The registry object
     */
    const virtual_registry& get_registry() const {
        return mObjectRegistry_;
    }

private:
    manager&         mManager_;
    virtual_registry mObjectRegistry_;
};

}} // namespace lxgui::gui

#endif
