#include <stdint.h>

extern char uart_getc(void);
extern void uart_putc(char c);
extern void uart_puts(const char* s);
extern void uart_hex(unsigned long h);

/* UART NS16550A 寄存器定義 */
#define UART_BASE 0x10000000UL
#define UART_RBR \
    ((volatile uint8_t*)(UART_BASE + 0x0))  // 接收緩衝 (DLAB=0) [cite: 532]
#define UART_THR \
    ((volatile uint8_t*)(UART_BASE + 0x0))  // 傳送保持 (DLAB=0) [cite: 532]
#define UART_IER \
    ((volatile uint8_t*)(UART_BASE + 0x1))  // 中斷使能 (DLAB=0) [cite: 532]
#define UART_LCR ((volatile uint8_t*)(UART_BASE + 0x3))  // 線路控制 [cite: 532]
#define UART_LSR ((volatile uint8_t*)(UART_BASE + 0x5))  // 線路狀態 [cite: 800]

#define LSR_DR   (1 << 0)  // Data Ready [cite: 803]
#define LSR_TDRQ (1 << 5)  // Transmit Holding Register Empty [cite: 844]
#define UART_IRQ 10        // UART 在 QEMU virt 模式下的 IRQ ID

/* PLIC 寄存器定義 (針對 S-mode Context) */
#define PLIC_BASE          0xc000000UL
#define PLIC_PRIORITY(irq) (PLIC_BASE + (irq) * 4)

/* * 根據 QEMU dts，Hart 0 S-mode 對應 Context 1
 * Enable 偏移: 0x2000 (Context 0) / 0x2080 (Context 1)
 * Threshold/Claim 偏移: 0x200000 (Context 0) / 0x201000 (Context 1)
 */
#define PLIC_S_ENABLE(hart)    (PLIC_BASE + 0x2080 + (hart) * 0x100)
#define PLIC_S_THRESHOLD(hart) (PLIC_BASE + 0x201000 + (hart) * 0x2000)
#define PLIC_S_CLAIM(hart)     (PLIC_BASE + 0x201004 + (hart) * 0x2000)

unsigned long boot_cpu_hartid = 0;

/* 宣告 CSR 操作函數 */
void irq_enable() {
    // 開啟 sstatus.SIE (Supervisor Interrupt Enable)
    asm volatile("csrsi sstatus, (1 << 1)");
}

void enable_external_interrupt() {
    // 開啟 sie.SEIE (Supervisor External Interrupt Enable)
    asm volatile(
        "li t0, (1 << 9);"
        "csrs sie, t0;");
}

void uart_init() {
    // 確保 DLAB=0 才能存取 IER
    *UART_LCR &= ~(0x80);

    // TODO: Enable RX interrupt (ERBFI: Enable Received Data Available
    // Interrupt) [cite: 897]
    *UART_IER |= 1;

    // TODO: Enable UART interrupt (讓 CPU 開始聽中斷)
    irq_enable();
}

void plic_init() {
    // TODO: Implement this function

    // (1) Set UART interrupt priority (優先級必須大於 Threshold)
    *(uint32_t*)(PLIC_PRIORITY(UART_IRQ)) = 1;

    // (2) Set UART interrupt enable for the boot hart (S-mode)
    // UART_IRQ 為 10，所以將第 10 位元設為 1
    *(uint32_t*)(PLIC_S_ENABLE(boot_cpu_hartid)) |= (1 << UART_IRQ);

    // (3) Set threshold for the boot hart (設為 0 以接收所有優先級 > 0 的中斷)
    *(uint32_t*)(PLIC_S_THRESHOLD(boot_cpu_hartid)) = 0;

    // (4) Enable external interrupts (開啟 CPU 端的外部中斷監聽)
    enable_external_interrupt();
}

int plic_claim() {
    // TODO: Implement this function
    // 讀取 Claim 暫存器獲取目前最高優先級的 IRQ ID
    return *(volatile uint32_t*)PLIC_S_CLAIM(boot_cpu_hartid);
}

void plic_complete(int irq) {
    // TODO: Implement this function
    // 修正：必須將剛才領取的 irq ID 寫回以結束中斷處理流程
    *(volatile uint32_t*)PLIC_S_CLAIM(boot_cpu_hartid) = irq;
}

void do_trap() {
    // 詢問 PLIC 是哪個硬體發出的中斷
    int irq = plic_claim();

    if (irq == UART_IRQ) {
        // 如果是 UART 中斷，讀取 RBR 暫存器取得字元 [cite: 804]
        char c = *UART_RBR;
        // 簡單的回顯 (Echo) 處理
        uart_putc(c == '\r' ? '\n' : c);
    }

    // 如果 irq 不為 0，通知 PLIC 處理完成
    if (irq)
        plic_complete(irq);
}

void start_kernel() {
    uart_puts("\nStarting kernel with UART interrupts ...\n");

    // 初始化 PLIC
    plic_init();

    // 初始化 UART 並開啟中斷
    uart_init();

    // 進入無窮迴圈，等待中斷觸發 do_trap
    while (1)
        ;
}