#pragma once

#include <stddef.h>
#include <sys/time.h>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <ratio>
#include <sstream>
#include <string>
#include <thread>
#include <compiling.h>

#define __read_sysreg(r, w, c, t) ({             \
         t __val;                \
         asm volatile(r " " c : "=r" (__val));   \
         __val;                  \
     })
#define read_sysreg(...)                 __read_sysreg(__VA_ARGS__)

__attribute__((__always_inline__))
inline uint64_t __rdtsc(void) {
#if IS_INTEL
	return __builtin_ia32_rdtsc();
#else
    uint64_t val = 0;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
#endif
}

__attribute__((__always_inline__))
inline uint64_t __rdtsc_hz(void) {
#if IS_INTEL
    return 3500000000ul;
#else
    uint64_t val = 0;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
#endif
}

struct TimeItTicks {
    const std::string what;
    uint64_t start = 0, end = 0;
#if IS_INTEL
    // it is estimated to be 3.5 HZ host
    static inline const uint64_t mult = 10;
    static inline const uint64_t div = 35;
#else
    // it is hardcode to known __rdtsc_hz() value 24000000 on M1
    static inline const uint64_t mult = 1000;
    static inline const uint64_t div = 24;
#endif
    TimeItTicks(const std::string &w) :
            what(w) {
        start = __rdtsc();
#if !defined(__x86_64)
        assert(__rdtsc_hz() == 24000000);
#endif
    }
    uint64_t report() {
        end = __rdtsc();
        std::cout << "TimeItTicks[" << what << "]=" << (end - start) << ", aprox " << (end - start) * mult / div << " ns"
                << std::endl;
        return (end - start);
    }
    ~TimeItTicks() {
        if (!end)
            report();
    }
};

inline
uint64_t GetTimerCountInNS(void) {
    struct timespec currenttime;
    clock_gettime(CLOCK_REALTIME, &currenttime);
    return UINT64_C(1000000000) * currenttime.tv_sec + currenttime.tv_nsec;
}

struct RenderDuration {
    std::chrono::nanoseconds raw;
#if IS_CPP20
    template<typename T>
    void render(std::ostream &out) const {
        std::ostringstream tmp;
        using duration_t = std::chrono::duration<double, T>;
        auto duration = std::chrono::duration_cast<duration_t>(raw);
        tmp << std::fixed << std::setprecision(3) << duration;
        out << tmp.str();
    }
    friend std::ostream& operator <<(std::ostream &out, const RenderDuration &v) {
        if (v.raw.count() > 1'000'000'000)
            v.render<std::ratio<1, 1>>(out);
        else if (v.raw.count() > 1'000'000)
            v.render<std::milli>(out);
        else if (v.raw.count() > 1'000)
            v.render<std::micro>(out);
        else
            v.render<std::nano>(out);
        return out;
    }
#else
    friend std::ostream& operator <<(std::ostream &out, const RenderDuration &v) {
    	return out << v.raw.count() << "ns";
    }
#endif
};

struct TimeItNs {
    const std::string what;
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;
    using duration_t = clock::duration;
    time_point start_, end_;
    TimeItNs(const std::string &w) :
            what(w) {
        start();
    }
    void start() {
        start_ = clock::now();
    }
    void stop() {
        end_ = clock::now();
    }
    duration_t duration() const {
        return end_ - start_;
    }
    void report() {
        std::cout << "TimeItNS[" << what << "]=" << RenderDuration { duration() } << std::endl;
    }
    ~TimeItNs() {
        if (end_ == time_point { }) {
            stop();
            report();
        }
    }
};

template<size_t loops>
struct TimeItNs_Repeat: TimeItNs {
    using TimeItNs::TimeItNs;
    template<typename OP, typename R>
    void run(OP &&F, R &&reset) {
        std::array<TimeItNs::duration_t, loops> attempts;
        for (size_t i = 0; i < loops; ++i) {
            reset();
            start();
            F();
            stop();
            attempts[i] = duration();
            std::this_thread::yield();
        }
        sort(attempts.begin(), attempts.end());
        std::cout << "TimeItNS[" << what << "]={min:" << RenderDuration { attempts.front() } //
                << ",mid:" << RenderDuration { *(attempts.begin() + loops / 2) } //
                << ",max:" << RenderDuration { attempts.back() } //
                << ",cnt:" << loops << '}' << std::endl;
    }
};
