#include "lxgui/gui_scrollframe.hpp"
#include "lxgui/gui_frame.hpp"
#include "lxgui/gui_texture.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_renderer.hpp"
#include "lxgui/gui_rendertarget.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_alive_checker.hpp"
#include "lxgui/gui_region_tpl.hpp"

namespace lxgui {
namespace gui
{
scroll_frame::scroll_frame(utils::control_block& mBlock, manager& mManager) :
    frame(mBlock, mManager)
{
    lType_.push_back(CLASS_NAME);
}

scroll_frame::~scroll_frame()
{
    // Make sure the scroll child is destroyed now.
    // It relies on this scroll_frame still being alive
    // when being destroyed, but the scroll_frame destructor
    // would be called before its inherited frame destructor
    // (which would otherwise take care of destroying the scroll child).
    if (pScrollChild_)
        remove_child(pScrollChild_);
}

bool scroll_frame::can_use_script(const std::string& sScriptName) const
{
    if (frame::can_use_script(sScriptName))
        return true;
    else if ((sScriptName == "OnHorizontalScroll") ||
        (sScriptName == "OnScrollRangeChanged") ||
        (sScriptName == "OnVerticalScroll"))
        return true;
    else
        return false;
}

void scroll_frame::fire_script(const std::string& sScriptName, const event_data& mData)
{
    if (!is_loaded())
        return;

    alive_checker mChecker(*this);
    base::fire_script(sScriptName, mData);
    if (!mChecker.is_alive())
        return;

    if (sScriptName == "OnSizeChanged")
        bRebuildScrollRenderTarget_ = true;
}

void scroll_frame::copy_from(const region& mObj)
{
    base::copy_from(mObj);

    const scroll_frame* pScrollFrame = down_cast<scroll_frame>(&mObj);
    if (!pScrollFrame)
        return;

    this->set_horizontal_scroll(pScrollFrame->get_horizontal_scroll());
    this->set_vertical_scroll(pScrollFrame->get_vertical_scroll());

    if (const frame* pOtherChild = pScrollFrame->get_scroll_child().get())
    {
        region_core_attributes mAttr;
        mAttr.sObjectType = pOtherChild->get_object_type();
        mAttr.sName = pOtherChild->get_raw_name();
        mAttr.lInheritance = {pScrollFrame->get_scroll_child()};

        utils::observer_ptr<frame> pScrollChild = create_child(std::move(mAttr));

        if (pScrollChild)
        {
            pScrollChild->set_special();
            pScrollChild->notify_loaded();
            this->set_scroll_child(remove_child(pScrollChild));
        }
    }
}

void scroll_frame::set_scroll_child(utils::owner_ptr<frame> pFrame)
{
    if (pScrollChild_)
    {
        pScrollChild_->set_renderer(nullptr);
        clear_strata_list_();
    }
    else if (!is_virtual() && !pScrollTexture_)
    {
        // Create the scroll texture
        auto pScrollTexture = create_region<texture>(layer_type::ARTWORK, "$parentScrollTexture");
        if (!pScrollTexture)
            return;

        pScrollTexture->set_special();
        pScrollTexture->set_all_points(observer_from(this));

        if (pScrollRenderTarget_)
            pScrollTexture->set_texture(pScrollRenderTarget_);

        pScrollTexture->notify_loaded();
        pScrollTexture_ = pScrollTexture;

        bRebuildScrollRenderTarget_ = true;
    }

    pScrollChild_ = pFrame;

    if (pScrollChild_)
    {
        add_child(std::move(pFrame));

        pScrollChild_->set_special();
        if (!is_virtual())
            pScrollChild_->set_renderer(observer_from(this));

        pScrollChild_->clear_all_points();
        pScrollChild_->set_point(anchor_data(anchor_point::TOPLEFT, get_name(), -mScroll_));

        update_scroll_range_();
        bUpdateScrollRange_ = false;
    }

    bRedrawScrollRenderTarget_ = true;
}

void scroll_frame::set_horizontal_scroll(float fHorizontalScroll)
{
    if (mScroll_.x != fHorizontalScroll)
    {
        mScroll_.x = fHorizontalScroll;
        lQueuedEventList_.push_back("OnHorizontalScroll");

        pScrollChild_->modify_point(anchor_point::TOPLEFT).mOffset = -mScroll_;
        pScrollChild_->notify_borders_need_update();

        bRedrawScrollRenderTarget_ = true;
    }
}

float scroll_frame::get_horizontal_scroll() const
{
    return mScroll_.x;
}

float scroll_frame::get_horizontal_scroll_range() const
{
    return mScrollRange_.x;
}

void scroll_frame::set_vertical_scroll(float fVerticalScroll)
{
    if (mScroll_.y != fVerticalScroll)
    {
        mScroll_.y = fVerticalScroll;
        lQueuedEventList_.push_back("OnVerticalScroll");

        pScrollChild_->modify_point(anchor_point::TOPLEFT).mOffset = -mScroll_;
        pScrollChild_->notify_borders_need_update();

        bRedrawScrollRenderTarget_ = true;
    }
}

float scroll_frame::get_vertical_scroll() const
{
    return mScroll_.y;
}

float scroll_frame::get_vertical_scroll_range() const
{
    return mScrollRange_.y;
}

void scroll_frame::update(float fDelta)
{
    vector2f mOldChildSize;
    if (pScrollChild_)
        mOldChildSize = pScrollChild_->get_apparent_dimensions();

    alive_checker mChecker(*this);
    base::update(fDelta);
    if (!mChecker.is_alive())
        return;

    if (pScrollChild_ && mOldChildSize != pScrollChild_->get_apparent_dimensions())
    {
        bUpdateScrollRange_ = true;
        bRedrawScrollRenderTarget_ = true;
    }

    if (is_visible())
    {
        if (bRebuildScrollRenderTarget_ && pScrollTexture_)
        {
            rebuild_scroll_render_target_();
            bRebuildScrollRenderTarget_ = false;
            bRedrawScrollRenderTarget_ = true;
        }

        if (bUpdateScrollRange_)
        {
            update_scroll_range_();
            bUpdateScrollRange_ = false;
        }

        if (pScrollChild_ && pScrollRenderTarget_ && bRedrawScrollRenderTarget_)
        {
            render_scroll_strata_list_();
            bRedrawScrollRenderTarget_ = false;
        }
    }
}

void scroll_frame::update_scroll_range_()
{
    const vector2f mApparentSize = get_apparent_dimensions();
    const vector2f mChildApparentSize = pScrollChild_->get_apparent_dimensions();

    mScrollRange_ = mChildApparentSize - mApparentSize;

    if (mScrollRange_.x < 0) mScrollRange_.x = 0;
    if (mScrollRange_.y < 0) mScrollRange_.y = 0;

    if (!is_virtual())
    {
        alive_checker mChecker(*this);
        fire_script("OnScrollRangeChanged");
        if (!mChecker.is_alive())
            return;
    }
}

void scroll_frame::notify_scaling_factor_updated()
{
    base::notify_scaling_factor_updated();

    bRebuildScrollRenderTarget_ = true;
}

void scroll_frame::rebuild_scroll_render_target_()
{
    const vector2f mApparentSize = get_apparent_dimensions();

    if (mApparentSize.x <= 0 || mApparentSize.y <= 0)
        return;

    float fFactor = get_manager().get_interface_scaling_factor();
    vector2ui mScaledSize = vector2ui(
        std::round(mApparentSize.x*fFactor), std::round(mApparentSize.y*fFactor));

    if (pScrollRenderTarget_)
    {
        pScrollRenderTarget_->set_dimensions(mScaledSize);
        pScrollTexture_->set_tex_rect(std::array<float,4>{0.0f, 0.0f, 1.0f, 1.0f});
        bUpdateScrollRange_ = true;
    }
    else
    {
        auto& mRenderer = get_manager().get_renderer();
        pScrollRenderTarget_ = mRenderer.create_render_target(mScaledSize);

        if (pScrollRenderTarget_)
            pScrollTexture_->set_texture(pScrollRenderTarget_);
    }
}

void scroll_frame::render_scroll_strata_list_()
{
    renderer& mRenderer = get_manager().get_renderer();

    mRenderer.begin(pScrollRenderTarget_);

    vector2f mView = vector2f(pScrollRenderTarget_->get_canvas_dimensions())/
        get_manager().get_interface_scaling_factor();

    mRenderer.set_view(matrix4f::translation(-get_borders().top_left()) * matrix4f::view(mView));

    pScrollRenderTarget_->clear(color::EMPTY);

    for (const auto& mStrata : lStrataList_)
    {
        render_strata_(mStrata);
    }

    mRenderer.end();
}

utils::observer_ptr<const frame> scroll_frame::find_topmost_frame(
    const std::function<bool(const frame&)>& mPredicate) const
{
    if (base::find_topmost_frame(mPredicate))
    {
        if (auto pHoveredFrame = frame_renderer::find_topmost_frame(mPredicate))
            return pHoveredFrame;

        return observer_from(this);
    }

    return nullptr;
}

void scroll_frame::notify_strata_needs_redraw(frame_strata mStrata)
{
    frame_renderer::notify_strata_needs_redraw(mStrata);

    bRedrawScrollRenderTarget_ = true;
    notify_renderer_need_redraw();
}

void scroll_frame::create_glue()
{
    create_glue_(this);
}

void scroll_frame::notify_rendered_frame(const utils::observer_ptr<frame>& pFrame, bool bRendered)
{
    if (!pFrame)
        return;

    frame_renderer::notify_rendered_frame(pFrame, bRendered);

    bRedrawScrollRenderTarget_ = true;
}

vector2f scroll_frame::get_target_dimensions() const
{
    return get_apparent_dimensions();
}

}
}
