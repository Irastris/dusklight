#include "dusk/interp/anim.h"

#include "JSystem/J3DGraphAnimator/J3DAnimation.h"

#include <algorithm>
#include <cmath>

namespace dusk::interp::anim {
namespace {

bool is_looping(unsigned attribute) {
    return attribute == J3DFrameCtrl::EMode_LOOP ||
           attribute == J3DFrameCtrl::EMode_LOOP_REVERSE;
}

float normalize_loop(float frame, float loop, float end) {
    const float span = end - loop;
    if (span <= 0.0f) {
        return frame;
    }
    frame = loop + std::fmod(frame - loop, span);
    if (frame < loop) {
        frame += span;
    }
    return frame;
}

}  // namespace

bool try_present(float previous_frame, const State& current, float step, float* out) {
    const bool looping = is_looping(current.attribute);
    const float span = current.end - current.loop;

    float delta = current.frame - previous_frame;
    if (looping && span > 0.0f) {
        if (current.rate >= 0.0f && delta < -span * 0.5f) {
            delta += span;
        } else if (current.rate < 0.0f && delta > span * 0.5f) {
            delta -= span;
        }
    } else if (!looping) {
        const float cap = std::max(4.0f, std::fabs(current.rate) * 2.0f);
        if (std::fabs(delta) > cap) {
            return false;
        }
    }

    float frame = previous_frame + delta * step;
    if (looping) {
        frame = normalize_loop(frame, current.loop, current.end);
    }
    if (!std::isfinite(frame)) {
        return false;
    }
    *out = frame;
    return true;
}

}  // namespace dusk::interp::anim
