#pragma once

#include <compiling.h>

struct ProcessInfo {
    static size_t getResidentSize() {
        std::ifstream myfile { "/proc/" + std::to_string(getpid()) + "/statm" };
        size_t v;
        myfile >> v >> v;
        return v * 4096;
    }
};

struct MemoryUsage {
    size_t prev = 0;
    size_t delta = 0;
    MemoryUsage() {
        size_t current = ProcessInfo::getResidentSize();
        std::cout << "memory usage initial: " << current << std::endl;
        prev = current;
    }
    void report(const char *what) {
        size_t current = ProcessInfo::getResidentSize();
        delta = current - prev;
        std::cout << "memory usage delta: " << delta << ' ' << what << std::endl;
        prev = current;
    }
    void report(const char *what, size_t element_size) {
        report(what);
        std::cout << "... which is " << delta / element_size << " elements of " << element_size << " bytes" << std::endl;
    }
};

struct PageMapEntry {
    void *entry;
    static constexpr uint64_t present = 1ul << 63;
    static constexpr uint64_t swapped = 1ul << 62;
    static constexpr uint64_t shared = 1ul << 61;
    static constexpr uint64_t exclusive = 1ul << 56;
    static constexpr uint64_t soft_dirty = 1ul << 55;
    static constexpr uint64_t pfn_mask = (1ul << 55) - 1;
    static constexpr uint64_t swap_offset_shift = 5;
    static constexpr uint64_t swap_type_mask = (1ul << swap_offset_shift) - 1;
    static constexpr uint64_t swap_offset_mask = (1ul << (55 - swap_offset_shift)) - 1;

    void* ptr() const {
        return (void*) ((uint64_t(entry) & pfn_mask) * PAGE_SIZE);
    }
    friend std::ostream& operator<<(std::ostream &out, const PageMapEntry &v) {
        uint64_t e = uint64_t(v.entry);
        if (e & present) {
            if (e & swapped) {
                out << "swapped{type:" << (e & swap_type_mask) << ",offset:" << ((e >> swap_offset_shift) & swap_offset_mask);
            } else {
                out << v.ptr();
                if (e & shared)
                    out << " shared";
                if (e & exclusive)
                    out << " exclusive";
                if (e & soft_dirty)
                    out << " soft_dirty";
            }
        } else {
            out << "none";
        }
        return out;
    }
};
static_assert(sizeof(PageMapEntry) == 8);

inline
void get_physical_pages(void *addr, PageMapEntry *buffer, size_t pages, size_t stride = 1) {
    int fd;
    assert(PAGE_SIZE == getpagesize());
    SYS_CALL(fd = open("/proc/self/pagemap", O_RDWR), "open");
    if (stride == 1) {
        SYS_CALL(pread(fd, buffer, sizeof(PageMapEntry) * pages, size_t(addr)/PAGE_SIZE * sizeof(PageMapEntry)), "pread");
    } else {
        // It is required when 16KB pages are simulated with 4KB real pages
        PageMapEntry *tmp = new PageMapEntry[pages * stride];
        size_t toRead = sizeof(PageMapEntry) * pages * stride;
        size_t read = pread(fd, tmp, toRead, size_t(addr) / PAGE_SIZE * sizeof(PageMapEntry));
        SYS_CALL_CHECK(read != toRead, "pread");
        for (size_t i = 0; i < pages; ++i)
            buffer[i] = tmp[i * stride];
        delete[] tmp;
    }
    SYS_CALL(close(fd), "close");
}

inline
void report_physical_pages(void *addr, size_t pages, size_t stride = 1) {
    std::vector<PageMapEntry> entries;
    entries.resize(pages);
    get_physical_pages(addr, &entries[0], pages, stride);
    for (size_t i = 0; i < pages; ++i)
        std::cout << "virt addr " << (void*) ((uint8_t*) addr + i * PAGE_SIZE * stride) << " physical " << entries[i]
                << std::endl;
}

template<typename GetColor>
inline
void report_physical_pages(void *addr, size_t pages, GetColor getColor, size_t stride = 1) {
    std::vector<PageMapEntry> entries;
    entries.resize(pages);
    get_physical_pages(addr, &entries[0], pages, stride);
    for (size_t i = 0; i < pages; ++i)
        std::cout << "virt addr " << (void*) ((uint8_t*) addr + i * PAGE_SIZE * stride) << " physical " << entries[i]
                << " color " << getColor(entries[i].ptr()) << std::endl;
}

inline
void* map_physical_memory(void *phys_addr, size_t pages) {
    int fd;
    SYS_CALL(fd = open("/dev/mem", O_RDWR), "open_dev_memory");
    void *ptr;
    SYS_CALL_MMAP(ptr = mmap(0, pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, size_t(phys_addr)), "mmap");
    return ptr;
}

inline
constexpr void* align_up(void *addr, size_t alignement) {
    size_t addr_i = size_t(addr);
    size_t missalignement = addr_i % alignement;
    return (void*) (missalignement ? (addr_i - missalignement + alignement) : addr_i);
}

inline
constexpr bool is_aligned(void *addr, size_t alignement) {
    return size_t(addr) % alignement == 0;
}

