#include "lxgui/gui_slider.hpp"
#include "lxgui/gui_frame.hpp"
#include "lxgui/gui_manager.hpp"
#include "lxgui/gui_texture.hpp"
#include "lxgui/gui_event.hpp"
#include "lxgui/gui_out.hpp"
#include "lxgui/gui_alive_checker.hpp"
#include "lxgui/gui_uiobject_tpl.hpp"

#include <sstream>
#include <algorithm>

namespace lxgui {
namespace gui
{
void step_value(float& fValue, float fStep)
{
    // Makes the value a multiple of the step :
    // fValue = N*fStep, where N is an integer.
    if (fStep != 0.0f)
        fValue = std::round(fValue/fStep)*fStep;
}

slider::slider(manager& mManager) : frame(mManager)
{
    lType_.push_back(CLASS_NAME);
}

std::string slider::serialize(const std::string& sTab) const
{
    std::ostringstream sStr;

    sStr << frame::serialize(sTab);
    sStr << sTab << "  # Orientation: ";
    switch (mOrientation_)
    {
        case orientation::HORIZONTAL : sStr << "HORIZONTAL"; break;
        case orientation::VERTICAL   : sStr << "VERTICAL";   break;
    }
    sStr << "\n";
    sStr << sTab << "  # Value      : " << fValue_ << "\n";
    sStr << sTab << "  # Min value  : " << fMinValue_ << "\n";
    sStr << sTab << "  # Max value  : " << fMaxValue_ << "\n";
    sStr << sTab << "  # Step       : " << fValueStep_ << "\n";
    sStr << sTab << "  # Click out  : " << bAllowClicksOutsideThumb_ << "\n";

    return sStr.str();
}

bool slider::can_use_script(const std::string& sScriptName) const
{
    if (frame::can_use_script(sScriptName))
        return true;
    else if (sScriptName == "OnValueChanged")
        return true;
    else
        return false;
}

void slider::on_script(const std::string& sScriptName, const event_data& mData)
{
    if (sScriptName == "OnLoad")
        enable_mouse(true);

    alive_checker mChecker(*this);
    frame::on_script(sScriptName, mData);
    if (!mChecker.is_alive())
        return;
}

void slider::copy_from(const uiobject& mObj)
{
    frame::copy_from(mObj);

    const slider* pSlider = down_cast<slider>(&mObj);
    if (!pSlider)
        return;

    this->set_value_step(pSlider->get_value_step());
    this->set_min_value(pSlider->get_min_value());
    this->set_max_value(pSlider->get_max_value());
    this->set_value(pSlider->get_value());
    this->set_thumb_draw_layer(pSlider->get_thumb_draw_layer());
    this->set_orientation(pSlider->get_orientation());
    this->set_allow_clicks_outside_thumb(pSlider->are_clicks_outside_thumb_allowed());

    if (const texture* pThumb = pSlider->get_thumb_texture().get())
    {
        auto pTexture = this->create_region<texture>(
            pThumb->get_draw_layer(), pThumb->get_name(),
            {pSlider->get_thumb_texture()});

        if (pTexture)
        {
            pTexture->set_special();
            pTexture->notify_loaded();
            this->set_thumb_texture(pTexture);
        }
    }
}

void slider::constrain_thumb_()
{
    if (fMaxValue_ == fMinValue_)
        return;

    const vector2f mApparentSize = get_apparent_dimensions();

    if ((mOrientation_ == orientation::HORIZONTAL && mApparentSize.x  > 0) ||
        (mOrientation_ == orientation::VERTICAL   && mApparentSize.y > 0))
    {
        float fValue = 0.0f;

        if (bThumbMoved_)
        {
            if (mOrientation_ == orientation::HORIZONTAL)
                fValue = pThumbTexture_->get_point(anchor_point::CENTER).mOffset.x/mApparentSize.x;
            else
                fValue = pThumbTexture_->get_point(anchor_point::CENTER).mOffset.y/mApparentSize.y;

            fValue = fValue * (fMaxValue_ - fMinValue_) + fMinValue_;
            fValue = std::clamp(fValue, fMinValue_, fMaxValue_);
            step_value(fValue, fValueStep_);
        }

        float fCoef = (fValue - fMinValue_)/(fMaxValue_ - fMinValue_);

        anchor& mAnchor = pThumbTexture_->modify_point(anchor_point::CENTER);
        if (mOrientation_ == orientation::HORIZONTAL)
            mAnchor.mOffset = vector2f(mApparentSize.x*fCoef, 0);
        else
            mAnchor.mOffset = vector2f(0, mApparentSize.y*fCoef);
    }
}

void slider::on_event(const event& mEvent)
{
    alive_checker mChecker(*this);

    frame::on_event(mEvent);
    if (!mChecker.is_alive())
        return;

    if (bIsMouseEnabled_ && get_manager().is_input_enabled())
    {
        if (mEvent.get_name() == "MOUSE_PRESSED")
        {
            update_mouse_in_frame_();

            anchor& mAnchor = pThumbTexture_->modify_point(anchor_point::CENTER);

            if (bMouseInThumb_)
            {
                get_manager().start_moving(
                    pThumbTexture_, &mAnchor,
                    mOrientation_ == orientation::HORIZONTAL ? constraint::X : constraint::Y,
                    std::bind(&slider::constrain_thumb_, this)
                );
                bThumbMoved_ = true;
            }
            else if (bMouseInFrame_ && bAllowClicksOutsideThumb_)
            {
                const vector2f mApparentSize = get_apparent_dimensions();

                float fValue;
                if (mOrientation_ == orientation::HORIZONTAL)
                {
                    float fOffset = mMousePos_.x - lBorderList_.left;
                    fValue = fOffset/mApparentSize.x;
                    set_value(fValue*(fMaxValue_ - fMinValue_) + fMinValue_);
                }
                else
                {
                    float fOffset = mMousePos_.y - lBorderList_.top;
                    fValue = fOffset/mApparentSize.y;
                    set_value(fValue*(fMaxValue_ - fMinValue_) + fMinValue_);
                }

                if (pThumbTexture_)
                {
                    float fCoef = (fValue_ - fMinValue_)/(fMaxValue_ - fMinValue_);

                    if (mOrientation_ == orientation::HORIZONTAL)
                        mAnchor.mOffset = vector2f(mApparentSize.x*fCoef, 0);
                    else
                        mAnchor.mOffset = vector2f(0, mApparentSize.y*fCoef);

                    get_manager().start_moving(
                        pThumbTexture_, &mAnchor,
                        mOrientation_ == orientation::HORIZONTAL ? constraint::X : constraint::Y
                    );
                    bThumbMoved_ = true;
                }
            }
        }
        else if (mEvent.get_name() == "MOUSE_RELEASED")
        {
            if (pThumbTexture_)
            {
                get_manager().stop_moving(*pThumbTexture_);
                bThumbMoved_ = false;
            }
        }
    }
}

void slider::set_min_value(float fMin)
{
    if (fMin != fMinValue_)
    {
        fMinValue_ = fMin;
        if (fMinValue_ > fMaxValue_) fMinValue_ = fMaxValue_;
        else step_value(fMinValue_, fValueStep_);

        if (fValue_ < fMinValue_)
        {
            fValue_ = fMinValue_;
            on_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_max_value(float fMax)
{
    if (fMax != fMaxValue_)
    {
        fMaxValue_ = fMax;
        if (fMaxValue_ < fMinValue_) fMaxValue_ = fMinValue_;
        else step_value(fMaxValue_, fValueStep_);

        if (fValue_ > fMaxValue_)
        {
            fValue_ = fMaxValue_;
            on_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_min_max_values(float fMin, float fMax)
{
    if (fMin != fMinValue_ || fMax != fMaxValue_)
    {
        fMinValue_ = std::min(fMin, fMax);
        fMaxValue_ = std::max(fMin, fMax);
        step_value(fMinValue_, fValueStep_);
        step_value(fMaxValue_, fValueStep_);

        if (fValue_ > fMaxValue_ || fValue_ < fMinValue_)
        {
            fValue_ = std::clamp(fValue_, fMinValue_, fMaxValue_);
            on_script("OnValueChanged");
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_value(float fValue, bool bSilent)
{
    fValue = std::clamp(fValue, fMinValue_, fMaxValue_);
    step_value(fValue, fValueStep_);

    if (fValue != fValue_)
    {
        fValue_ = fValue;

        if (!bSilent)
            on_script("OnValueChanged");

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_value_step(float fValueStep)
{
    if (fValueStep_ != fValueStep)
    {
        fValueStep_ = fValueStep;

        step_value(fMinValue_, fValueStep_);
        step_value(fMaxValue_, fValueStep_);

        float fOldValue = fValue_;
        fValue_ = std::clamp(fValue_, fMinValue_, fMaxValue_);
        step_value(fValue_, fValueStep_);

        if (fValue_ != fOldValue)
            on_script("OnValueChanged");

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_thumb_texture(utils::observer_ptr<texture> pTexture)
{
    pThumbTexture_ = std::move(pTexture);
    if (!pThumbTexture_)
        return;

    pThumbTexture_->set_draw_layer(mThumbLayer_);
    pThumbTexture_->clear_all_points();
    pThumbTexture_->set_point(anchor_data(
        anchor_point::CENTER, pThumbTexture_->get_parent().get() == this ? "$parent" : sName_,
        mOrientation_ == orientation::HORIZONTAL ? anchor_point::LEFT : anchor_point::TOP
    ));

    notify_thumb_texture_needs_update_();
}

void slider::set_orientation(orientation mOrientation)
{
    if (mOrientation != mOrientation_)
    {
        mOrientation_ = mOrientation;
        if (pThumbTexture_)
        {
            pThumbTexture_->set_point(anchor_data(
                anchor_point::CENTER, sName_,
                mOrientation_ == orientation::HORIZONTAL ? anchor_point::LEFT : anchor_point::TOP
            ));
        }

        notify_thumb_texture_needs_update_();
    }
}

void slider::set_orientation(const std::string& sOrientation)
{
    orientation mOrientation = orientation::HORIZONTAL;
    if (sOrientation == "VERTICAL")
        mOrientation = orientation::VERTICAL;
    else if (sOrientation == "HORIZONTAL")
        mOrientation = orientation::HORIZONTAL;
    else
    {
        gui::out << gui::warning << "gui::" << lType_.back() << " : "
            "Unknown orientation : \""+sOrientation+"\". Using \"HORIZONTAL\"." << std::endl;
    }

    set_orientation(mOrientation);
}

void slider::set_thumb_draw_layer(layer_type mThumbLayer)
{
    mThumbLayer_ = mThumbLayer;
    if (pThumbTexture_)
        pThumbTexture_->set_draw_layer(mThumbLayer_);
}

void slider::set_thumb_draw_layer(const std::string& sThumbLayer)
{
    if (sThumbLayer == "ARTWORK")
        mThumbLayer_ = layer_type::ARTWORK;
    else if (sThumbLayer == "BACKGROUND")
        mThumbLayer_ = layer_type::BACKGROUND;
    else if (sThumbLayer == "BORDER")
        mThumbLayer_ = layer_type::BORDER;
    else if (sThumbLayer == "HIGHLIGHT")
        mThumbLayer_ = layer_type::HIGHLIGHT;
    else if (sThumbLayer == "OVERLAY")
        mThumbLayer_ = layer_type::OVERLAY;
    else
    {
        gui::out << gui::warning << "gui::" << lType_.back() << " : "
            "Unknown layer type : \""+sThumbLayer+"\". Using \"OVERLAY\"." << std::endl;
        mThumbLayer_ = layer_type::OVERLAY;
    }

    if (pThumbTexture_)
        pThumbTexture_->set_draw_layer(mThumbLayer_);
}

float slider::get_min_value() const
{
    return fMinValue_;
}

float slider::get_max_value() const
{
    return fMaxValue_;
}

float slider::get_value() const
{
    return fValue_;
}

float slider::get_value_step() const
{
    return fValueStep_;
}

slider::orientation slider::get_orientation() const
{
    return mOrientation_;
}

layer_type slider::get_thumb_draw_layer() const
{
    return mThumbLayer_;
}

void slider::set_allow_clicks_outside_thumb(bool bAllow)
{
    bAllowClicksOutsideThumb_ = bAllow;
}

bool slider::are_clicks_outside_thumb_allowed() const
{
    return bAllowClicksOutsideThumb_;
}

bool slider::is_in_frame(const vector2f& mPosition) const
{
    if (bAllowClicksOutsideThumb_)
    {
        if (pThumbTexture_)
            return frame::is_in_frame(mPosition) || pThumbTexture_->is_in_region(mPosition);
        else
            return frame::is_in_frame(mPosition);
    }
    else
    {
        if (pThumbTexture_)
            return pThumbTexture_->is_in_region(mPosition);
        else
            return false;
    }
}

void slider::notify_mouse_in_frame(bool bMouseInFrame, const vector2f& mMousePos)
{
    if (bAllowClicksOutsideThumb_)
        frame::notify_mouse_in_frame(bMouseInFrame, mMousePos);

    bMouseInThumb_ = (bMouseInFrame && pThumbTexture_ && pThumbTexture_->is_in_region(mMousePos));
}

void slider::update(float fDelta)
{
    alive_checker mChecker(*this);
    frame::update(fDelta);
    if (!mChecker.is_alive())
        return;

    if ((bUpdateThumbTexture_ || bThumbMoved_) && pThumbTexture_)
    {
        const vector2f mApparentSize = get_apparent_dimensions();

        if ((mOrientation_ == orientation::HORIZONTAL && mApparentSize.x > 0) ||
            (mOrientation_ == orientation::VERTICAL   && mApparentSize.y > 0))
        {
            if (fMaxValue_ == fMinValue_)
            {
                pThumbTexture_->hide();
                return;
            }
            else
                pThumbTexture_->show();

            float fOldValue = fValue_;

            if (bThumbMoved_)
            {
                if (mOrientation_ == orientation::HORIZONTAL)
                    fValue_ = pThumbTexture_->get_point(anchor_point::CENTER).mOffset.x/mApparentSize.x;
                else
                    fValue_ = pThumbTexture_->get_point(anchor_point::CENTER).mOffset.y/mApparentSize.y;

                fValue_ *= (fMaxValue_ - fMinValue_);
                fValue_ += fMinValue_;
                fValue_ = std::clamp(fValue_, fMinValue_, fMaxValue_);
                step_value(fValue_, fValueStep_);

                if (fValue_ != fOldValue)
                    on_script("OnValueChanged");
            }

            float fCoef = (fValue_ - fMinValue_)/(fMaxValue_ - fMinValue_);

            anchor& mAnchor = pThumbTexture_->modify_point(anchor_point::CENTER);
            if (mOrientation_ == orientation::HORIZONTAL)
                mAnchor.mOffset = vector2f(mApparentSize.x*fCoef, 0);
            else
                mAnchor.mOffset = vector2f(0, mApparentSize.y*fCoef);

            pThumbTexture_->notify_borders_need_update();
            pThumbTexture_->update(fDelta);

            bUpdateThumbTexture_ = false;
        }
    }
}

void slider::notify_borders_need_update() const
{
    frame::notify_borders_need_update();
    notify_thumb_texture_needs_update_();
}

void slider::create_glue()
{
    create_glue_(this);
}

void slider::notify_thumb_texture_needs_update_() const
{
    bUpdateThumbTexture_ = true;
}
}
}