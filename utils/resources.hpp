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
    uint64_t entry;
    static constexpr uint64_t present = 1ul << 63;
    static constexpr uint64_t swapped = 1ul << 62;
    static constexpr uint64_t shared = 1ul << 61;
    static constexpr uint64_t exclusive = 1ul << 56;
    static constexpr uint64_t soft_dirty = 1ul << 55;
    static constexpr uint64_t pfn_mask = (1ul << 55) - 1;
    static constexpr uint64_t swap_offset_shift = 5;
    static constexpr uint64_t swap_type_mask = (1ul << swap_offset_shift) - 1;
    static constexpr uint64_t swap_offset_mask = (1ul << (55 - swap_offset_shift)) - 1;
    // unrelated additions
    static constexpr uint64_t page_color_mask = (1ul << 3) - 1;

    void* ptr() const {
        return (void*) ((entry & pfn_mask) * PAGE_SIZE);
    }
    friend std::ostream& operator<<(std::ostream &out, const PageMapEntry &v) {
        uint64_t e = v.entry;
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
                out << " color " << (e & page_color_mask);
            }
        } else {
            out << "none";
        }
        return out;
    }
};
static_assert(sizeof(PageMapEntry) == 8);

void get_physical_pages(void *addr, PageMapEntry *buffer, size_t pages) {
    int fd;
    SYS_CALL(fd = open("/proc/self/pagemap", O_RDWR), "open");
    SYS_CALL(pread(fd, buffer, sizeof(PageMapEntry) * pages, size_t(addr)/PAGE_SIZE * sizeof(PageMapEntry)), "pread");
    SYS_CALL(close(fd), "close");
}

void report_physical_pages(void *addr, size_t pages) {
    std::vector<PageMapEntry> entries;
    entries.resize(pages);
    get_physical_pages(addr, &entries[0], pages);
    for (size_t i = 0; i < pages; ++i)
        std::cout << "virt addr " << (void*) ((uint8_t*) addr + i * PAGE_SIZE) << " physical " << entries[i] << std::endl;
}

void* map_physical_memory(void *phys_addr, size_t pages) {
    int fd;
    SYS_CALL(fd = open("/dev/mem", O_RDWR), "open_dev_memory");
    void *ptr;
    SYS_CALL_MMAP(ptr = mmap(0, pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, size_t(phys_addr)), "mmap");
    return ptr;
}

