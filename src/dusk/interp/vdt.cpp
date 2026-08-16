#include "dusk/interp/vdt.h"

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

}  // namespace dusk::vdt
