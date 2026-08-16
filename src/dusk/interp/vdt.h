#pragma once

#include "dusk/game_clock.h"

#include "dolphin/types.h"

namespace dusk::vdt {

constexpr int kCapacity = 32;
constexpr int kRateCapacity = 64;
constexpr int kAdvanceCapacity = 32;

inline f32 clamped_frac(f32 timer, s16 duration) {
    if (duration <= 0) {
        return 1.0f;
    }
    f32 frac = timer / (f32)duration;
    if (frac < 0.0f) {
        return 0.0f;
    }
    if (frac > 1.0f) {
        return 1.0f;
    }
    return frac;
}

inline bool step_rate_timer(f32& timer, s16 duration, bool fade_in) {
    if (fade_in) {
        if (timer < duration) {
            timer += game_clock::original_frames();
            if (timer > duration) {
                timer = duration;
            }
        }
    } else if (timer > 0.0f) {
        timer -= game_clock::original_frames();
        if (timer < 0.0f) {
            timer = 0.0f;
        }
    }
    return fade_in ? timer >= duration : timer <= 0.0f;
}

template <typename Owner, typename Payload>
struct Entry {
    Owner* owner = nullptr;
    s16 duration = 0;
    Payload payload{};
};

template <typename Owner, typename Payload, int Capacity = kCapacity>
class Table {
public:
    Entry<Owner, Payload>* find(Owner* owner) {
        for (int i = 0; i < Capacity; ++i) {
            if (m_slots[i].owner == owner) {
                return &m_slots[i];
            }
        }
        return nullptr;
    }

    Entry<Owner, Payload>* acquire(Owner* owner) {
        Entry<Owner, Payload>* entry = find(owner);
        if (entry != nullptr) {
            return entry;
        }
        for (int i = 0; i < Capacity; ++i) {
            if (m_slots[i].owner == nullptr) {
                m_slots[i].owner = owner;
                return &m_slots[i];
            }
        }
        return nullptr;
    }

    void unlink(Owner* owner) {
        Entry<Owner, Payload>* entry = find(owner);
        if (entry == nullptr) {
            return;
        }
        *entry = Entry<Owner, Payload>{};
    }

    bool request(Owner* owner, s16 duration, const Payload& payload) {
        Entry<Owner, Payload>* entry = acquire(owner);
        if (entry == nullptr) {
            return false;
        }
        entry->duration = duration;
        entry->payload = payload;
        return true;
    }

    template <typename Fn>
    void each(Fn fn) {
        for (int i = 0; i < Capacity; ++i) {
            Entry<Owner, Payload>& entry = m_slots[i];
            if (entry.owner == nullptr) {
                continue;
            }
            fn(entry);
        }
    }

    template <typename Sample, typename Apply>
    void drain(Sample sample, Apply apply) {
        for (int i = 0; i < Capacity; ++i) {
            Entry<Owner, Payload>& entry = m_slots[i];
            if (entry.owner == nullptr) {
                continue;
            }

            Owner* owner = entry.owner;
            f32& timer = sample(owner);
            if (timer < entry.duration) {
                timer += game_clock::original_frames();
                if (timer > entry.duration) {
                    timer = entry.duration;
                }
            }

            f32 rate = owner->rateCalc(entry.duration, timer, entry.payload.calc);
            apply(owner, entry.payload, rate);
            if (timer >= entry.duration) {
                entry = Entry<Owner, Payload>{};
            }
        }
    }

private:
    Entry<Owner, Payload> m_slots[Capacity]{};
};

void register_advance(void (*advance)());
void advance_all();

}  // namespace dusk::vdt
