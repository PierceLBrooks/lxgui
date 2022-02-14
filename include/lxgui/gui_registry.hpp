#ifndef LXGUI_GUI_REGISTRY_HPP
#define LXGUI_GUI_REGISTRY_HPP

#include "lxgui/lxgui.hpp"
#include "lxgui/utils_observer.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace lxgui::gui {

class region;

/// Keeps track of created UI objects and records their names for lookup.
class registry {
public:
    registry()                      = default;
    virtual ~registry()             = default;
    registry(const registry& m_mgr) = default;
    registry(registry&& m_mgr)      = default;
    registry& operator=(const registry& m_mgr) = default;
    registry& operator=(registry&& m_mgr) = default;

    /// Checks the provided string is suitable for naming a region.
    /** \param sName The string to test
     *   \return 'true' if the provided string can be the name of a region
     */
    bool check_region_name(std::string_view s_name) const;

    /// Adds a region to be handled by this registry.
    /** \param pObj The object to add
     *   \return 'false' if the name of the region was already taken
     */
    bool add_region(utils::observer_ptr<region> p_obj);

    /// Removes a region from this registry.
    /** \param mObj The object to remove
     */
    void remove_region(const region& m_obj);

    /// Returns the region associated with the given name.
    /** \param sName    The name of the region you're after
     *   \return The region associated with the given name, or nullptr if not found
     */
    utils::observer_ptr<const region> get_region_by_name(std::string_view s_name) const;

    /// Returns the region associated with the given name.
    /** \param sName    The name of the region you're after
     *   \return The region associated with the given name, or nullptr if not found
     */
    utils::observer_ptr<region> get_region_by_name(std::string_view s_name) {
        return utils::const_pointer_cast<region>(
            const_cast<const registry*>(this)->get_region_by_name(s_name));
    }

private:
    template<typename T>
    using string_map = std::unordered_map<std::string, T>;

    string_map<utils::observer_ptr<region>> named_object_list_;
};

} // namespace lxgui::gui

#endif
