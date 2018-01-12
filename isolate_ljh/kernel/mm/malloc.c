#include <exc.h>
#include <zjunix/syscall.h>
#include <zjunix/mmu/malloc.h>


// size < 4096
unsigned int* init_slab_malloc(unsigned int ASID, unsigned int size) {
    unsigned int* P = kmalloc(4096);
    
}

unsigned int* malloc(unsigned int ASID, unsigned int size) {

}

// 133
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context) {

}

unsigned int* load_mips_bin(unsigned char* filename);
