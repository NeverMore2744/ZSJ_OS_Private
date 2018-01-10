#define tlb_test
//In arch.h
//extern volatile unsigned int tlb_ready_index;

#include <arch.h>

extern unsigned int *(*PGT_BaseAddrs[256])[2048];

void test_();
void delay();
void init_MMU();

void insert_new_tlb_entry(unsigned int ASID, unsigned char* VAddr0, unsigned char* VAddr1);
// Vaddr0 and Vaddr1 can't be null at the same time

void print_tlb_contexts();

void register_TLB_exceptions();