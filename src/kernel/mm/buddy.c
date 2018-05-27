#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/list.h>
#include <zjunix/lock.h>
#include <zjunix/utils.h>
//#define __BUDDY_DEBUG__
//#define __BUDDY_FREE_DEBUG__

unsigned int kernel_start_pfn, kernel_end_pfn;

struct page *pages;
struct buddy_sys buddy;

// void set_bplevel(struct page* bp, unsigned int bplevel)
//{
//	bp->bplevel = bplevel;
//}

void buddy_info() {
    unsigned int index;
    /*kernel_printf("Buddy-system :\n");
    kernel_printf("\tstart page-frame number : %x\n", buddy.buddy_start_pfn);
    kernel_printf("\tend page-frame number : %x\n", buddy.buddy_end_pfn);*/
    for (index = 0; index <= MAX_BUDDY_ORDER; ++index) {
        kernel_printf("\t(%x)# : %x frees     ", index, buddy.freelist[index].nr_free);
        if (index % 3 == 2) kernel_printf("\n");
    }
}

// this function is to init all memory with page struct
void init_pages(unsigned int start_pfn, unsigned int end_pfn) {
    unsigned int i;
    for (i = start_pfn; i < end_pfn; i++) {
        clean_flag(pages + i, -1);
        set_flag(pages + i, _PAGE_RESERVED);
        (pages + i)->reference = 1;
        (pages + i)->virtual = (void *)(-1);
        (pages + i)->bplevel = (-1);
        //(pages + i)->slabp = 0;  // initially, the free space is the whole page
        INIT_LIST_HEAD(&(pages[i].list));
    }
}

void init_buddy() {
    unsigned int bpsize = sizeof(struct page);
    unsigned char *bp_base;
    unsigned int i;
    
#ifdef __BUDDY_DEBUG__
    bootmm_message();
#endif
    bp_base = bootmm_alloc_pages(bpsize * bmm.max_pfn, _MM_KERNEL, 1 << PAGE_SHIFT);
    /*
        unsigned char *bootmm_alloc_pages(unsigned int size, unsigned int type, unsigned int align);
        Its return value[31] is not 1.
    */
    
    // bootmm is used here in buddy
    if (!bp_base) {
        // the remaining memory must be large enough to allocate the whole group
        // of buddy page struct
        kernel_printf("\nERROR : bootmm_alloc_pages failed!\nInit buddy system failed!\n");
        while (1)
            ;
    }
    pages = (struct page *)((unsigned int)bp_base | 0x80000000);

    init_pages(0, bmm.max_pfn);

    kernel_start_pfn = 0;
    kernel_end_pfn = 0;
    // Take the final segment of bootmm
    for (i = 0; i < bmm.cnt_infos; ++i) {
        if (bmm.info[i].end > kernel_end_pfn)
            kernel_end_pfn = bmm.info[i].end;
    }
    kernel_end_pfn >>= PAGE_SHIFT;
    // Start allocating after the final segment of bootmm
    // #define MAX_BUDDY_ORDER 4
    // alignment
    buddy.buddy_start_pfn = (kernel_end_pfn + (1 << MAX_BUDDY_ORDER) - 1) &
                            ~((1 << MAX_BUDDY_ORDER) - 1);              // the pages that bootmm using cannot be merged into buddy_sys
    buddy.buddy_end_pfn = bmm.max_pfn & ~((1 << MAX_BUDDY_ORDER) - 1);  // remain 2 pages for I/O

#ifdef __BUDDY_DEBUG__
    kernel_printf("  pages = (addr)%x\n", pages);
    kernel_printf("  start_pfn = %x\n", buddy.buddy_start_pfn);    
    kernel_printf("  end_pfn = %x\n", buddy.buddy_end_pfn);
#endif

    // init freelists of all bplevels
    for (i = 0; i < MAX_BUDDY_ORDER + 1; i++) {
        buddy.freelist[i].nr_free = 0;
        INIT_LIST_HEAD(&(buddy.freelist[i].free_head));
    }
    buddy.start_page = pages + buddy.buddy_start_pfn;
    // buddy.start_page is a *page (pointer)
    init_lock(&(buddy.lock));
#ifdef __BUDDY_DEBUG__
    buddy_info();
#endif

    for (i = buddy.buddy_start_pfn; i < buddy.buddy_end_pfn; ++i) {
        __free_pages(pages + i, 0, 0);
    }
#ifdef __BUDDY_DEBUG__
    buddy_info();
#endif
}

/* bplevel: 粒度. 0,1,2,3,4
 * __free_pages() passes all the free pages in the bootmm to buddy
 * If it is not in the bootmm, then also free it and combine the pages
 * @param pbpage: The start page
 * @bplevel: up
 * 
 * */
void __free_pages(struct page *pbpage, unsigned int bplevel, unsigned int mark) {
    /* page_idx -> the current page
     * bgroup_idx -> the buddy group that current page is in (going to be combined)
     */
    unsigned int page_idx, bgroup_idx;
    unsigned int combined_idx, tmp;
    struct page *bgroup_page;

    // dec_ref(pbpage, 1);
    // if(pbpage->reference)
    //	return;
    lockup(&buddy.lock);
    /*
    ++buddy.freelist[bplevel].nr_free;
    list_add_tail(&(bgroup_page->list), &(buddy.freelist[bplevel]));
    */
    page_idx = pbpage - buddy.start_page;
    // complier do the sizeof(struct) division operation, and now page_idx is the index
    clean_flag(pbpage, _PAGE_ALLOCED);
    clean_flag(pbpage, _PAGE_BUDDY);
    clean_flag(pbpage, _PAGE_SLAB);
    set_flag(pbpage, _PAGE_RESERVED);
    // 几个指针之间的大小关系：
    // buddy.start_page -- [+page_idx] -- pbpage -- [+bgroup_idx-page_idx] -- bgroup_page
    // 把相邻的伙伴块合并
    // (Example: pidx = 19, bplevel = 0, then bgidx = 18)
    while (bplevel < MAX_BUDDY_ORDER) {
        bgroup_idx = page_idx ^ (1 << bplevel);
        bgroup_page = pbpage + (bgroup_idx - page_idx);
        // kernel_printf("group%x %x\n", (page_idx), bgroup_idx);
#ifdef __BUDDY_FREE_DEBUG__
        if (mark) {
            kernel_printf("  page_idx = %x, bgroup_idx = %x, bplevel = %x, ", page_idx, bgroup_idx, bplevel);
            kernel_printf("  bgroup_page->bplevel=%x\n", bgroup_page->bplevel);
            kernel_printf("  bgroup_page->flag=%x\n", bgroup_page->flag);
        }
#endif
        if (!_is_same_bplevel(bgroup_page, bplevel) || has_flag(bgroup_page, _PAGE_ALLOCED)) {
            // kernel_printf("%x %x\n", bgroup_page->bplevel, bplevel);
            break;
        }
        list_del_init(&bgroup_page->list);  // 把page从小伙伴块的空闲链表中删除
        // delete and init
        --buddy.freelist[bplevel].nr_free;  // 小伙伴块的空闲链表中，结点数减1
        set_bplevel(bgroup_page, -1);       // 把page加入大伙伴块的空闲链表中
        combined_idx = bgroup_idx & page_idx; // 合并后，伙伴块的索引
        pbpage += (combined_idx - page_idx);   // 下一个需要合并的伙伴块对应page管理结构的地址
        page_idx = combined_idx;   
        ++bplevel;  // 粒度加1，对更大的伙伴块进行操作
    }
    set_bplevel(pbpage, bplevel);
    list_add(&(pbpage->list), &(buddy.freelist[bplevel].free_head));
    ++buddy.freelist[bplevel].nr_free;
    // kernel_printf("v%x__addto__%x\n", &(pbpage->list),
    // &(buddy.freelist[bplevel].free_head));
    unlock(&buddy.lock);
}

// It returns a (struct page*), but not an address pointer
struct page *__alloc_pages(unsigned int bplevel) {
    unsigned int current_order, size;
    struct page *page, *buddy_page;
    struct freelist *free;

    lockup(&buddy.lock);  // 加锁
#ifdef __BUDDY_DEBUG__
        kernel_printf("  bplevel = %x ", bplevel);
#endif
    for (current_order = bplevel; current_order <= MAX_BUDDY_ORDER; ++current_order) {
        free = buddy.freelist + current_order;
        // 对应伙伴块的空闲链表的链表头
        if (!list_empty(&(free->free_head))) { // 找到一个伙伴块
            page = container_of(free->free_head.next, struct page, list);
                // 求出page结构的链表结点所在的page中
            // #define container_of(ptr, type, member) ((type*)((char*)ptr - (char*)&(((type*)0)->member)))
            list_del_init(&(page->list));   // 从大伙伴块空闲链表中删除
            set_bplevel(page, bplevel);     // 设置该伙伴块的大小
            set_flag(page, _PAGE_ALLOCED);  // 标记为已分配 
            set_flag(page, _PAGE_BUDDY);    // 标记为已被buddy分配
            // set_ref(page, 1);
            --(free->nr_free);
            size = 1 << current_order;
            while (current_order > bplevel) {  // 把伙伴块切割成小伙伴块直至能满足分配的伙伴块的大小要求
                --free; // free points to the smaller level of list
                --current_order;
                size >>= 1;
                buddy_page = page + size;   
                // 把大buddy分割为两个小buddy
                // 把第二个buddy加入到小伙伴块空闲链表中
                list_add(&(buddy_page->list), &(free->free_head));
                ++(free->nr_free);  // 第二个buddy块的
                set_bplevel(buddy_page, current_order); 
            }
            unlock(&buddy.lock); // 返回前记得解锁
            return page;
        }
    }
    unlock(&buddy.lock);
    return 0;
}

void *alloc_pages(unsigned int bplevel) {
    struct page *page = __alloc_pages(bplevel);

    if (!page)
        return 0;

    return (void *)((page - pages) << PAGE_SHIFT);
    // This return value[31] is not one
    // page-pages is the physical page number 
    // (void *)((page - pages) << PAGE_SHIFT) is the real address
}

// 计算内容空间大小所对应的伙伴块级别（粒度）
unsigned int siz2bplevel(unsigned int size) {
    unsigned int ans = 0, base = 1;
    size >>= PAGE_SHIFT;
    while (base < size) {
        ans++;
        base<<=1;
    }
    return ans;
}

void free_pages(void *addr, unsigned int bplevel) {
    __free_pages(pages + ((unsigned int)addr >> PAGE_SHIFT), bplevel, 1);
    // addr >> PAGE_SHIFT is the page number
}
