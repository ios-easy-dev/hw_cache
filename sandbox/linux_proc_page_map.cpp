#include <compiling.h>
#include <resources.hpp>

using namespace std;

struct MemTest {
    size_t pages = 18;
    uint8_t *addr;
    void allocate() {
        assert(PAGE_SIZE == getpagesize());
        addr = (uint8_t*) mmap(0, PAGE_SIZE * pages, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE/*| MAP_POPULATE*/, 0, 0);
        memset(addr, 1, PAGE_SIZE * pages);
        munmap(addr, PAGE_SIZE);
        munmap(addr + PAGE_SIZE * (pages - 1), PAGE_SIZE);
    }
    void modify_via_physical_memory() {
        const size_t modify_page_index = 2;
        const size_t modify_page_offset = 113;
        const int modify_page_value = 111;
        assert(modify_page_index < pages);
        // change memory via direct physical page access
        cout << "modifying via direct physical memory access /dev/mem" << endl;
        vector<PageMapEntry> entries;
        entries.resize(pages);
        get_physical_pages(addr, &entries[0], pages);

        void *phys_addr = entries[modify_page_index].ptr();
        uint8_t *addr2 = (uint8_t*) map_physical_memory(phys_addr, 1);
        cout << "mapping physical address " << phys_addr << " to virtual " << (void*) addr2 << endl;
        report_physical_pages(addr2, 1);

        cout << "writing " << modify_page_value << " by virtual " << (void*) (&addr2[modify_page_offset])
                << " and reading by virtual " << (void*) (&addr[modify_page_index * PAGE_SIZE + modify_page_offset]) << endl;
        addr2[modify_page_offset] = modify_page_value;
        int read_value = addr[modify_page_index * PAGE_SIZE + modify_page_offset];
        cout << "read " << read_value << std::endl;
        CHECK(read_value == modify_page_value, "Failed to write via /dev/mem");
    }
    void run() {
        allocate();
        report_physical_pages(addr, pages);
        cout << endl;
        modify_via_physical_memory();
        cout << endl;
    }
};

int main() {
    MemTest test;
    test.run();
}

