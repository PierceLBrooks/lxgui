#include "lxgui/gui_frame_renderer.hpp"

#include "lxgui/gui_frame.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/utils_range.hpp"
#include "lxgui/utils_string.hpp"

namespace lxgui::gui {

template<typename T>
void check_sorted(const T& list) {
    gui::out << "----------" << std::endl;
    for (auto iter = list.begin(); iter != list.end(); ++iter) {
        gui::out << " - " << (*iter)->get_name() << ": "
                 << utils::to_string((*iter)->get_effective_frame_strata()) << ", "
                 << (*iter)->get_level() << std::endl;

        if (iter != list.begin()) {
            gui::out << " is greater than last? " << list.comparator()(*(iter - 1), *iter)
                     << std::endl;
        }
    }
    gui::out << "----------" << std::endl;

    if (!std::is_sorted(list.begin(), list.end(), list.comparator())) {
        throw gui::exception("frame_renderer", "Frame list not sorted!!");
    }
}

struct strata_comparator {
    bool operator()(frame_strata s1, frame_strata s2) const {
        using int_type        = std::underlying_type_t<frame_strata>;
        const auto strata_id1 = static_cast<int_type>(s1);
        const auto strata_id2 = static_cast<int_type>(s2);
        return strata_id1 < strata_id2;
    }

    bool operator()(const frame* f1, frame_strata s2) const {
        return operator()(f1->get_effective_frame_strata(), s2);
    }

    bool operator()(frame_strata s1, const frame* f2) const {
        return operator()(s1, f2->get_effective_frame_strata());
    }

    bool operator()(const frame* f1, const frame* f2) const {
        return operator()(f1->get_effective_frame_strata(), f2->get_effective_frame_strata());
    }
};

bool frame_renderer::frame_comparator::operator()(const frame* f1, const frame* f2) const {
    using int_type        = std::underlying_type_t<frame_strata>;
    const auto strata_id1 = static_cast<int_type>(f1->get_effective_frame_strata());
    const auto strata_id2 = static_cast<int_type>(f2->get_effective_frame_strata());

    if (strata_id1 < strata_id2)
        return true;
    if (strata_id1 > strata_id2)
        return false;

    const auto level1 = f1->get_level();
    const auto level2 = f2->get_level();

    if (level1 < level2)
        return true;
    if (level1 > level2)
        return false;

    return f1 < f2;
}

frame_renderer::frame_renderer() {
    for (std::size_t i = 0; i < strata_list_.size(); ++i) {
        strata_list_[i].id = static_cast<frame_strata>(i);
    }
}

void frame_renderer::notify_strata_needs_redraw_(strata& strata) {
    strata.redraw_flag = true;
}

void frame_renderer::notify_strata_needs_redraw(frame_strata strata_id) {
    notify_strata_needs_redraw_(strata_list_[static_cast<std::size_t>(strata_id)]);
}

void frame_renderer::notify_rendered_frame(const utils::observer_ptr<frame>& obj, bool rendered) {
    if (!obj)
        return;

    if (rendered) {
        auto [iter, inserted] = sorted_frame_list_.insert(obj.get());
        if (!inserted) {
            throw gui::exception("frame_renderer", "Frame was already in this renderer");
        }
    } else {
        auto iter = sorted_frame_list_.find(obj.get());
        if (iter == sorted_frame_list_.end()) {
            throw gui::exception("frame_renderer", "Could not find frame in this renderer");
        }

        sorted_frame_list_.erase(iter);
    }

    for (std::size_t i = 0; i < strata_list_.size(); ++i) {
        strata_list_[i].range = get_strata_range_(static_cast<frame_strata>(i));
    }

    const auto frame_strata = obj->get_effective_frame_strata();
    auto&      strata       = strata_list_[static_cast<std::size_t>(frame_strata)];

    frame_list_updated_ = true;
    notify_strata_needs_redraw_(strata);
}

void frame_renderer::notify_frame_strata_changed(
    const utils::observer_ptr<frame>& /*obj*/,
    frame_strata old_strata_id,
    frame_strata new_strata_id) {

    std::stable_sort(
        sorted_frame_list_.begin(), sorted_frame_list_.end(), sorted_frame_list_.comparator());

    for (std::size_t i = 0; i < strata_list_.size(); ++i) {
        strata_list_[i].range = get_strata_range_(static_cast<frame_strata>(i));
    }

    auto& old_strata = strata_list_[static_cast<std::size_t>(old_strata_id)];
    auto& new_strata = strata_list_[static_cast<std::size_t>(new_strata_id)];

    frame_list_updated_ = true;
    notify_strata_needs_redraw_(old_strata);
    notify_strata_needs_redraw_(new_strata);
}

std::pair<std::size_t, std::size_t>
frame_renderer::get_strata_range_(frame_strata strata_id) const {
    auto range = std::equal_range(
        sorted_frame_list_.begin(), sorted_frame_list_.end(), strata_id, strata_comparator{});

    return {range.first - sorted_frame_list_.begin(), range.second - sorted_frame_list_.begin()};
}

void frame_renderer::notify_frame_level_changed(
    const utils::observer_ptr<frame>& obj, int /*old_level*/, int /*new_level*/) {

    const auto strata_id = obj->get_effective_frame_strata();

    auto& strata_obj = strata_list_[static_cast<std::size_t>(strata_id)];

    auto begin = sorted_frame_list_.begin() + strata_obj.range.first;
    auto last  = sorted_frame_list_.begin() + strata_obj.range.second;

    std::stable_sort(begin, last, sorted_frame_list_.comparator());

    frame_list_updated_ = true;
    notify_strata_needs_redraw_(strata_obj);
}

utils::observer_ptr<const frame>
frame_renderer::find_topmost_frame(const std::function<bool(const frame&)>& predicate) const {
    // Iterate through the frames in reverse order from rendering (frame on top goes first)
    for (const auto* obj : utils::range::reverse(sorted_frame_list_)) {
        if (obj->is_visible()) {
            if (auto topmost = obj->find_topmost_frame(predicate))
                return topmost;
        }
    }

    return nullptr;
}

int frame_renderer::get_highest_level(frame_strata strata_id) const {
    auto range = strata_list_[static_cast<std::size_t>(strata_id)].range;
    auto begin = sorted_frame_list_.begin() + range.first;
    auto last  = sorted_frame_list_.begin() + range.second;

    if (last != begin) {
        --last;
        return (*last)->get_level();
    }

    return 0;
}

void frame_renderer::render_strata_(const strata& strata_obj) const {
    auto begin = sorted_frame_list_.begin() + strata_obj.range.first;
    auto end   = sorted_frame_list_.begin() + strata_obj.range.second;

    for (auto iter = begin; iter != end; ++iter) {
        (*iter)->render();
    }
}

void frame_renderer::clear_strata_list_() {
    sorted_frame_list_.clear();
    frame_list_updated_ = true;
}

bool frame_renderer::has_strata_list_changed_() const {
    return frame_list_updated_;
}

void frame_renderer::reset_strata_list_changed_flag_() {
    frame_list_updated_ = false;
}

} // namespace lxgui::gui
