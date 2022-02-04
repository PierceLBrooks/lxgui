#include "lxgui/gui_layoutnode.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_scrollframe.hpp"

namespace lxgui::gui {

void scroll_frame::parse_all_nodes_before_children_(const layout_node& mNode) {
    frame::parse_all_nodes_before_children_(mNode);
    parse_scroll_child_node_(mNode);
}

void scroll_frame::parse_scroll_child_node_(const layout_node& mNode) {
    if (const layout_node* pScrollChildNode = mNode.try_get_child("ScrollChild")) {
        if (pScrollChildNode->get_children_count() == 0) {
            gui::out << gui::warning << pScrollChildNode->get_location()
                     << " : "
                        "ScrollChild node needs a child node."
                     << std::endl;
            return;
        }

        if (pScrollChildNode->get_children_count() > 1) {
            gui::out << gui::warning << pScrollChildNode->get_location()
                     << " : "
                        "ScrollChild node needs only one child node; other nodes will be ignored."
                     << std::endl;
            return;
        }

        const layout_node& mChildNode   = pScrollChildNode->get_child(0);
        auto               pScrollChild = parse_child_(mChildNode, "");
        if (!pScrollChild)
            return;

        const layout_node* pAnchors = mChildNode.try_get_child("Anchors");
        if (pAnchors) {
            gui::out << gui::warning << pAnchors->get_location() << " : "
                     << "Scroll child's anchors are ignored." << std::endl;
        }

        if (!mChildNode.has_child("Size")) {
            gui::out << gui::warning << mChildNode.get_location()
                     << " : "
                        "Scroll child needs its size to be defined in a Size block."
                     << std::endl;
        }

        this->set_scroll_child(remove_child(pScrollChild));
    }
}

} // namespace lxgui::gui
