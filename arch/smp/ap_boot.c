#include "asm/smp_boot.h"
#include "asm/apic.h"
#include "arch/io.h"
#include "relis/printk.h"
#include "relis/smp.h"
#include <stdint.h>

void ap_main(void) {
    outb(0x3F8, 'A');

    remap_pic();
    lapic_write(0xE0, 0xFFFFFFFF);
    lapic_write(LAPIC_SVR, LAPIC_ENABLE | 0xFF);
    lapic_write(LAPIC_ERROR, 0);

    outb(0x3F8, 'P');

    __sync_fetch_and_add(&num_cpus, 1);
    printk("SMP: AP %d online", lapic_get_id());

    while (1) {
        __asm__ volatile("hlt");
    }
}
