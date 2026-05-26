#include "number_button.hpp"

#include "Z2AudioLib/Z2SeMgr.h"
#include "m_Do/m_Do_audio.h"

#include <charconv>
#include <fmt/format.h>

namespace dusk::ui {

NumberButton::NumberButton(Rml::Element* parent, Props props)
    : BaseStringButton(parent, {.key = std::move(props.key), .type = "number"}),
      mGetValue(std::move(props.getValue)), mSetValue(std::move(props.setValue)),
      mIsDisabled(std::move(props.isDisabled)), mIsModified(std::move(props.isModified)),
      mMin(props.min), mMax(props.max), mStep(props.step), mPrefix(std::move(props.prefix)),
      mSuffix(std::move(props.suffix)) {
    Component::listen(mRoot, Rml::EventId::Keydown, [this](Rml::Event& event) {
        if (disabled()) {
            return;
        }
        if (map_nav_event(event) != NavCommand::Confirm) {
            return;
        }
        if (!is_editing()) {
            start_editing();
        } else {
            request_stop_editing(true, true);
        }
        event.StopImmediatePropagation();
    });

    Component::listen(mRoot, Rml::EventId::Click, [this](Rml::Event& event) {
        if (disabled() || is_editing() || !contains(event.GetTargetElement())) {
            return;
        }
        set_scrollable(!mScrollable);
        event.StopImmediatePropagation();
    });

    Component::listen(mRoot, Rml::EventId::Dblclick, [this](Rml::Event& event) {
        if (disabled() || is_editing() || !contains(event.GetTargetElement())) {
            return;
        }
        start_editing();
        event.StopImmediatePropagation();
    });

    Component::listen(mRoot, Rml::EventId::Blur, [this](Rml::Event& event) {
        if (contains(event.GetTargetElement())) {
            set_scrollable(false);
        }
    });

    Component::listen(mRoot, Rml::EventId::Mouseout, [this](Rml::Event& event) {
        if (event.GetTargetElement() == mRoot) {
            set_scrollable(false);
        }
    });

    Component::listen(mRoot, Rml::EventId::Mousescroll, [this](Rml::Event& event) {
        if (disabled() || is_editing() || !mScrollable || !contains(event.GetTargetElement())) {
            return;
        }

        const float wheelDeltaY = event.GetParameter("wheel_delta_y", 0.0f);
        const float absWheelDeltaY = wheelDeltaY < 0.0f ? -wheelDeltaY : wheelDeltaY;
        if (absWheelDeltaY == 0.0f) {
            return;
        }

        const int newValue = std::clamp(mGetValue() + (wheelDeltaY < 0.0f ? 1 : -1) * mStep, mMin, mMax);
        if (newValue != mGetValue()) {
            mSetValue(newValue);
            mDoAud_seStartMenu(kSoundItemChange);
        }

        event.StopPropagation();
    });
}

bool NumberButton::modified() const {
    if (mIsModified) {
        return mIsModified();
    }
    return BaseStringButton::modified();
}

bool NumberButton::disabled() const {
    if (mIsDisabled) {
        return mIsDisabled();
    }
    return BaseStringButton::disabled();
}

Rml::String NumberButton::format_value() {
    return fmt::format("{}{}{}", mPrefix, mGetValue(), mSuffix);
}

Rml::String NumberButton::input_value() {
    return fmt::to_string(mGetValue());
}

void NumberButton::set_value(Rml::String value) {
    if (!mSetValue) {
        return;
    }

    int parsedValue = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsedValue);
    if (result.ec != std::errc() || result.ptr != end) {
        return;
    }

    mSetValue(std::clamp(parsedValue, mMin, mMax));
}

bool NumberButton::handle_nav_command(NavCommand cmd) {
    if (cmd == NavCommand::Confirm) {
        return false;
    }

    if (!is_editing() && (cmd == NavCommand::Left || cmd == NavCommand::Right)) {
        const int newValue = std::clamp(
            mGetValue() + (cmd == NavCommand::Right ? mStep : -mStep), mMin, mMax);
        if (newValue != mGetValue()) {
            mSetValue(newValue);
            mDoAud_seStartMenu(kSoundItemChange);
        }
        return true;
    }
    return BaseStringButton::handle_nav_command(cmd);
}

void NumberButton::set_scrollable(bool value) {
    mScrollable = value;
    mRoot->SetClass("scrollable", value);
}

void NumberButton::on_editing_started() {
    set_scrollable(false);
}
}  // namespace dusk::ui
