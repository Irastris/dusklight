#include "dusk/interp/world_point.h"

#include "dusk/interp/dual_buffer.h"
#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/sim_snapshot.h"
#include "m_Do/m_Do_lib.h"

#include <cstdint>

namespace {

struct Record {
    uint64_t epoch = ~uint64_t{0};
    dusk::interp::Channel<cXyz> pos;
};

}  // namespace

namespace dusk::interp {

void capture_world_point(const void* key, const cXyz& outgoing) {
    if (key == nullptr || !should_capture()) {
        return;
    }

    Record& rec = get<Record>(key);
    rec.pos.capture(rec.epoch, outgoing);
}

void invalidate_world_point(const void* key) {
    if (Record* rec = find<Record>(key)) {
        *rec = {};
    }
}

void erase_world_point(const void* key) {
    erase_owned_buffers(key);
}

bool present_world_point(const void* key, const cXyz& current, Vec* out_screen) {
    if (out_screen == nullptr) {
        return false;
    }

    const Record* rec = find<Record>(key);
    const float step = get_interpolation_step();
    if (rec == nullptr || rec->pos.previous() == nullptr || !is_enabled() ||
        !project_recorded_pair(rec->pos.previous(), &current, step, out_screen))
    {
        mDoLib_project(const_cast<cXyz*>(&current), out_screen);
    }
    return true;
}

}  // namespace dusk::interp
