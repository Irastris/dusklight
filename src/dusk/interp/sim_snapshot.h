#pragma once

#include "dusk/game_clock.h"
#include "dusk/interp/frame_interpolation.h"

#include <cstdint>

#ifdef __cplusplus
namespace dusk::interp {
namespace detail {

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
    if (!should_capture()) {
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

}  // namespace detail

template <typename T>
class Channel {
public:
    void capture(uint64_t& epoch, const T& outgoing) {
        if (detail::roll_sim_snapshot(epoch, m_snap) == detail::SimSnapshotRoll::Capture) {
            m_prev = outgoing;
        }
    }

    template <typename U>
    void capture(uint64_t& epoch, const T& outgoing, Channel<U>& sibling) {
        if (detail::roll_sim_snapshot(epoch, m_snap, sibling.m_snap) ==
            detail::SimSnapshotRoll::Capture)
        {
            m_prev = outgoing;
        }
    }

    const T* previous() const {
        return m_snap.prev_valid ? &m_prev : nullptr;
    }

private:
    template <typename>
    friend class Channel;

    T m_prev{};
    detail::SimSnapshot m_snap;
};

}  // namespace dusk::interp
#endif
