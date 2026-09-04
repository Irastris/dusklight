#pragma once

#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/lerp.h"

#include "SSystem/SComponent/c_xyz.h"

#include <dolphin/mtx.h>

#include <cstring>
#include <new>
#include <stdint.h>

#ifdef __cplusplus
namespace dusk::interp {

class WeatherBuffer;

namespace detail {

template <typename T>
class DualBufferCore {
    friend class WeatherBuffer;

public:
    explicit DualBufferCore(T* dst = NULL)
        : m_prev(NULL),
          m_curr(NULL),
          m_capacity(0),
          m_prev_valid(false),
          m_curr_valid(false),
          m_count(0),
          m_dst(dst),
          m_post(NULL),
          m_post_user(NULL),
          m_rolled_seq(0) {}

    void bind(T* dst) { m_dst = dst; }

    void reset() {
        m_prev_valid = false;
        m_curr_valid = false;
        m_count = 0;
        m_rolled_seq = 0;
    }

    bool ready() const { return m_prev_valid && m_curr_valid; }

    void capture_and_schedule(const T* src, int count, void (*post)(void*) = NULL,
                              void* post_user = NULL) {
        roll();
        capture(src, count);
        schedule(post, post_user);
    }

    void writeback(T* src_and_dst, int count, void (*post)(void*) = NULL, void* post_user = NULL) {
        bind(src_and_dst);
        capture_and_schedule(src_and_dst, count, post, post_user);
    }

    void writeback_on_sim_tick(T* src_and_dst, int count, void (*post)(void*) = NULL,
                               void* post_user = NULL) {
        bind(src_and_dst);
        capture_on_sim(src_and_dst, count);
        schedule(post, post_user);
    }

    void capture_on_sim(const T* src, int count) {
        on_sim_tick();
        capture(src, count);
    }

protected:
    void bind_storage(T* prev, T* curr, int capacity) {
        if (m_count > 0 && m_prev != NULL && m_curr != NULL && prev != NULL && curr != NULL &&
            (prev != m_prev || curr != m_curr))
        {
            std::memcpy(prev, m_prev, static_cast<size_t>(m_count) * sizeof(T));
            std::memcpy(curr, m_curr, static_cast<size_t>(m_count) * sizeof(T));
        }
        m_prev = prev;
        m_curr = curr;
        m_capacity = capacity;
    }

private:
    bool fits(int count) const {
        if (count > m_capacity) {
            return false;
        }
        return count > 0;
    }

    void roll() {
        if (!is_enabled() || !m_curr_valid || m_count <= 0) {
            return;
        }
        std::memcpy(m_prev, m_curr, static_cast<size_t>(m_count) * sizeof(T));
        m_prev_valid = true;
    }

    void capture(const T* src, int count) {
        if (!fits(count) || !is_enabled() || src == NULL) {
            return;
        }
        std::memcpy(m_curr, src, static_cast<size_t>(count) * sizeof(T));
        m_count = count;
        m_curr_valid = true;
    }

    void apply(T* dst, int count) const {
        if (!fits(count) || dst == NULL || !ready()) {
            return;
        }
        const f32 step = get_interpolation_step();
        for (int i = 0; i < count; ++i) {
            lerp(dst[i], m_prev[i], m_curr[i], step);
        }
    }

    void schedule(void (*post)(void*) = NULL, void* post_user = NULL) {
        if (!is_enabled() || m_dst == NULL || !fits(m_count)) {
            return;
        }
        m_post = post;
        m_post_user = post_user;
        add_interpolation_callback(&present_trampoline, this);
    }

    static void present_trampoline(void* user) {
        static_cast<DualBufferCore*>(user)->present();
    }

    void present() {
        apply(m_dst, m_count);
        if (m_post != NULL) {
            m_post(m_post_user);
        }
    }

    void on_sim_tick() {
        const uint64_t seq = sim_tick_seq();
        if (seq == m_rolled_seq) {
            return;
        }
        m_rolled_seq = seq;
        roll();
    }

    T* m_prev;
    T* m_curr;
    int m_capacity;
    bool m_prev_valid;
    bool m_curr_valid;
    int m_count;
    T* m_dst;
    void (*m_post)(void*);
    void* m_post_user;
    uint64_t m_rolled_seq;
};

}  // namespace detail

template <typename T, int capacity>
class DualBuffer : public detail::DualBufferCore<T> {
public:
    explicit DualBuffer(T* dst = NULL) : detail::DualBufferCore<T>(dst) {
        this->bind_storage(m_inline_prev, m_inline_curr, capacity);
    }

    DualBuffer(const DualBuffer& other) : detail::DualBufferCore<T>(other) {
        this->bind_storage(m_inline_prev, m_inline_curr, capacity);
    }

    DualBuffer& operator=(const DualBuffer& other) {
        if (this != &other) {
            detail::DualBufferCore<T>::operator=(other);
            this->bind_storage(m_inline_prev, m_inline_curr, capacity);
        }
        return *this;
    }

private:
    T m_inline_prev[capacity];
    T m_inline_curr[capacity];
};

class WeatherBuffer {
public:
    WeatherBuffer()
        : world(NULL), sim(NULL), skip(NULL), store(NULL), count(0), capacity(0) {}

    ~WeatherBuffer() { release(); }

    WeatherBuffer(const WeatherBuffer&) = delete;
    WeatherBuffer& operator=(const WeatherBuffer&) = delete;

    void reset() {
        buf.reset();
        count = 0;
    }

    void release() {
        reset();
        delete[] store;
        delete[] skip;
        store = NULL;
        world = NULL;
        sim = NULL;
        skip = NULL;
        buf.bind_storage(NULL, NULL, 0);
        capacity = 0;
    }

    template <typename Sample>
    void capture(int n, f32 snap_dist, Sample sample) {
        if (!is_enabled() || n <= 0 || n > kMaxCount || !ensure(n)) {
            reset();
            return;
        }

        for (int i = 0; i < n; i++) {
            world[i] = sample(i);
            const bool new_index = i >= count;
            const bool relocated = !new_index && world[i].abs(sim[i]) > snap_dist;
            skip[i] = new_index || relocated;
            sim[i] = world[i];
        }
        count = n;
        buf.writeback_on_sim_tick(world, n, &snap_skipped, this);
    }

    void try_read(int i, cXyz* out) const {
        if (is_enabled() && buf.ready() && i >= 0 && i < count) {
            *out = world[i];
        }
    }

private:
    static const int kMaxCount = 500;

    bool ensure(int n) {
        if (n <= capacity) {
            return store != NULL;
        }

        cXyz* next_store = new (std::nothrow) cXyz[static_cast<size_t>(n) * 4];
        if (next_store == NULL) {
            return false;
        }
        u8* next_skip = new (std::nothrow) u8[static_cast<size_t>(n)];
        if (next_skip == NULL) {
            delete[] next_store;
            return false;
        }

        cXyz* next_sim = next_store + 3 * n;
        if (count > 0 && sim != NULL) {
            std::memcpy(next_sim, sim, static_cast<size_t>(count) * sizeof(cXyz));
        }

        buf.bind_storage(next_store, next_store + n, n);
        delete[] store;
        delete[] skip;
        store = next_store;
        world = next_store + 2 * n;
        sim = next_sim;
        skip = next_skip;
        capacity = n;
        return true;
    }

    static void snap_skipped(void* user) {
        WeatherBuffer* self = static_cast<WeatherBuffer*>(user);
        for (int i = 0; i < self->count; i++) {
            if (self->skip[i]) {
                self->world[i] = self->sim[i];
            }
        }
    }

    detail::DualBufferCore<cXyz> buf;
    cXyz* world;
    cXyz* sim;
    u8* skip;
    cXyz* store;
    int count;
    int capacity;
};

namespace detail {
void* acquire(const void* key, const void* type, void* (*make)(), void (*destroy)(void*));
void* find(const void* key, const void* type);
}

template <typename Record>
const void* record_type_token() {
    static const char token{};
    return &token;
}

template <typename Record>
Record& get(const void* key) {
    return *static_cast<Record*>(detail::acquire(
        key, record_type_token<Record>(), []() -> void* { return new Record; },
        [](void* p) { delete static_cast<Record*>(p); }));
}

template <typename Record>
Record* find(const void* key) {
    return static_cast<Record*>(detail::find(key, record_type_token<Record>()));
}

void erase_owned_buffers(const void* key);
void clear_owned_buffers();
void clear_weather_buffers();

}  // namespace dusk::interp
#endif
