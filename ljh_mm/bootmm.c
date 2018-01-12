#include <arch.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/utils.h>
//#define ___BOOTMM__

struct bootmm bmm;
unsigned int firstusercode_start;
unsigned int firstusercode_len;

// const value for ENUM of mem_type
char *mem_msg[] = {"Kernel code/data", "Mm Bitmap", "Vga Buffer", "Kernel page directory", "Kernel page table", "Dynamic", "Reserved"};


void bootmm_message() {
    kernel_printf("  phymm = %x    ", bmm.phymm);
    kernel_printf("  max_pfn = %x    ", bmm.max_pfn);
    kernel_printf("  s_map = %x    ", bmm.s_map);
    kernel_printf("  e_map = %x\n", bmm.e_map);
    kernel_printf("  last_alloc_end = %x    ", bmm.last_alloc_end);
    kernel_printf("  cnt_infos = %x\n", bmm.cnt_infos);
    unsigned int i;
    for(i=0; i<bmm.cnt_infos; i++) {
        kernel_printf("  %x: %x %x %x\n", i,
            bmm.info[i].start,
            bmm.info[i].end,
            bmm.info[i].type);
    }
}

// set the content of struct bootmm_info
void set_mminfo(struct bootmm_info *info, unsigned int start, unsigned int end, unsigned int type) {
    info->start = start;
    info->end = end;
    info->type = type;
}
unsigned char bootmmmap[MACHINE_MMSIZE >> PAGE_SHIFT];
/* Takes 128KB spaces of memory */



/* 
* Insert a mminfo at the end of the array
* @param mm 	: the bootmm struct pointer
* @param start	: The start page frame number of inserted page
* @param end    : The end page frame number of inserted page
* @param type   : The type of the inserted page
*/

unsigned int pushback_mminfo(struct bootmm *mm, unsigned int start, unsigned int end, unsigned int type) {
    unsigned int i;
    if (mm->cnt_infos >= MAX_INFO)
        return 0;
    set_mminfo(mm->info + mm->cnt_infos, start, end, type);
    mm->cnt_infos++;
    return 1;
}

/*
* Empty the index-th info element
    and increment "mm->cnt_infos".
* @param mm 	: the bootmm struct pointer
* @param index	: The index of the bootmm info array
*/

unsigned int move_mminfo(struct bootmm *mm, unsigned int index) {
    unsigned int i, start, end, type;
    if (mm->cnt_infos >= MAX_INFO)
        return 0;
    mm->cnt_infos ++;
    for (i = mm->cnt_infos; i > index; i--) {
        start = mm->info[i-1].start;
        end = mm->info[i-1].end;
        type = mm->info[i-1].type;
        set_mminfo(mm->info + i, start, end, type);
    }
    return 1;
}

/* 
* Used by insert_mminfo
* @param mm 	: the bootmm struct pointer
* @param start	: The start page frame number of inserted page
* @param end    : The end page frame number of inserted page
* @param type   : The type of the inserted page
*/

unsigned int insert_mminfo_no_lapped(struct bootmm *mm, unsigned int start, unsigned int end, unsigned int type) {
    unsigned int i;
#ifdef ___BOOTMM__
    kernel_printf("  %x %x %x\n", start, end, type);
#endif
    if (mm->cnt_infos == 0) {
        pushback_mminfo(mm, start, end, type);
        return 1;
    }
    if (mm->cnt_infos >= MAX_INFO) 
        return 0;
    for (i = 0; i < mm->cnt_infos; i++) {
        if (i != mm->cnt_infos - 1) {
            if (i > 0) {
                if (start > mm->info[i].end && end < mm->info[i+1].start) {
                    // Make the i+1 th info[] to empty
                    move_mminfo(mm, i+1);
                    set_mminfo(mm->info + (i+1), start, end, type);
                    return 1;
                }
            }
            else { // The first segment
                if (end < mm->info[i+1].start) {
                    move_mminfo(mm, 0);
                    set_mminfo(mm->info, start, end, type);
                    return 1;
                }
            }
        } 
        else if (start > mm->info[i].start) {
            pushback_mminfo(mm, start, end, type);
            return 1;
        }
        if (mm->info[i].start > end) return 0;
    }
    return 0;
}

/* 
* return value list:
*		0 -> insert_mminfo failed
*		1 -> insert non-related mm_segment      
            [-***-]             [-new-] [-***-]
*		2 -> insert forward-connecting segment  
            [-old-]             [-new-] [-old_diff_type-]   -> [-old+new-][-old_diff_type-]
*		4 -> insert following-connecting segment
            [-***-]             [-new-] [-old-]             -> [-old_diff_type-] [-new+old-]
*		6 -> insert forward-connecting segment to after-last position
            [-old-]             [-new-] end                 -> [-old+new-]
*		7 -> insert bridge-connecting segment(remove_mminfo is called
            for deleting
            [-old-]             [-new-] [-old-]             -> [-old+new+old-]

* @param mm 	: the bootmm struct pointer
* @param start	: The start page frame number of inserted page
* @param end    : The end page frame number of inserted page
* @param type   : The type of the inserted page
*/
unsigned int insert_mminfo(struct bootmm *mm, unsigned int start, unsigned int end, unsigned int type) {
    unsigned int i;
    for (i = 0; i < mm->cnt_infos; i++) {
        if (mm->info[i].type != type)
            continue;  // ignore the type-mismatching items to find one likely
                       // mergable
        if (mm->info[i].end == start - 1) { // [-old-][-new-]
                // Three conditions:
                // 7. The same type old segment
                // 2. The different type old segment
                // 6. The end
            if ((i + 1) < mm->cnt_infos) {
                // current info is still not the last segment
                if (mm->info[i + 1].type != type) {  // [-old-][-new-][-old_diff_type-]
                    mm->info[i].end = end;
                    return 2; 
                } else { // [-old-][-new-][-old2_same_type-]
                    if (mm->info[i + 1].start - 1 == end) { 
                        mm->info[i].end = mm->info[i + 1].end;
                        remove_mminfo(mm, i + 1);
                        return 7;
                    }
                }
            } else {  // current info is the last segment
                // [-old-][-new-]
                // extend the last segment to contain the new-in mm
                mm->info[i].end = end;
                return 6;
            }
        }
        if (mm->info[i].start - 1 == end) {  // [-new-][-old-]
                // Connect to the following segment
            kernel_printf("type of %d : %x, type: %x", i, mm->info[i].type, type);
            mm->info[i].start = start;
            return 4;
        }
    }

    if (mm->cnt_infos >= MAX_INFO)
        return 0;  
        // cannot allocate segments
    insert_mminfo_no_lapped(mm, start, end, type);
    return 1;  // individual segment(non-connecting to any other)
}

/* get one sequential memory area to be split into two parts
 * (set the former one.end = split_start-1)
 * (set the latter one.start = split_start)
 */
unsigned int split_mminfo(struct bootmm *mm, unsigned int index, unsigned int split_start) {
    unsigned int start, end;
    unsigned int tmp;

    start = mm->info[index].start;
    end = mm->info[index].end;
    split_start &= PAGE_ALIGN;  // PAGE_ALIGN = 0xffff_f000
    split_start >>= PAGE_SHIFT;     
        /*
            split_start should be the frame number instead of byte addresses
        */

    if ((split_start <= start) || (split_start >= end))
        return 0;  // split_start out of range

    if (mm->cnt_infos == MAX_INFO)
        return 0;  // number of segments are reaching max, cannot alloc anymore
                   // segments
    // using copy and move, to get a mirror segment of mm->info[index]
    for (tmp = mm->cnt_infos - 1; tmp >= index; --tmp) {
        mm->info[tmp + 1] = mm->info[tmp];
    }
    mm->info[index].end = split_start - 1;
    mm->info[index + 1].start = split_start;
    mm->cnt_infos++;
    return 1;
}

// remove the mm->info[index]
void remove_mminfo(struct bootmm *mm, unsigned int index) {
    unsigned int i;
    if (index >= mm->cnt_infos)
        return;

    if (index + 1 < mm->cnt_infos) {
        for (i = (index + 1); i < mm->cnt_infos; i++) {
            mm->info[i - 1] = mm->info[i];
        }
    }
    mm->cnt_infos--;
}

void init_bootmm() {
    unsigned int index;
    unsigned char *t_map;
    unsigned int end;
    unsigned int k;
    end = 16 * 1024 * 1024; 
        // The final byte of the boot memory
    kernel_memset(&bmm, 0, sizeof(bmm));
    bmm.phymm = get_phymm_size();  // 512*1024*1024, 512M
    bmm.max_pfn = bmm.phymm >> PAGE_SHIFT;  // 2^17 frames
    bmm.s_map = bootmmmap;  
        // unsigned char bootmmmap[MACHINE_MMSIZE >> PAGE_SHIFT];
        // The first map of boot memory
        // A map is corresponded to a [[page frame]]
    bmm.e_map = bootmmmap + sizeof(bootmmmap); 
        // The end map of boot memory
        // s_map -> e_map contains only the boot memory
    bmm.cnt_infos = 0;
        // get number of infos stored in bootmm now
    kernel_memset(bmm.s_map, PAGE_FREE, sizeof(bootmmmap));
        // Set all the page frames to free
#ifdef ___BOOTMM__
    kernel_printf("OK1!\n");
#endif
    k = insert_mminfo(&bmm, 0, (unsigned int)((end - 1)), _MM_KERNEL);
#ifdef ___BOOTMM__
    kernel_printf("OK2! k=%x\n", k);
#endif
        // Set all the frames before end to kernal
    bmm.last_alloc_end = (((unsigned int)(end) >> PAGE_SHIFT) - 1);
        // The last allocation address
    for (index = 0; index<end>> PAGE_SHIFT; index++) {
        bmm.s_map[index] = PAGE_USED;
    }
}

/*
 * set value of page-bitmap-indicator
 * @param s_pfn	: page frame start node
 * @param cnt	: the number of pages to be set
 * @param value	: the value to be set
 */
void set_maps(unsigned int s_pfn, unsigned int cnt, unsigned char value) {
    while (cnt) {
        bmm.s_map[s_pfn] = (unsigned char)value;
        --cnt;
        ++s_pfn;
    }
}

/*
 * This function is to find sequential page_cnt number of pages to allocate
 * @param page_cnt : the number of pages requested
 * @param s_pfn    : the allocating begin page frame node
 * @param e_pfn	   : the allocating end page frame node
 * return value  = 0 :: allocate failed, else return index(page start)
 * return value != 0 :: The start address of the page(not in kernel mode)
 */

unsigned char *find_pages(unsigned int page_cnt, unsigned int s_pfn, unsigned int e_pfn, unsigned int align_pfn) {
    unsigned int index, tmp;
    unsigned int cnt;

    s_pfn += (align_pfn - 1); // Align the s_pfn using align_pfn
    s_pfn &= ~(align_pfn - 1);

    for (index = s_pfn; index < e_pfn;) {
        if (bmm.s_map[index] == PAGE_USED) {
            ++index;
            continue;
        }

        cnt = page_cnt;
        tmp = index;
        while (cnt) {
            if (tmp >= e_pfn)
                return 0;
            // reaching end, but allocate request still cannot be satisfied

            if (bmm.s_map[tmp] == PAGE_FREE) {
                tmp++;  // find next possible free page
                cnt--;
            }
            if (bmm.s_map[tmp] == PAGE_USED) {
                break;
            }
        }
        if (cnt == 0) {  // cnt = 0 indicates that the specified page-sequence found
            bmm.last_alloc_end = tmp - 1;
            set_maps(index, page_cnt, PAGE_USED);
            return (unsigned char *)(index << PAGE_SHIFT);
        } else {
            index = tmp + align_pfn;  // there will be no possible memory space
                                      // to be allocated before tmp
        }
    }
    return 0;
}

unsigned char *bootmm_alloc_pages(unsigned int size, unsigned int type, unsigned int align) {
    unsigned int size_inpages;
    unsigned char *res;

    size += ((1 << PAGE_SHIFT) - 1);
    size &= PAGE_ALIGN;
    size_inpages = size >> PAGE_SHIFT;

    // in normal case, going forward is most likely to find suitable area
    res = find_pages(size_inpages, bmm.last_alloc_end + 1, bmm.max_pfn, align >> PAGE_SHIFT);
    if (res) {
        insert_mminfo(&bmm, (unsigned int)res, (unsigned int)res + size - 1, type);
        return res;
    }

    // when system request a lot of operations in booting, then some free area
    // will appear in the front part
    res = find_pages(size_inpages, 0, bmm.last_alloc_end, align >> PAGE_SHIFT);
    if (res) {
        insert_mminfo(&bmm, (unsigned int)res, (unsigned int)res + size - 1, type);
        return res;
    }
    return 0;  // not found, return NULL
}

void bootmap_info(unsigned char *msg) {
    unsigned int index;
    kernel_printf("%s :\n", msg);
    for (index = 0; index < bmm.cnt_infos; ++index) {
        kernel_printf("\t%x-%x : %s\n", bmm.info[index].start, bmm.info[index].end, mem_msg[bmm.info[index].type]);
    }
}
