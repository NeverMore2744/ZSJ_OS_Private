#include <arch.h>
#include <driver/vga.h>
#include <zjunix/slab.h>
#include <zjunix/utils.h>

#define KMEM_ADDR(PAGE, BASE) ((((PAGE) - (BASE)) << PAGE_SHIFT) | 0x80000000)
#define ptr_equal(ptr1, ptr2) (((void*)ptr1) == ((void*)ptr2))

/*
 * one list of PAGE_SHIFT(now it's 12) possbile memory size
 * 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, (2 undefined)
 * in current stage, set (2 undefined) to be (4, 2048)
 */
struct kmem_cache kmalloc_caches[PAGE_SHIFT]; // Use struct variables as a cache

static unsigned int size_kmem_cache[PAGE_SHIFT] = 
    {96, 192,   8,  16,   32,   64, 
    128, 256, 512,  1024, 1536, 2048};
static unsigned int maxnum_kmem_cache[PAGE_SHIFT] = 
    {40, 20,  340,  204,  113,  60, 
     30, 15,    7,  3,    2,    1};
static unsigned int remainer_kmem_cache[PAGE_SHIFT] = 
    {96, 176,  16,  16,   28,   16, 
    132, 196, 484,  1012, 1024, 2044};  // must be larger than or equal to 12

// init the struct kmem_cache_cpu
void init_kmem_cpu(struct kmem_cache_cpu *kcpu) {
    kcpu->page = 0;
    kcpu->freeobj = 0;
}

// init the struct kmem_cache_node
void init_kmem_node(struct kmem_cache_node *knode) {
    INIT_LIST_HEAD(&(knode->full));         // Init the list
    INIT_LIST_HEAD(&(knode->partial));  
}

/* we have *src_prev == ptr
 * src_prev -> ptr -> ...1
 * dest_prev -> *dest_prev ->...2
 * 
 * TO
 * 
 * src->prev -> ...1
 * dest_prev -> ptr -> *dest_prev -> ...2
 */
unsigned int move_node(void* src_prev, void* dest_prev, void* ptr, void* nil) {
    if (ptr_equal(ptr, nil)) return 0;
    *src_prev = *ptr;
    *ptr = *dest_prev;
    *dest_prev = ptr;
}

void init_each_slab(struct kmem_cache *cache, unsigned int size, unsigned int maxnum, unsigned int remainder) {
    cache->objsize = size;      // objsize = aligned(size), using the length of an integer
    cache->objsize += (SIZE_INT - 1);  
        // Align the objsize using the address width
    cache->objsize &= ~(SIZE_INT - 1);
    cache->size = cache->objsize + sizeof(void *);  
        // add one char as mark(available)
    cache->offset = remainder;      
    cache->maxnum = maxnum;
        // cache->offset points to the offset of the first object
    init_kmem_cpu(&(cache->cpu));
    init_kmem_node(&(cache->node));
}

void init_slab() {
    unsigned int i;

    for (i = 0; i < PAGE_SHIFT; i++) {
        init_each_slab(&(kmalloc_caches[i]), 
                        size_kmem_cache[i], maxnum_kmem_cache[i],
                        remainer_kmem_cache[i] - sizeof(unsigned char*));
    }
#ifdef SLAB_DEBUG
    kernel_printf("Setup Slub ok :\n");
    kernel_printf("\tcurrent slab cache size list:\n\t");
    for (i = 0; i < PAGE_SHIFT; i++) {
        kernel_printf("%x %x ", kmalloc_caches[i].objsize, (unsigned int)(&(kmalloc_caches[i])));
    }
    kernel_printf("\n");
#endif  // ! SLAB_DEBUG
}

// ATTENTION: sl_objs is the reuse of bplevel
// ATTENTION: slabp must be set right to add itself to reach the end of the page
// 		e.g. if size = 96 then 4096 / 96 = .. .. 64 then slabp starts at
// 64
void format_slabpage(struct kmem_cache *cache, struct page *page) {
    unsigned char *m_start = (unsigned char *)KMEM_ADDR(page, pages);
    unsigned char *moffset = m_start;  
        // physical addr of this page
    struct slab_head *s_head = (struct slab_head *)m_start;
    s_head->nr_objs = 0;
    s_head->used_start_ptr = (void*)m_start;
    s_head->empty_start_ptr = (void*)((unsigned char*)m_start + cache->offset - sizeof(unsigned char*));
        /* 
            Add the "s_head" struct to the first several bytes of the page
            struct slab_head {
                void *end_ptr;
                unsigned int nr_objs;
            }; 
        */
    unsigned char *ptr;
    moffset += cache->offset;

    set_flag(page, _PAGE_SLAB);
    do {
        ptr = (unsigned int *)(moffset - sizeof(unsigned char *));
        *ptr = (unsigned int)ptr + cache->size; 
            // Points to the next slabs
        moffset += cache->size;     // cache->size == cache->objsize + sizeof(char *)
    } while (moffset & ~((1<<PAGE_SHIFT)-1));

    ptr = (void*)(m_start + ((1 << PAGE_SHIFT) - sizeof(unsigned char*))));
        // The final pointer
    *ptr = m_start; 
        // The last pointer points to the beginning of page

    cache->cpu.page = page; // Get a page
    cache->cpu.freeobj = (void *)(m_start + cache->offset);
    page->virtual = (void *)cache;
    //page->slabp = (unsigned int)(cache->cpu.freeobj);
}

void *slab_alloc(struct kmem_cache *cache) {
    struct slab_head *s_head;
    void *object_ptr;
    void *object = 0;
    struct page *newpage;

    if (cache->cpu.page == 0) {
        if (list_empty(&(cache->node.partial))) {
            // call the buddy system to allocate one more page to be slab-cache
            newpage = __alloc_pages(0);  // get bplevel = 0 page === one page
            if (!newpage) {
                // allocate failed, memory in system is used up
                kernel_printf("ERROR: slab request one page in cache failed\n");
                while (1)
                    ;
            }
        #ifdef SLAB_DEBUG
            kernel_printf("\tnew page, index: %x \n", newpage - pages);
        #endif  // ! SLAB_DEBUG
                // using standard format to shape the new-allocated page,
                // set the new page to be cpu.page
            set_flag(newpage, _PAGE_SLAB);
            list_add_tail(&(newpage->list), &(cache->node.partial));
            format_slabpage(cache, newpage);
                // format_slabpage updates cache->cpu automatically
        }
    }
    //cache->cpu.page->slabp = (unsigned int)(cache->cpu.freeobj);
    object = cache->cpu.freeobj;
    object_ptr = (void *)((unsigned char*)object_ptr - sizeof(unsigned char*));
    s_head = (struct slab_head *)KMEM_ADDR(cache->cpu.page, pages);

    s_head->empty_start_ptr = *((void*)(*(s_head->empty_start_ptr)));
    if (s_head->used_start_ptr == (void*)s_head) {
        *object_ptr = (void*)s_head;
        s_head->used_start_ptr = object_ptr;
    }
    ++(s_head->nr_objs);

    if (s_head->nr_objs == cache->maxnum) {  // This slab is full
        list_del_init(&(cache->cpu.page->list));
        list_add_tail(&(cache->cpu.page->list), &(cache->node.full));
#ifdef SLAB_DEBUG
        kernel_printf("%x\n", list_empty(&(cache->node.partial)));
#endif
    }
    
    // slab may be full after this allocation
    return object;
}

/*
 * @param: object is the address of the allocated page
 * But not in the kernel format(smaller than 0x7fffffff)
 * @cache: The kmem_cache
 * */
void slab_free(struct kmem_cache *cache, void *object) {
    void * object_ptr;
    void * pi;
    struct page *opage = pages + ((unsigned int)object >> PAGE_SHIFT);
    unsigned int *ptr;
    unsigned int succ = 1;
    struct slab_head *s_head = (struct slab_head *)KMEM_ADDR(opage, pages);

    if (!(s_head->nr_objs)) {  
        // There is no objects to be freed
        kernel_printf("ERROR : slab_free error!\n");
        while (1)
            ;
    }

    object_ptr = (void*)((unsigned char*)object - sizeof(unsigned char*));
    pi = s_head->used_start_ptr;
    // The operation on the slabed page:
    // Move the pointers and modify the linked list
    while (1) {
        if (ptr_equal(*pi, slab_head)) {
            kernel_printf("ERROR : No objects to free!");
            succ = 0;
            break;
        }
        if (ptr_equal(*pi, object_ptr)) {
            move_node(pi, s_head->empty_start_ptr, object_ptr, s_head);
            break;
        }
        pi = (void*)(*pi);  // traverse the linked list
    }
    if (!succ) return;
    if (s_head->nr_objs + 1 == cache->maxnum) {
        list_del_init(&(opage->list));
        list_add_tail(&(opage->list), &(cache->node.partial));;
    } else if (!(s_head->nr_objs)) {
        __free_pages(opage, 0);
    }
    if (opage == cache->cpu.page) {
        
    }

    if (list_empty(&(opage->list)))
        return;

}

// find the best-fit slab system for (size)
unsigned int get_slab(unsigned int size) {
    unsigned int itop = PAGE_SHIFT;
    unsigned int i;
    unsigned int bf_num = (1 << (PAGE_SHIFT - 1));  // half page
    unsigned int bf_index = PAGE_SHIFT;             // record the best fit num & index

    for (i = 0; i < itop; i++) {
        if ((kmalloc_caches[i].objsize >= size) && (kmalloc_caches[i].objsize < bf_num)) {
            bf_num = kmalloc_caches[i].objsize;
            bf_index = i;
        }
    }
    return bf_index;
}

//keep this function, important.
//it has been largely used by LIANG.
void *kmalloc(unsigned int size) {
    struct kmem_cache *cache;
    unsigned int bf_index;

    if (!size)
        return 0;

    // if the size larger than the max size of slab system, then call buddy to
    // solve this
    if (size > kmalloc_caches[PAGE_SHIFT - 1].objsize) {
        size += (1 << PAGE_SHIFT) - 1;
        size &= ~((1 << PAGE_SHIFT) - 1);
        return (void *)(KERNEL_ENTRY | (unsigned int)alloc_pages(size >> PAGE_SHIFT));
    }

    bf_index = get_slab(size);
    if (bf_index >= PAGE_SHIFT) {
        kernel_printf("ERROR: No available slab\n");
        while (1)
            ;
    }
    return (void *)(KERNEL_ENTRY | (unsigned int)slab_alloc(&(kmalloc_caches[bf_index])));
}

void kfree(void *obj) {
    struct page *page;

    obj = (void *)((unsigned int)obj & (~KERNEL_ENTRY));
    page = pages + ((unsigned int)obj >> PAGE_SHIFT);
    if (!(page->flag == _PAGE_SLAB)) 
        return free_pages((void *)((unsigned int)obj & ~((1 << PAGE_SHIFT) - 1)), page->bplevel);

    return slab_free(page->virtual, obj);
}
