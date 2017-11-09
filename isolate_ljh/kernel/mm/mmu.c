#include <zjunix/mmu/mmu.h>

#define VPN2_mask ((1<<(PAGE_SHIFT+1)) - 1)
#define ASID_mask ((1<<8) - 1)

#pragma GCC push_options
#pragma GCC optimize("O0")

void insert_new_tlb_entry(unsigned int ASID, unsigned char* VAddr, unsigned char* PAddr0, unsigned char* PAddr1) {
    // VAddr cannot be NULL
    assert (VAddr != 0, "[insert_new_tlb_entry]VAddr == NULL !!")
    unsigned int cp0EntryHi, cp0EntryLo0, cp0EntryLo1;
    cp0EntryHi = ((unsigned int)VAddr & (~VPN2_mask)) | (ASID & (~ASID_mask));
    cp0EntryLo0 = ((unsigned int)PAddr0 >> 6) & 0x1ffffc0) | 0x1a;
    cp0EntryLo0 = ((ENTRY >> 6) & 0x01ffffc0) | 0x1e;
    
    asm volatile(
        "mfc0 %0, $9, 6\n\t"
        "mfc0 %1, $9, 7\n\t"
        : "=r"(ticks_low), "=r"(ticks_high));

    asm volatile(
        "li $t0, 1\n\t"
        "mtc0 $t0, $10\n\t"  // EntryHi = 1, ASID = 1, VPN2 = 0 (????)  Only a demo
        "mtc0 $zero, $5\n\t" // PageMask = 0(1 Page = 4KB)
        "move $t0, %0\n\t"   
        "mtc0 $t0, $2\n\t"   // EntryLo0 = cp0EntryLo0
        "mtc0 $zero, $3\n\t" // EntryLo1 = 0
        "mtc0 $zero, $0\n\t" // Index = 0
        "nop\n\t"
        "nop\n\t"
        "tlbwi"
        : "=r"(cp0EntryLo0));
}

#pragma GCC pop_options