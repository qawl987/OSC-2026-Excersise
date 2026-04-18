#include <stdint.h>
extern char uart_getc(void);
extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);
extern int hextoi(const char* s, int n);
extern int align(int n, int byte);
extern int memcmp(const void* s1, const void* s2, int n);
extern void* alloc_page();
extern void handle_exception(void);
extern void ret_from_exception(void);
// TODO: Check the RAM disk base address
#define INITRD_BASE 0x0000000088200000
#define STACK_SIZE  0x1000

struct cpio_t {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
};

// TODO: Define the trap frame structure
struct pt_regs {
    unsigned long ra;  // 8 * 0
    unsigned long
        sp;  // 8 * 1 (原本你寫 sscratch，但邏輯上它是 User 的 Stack Pointer)
    unsigned long gp;        // 8 * 2
    unsigned long tp;        // 8 * 3
    unsigned long t0;        // 8 * 4
    unsigned long t1;        // 8 * 5
    unsigned long t2;        // 8 * 6
    unsigned long s0;        // 8 * 7
    unsigned long s1;        // 8 * 8
    unsigned long a0;        // 8 * 9
    unsigned long a1;        // 8 * 10
    unsigned long a2;        // 8 * 11
    unsigned long a3;        // 8 * 12
    unsigned long a4;        // 8 * 13
    unsigned long a5;        // 8 * 14
    unsigned long a6;        // 8 * 15
    unsigned long a7;        // 8 * 16
    unsigned long s2;        // 8 * 17
    unsigned long s3;        // 8 * 18
    unsigned long s4;        // 8 * 19
    unsigned long s5;        // 8 * 20
    unsigned long s6;        // 8 * 21
    unsigned long s7;        // 8 * 22
    unsigned long s8;        // 8 * 23
    unsigned long s9;        // 8 * 24
    unsigned long s10;       // 8 * 25
    unsigned long s11;       // 8 * 26
    unsigned long t3;        // 8 * 27
    unsigned long t4;        // 8 * 28
    unsigned long t5;        // 8 * 29
    unsigned long t6;        // 8 * 30
    unsigned long epc;       // 8 * 31 (即 sepc)
    unsigned long status;    // 8 * 32 (即 sstatus)
    unsigned long cause;     // 8 * 33 (即 scause)
    unsigned long badvaddr;  // 8 * 34 (即 stval)
};

int exec(const char* filename) {
    char* p = (char*)INITRD_BASE;
    while (memcmp(p + sizeof(struct cpio_t), "TRAILER!!!", 10)) {
        struct cpio_t* hdr = (struct cpio_t*)p;
        int namesize = hextoi(hdr->namesize, 8);
        int filesize = hextoi(hdr->filesize, 8);
        int headsize = align(sizeof(struct cpio_t) + namesize, 4);
        int datasize = align(filesize, 4);

        if (!memcmp(p + sizeof(struct cpio_t), filename, namesize)) {
            uintptr_t user_entry = (uintptr_t)(p + headsize);
            void* user_stack = alloc_page();
            uintptr_t user_sp = (uintptr_t)user_stack + STACK_SIZE;

            /* 在核心堆疊上預留空間 */
            register unsigned long current_sp __asm__("sp");
            struct pt_regs* regs =
                (struct pt_regs*)(current_sp - sizeof(struct pt_regs));

            /* 初始化上下文 */
            for (int i = 0; i < sizeof(struct pt_regs) / 8; i++) {
                ((unsigned long*)regs)[i] = 0;
            }

            /* 核心邏輯：設定這個行程「醒來」後的樣子 */
            regs->epc = user_entry;  // 預計跳轉的 PC
            regs->sp = user_sp;      // 預計使用的 SP

            /* 設定 sstatus (SPP=0 代表返回 User) */
            regs->status = (1 << 5);

            /* 為下一次中斷埋伏筆：進入核心時要換回來的 Kernel SP */
            __asm__ volatile("csrw sscratch, %0" : : "r"(current_sp));

            /* 執行恢復與切換 */
            __asm__ volatile(
                "mv sp, %0 \n"
                "j ret_from_exception"
                :
                : "r"(regs)
                : "memory");
        }
        p += headsize + datasize;
    }
    return -1;
}

void do_trap(struct pt_regs* regs) {
    // TODO: Implement this function

    // (1) Print the sepc and scause registers
    uart_puts("sepc: ");
    uart_hex(regs->epc);
    uart_puts(" scause: ");
    uart_hex(regs->cause);
    uart_putc('\n');
    // (2) Increment the sepc register by 4 for traps
    if (regs->cause == 8) {
        regs->epc += 4;
    }
    return;
}

void start_kernel() {
    uart_puts("\nStarting kernel ...\n");
    if (exec("prog.bin"))
        uart_puts("Failed to exec user program!\n");
    while (1) {
        uart_putc(uart_getc());
    }
}
