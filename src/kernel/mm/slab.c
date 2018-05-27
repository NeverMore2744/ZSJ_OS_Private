#include <arch.h>
#include <driver/vga.h>
#include <zjunix/buddy.h>
#include <zjunix/slab.h>
#include <zjunix/lock.h>
#include <zjunix/utils.h>

#define SLAB_DEBU
#define KMEM_ADDR(PAGE, BASE) ((((PAGE) - (BASE)) << PAGE_SHIFT) | 0x80000000)
#define ptr_equal(ptr1, ptr2) (((unsigned int*)(ptr1)) == ((unsigned int*)(ptr2)))

struct lock_t slab_lock;
/*
 * one list of PAGE_SHIFT(now it's 12) possbile memory size
 * 96, 192, 8, 16, 32, 64, 128, 256, 512, 1024, (2 undefined)
 * in current stage, set (2 undefined) to be (4, 2048)
 */
struct kmem_cache kmalloc_caches[PAGE_SHIFT]; // Use struct variables as a cache

static unsigned int size_kmem_cache[PAGE_SHIFT] =  // 对应各个objsize而不是size的大小
    {96, 192,   8,  16,   32,   64, 
    128, 256, 512,  1024, 1536, 2036};
static unsigned int maxnum_kmem_cache[PAGE_SHIFT] =  // 对应各个maxnum的大小
    {40, 20,  340,  204,  113,  60, 
     30, 15,    7,  3,    2,    2};
static unsigned int remainer_kmem_cache[PAGE_SHIFT] = // 对应各个offset的大小
    {96, 176,  16,  16,   28,   16, 
    136, 196, 484,  1012, 1016, 16};  // 必须大于等于16

void each_slab_info(struct kmem_cache* cache) {
    kernel_printf("  objsize=%x, size=%x, offset=%x, maxnum=%x\n", 
        cache->objsize, cache->size, cache->offset, cache->maxnum);
    kernel_printf("  cpu-freeobj:%x, cpu-page:%x; ", 
        cache->cpu.freeobj, cache->cpu.page);
    kernel_printf(" [page start:%x] \n", 
        KMEM_ADDR(cache->cpu.page, pages));
    kernel_printf("  node-partial:%x, node-full:%x \n", 
        cache->node.partial, cache->node.full);
}


void slab_info() {
    unsigned int i;
    for (i=0; i<PAGE_SHIFT; i++) {
        each_slab_info(&(kmalloc_caches[i]));
    }
}

void cache_node_info(struct kmem_cache_node* node) {
    struct list_head* sh = &(node->partial);
    struct list_head* ptr = sh->next;
    kernel_printf("\n  partial: ", sh);
    while (ptr != sh) {
        kernel_printf(" %x ", ptr);
        ptr=ptr->next;
    }
    sh = &(node->full);
    ptr = sh->next;
    kernel_printf("\n  full: ", sh);
    while (ptr != sh) {
        kernel_printf(" %x ", ptr);
        ptr=ptr->next;
    }
    kernel_printf("\n");
}


void show_slab_page(struct page* page) {
    if (page == 0) {
        kernel_printf("\n  [Slab page display failed, empty page]\n");
        return;
    }
    if (!has_flag(page, _PAGE_SLAB)) {
        return;
    }
    //kernel_printf("\n [Slab page display start]");
    unsigned int* mstart = (unsigned int*)KMEM_ADDR(page, pages);
    struct slab_head* s_head = (struct slab_head*)mstart;
    struct kmem_cache* cache = (struct kmem_cache*)(page->virtual);
    //each_slab_info(cache);
    cache_node_info(&(cache->node));
    kernel_printf("  mstart=%x, used_ptr=%x, empty_ptr=%x, nr=%x;  ", 
        mstart, s_head->used_start_ptr, s_head->empty_start_ptr, s_head->nr_objs);
    unsigned int* ptr;
    ptr = s_head->used_start_ptr;
    kernel_printf(" \n  Used: ");
    if ((unsigned int)(s_head->used_start_ptr) & ((1 << PAGE_SHIFT) - 1))  
        kernel_printf("  %x ", s_head->used_start_ptr);
    while(!ptr_equal(*ptr, mstart) && !ptr_equal(*ptr, (unsigned int)mstart | ((1<<PAGE_SHIFT)-4))) {
        kernel_printf("  %x ", *ptr);
        ptr = (unsigned int*)(*ptr);
    }
    ptr = s_head->empty_start_ptr;
    kernel_printf(" \n Empty: ");
    if ((unsigned int)(s_head->empty_start_ptr) & ((1 << PAGE_SHIFT) - 1))  
        kernel_printf("  %x ", s_head->empty_start_ptr);
    while(!ptr_equal(*ptr, mstart) && !ptr_equal(*ptr, (unsigned int)mstart | ((1<<PAGE_SHIFT)-4))) {
        kernel_printf("  %x ", *ptr);
        ptr = (unsigned int*)(*ptr);
    }
    kernel_printf("\n");
    //kernel_printf("\n [Slab page display end]\n");
}

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
unsigned int move_node(unsigned int* src_prev, unsigned int* dest_prev, unsigned int* ptr, unsigned int* nil) {
    if (ptr_equal(ptr, nil)) return 0;
    *src_prev = *ptr;
    *ptr = *dest_prev;
    *dest_prev = (unsigned int)ptr;
}

void init_each_slab(struct kmem_cache *cache, unsigned int size, unsigned int maxnum, unsigned int remainder) {
    cache->objsize = size;      // 数据块的大小
    cache->objsize += (SIZE_INT - 1);  
        // SIZE_INT == 4，按int的大小对齐
    cache->objsize &= ~(SIZE_INT - 1);
    cache->size = cache->objsize + sizeof(unsigned char *);  
        // 数据块的大小 + 指针的大小
    cache->offset = remainder;
        // offset的大小 
    cache->maxnum = maxnum;
        // 一个页中最多分配的数据块数目
    init_kmem_cpu(&(cache->cpu));
    init_kmem_node(&(cache->node));
}

void init_slab() {
    unsigned int i;
    init_lock(&(slab_lock));
    for (i = 0; i < PAGE_SHIFT; i++) {
        init_each_slab(&(kmalloc_caches[i]), 
                        size_kmem_cache[i], maxnum_kmem_cache[i],
                        remainer_kmem_cache[i]);
    }
#ifdef SLAB_DEBUG2
    kernel_printf("Setup Slub ok :\n");
    kernel_printf("\tcurrent slab cache size list:\n\t");
    for (i = 0; i < PAGE_SHIFT; i++) {
        kernel_printf("%x %x %x %x\n", kmalloc_caches[i].objsize, 
            (unsigned int)(&(kmalloc_caches[i])), 
            (unsigned int)(kmalloc_caches[i].size),
            (unsigned int)(kmalloc_caches[i].maxnum),
            (unsigned int)(kmalloc_caches[i].offset)
            );
    }
    kernel_printf("\n");
#endif  // ! SLAB_DEBUGs
}

void format_slabpage(struct kmem_cache *cache, struct page *page) {
    unsigned int *m_start = (unsigned int *)KMEM_ADDR(page, pages);
        // 得到页框的起始物理地址
    unsigned int *moffset = m_start;  
        // 页框的起始物理地址
    struct slab_head *s_head = (struct slab_head *)m_start;
    s_head->nr_objs = 0;  // 初始化，该页内没有数据块被分配
#ifdef SLAB_DEBUG_3
    kernel_printf("[2]nr_objs=%x", s_head->nr_objs);
#endif
    s_head->used_start_ptr = m_start;  // 已用数据块链表置空
#ifdef SLAB_DEBUG_3
    kernel_printf("[3]used_start_ptr=%x, m_start=%x\n", s_head->used_start_ptr, m_start);
#endif
    // 未用数据块链表的第一个指针指向第一个数据块的指针，即m_start + offset - 4
    s_head->empty_start_ptr = (unsigned int*)((unsigned char*)m_start + cache->offset - sizeof(unsigned char*));
#ifdef SLAB_DEBUG_3
    kernel_printf("[4]empty_start_ptr=%x", s_head->empty_start_ptr);
#endif
        /* 
            Add the "s_head" struct to the first several bytes of the page
            struct slab_head {
                void *end_ptr;
                unsigned int nr_objs;
            }; 
        */
#ifdef SLAB_DEBUG_3
    //kernel_printf("[Formating]m_start=%x, moffset=%x, slab_head=%x\n", m_start, moffset, s_head);
    kernel_printf("[Formating]nr_objs=%x, usedptr=%x, empty_ptr=%x\n", s_head->nr_objs, s_head->used_start_ptr, s_head->empty_start_ptr);
#endif
    unsigned int *ptr;
    unsigned int cnt=0;
    moffset = (unsigned int*)((unsigned char*) moffset + cache->offset);
    // moffset指向一个数据块
    set_flag(page, _PAGE_SLAB);
    do {
        ptr = (unsigned int*)((unsigned char*)moffset - sizeof(unsigned char *));
            // 指针的地址为数据块的地址-4
        *ptr = (unsigned int)ptr + cache->size;
            // 指向下一个指针
        moffset = (unsigned int*)((unsigned int)moffset + cache->size);     
            // 指针向下一个数据块移动
#ifdef SLAB_DEBUG2
    kernel_printf("  --ptr=%x, moffset=%x---   ", ptr, moffset);
    cnt++;
    if (cnt%4==3) kernel_printf("\n  ");
#endif
    } while ((unsigned int)moffset - (unsigned int)m_start < (1 << PAGE_SHIFT));

    ptr = (unsigned int*)((unsigned int)m_start + ((1 << PAGE_SHIFT) - sizeof(unsigned char*)));
        // 最后一个指针的地址
    *ptr = (unsigned int)m_start; 
        // 最后一个指针指向页的开头

    // 设置当前cache分配所正在使用的页
    cache->cpu.page = page; // 指向一个page数据结构
    cache->cpu.freeobj = (void *)((unsigned char*)m_start + cache->offset);
        // 指向空闲数据块单向链表的第一个元素（第一个数据块）
    page->virtual = (void *)cache;
        // 使page的virtual指针指向对应的cache
}

void *slab_alloc(struct kmem_cache *cache, unsigned int mark) {
    lockup(&(slab_lock));  // 加锁

    struct slab_head *s_head;
    unsigned int* object_ptr;
    unsigned int* object = 0;
    struct page *newpage;
    if (cache->cpu.page == 0) {
        if (list_empty(&(cache->node.partial))) {
            // partial链表为空，调用buddy的方法新分配一个页
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
                // 用format_slabpage函数对该slab页进行格式化
                // 并把这个新页的对应page数据结构指针赋值给cpu.page
            set_flag(newpage, _PAGE_SLAB); // 标记为以slab的方法来使用这个页
            list_add_tail(&(newpage->list), &(cache->node.partial));
                // 把这个新分配的slab页插入到partial链表中
            format_slabpage(cache, newpage);
                // 对新分配的页进行格式化
#ifdef SLAB_DEBUG_3
            kernel_printf("\tAfter formatting:\n");
            show_slab_page(newpage);
#endif
        } else {
            cache->cpu.page = container_of(cache->node.partial.next, struct page, list); 
                // 用container_of宏得到链表结点所在的page数据结构
            s_head = (struct slab_head*)KMEM_ADDR(cache->cpu.page, pages);
                // 找到这个页的开头，以及这个页所对应的slab_head数据结构的信息
            cache->cpu.freeobj = (unsigned int*)((unsigned char*)(s_head->empty_start_ptr) + sizeof(unsigned char*));
                // 用empty_start_ptr所指向的第一个空数据块作为freeobj的指向对象
        }
    }
    object = (unsigned int*)cache->cpu.freeobj;  // 准备返回给函数的调用者
    object_ptr = (unsigned int*)((unsigned char*)object - sizeof(unsigned char*));
        // 得到这个新分配出去的数据块所对应的指针所在地址（就在这个空数据块的前面）
    s_head = (struct slab_head *)KMEM_ADDR(cache->cpu.page, pages);
        // 得到这个页的slab_head指针，也就是这个页的开头
    cache->cpu.freeobj = (unsigned int*)((unsigned char*)(*object_ptr) + sizeof(unsigned char*));
        // 移动freeobj指针，使它指向下一个空的数据块
        // *object_ptr也就是取object_ptr的值，也就是它所指向的下一个空数据块指针的地址
    s_head->empty_start_ptr = (unsigned int*)(*(s_head->empty_start_ptr));
        // 把empty链表中的第一个元素删除
    *object_ptr = (unsigned int)(s_head->used_start_ptr);
    s_head->used_start_ptr = object_ptr;
        // 把新的数据块插入到used链表中
    ++(s_head->nr_objs);
        // 该页已分配的数据块数量加1

#ifdef SLAB_DEBUG
    show_slab_page(cache->cpu.page);
#endif

    if (s_head->nr_objs == cache->maxnum) {  // 检查这个页是否被分配完了
        list_del_init(&(cache->cpu.page->list)); // 把它从partial链表中移除
        list_add_tail(&(cache->cpu.page->list), &(cache->node.full));
            // 把它插入到full链表中去
#ifdef SLAB_DEBUG
        kernel_printf("%x\n", list_empty(&(cache->node.partial)));
#endif
        cache->cpu.page = 0; // 清空cache->cpu的内容，让它不指向任何页
        cache->cpu.freeobj = 0;
    }
    
    unlock(&(slab_lock));
    // 解锁
    return object;
}

/*
 * @param: object is the address of the allocated page
 * But not in the kernel format(smaller than 0x7fffffff)
 * @cache: The kmem_cache
 * */
void slab_free(struct kmem_cache *cache, void *object) {
    unsigned int* object_ptr;
    unsigned int* pi, *pi2;
    struct page *opage = pages + ((unsigned int)object >> PAGE_SHIFT);
    unsigned int *ptr;
    unsigned int succ = 1;
    unsigned int stop = 0;
    struct slab_head *s_head = (struct slab_head *)KMEM_ADDR(opage, pages);
        // 得到页内的slab_head开头指针
    
    lockup(&(slab_lock));
        // 给slab加锁

    if (!(s_head->nr_objs)) {  
        // 没有数据块可以被释放，报错
        kernel_printf("object = %x\n", object);
        kernel_printf("ERROR : slab_free error!\n");
        while (1)
            ;
    }

    // 找到与该数据块前一个数据块的配套的指针位置（就在这个数据块的前一个word
    object_ptr = (unsigned int*)((unsigned char*)(((unsigned int)object) | KERNEL_ENTRY) - sizeof(unsigned char*));
    pi = (unsigned int*)(&(s_head->used_start_ptr));
    // 在used单向链表中遍历，直至找到所要释放的块为止
    stop = 0;
    while (!stop) {
        if (ptr_equal(*pi, s_head)) { // 如果找不到这个要释放的数据块（到了链表的末端），则报错
            kernel_printf("ERROR : No objects to free!");
            succ = 0;
            stop = 1;
        }
        if (ptr_equal(*pi, object_ptr)) { // 如果找到了某个指针，它指向的是目标数据块的配套指针
            *pi = (unsigned int)(*(unsigned int*)(*pi));  // 把这个要释放的数据块从used链表里删除
            *object_ptr = (unsigned int)(s_head->empty_start_ptr); // 把它移到empty链表中
            s_head->empty_start_ptr = object_ptr;
            stop = 1; // 遍历结束
        }
        if (!stop) {
#ifdef SLAB_DEBUG2
            kernel_printf("[slab_free]Continue~\n");
#endif
            pi = (unsigned int*)(*pi);  // 还没找到则遍历这个链表
        }
    }
    if (!succ) return;
    --s_head->nr_objs;
#ifdef SLAB_DEBUG2
    kernel_printf("[slab_free]s_head->nr_objs=%x\n",s_head->nr_objs);
#endif
    // 1 - 判断释放前是否满
    if (s_head->nr_objs + 1 == cache->maxnum) {
        list_del_init(&(opage->list));
        list_add_tail(&(opage->list), &(cache->node.partial));
            // 从full移动到partial
        init_kmem_cpu(&(cache->cpu)); // 清空cache->cpu，保证安全
    } 
    // 2 - 判断释放后是否空
    if (!(s_head->nr_objs) && opage != cache->cpu.page) {
        list_del_init(&(opage->list)); // 从partial中删除
        if (opage != cache->cpu.page)
            __free_pages(opage, 0, 0); // 用buddy把此页释放
        init_kmem_cpu(&(cache->cpu)); // 清空cache->cpu，保证安全
    }
    // 3 - 判断释放时是否为正在用于分配的页，是则更新freeobj
    if (opage == cache->cpu.page) {
        cache->cpu.freeobj = (unsigned int*)((unsigned int)(s_head->empty_start_ptr) + sizeof(unsigned char*));
    }
    
    unlock(&(slab_lock));
}

// find the best-fit slab system for (size)
unsigned int get_slab(unsigned int size) {
    unsigned int itop = PAGE_SHIFT;
    unsigned int i;
    unsigned int bf_num = 2040;              // half page
    unsigned int bf_index = PAGE_SHIFT;      // record the best fit num & index

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
    //kernel_printf("[kmalloc start] size=%x\n", size);
    struct kmem_cache *cache;
    unsigned int bf_index;  // kmem_cache数据的索引
    void* out_ptr;

    if (!size)
        return 0;

    // 如果申请的内存空间大于slab所能分配的数据块的最大空间，则用buddy进行分配
    if (size > kmalloc_caches[PAGE_SHIFT - 1].objsize) {
        size += (1 << PAGE_SHIFT) - 1;  // 对size进行页对齐
        size &= ~((1 << PAGE_SHIFT) - 1);
        out_ptr = (void *)(KERNEL_ENTRY | (unsigned int)alloc_pages(siz2bplevel(size)));
            // 加上内核态地址的标志位（最高位置1），并用alloc_pages方法分配
        return out_ptr;
    }

    bf_index = get_slab(size);  // 得到对应的kmem_cache的数组下标
    if (bf_index >= PAGE_SHIFT) {
        kernel_printf("ERROR: No available slab\n");
        while (1)
            ;
    }

#ifdef SLAB_DEBUG2
    kernel_printf("  \n   Use slab~ size=%x\n", size);
#endif
    // 用slab进行分配
    out_ptr = (void *)(KERNEL_ENTRY | (unsigned int)slab_alloc(&(kmalloc_caches[bf_index]), 0));
    return out_ptr;
}

void kfree(void *obj) {
    struct page *page;

    obj = (void *)((unsigned int)obj & (~KERNEL_ENTRY)); // 去掉内核态地址标记
    page = pages + ((unsigned int)obj >> PAGE_SHIFT); // 得到对应的page数据结构指针
    if (!has_flag(page, _PAGE_SLAB)) {  // 判断该页是否被slab使用
        // 不是，则用buddy的释放方法释放
        return free_pages((void *)((unsigned int)obj & ~((1 << PAGE_SHIFT) - 1)), page->bplevel);
    }
    // 是，则用slab的释放方法释放
    return slab_free(page->virtual, obj);
}
