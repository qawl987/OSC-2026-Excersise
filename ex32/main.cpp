#include <iostream>
#include <list>
#include <vector>

#define PAGE_SIZE  (1UL << 12)
#define NUM_PAGES  0x280000
#define MAX_ORDER  10
#define PAGE_SHIFT 12

typedef unsigned long phys_addr_t;

struct page {
    int order = 0;
    int refcount = 0;
};

std::vector<page> mem_map;
std::vector<std::list<page*>> free_area;

unsigned long addr_to_page(unsigned long addr, unsigned long mem_base) {
    return (addr - mem_base) / PAGE_SIZE;
}

struct page* get_buddy(struct page* page, unsigned int order) {
    return &mem_map[(page - mem_map.data()) ^ (1 << order)];
}

void memory_reserve(phys_addr_t base, size_t size) {
    // TODO: Implement this function
    unsigned long start_pfn = base >> PAGE_SHIFT;
    unsigned long end_pfn = (base + size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    unsigned long mem_base = (unsigned long)&mem_map[0];
    for (int order = MAX_ORDER; order >= 0; order--) {
        // 內層迴圈：走訪這個 order 裡所有的 block
        // 注意：因為你可能會在迴圈中把 block 從 list 移除，
        for (auto it = free_area[order].begin();
             it != free_area[order].end();) {
            struct page* curr = *it;
            unsigned long block_start_pfn = curr - mem_map.data();
            unsigned long block_end_pfn = block_start_pfn + (1 << order);

            if (block_end_pfn <= start_pfn || block_start_pfn >= end_pfn) {
                it++;
            } else if (block_start_pfn >= start_pfn &&
                       block_end_pfn <= end_pfn) {
                curr->refcount++;
                it = free_area[order].erase(it);
            } else {
                it = free_area[order].erase(it);
                int next_order = order - 1;
                curr->order = next_order;
                free_area[next_order].emplace_back(curr);
                struct page* buddy = get_buddy(curr, next_order);
                buddy->order = next_order;
                free_area[next_order].emplace_back(buddy);
            }
        }
    }
}

void dump() {
    for (int i = MAX_ORDER; i >= 0; i--)
        std::cout << "free_area[" << i << "] " << free_area[i].size()
                  << std::endl;
}

void mm_init() {
    mem_map.resize(NUM_PAGES);
    free_area.resize(MAX_ORDER + 1);
    for (size_t i = 0; i < NUM_PAGES; i += (1 << MAX_ORDER)) {
        mem_map[i].order = MAX_ORDER;
        free_area[MAX_ORDER].push_back(&mem_map[i]);
    }
    memory_reserve(0, 0x82a69510);
}

int main() {
    mm_init();
    dump();
    return 0;
}
