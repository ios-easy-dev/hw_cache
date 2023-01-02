#include <compiling.h>
#include <resources.hpp>
#include <stream_utils.h>
#include <timing.hpp>
#include <algorithm> // IWYU keep
#include <bitset>
#include <iomanip>
#include <iterator>

using namespace std;

// getconf -a | grep CACHE

/*
L1 DEDUCTIONS
- 14 bits 16K page size
- 7 bits 128B cache line size
- 128K/16K = 8 pages in L1 - 8 WAY CACHE
- 16K/128 = 128 sets in L1 each way

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
        struct Physical {
            static constexpr size_t bits = 48;
        };
    };
    struct L2 {
        static constexpr size_t size = 12_MB;
        static constexpr size_t ways = 12;
        static constexpr size_t way_size = size / ways;
    };
};

struct Haswell {
    struct Memory {
        struct CacheLine {
            static constexpr size_t size = 64_B;
            static constexpr size_t bits = log2(size);
        };
        struct Page {
            static constexpr size_t size = 4_KB;
            static constexpr size_t bits = log2(size);
        };
        struct Physical {
            static constexpr size_t bits = 48;
        };
    };
    struct L2 {
        static constexpr size_t size = 256_KB;
        static constexpr size_t ways = 8;
        static constexpr size_t way_size = size / ways;
    };
};


template<typename _T, typename _L>
struct PageColors {
    using T = IDE_DEFAULT_TYPE(_T, M1);
    using L = IDE_DEFAULT_TYPE(_L, T::L2);
    using M = T::Memory;
    static constexpr size_t colors = L::way_size / T::Memory::Page::size;
    static constexpr size_t bits = log2(colors);
    // https://dinhtta.github.io/cache/
    // m (tag bits) + s (set bits) + b (line offset bits)
    // s = c (color bits) + i (uncolored)
    static constexpr size_t b_bits = M::CacheLine::bits;
    static constexpr size_t i_bits = M::Page::bits - b_bits;
    static constexpr size_t c_bits = bits;
    constexpr static uint64_t make_mask(uint64_t) {
        return (1ul << bits) - 1;
    }
    constexpr static uint64_t extract_bits(uint64_t v, size_t n, size_t c) {
        return (v >> n) & make_mask(c);
    }
    constexpr static size_t get_page_color(void *p) {
        return extract_bits(uint64_t(p), b_bits + i_bits, c_bits);
    }
    constexpr static size_t get_colors_cnt() {
        return 1ul << c_bits;
    }
};

using PageColors_M1L2 = PageColors<M1,M1::L2>;
static_assert(PageColors_M1L2::b_bits == 7);
static_assert(PageColors_M1L2::i_bits == 7);
static_assert(PageColors_M1L2::c_bits == 6);

//static_assert(M1::Memory::CacheLine::size == CACHE_LINE_SIZE);
//static_assert(M1::Memory::Page::size == PAGE_SIZE);

template<typename _T>
struct TestL2 {
    size_t population = 1024;
    using paddr_t = uint8_t*;
    using vaddr_t = uint8_t*;
    using page_indices_t = vector<size_t>;
    using cmap_t = vector<page_indices_t>;
    using cmap_idx_t = vector<cmap_t::const_iterator>;
    using T = IDE_DEFAULT_TYPE(_T, M1);
    using M = T::Memory;
    using C = PageColors<T, typename T::L2>;
    using L = T::L2;
    vaddr_t base;
    vector<PageMapEntry> base_map;
    cmap_t cmap;                // map of color to vector of pages indices
    cmap_idx_t cmap_idx;        // vector of iterators into original map sorted by pages count
    constexpr static int mmap_prot = PROT_READ | PROT_WRITE;
    constexpr static int mmap_flags = MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE;
    constexpr static size_t straddle = M::Page::size / PAGE_SIZE;
    static_assert(M::Page::size % PAGE_SIZE == 0);

    void allocate() {
        // assert(T::Memory::CacheLine::size == getpagesize());
        size_t size = M::Page::size * population;
        // Make note that without MAP_POPULATE physical addresses will not be available
        void *raw;
        SYS_CALL_MMAP(raw = mmap(0, size + M::Page::size, mmap_prot, mmap_flags, -1, 0), "mmap");
        base = (vaddr_t) align_up(raw, M::Page::size);
        memset(base, 113, size);
        cout << "base virtual address " << (void*) base << ", size " << size << endl;
        assert(is_aligned(base, M::Page::size));
        base_map.resize(population);
        get_physical_pages(base, &base_map[0], population, straddle);
    }

    void build_colored_pages_map() {
        cmap.resize(C::get_colors_cnt());
        for (auto &e : base_map) {
            void *paddr = e.ptr();
            size_t color = C::get_page_color(paddr);
            cmap.at(color).push_back(&e - &base_map[0]);
        }
    }

    void create_index() {
        for (auto it = cmap.cbegin(); it != cmap.cend(); ++it)
            if (it->size())
                cmap_idx.push_back(it);
        sort(cmap_idx.begin(), cmap_idx.end(), [](const auto &it1, const auto &it2) {
            return it1->size() < it2->size();
        });
    }

    void dump_colors_index() {
        for (auto &it : cmap_idx) {
            size_t cnt = 0;
            Delimiter comma { ',' };
            cout << "color " << (&*it - &cmap[0]) << " pages " << it->size() << ' ';
            for (auto &i : *it) {
                cout << comma << i << '@' << base_map[i].ptr();
                if (++cnt > 8) {
                    cout << ",...";
                    break;
                }
            }
            cout << endl;
        }
    }

    void* getVirtualAddress(size_t idx) {
        return base + M::Page::size * idx;
    }
    void* getPhisycalAddress(size_t idx) {
        return base_map[idx].ptr();
    }

    void report_page(size_t idx) {
        void *vaddr = getVirtualAddress(idx);
        void *paddr = getPhisycalAddress(idx);
        cout << "page #" << setw(3) << idx << ", vaddr " << vaddr << ", physical " << paddr << " "
                << bitset<64>(uint64_t(paddr)) << " color " << C::get_page_color(paddr) << endl;
    }

    void test_bad_mapping() {
        cout << "Pages to be used in bad way test\n"
                << "All pages of same color and amount of pages exceeds amount of ways by 1\n"
                << "It causes cache spill inside tight loop" << endl;
        vaddr_t bad_mapping = nullptr;
        const size_t test_pages = L::ways + 1;         // we need up to cache ways + 1 to cause saturation
        size_t size = M::Page::size * test_pages;
        void *raw;
        SYS_CALL_MMAP(raw = mmap(0, size + M::Page::size, mmap_prot, mmap_flags, -1, 0), "mmap");
        bad_mapping = (vaddr_t) align_up(raw, M::Page::size);
        assert(is_aligned(bad_mapping, M::Page::size));
        cout << "bad mapping base " << (void*) bad_mapping << endl;

        for (size_t page_set = 1; page_set < test_pages; ++page_set) {
            // page_set 1 maps same page test_pages times
            // performance drops with 9 different pages since 8 pages can fit M1 L1
            cout << "TEST " << test_pages << " WITH " << page_set << " different PAGES of same color" << endl;
            auto &pages = *cmap_idx.back(); // vector of pages with same color of max frequency
            for (size_t i = 0; i < test_pages; ++i) {
                size_t page_idx = pages[i % page_set];
                report_page(page_idx);
                vaddr_t vaddr, vaddr_new = bad_mapping + M::Page::size * i, vaddr_old = (vaddr_t) getVirtualAddress(page_idx);
                const int remap_flags = MREMAP_FIXED | MREMAP_MAYMOVE;
                SYS_CALL_MMAP(vaddr = (vaddr_t ) mremap(vaddr_old, 0, M::Page::size, remap_flags, vaddr_new), "mremap");
                SYS_CALL_CHECK(vaddr != vaddr_new, "mremap");
                mem_load(vaddr);
            }
            cout << "value " << (int) bad_mapping[0] << endl;
            // now we have contiguous space and can run tight loop test
            test_loop(bad_mapping, test_pages);
        }
    }

    void test_loop(vaddr_t addr, size_t pages) {
        size_t size = pages * M::Page::size;
        cout << "test " << pages << " pages at " << (void*) addr << " total size " << size << endl;
        report_physical_pages(addr, pages, C::get_page_color, straddle);
        const size_t total_size = M::Page::size * pages;
        const size_t stride = M::CacheLine::size;
        const size_t strides = total_size / stride;
        const size_t loops = 1 << 10;

        auto run = [&]() {
            for (size_t i = 0; i < loops; ++i) {
                uint8_t *p = addr;
                for (size_t j = 0; j < strides; ++j) {
                    *p = i;
                    p += stride;
                }
            }
        };
        auto reset = []() {
        };
        TimeItNs_Repeat<loops> timeItNs { "cache_line_write_access" };
        timeItNs.run(run, reset);
    }

    void info() {
        cout << "page size {model:" << M::Page::size << ",os:" << getpagesize() << ",straddle:" << straddle
                << "} cache line size " << M::CacheLine::size << endl;
        cout << "L2 bits {color:" << C::c_bits << ",index:" << C::i_bits << ",line:" << C::b_bits << "}, colors: "
                << C::get_colors_cnt() << ", ways: " << L::ways << endl;
    }

    void run() {
        info();
        allocate();
        build_colored_pages_map();
        create_index();
        dump_colors_index();
        test_bad_mapping();
    }
};

int main() {
#if IS_ARM
    TestL2<M1> test;
#else
    TestL2<Haswell> test;
#endif
    test.run();
}
