#define tlb_test
//In arch.h
//extern volatile unsigned int tlb_ready_index;

#include <arch.h>
#include <zjunix/pc.h>

extern unsigned int *(*PGT_BaseAddrs[256])[2048];

void test_();
void delay();
void init_MMU();
void init_TLB();

void TLBModified_exception(unsigned int status, unsigned int cause, context* pt_context);
void TLBMiss_Invalid_exception(unsigned int status, unsigned int cause, context* pt_context);
void TLBMiss_refill_exception(unsigned int status, unsigned int cause, context* pt_context);
void insert_new_tlb_entry(unsigned int ASID, unsigned int VPN2, unsigned int we0, unsigned int we1);
void TLB_info();
void read_TLB(unsigned int index, 
    unsigned int* cp0EntryHi_ptr,
    unsigned int* cp0EntryLo0_ptr,
    unsigned int* cp0EntryLo1_ptr);
// Vaddr0 and Vaddr1 can't be null at the same time

void print_tlb_contexts();
void pgtable_info();
unsigned int create_PGT(unsigned int ASID);
unsigned int create_PDT(unsigned int ASID, unsigned int PGT_entry);
unsigned int delete_PGT(unsigned int ASID);
unsigned int release_PGT_entry(unsigned int ASID, unsigned int PGT_entry);
unsigned int delete_PDT(unsigned int* PDT_BaseAddr);
unsigned int release_PDT_entry(unsigned int* PDT_BaseAddr, unsigned int PDT_entry);
unsigned int create_PDT_entry(unsigned int ASID, unsigned int VPN2, unsigned int pages_num);
unsigned int Set_valid(unsigned int ASID, unsigned int BadVAddr);
unsigned int Set_dirty(unsigned int ASID, unsigned int BadVAddr);