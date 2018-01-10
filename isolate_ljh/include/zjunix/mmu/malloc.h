#ifdef _ZSJOS_MALLOC_H__
#define _ZSJOS_MALLOC_H__



unsigned int* init_slab_malloc(unsigned int ASID, unsigned int size);

unsigned int* malloc(unsigned int ASID, unsigned int size);

// Syscall num. 133
void syscall_malloc(unsigned int status, unsigned int cause, context* pt_context);

unsigned int* load_mips_bin(unsigned char* filename);

#endif