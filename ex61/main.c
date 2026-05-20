extern char uart_getc(void);
extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);

/* Memory map */
#define PAGE_OFFSET 0xffffffc000000000UL
#define PAGE_SIZE   (1UL << 12)
#define PMD_SIZE    (1UL << 21)
#define PGD_SIZE    (1UL << 30)

/* VA bit-field shifts (Sv39) */
#define PGD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12

#define ENTRIES_PER_TABLE 512

#define KERNEL_PGD_INDEX(va) ((va >> PGD_SHIFT) & 0x1FF)

#define LINEAR_MAP_GIB 4

/* PTE descriptor bits (Sv39) */
#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

#define PROT_KERNEL (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_A | PTE_D)

#define SATP_SV39         (8UL << 60)
#define MAKE_SATP(pgd_pa) (SATP_SV39 | ((unsigned long)(pgd_pa) >> 12))

#define MAKE_PTE(pa, flags) ((((unsigned long)(pa)) >> 12) << 10 | (flags))

static unsigned long
    __attribute__((section(".data"),
                   aligned(PAGE_SIZE))) pgd[ENTRIES_PER_TABLE] = {0};

static unsigned long __attribute__((
    section(".data"),
    aligned(PAGE_SIZE))) pmd[LINEAR_MAP_GIB][ENTRIES_PER_TABLE] = {{0}};

void setup_vm(void) {
    // TODO: Set up page tables for identity mapping and kernel mapping
    for (int g = 0; g < 4; g++) {        // 跑 4 個 1 GB
        for (int p = 0; p < 512; p++) {  // 每個 1 GB 裡面切 512 個 2 MB
            // 精確算出每一個 2 MB 的實體位址 (PA)
            unsigned long pa =
                0x80000000UL + (g * 0x40000000UL) + (p * 0x200000UL);

            // 寫入 PMD 項目（因為帶有 PROT_KERNEL 權限，硬體會認定這是 2MB
            // 葉子節點）
            pmd_identity[g][p] = MAKE_PTE(pa, PROT_KERNEL);
            pmd_kernel[g][p] = MAKE_PTE(pa, PROT_KERNEL);
        }
    }

    for (int i = 0; i < 4; i++) {
        unsigned long pmd_id_pa = (unsigned long)pmd_identity[i];
        unsigned long pmd_k_pa = (unsigned long)pmd_kernel[i];
        if (pmd_id_pa >= PAGE_OFFSET)
            pmd_id_pa -= PAGE_OFFSET;
        if (pmd_k_pa >= PAGE_OFFSET)
            pmd_k_pa -= PAGE_OFFSET;
        unsigned long low_va = 0x80000000UL + (i * 0x40000000UL);
        unsigned long high_va = PAGE_OFFSET + low_va;
        pgd[KERNEL_PGD_INDEX(low_va)] = MAKE_PTE(pmd_id_pa, PTE_V);
        pgd[KERNEL_PGD_INDEX(high_va)] = MAKE_PTE(pmd_k_pa, PTE_V);
    }

    unsigned long uart_pa = 0x10000000UL;
    pgd[KERNEL_PGD_INDEX(PAGE_OFFSET + uart_pa)] =
        MAKE_PTE(0x00000000UL, PROT_KERNEL);
    unsigned long pgd_pa = (unsigned long)pgd;
    if (pgd_pa >= PAGE_OFFSET) {
        pgd_pa -= PAGE_OFFSET;
    }
    unsigned long satp_val = MAKE_SATP(pgd_pa);
    asm volatile("csrw satp, %0" : : "r"(satp_val));
    asm volatile("sfence.vma");
}

void drop_identity_map(void) {
    // TODO: Drop identity mapping
    for (int i = 0; i < 4; i++) {
        unsigned long low_va = 0x80000000UL + (i * 0x40000000UL);

        // 核心修正：直接把最高層的 PGD 格子清空
        pgd[KERNEL_PGD_INDEX(low_va)] = 0;
    }
    asm volatile("sfence.vma");
}

void start_kernel(void) {
    uart_puts("\nStarting kernel at : ");
    uart_hex((unsigned long)start_kernel);
    uart_puts("\n");
    while (1) {
        uart_putc(uart_getc());
    }
}