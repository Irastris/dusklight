#include "dusk/interp/vdt.h"

#include "dusk/game_clock.h"

#include "JSystem/J2DGraph/J2DAnimation.h"
#include "SSystem/SComponent/c_lib.h"

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

bool present_chase(float* value, float target, float scale, float maxStep, float snap) {
    if (*value != target) {
        present_addCalc2(value, target, scale, maxStep, snap);
        return true;
    }
    return false;
}

}  // namespace dusk::vdt
