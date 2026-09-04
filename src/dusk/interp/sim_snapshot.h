#pragma once

#include "dusk/game_clock.h"
#include "dusk/interp/frame_interpolation.h"

#include <cstdint>

#ifdef __cplusplus
namespace dusk::interp {

struct SimSnapshot {
    uint64_t tick = 0;
    bool prev_valid = false;
};

enum class SimSnapshotRoll {
    Skip,
    Invalidate,
    Capture,
};

inline SimSnapshotRoll roll_sim_snapshot(uint64_t& epoch, SimSnapshot& channel) {
    if (!game_clock::is_sim_frame()) {
        return SimSnapshotRoll::Skip;
    }

    const uint64_t current_epoch = game_clock::g_frameTiming.presentationEpoch;
    const uint64_t seq = sim_tick_seq();
    if (current_epoch != epoch) {
        channel.prev_valid = false;
        epoch = current_epoch;
        channel.tick = seq;
        return SimSnapshotRoll::Invalidate;
    }

    if (seq == channel.tick) {
        return SimSnapshotRoll::Skip;
    }

    channel.prev_valid = true;
    channel.tick = seq;
    return SimSnapshotRoll::Capture;
}

inline SimSnapshotRoll roll_sim_snapshot(uint64_t& epoch, SimSnapshot& channel, SimSnapshot& sibling) {
    const SimSnapshotRoll roll = roll_sim_snapshot(epoch, channel);
    if (roll == SimSnapshotRoll::Invalidate) {
        sibling.prev_valid = false;
        sibling.tick = channel.tick;
    }
    return roll;
}

}  // namespace dusk::interp
#endif
