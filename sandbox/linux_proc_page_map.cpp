#include <compiling.h>
#include <resources.hpp>
#include <unordered_map>

using namespace std;

// getconf -a | grep CACHE

/*
 L2 CACHE ON M1
 - 12 MB per one core
 - 12 WAY -> 1MB each way
 - 1MB/16KB = 64 PAGES/COLORS in a way
 - 7 bits inside cache line
 - 7 bits to support VIPT out of 14 bits inside cache line (intel has 6 + 6)
 - 6 bits page color
 */

constexpr size_t operator"" _B(unsigned long long v) {
    return v;
}
constexpr size_t operator"" _KB(unsigned long long v) {
    return v * 1024;
}
constexpr size_t operator"" _MB(unsigned long long v) {
    return v * 1024 * 1024;
}
constexpr size_t log2(size_t n) {
    return ((n < 2) ? 0 : 1 + log2(n / 2));
}

struct M1 {
    struct Memory {
        struct CacheLine {
            static constexpr size_t size = 128_B;
            static constexpr size_t bits = log2(size);
        };
        struct Page {
            static constexpr size_t size = 16_KB;
            static constexpr size_t bits = log2(size);
        };
    };
    struct L2 {
        static constexpr size_t size = 12_MB;
        static constexpr size_t ways = 12;
        static constexpr size_t way_size = size / ways;
        struct PageColor {
            static constexpr size_t colors = L2::way_size / Memory::Page::size;
            static constexpr size_t bits = log2(colors);
        };
        struct Bits {
            static constexpr size_t offset = Memory::CacheLine::bits;
            static constexpr size_t index = Memory::Page::bits - offset;
            static constexpr size_t color = PageColor::bits;
        };
        static_assert(Bits::offset == 7);
        static_assert(Bits::index == 7);
        static_assert(Bits::color == 6);
    };
};

static_assert(M1::Memory::CacheLine::size == CACHE_LINE_SIZE);
static_assert(M1::Memory::Page::size == PAGE_SIZE);

template<typename T = M1>
struct TestL2 {
    size_t pages = 1024 * 16;
    using paddr_t = uint8_t*;
    using vaddr_t = uint8_t*;
    using page_indices_t = vector<size_t>;
    using pmap_t = unordered_map<paddr_t, page_indices_t>;
    vaddr_t vaddrs;
    pmap_t pmap;

    void allocate() {
//        assert(M1:: == getpagesize());
        vaddrs = (vaddr_t) mmap(0, T::Memory::Page::size * pages, PROT_READ | PROT_WRITE, //
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 0, 0);
    }
    void build_physical_pages_map() {
        vector<PageMapEntry> entries;
        entries.resize(pages);
        get_physical_pages(vaddrs, &entries[0], pages);
//        for(auto& e: entries)
//            pmap[e.entry]
    }

    void run() {
        allocate();
    }
};

int main() {
    TestL2<M1> test;
    test.run();
}
