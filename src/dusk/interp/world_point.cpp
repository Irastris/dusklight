#include "dusk/interp/world_point.h"

#include "dusk/game_clock.h"
#include "dusk/interp/dual_buffer.h"
#include "dusk/interp/frame_interpolation.h"
#include "m_Do/m_Do_lib.h"

#include <cstdint>

namespace dusk::interp {
bool project_recorded_pair(Vec const* previous_world, Vec const* current_world, float step, Vec* out_screen);
}

namespace {

struct Record {
    cXyz prev{};
    uint64_t tick = 0;
    uint64_t epoch = ~uint64_t{0};
    bool prev_valid = false;
};

}  // namespace

namespace dusk::interp {

void capture_world_point(const void* key, const cXyz& outgoing) {
    if (key == nullptr || !game_clock::is_sim_frame()) {
        return;
    }

    Record& rec = get<Record>(key);
    const uint64_t epoch = game_clock::g_frameTiming.presentationEpoch;
    const uint64_t seq = sim_tick_seq();
    if (epoch != rec.epoch) {
        rec.prev_valid = false;
        rec.epoch = epoch;
        rec.tick = seq;
        return;
    }

    if (seq != rec.tick) {
        rec.prev = outgoing;
        rec.prev_valid = true;
        rec.tick = seq;
    }
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
    if (rec == nullptr || !rec->prev_valid || !is_enabled() ||
        !project_recorded_pair(&rec->prev, &current, step, out_screen))
    {
        mDoLib_project(const_cast<cXyz*>(&current), out_screen);
    }
    return true;
}

}  // namespace dusk::interp
