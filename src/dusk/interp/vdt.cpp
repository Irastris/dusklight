#include "dusk/interp/vdt.h"

#include "dusk/game_clock.h"

#include "JSystem/J2DGraph/J2DAnimation.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"

#include <cmath>

namespace dusk::vdt {
namespace {

void (*s_advances[kAdvanceCapacity])(){};
int s_advanceCount = 0;

}  // namespace

void register_advance(void (*advance)()) {
    if (advance == nullptr) {
        return;
    }
    for (int i = 0; i < s_advanceCount; ++i) {
        if (s_advances[i] == advance) {
            return;
        }
    }
    if (s_advanceCount >= kAdvanceCapacity) {
        return;
    }
    s_advances[s_advanceCount++] = advance;
}

void advance_all() {
    for (int i = 0; i < s_advanceCount; ++i) {
        s_advances[i]();
    }
}

void present_looping(float& frame, J2DAnmBase* anm, float speed) {
    if (anm == nullptr) {
        return;
    }
    advance_looping_frame(frame, speed, anm->getFrameMax());
    anm->setFrame(frame);
}

void present_toward(float& frame, float target, J2DAnmTransform* anm, J2DPane* pane) {
    if (anm == nullptr || frame == target) {
        return;
    }
    if (pane != nullptr && pane->mTransform != anm) {
        return;
    }
    advance_toward_frame(frame, target, 2.0f);
    anm->setFrame(frame);
    if (pane != nullptr) {
        pane->animationTransform();
    }
}

void present_addCalc(float* value, float target, float scale, float maxStep, float minStep) {
    if (*value == target) {
        return;
    }
    cLib_addCalc(value, target, scale, maxStep * game_clock::original_frames(),
                 minStep * game_clock::original_frames());
}

void present_addCalc2(float* value, float target, float scale, float maxStep, float snap) {
    if (*value == target) {
        return;
    }
    cLib_addCalc2(value, target, scale, maxStep * game_clock::original_frames());
    if (fabsf(*value - target) < snap) {
        *value = target;
    }
}

float present_sine_ease(float i_max, float i_value) {
    if (i_max <= 0.0f) {
        return 1.0f;
    }
    if (i_value < 0.0f) {
        i_value = 0.0f;
    } else if (i_value > i_max) {
        i_value = i_max;
    }
    float v = i_value / i_max;
    v = cM_ssin((int)(0.5f * (32768.0f * v)));
    return v * v;
}

bool present_chase(float* value, float target, float scale, float maxStep, float snap) {
    if (*value != target) {
        present_addCalc2(value, target, scale, maxStep, snap);
        return true;
    }
    return false;
}

}  // namespace dusk::vdt
