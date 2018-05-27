#include <zjunix/ljh/mmu.h>
#include <zjunix/syscall.h>
#include <zjunix/slab.h>
#include <exc.h>
#include <assert.h>

#define PAGE_SHIFT_MMU 8
#define PAGE_BITS 12
#define VPN_mask ((1 << PAGE_SHIFT_MMU) - 1)
#define VPN2_mask ((1 << (PAGE_SHIFT_MMU + 1)) - 1)
#define ASID_mask ((1 << PAGE_SHIFT_MMU) - 1)

#define NULL 0
#define VAddr2VPN(n) (n >> PAGE_BITS)
#define VAddr2VPN2(n) (n >> (PAGE_BITS + 1))
#define VAddr2PGE(n) (n >> (PAGE_BITS + 1 + PAGE_SHIFT_MMU))
#define VAddr2PDE(n) ((n >> (PAGE_BITS + 1)) & VPN_mask)
#define VAddr2PDE4(n) ((n >> (PAGE_BITS - 1)) & (VPN_mask << 2))
#define VPN22PGE(n) (n >> PAGE_SHIFT_MMU)
#define VPN22PDE(n) (n & VPN_mask)
#define VPN2PGE(n) (n >> (PAGE_SHIFT_MMU))
#define VPN2PDE(n) ((n >> 1) & VPN_mask)
#define VPN13_sel(n) (n & (1 << (PAGE_BITS + 1)) > 0 ? 1 : 0)

#pragma GCC push_options
#pragma GCC optimize("O0")

/* The PGE Base address of every process */
unsigned int *(*PGT_BaseAddrs[256])[1024];  

 /* 2048 entries in the PGT */

/*
    Insert the TLB entry using the page table(2-level)
*/

void init_TLB() {  // Clear TLB
    asm volatile(
        "mtc0 $zero, $2\n\t"
        "mtc0 $zero, $3\n\t"
        "mtc0 $zero, $5\n\t"
        "mtc0 $zero, $10\n\t"

        "move $v0, $zero\n\t"
        "li $v1, 32\n"

    "init_pgtable_L1:\n\t"
        "mtc0 $v0, $0\n\t"
        "addi $v0, $v0, 1\n\t"
        "bne $v0, $v1, init_pgtable_L1\n\t"
        "tlbwi\n\t"
        "nop");
}

void tlbwi(unsigned int index, unsigned int cp0EntryHi, unsigned int cp0EntryLo0, unsigned int cp0EntryLo1) {
    unsigned int cp0regs[4];
    unsigned int* cp0regs_ptr;
    cp0regs[0] = cp0EntryHi;
    cp0regs[1] = cp0EntryLo0;
    cp0regs[2] = cp0EntryLo1;
    cp0regs[3] = index;
    cp0regs_ptr = (unsigned int*)cp0regs;
    asm volatile(
        "lw $k0, 0(%0) \n\t"    // cp0EntryHi
        "lw $k1, 4(%0) \n\t"    // cp0EntryLo0
        "mtc0 $k0, $10 \n\t"
        "mtc0 $k1, $2  \n\t"
        "lw $k0, 8(%0) \n\t"    // cp0EntryLo1
        "lw $k1, 12(%0) \n\t"   // index
        "mtc0 $k0, $3  \n\t"
        "mtc0 $k1, $0  \n\t"
        "mtc0 $zero, $5 \n\t"   // pageMask
        "tlbwi         \n\t"
        "nop \n\t"
        "nop \n\t"
        : "=r"(cp0regs_ptr)
    );
    
}

// Use an array to store data, 
// in order to avoid potential bugs
void read_TLB(unsigned int index, 
    unsigned int* cp0EntryHi_ptr,
    unsigned int* cp0EntryLo0_ptr,
    unsigned int* cp0EntryLo1_ptr) {
    unsigned int cp0Entry[10];
    unsigned int* cp0Entry_ptr;
    cp0Entry_ptr = (unsigned int*)cp0Entry;
    cp0Entry[3] = index;
    asm volatile(
        "lw $k0, 12(%0) \n\t"
        "andi $k0, %0, 31 \n\t"
        "mtc0 $k0, $0 \n\t"
        "nop \n\t"
        "nop \n\t"
        "tlbr \n\t"
        "nop \n\t"
        "nop \n\t"
        "mfc0 $k0, $10 \n\t"    // cp0EntryHi
        "sw $k0, 0(%0) \n\t"
        "mfc0 $k0, $2 \n\t"     // cp0EntryLo0
        "mfc0 $k1, $3 \n\t"     // cp0EntryLo1
        "sw $k0, 4(%0) \n\t"
        "sw $k1, 8(%0) \n\t"
        "nop \n\t"
        "nop \n\t"
        : "=r"(cp0Entry_ptr)
    );
    *cp0EntryHi_ptr = cp0Entry[0];
    *cp0EntryLo0_ptr = cp0Entry[1];
    *cp0EntryLo1_ptr = cp0Entry[2];
}

void TLB_info() {
    kernel_printf(" [TLB info]");
    unsigned int cp0EntryHi, cp0EntryLo0, cp0EntryLo1, cp0pageMask;
    unsigned int i = 0;
    for (i = 0; i < 32; i++) {  // F(i,0,32)
        read_TLB(i, &cp0EntryHi, &cp0EntryLo0, &cp0EntryLo1);
        //if (cp0EntryHi)
            kernel_printf(" [%x]cp0EntryHi=%x, cp0EntryLo0=%x, cp0EntryLo1=%x\n", 
                i, cp0EntryHi, cp0EntryLo0, cp0EntryLo1);
    }
}

void pgtable_info() {
    unsigned int i, j, k;
    unsigned int exist=0;
    for (i=0; i<256; i++) {
        if (PGT_BaseAddrs[i] != 0) {
            exist = 1;
            kernel_printf("  [%x]PGT_BaseAddrs=%x\n", i, PGT_BaseAddrs[i]);
            for (j=0; j<2048; j++) {
                if ((*(PGT_BaseAddrs[i]))[j] != 0) {
                    kernel_printf("    [%x]PDT_BaseAddrs=%x\n", j, PGT_BaseAddrs[i][0][j]);
                    for (k=0; k<256; k++) {
                        if (PGT_BaseAddrs[i][0][j][k<<2])
                            kernel_printf("      [%x]hi=%x, lo0=%x, lo1=%x\n",
                                k,
                                PGT_BaseAddrs[i][0][j][k<<2],
                                PGT_BaseAddrs[i][0][j][(k<<2)+2],
                                PGT_BaseAddrs[i][0][j][(k<<2)+3]);
                    }
                }
            }
        }
    }
    if (!exist) kernel_printf("   The page table is empty!\n");
}

void insert_new_tlb_entry(unsigned int ASID, unsigned int VPN2, unsigned int we0, unsigned int we1) {
    unsigned int cp0EntryHi, cp0EntryLo0, cp0EntryLo1, cp0pageMask;
    unsigned int PGT_entry = VPN2 >> 8;
    unsigned int PDT_entry = VPN2 & 255;
    unsigned int PDT_entry_4 = PDT_entry << 2;
    unsigned int* PDT_BaseAddr;
    unsigned int cp0regs[5];
    assert(we0 <= 1 & we1 <= 1 , "[insert_new_tlb_entry]we>1;");

    if (PGT_BaseAddrs[ASID] == NULL) create_PGT(ASID);  // 如果不存在第一级页表，则创建它
    PDT_BaseAddr = PGT_BaseAddrs[ASID][0][PGT_entry];   // 找到第二级页表的地址
    if (PDT_BaseAddr == NULL) create_PDT(ASID, PGT_entry); // 如果不存在第二级页表，则创建它
    cp0EntryHi = PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4];  // 找到这个表项的cp0EntryHi对应值
    if (cp0EntryHi == NULL) {
        create_PDT_entry(ASID, VPN2, 0);  // 创建这个页表项
        cp0EntryHi = PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4];
    }
    cp0pageMask = PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4+1];  // 读取页表项中cp0PageMask的值
    if (we0 == 1)
        PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4+2] |= 4;
    if (we1 == 1)
        PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4+3] |= 4;
    cp0EntryLo0 = PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4+2]; // 读取页表项中cp0EntryLo0的值
    cp0EntryLo1 = PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4+3]; // 读取页表项中cp0EntryLo1的值
    cp0regs[0] = cp0EntryHi;
    cp0regs[1] = cp0EntryLo0;
    cp0regs[2] = cp0EntryLo1;

    asm volatile(
        "mtc0 $zero, $0 \n\t"       /* Index = 0 */
        "ori  $k0, $zero, 32 \n\t"
    "Next_table_entry: \n\t" 
        "tlbr \n\t"                 /* Read TLB */
        "nop  \n\t"
        "nop  \n\t"
        "mfc0 $k1, $0 \n\t"         /* $k1 = index */
        "addiu $k1, $k1, 1 \n\t"    /* $k1 ++ */
        "mtc0 $k1, $0 \n\t"         /* Index = $k1 */
        "beq  $k1, $k0, No_empty_TLB_entry \n\t"  /* Index = 32, Need to replace an entry */
        "nop  \n\t"
        "mfc0 $k1, $10 \n\t"        /* $k0 = EntryHi */
        "nop  \n\t"
        "nop  \n\t"
        "bne  $k1, $zero, Next_table_entry \n\t"  /* EntryHi != 0, The entry is not full */
        "nop  \n\t"
        "mfc0 $k1, $0 \n\t"
        "addu $k1, $k1, -1 \n\t"
        "mtc0 $k1, $0 \n\t"         /* Index = Index - 1 */
        "lw   $k0, 0(%0) \n\t"
        "mtc0 $k0, $10 \n\t"         /* EntryHi = cp0EntryHi */
        "mtc0 $zero, $5 \n\t"       /* PageMask = 0 */
        "lw   $k0, 4(%0) \n\t"
        "mtc0 $k0, $2 \n\t"          /* EntryLo0 = cp0EntryLo0 */
        "lw   $k0, 8(%0) \n\t"
        "mtc0 $k0, $3 \n\t"          /* EntryLo1 = cp0EntryLo1 */
        "nop  \n\t"
        "nop  \n\t"
        "tlbwi \n\t"
        "j    end_write_TLB \n\t"
        "nop  \n\t"

    "No_empty_TLB_entry: \n\t"      
        "lw   $k0, 0(%0) \n\t"
        "mtc0 $k0, $10 \n\t"         /* EntryHi = cp0EntryHi */
        "mtc0 $zero, $5 \n\t"       /* PageMask = 0 */
        "lw   $k0, 4(%0) \n\t"
        "lw   $k0, 8(%0) \n\t"
        "mtc0 $k0, $2 \n\t"         /* EntryLo0 = cp0EntryLo0 */
        "mtc0 $k1, $3 \n\t"         /* EntryLo1 = cp0EntryLo1 */
        "nop  \n\t"
        "nop  \n\t"
        "tlbwr \n\t"                /* Repalce the TLB entry randomly because we cannot get the reference information */
    "end_write_TLB:"
        :: "r"(cp0regs));
}

/* 
    3 types of TLB exceptions: TLB refill, TLB invalid, TLB modified 
    1) TLB refill: No entry matched -> TLBL, TLBS
    2) TLB invalid: The matched entry is invalid -> TLBL, TLBS
    3) TLB modified: The page is modified but D=0 -> TLBM
*/

/*
    Cause[6:2]: ExcCode
    Refill:
    Cause = TLBL(2): TLB exception (load or instruction fetch -> read)
    Cause = TLBS(3): TLB exception (store -> write)

    8 - BadVAddr = failing address
    4 - Context[BadVPN2] = VA[31:13] of the failing address
    10 - EntryHi[VPN2] = VA[31:13] of the FA
            [ASID] = ASID
    2,3 - EntryLo0 and EntryLo1 unpredictable

    Process: 
    1. Find the page table entry
    2. Use insert_new_tlb_entry to insert
    3. Return
*/
void TLBMiss_refill_exception(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int BadVPN2 = 0, BadVAddr = 0;
    unsigned int ASID = 0;
    unsigned int ExcCode = (cause >> 2) & 31;
    unsigned int we0 = 0, we1 = 0;
    asm volatile (
        "mfc0 $k0, $10  \n\t"     /* The EntryHi register */
        "nop  \n\t"
        "andi %0, $k0, 0xff  \n\t"
        "mfc0 $k1, $4  \n\t"      /* The context register */
        "nop  \n\t"
        "sll  $k1, $k1, 10  \n\t"
        "srl  %1, $k1, 14  \n\t"
        "mfc0 $k0, $8  \n\t"      /* The BadVAddr register */
        "nop  \n\t"
        "addu %2, $k0, 0"
        : "=r"(ASID), "=r"(BadVPN2), "=r"(BadVAddr)
    );
    if (ExcCode == 3) {
        if (BadVAddr & 4096) we1 = 1;
        else we0 = 1;
    }
    insert_new_tlb_entry(ASID, BadVPN2, we0, we1);
}

/*
    Cause[6:2]: ExcCode
    Invalid
    Cause = TLBL(2): TLB exception (load or instruction fetch -> read)
    Cause = TLBS(3): TLB exception (store -> write)

    BadVAddr = failing address
    Context[BadVPN2] = VA[31:13] of the failing address
    EntryHi[VPN2] = VA[31:13] of the FA
            [ASID] = ASID
    EntryLo0 and EntryLo1 unpredictable
*/

void TLBMiss_Invalid_exception(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int ExcCode = (cause & (31<<2)) >> 2;
    unsigned int ASID = 12, BadVPN2 = 19;
    asm volatile (
        "mfc0 $k0, $10  \n\t"   /* The EntryHi register */
        "andi %0, $k0, 0xff \n\t"
        "mfc0 $k1, $4  \n\t"    /* The context register */
        "sll  $k1, $k1, 10  \n\t"
        "srl  %1, $k1, 14  \n\t"
        : "=r"(ASID), "=r"(BadVPN2)  // 得到发生错误的进程号ASID，以及虚拟页号BadVPN2
    );

    if (ExcCode == 2) { /* TLBL，直接报错，显示ASID与BadVPN2 */  
        kernel_printf("Error: TLBL - The TLB entry is invalid ! ASID = %x, BadVPN2 = %x", ASID, BadVPN2);
        while (1) ;
    }
    else if (ExcCode == 3) { /* TLBS，直接报错，显示ASID与BadVPN2 */
        kernel_printf("Error: TLBS - The TLB entry is invalid ! ASID = %x, BadVPN2 = %x", ASID, BadVPN2);
        while (1) ;
    }
}


/*
    Cause = 1: TLB modification exception
    BadVAddr = The failing address
    Context[22:4 ] = The VA[31:13]
    EntryHi[31:13] = The VA[31:13]
    EntryHi[7:0] = ASID
*/
void TLBModified_exception(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int Regs[3];
    unsigned int* Regs_ptr = Regs;
    unsigned int BadVPN2;
    unsigned int ASID;
    unsigned int BadVAddr;
    asm volatile (
        "mfc0 $k0, $10  \n\t"   /* The EntryHi register */
        "andi $k0, $k0, 0xff  \n\t"
        "sw   $k0, 0(%0) \n\t"
        "mfc0 $k1, $4  \n\t"    /* The context register */
        "sll  $k1, $k1, 10  \n\t"
        "srl  $k1, $k1, 14  \n\t"
        "sw   $k1, 4(%0) \n\t"
        "mfc0 $k0, $8  \n\t"      /* The BadVAddr register */
        "sw   $k0, 8(%0) \n\t"
        "tlbp \n\t"  // 找到引发异常的TLB项
        "nop \n\t"
        "nop \n\t"
        "tlbr \n\t"  // 把引发异常的TLB项从TLB从读出来，放到EntryHi, PageMask, EntryLo0, EntryLo1寄存器中
        "nop \n\t"
        "nop \n\t"
        "mfc0 $k0, $2 \n\t"  // 把EntryLo0的值读到$k0寄存器里
        "mfc0 $k1, $3 \n\t"  // 把EntryLo1的值读到$k1寄存器里
        "ori  $k0, $k0, 4 \n\t" // 把dirty位置1
        "ori  $k1, $k1, 4 \n\t" // 把dirty位置1
        "mtc0 $k0, $2 \n\t"  // 写回到tlb中
        "mtc0 $k1, $3 \n\t"
        "nop \n\t"
        "nop \n\t"
        "tlbwi \n\t"
        "nop \n\t"
        "nop \n\t"
        : "=r"(Regs_ptr)
    );
    ASID = Regs[0], BadVPN2 = Regs[1], BadVAddr = Regs[2];
        // 用于调试，打印发生TLB modified异常的进程号(ASID)，虚拟页号2(BadVPN2)与虚拟地址(BadVAddr)
    kernel_printf("[TLBModified]: ASID=%x, BadVPN2=%x, BadVAddr=%x\n", ASID, BadVPN2, BadVAddr);
    Set_dirty(ASID, BadVAddr);  // 把页表中相应的项也标记为脏的
}
#pragma GCC pop_options

void init_MMU() {
    unsigned int i;
    for(i=0; i<256; i++) PGT_BaseAddrs[i] = NULL; /* Clear the page table */
    register_exception_handler(1, TLBModified_exception);
    register_exception_handler(2, TLBMiss_Invalid_exception);
    register_exception_handler(3, TLBMiss_Invalid_exception);
    register_refill_exception_handler(TLBMiss_refill_exception);
}


// ASID是对应的进程号
// 这个函数在某个用户进程被创建时被调用，初始化一个第一级页表
unsigned int create_PGT(unsigned int ASID) {
    assert((ASID <= 255),
            "[Create_PGT]Error, the ASID is too big!");
    if (PGT_BaseAddrs[ASID] != NULL)  // 已经存在一个第一级页表
            kernel_printf("[Create_PGT]Error, the PGE table exists!");
    PGT_BaseAddrs[ASID] = (unsigned int *((*)[1024]))kmalloc(4096);
    // 这个表中直接存第一级页表的地址，没有valid位，因为只要这里的数不为0，就说明一定有一个地址被分配
    // 有地址被分配，就说明这个表项是正在被使用的
    unsigned int i;
    for (i=0; i<1024; i++) PGT_BaseAddrs[ASID][0][i] = NULL;  /* No PDT */
    // 初始化第一级页表的每一项，把它们都赋值为0
    return 1;
}

// ASID是对应的进程号
// PGT_entry是第一级页表的对应项
// 这个函数先在PGT_BaseAddrs中找到对应的第一级页表起始地址，再在第一级页表中对应的项中装入新创建的第二级页表起址
unsigned int create_PDT(unsigned int ASID, unsigned int PGT_entry) {
    assert((ASID <= 255),  // 保证进程号不大于255
        "[Create_PDT]Error, the ASID is too big!");
    assert((PGT_BaseAddrs[ASID][0][PGT_entry] == NULL),  // 保证对应的第二级页表是空的
        "[Create_PDT]Error, the PGE entry has exist!");
    PGT_BaseAddrs[ASID][0][PGT_entry] = (unsigned int*)kmalloc(4096); // 分配一个页，作为第二级页表
    unsigned int i;
    for (i=0; i<1024; i++) PGT_BaseAddrs[ASID][0][PGT_entry][i] = 0; // 对表中的每一个项初始化
    return 1;
}


/* 用户进程被杀死时调用 */
unsigned int delete_PGT(unsigned int ASID) {
    if (PGT_BaseAddrs[ASID] == NULL) return 0;
        // 如果第一级页表本来就不存在，则删除失败
    unsigned int i;
    for (i=0; i<1024; i++) {
        release_PGT_entry(ASID, i); // 清除每个第一级页表项
    }
    kfree(PGT_BaseAddrs[ASID]); // 释放第一级页表所占用的空间
    PGT_BaseAddrs[ASID] = NULL;
    return 1;
}

/* Delete a PGT entry
   Returns 0 if the entry is empty */
unsigned int release_PGT_entry(unsigned int ASID, unsigned int PGT_entry) {
    unsigned int i;
    unsigned int** PGT_entry_addr;
    unsigned int* PDT_BaseAddr;
    assert((ASID <= 255),  // 保证进程号不大于255
        "[release_PGT_entry]Error, the ASID is too big!");
    assert((PGT_entry <= 1023),  // 保证第一级页表项索引不大于1023
        "[release_PGT_entry]Error, the PGT_entry is too big!");
    PGT_entry_addr = &(PGT_BaseAddrs[ASID][0][PGT_entry]);
    PDT_BaseAddr = *PGT_entry_addr;
    if (PDT_BaseAddr = NULL) return 0;
    delete_PDT(PDT_BaseAddr);
    *PGT_entry_addr = NULL;  /* 把第一级页表的这一项清空 */
    return 1;
}

// 删除一个第二级页表
unsigned int delete_PDT(unsigned int* PDT_BaseAddr) {
    if (PDT_BaseAddr == NULL) return 0;
    unsigned int i;
    for (i=0; i<256; i++) {  /* 1024 words, i.e. 256 entries in a page directory table */
        /* 
            An entry:
           i*4:     EntryHi     [31-VPN2-13; 12---8; 7-ASID-0]
           i*4+1:   PageMask    [28-Mask-13]  all zero -> 4KB
           i*4+2:   EntryLo0    [29-PFN-6; 5-C-3; 2-D; 1-V; 0-G]
           i*4+3:   EntryLo1    
        */
        if (PDT_BaseAddr[i*4] != 0)  /* ASID不是0，找到一个合法数据 */
            release_PDT_entry(PDT_BaseAddr, i); // 调用release_PDT_entry函数把这个合法数据清除
    }
    kfree(PDT_BaseAddr);  // 释放这个第二级页表所占的空间
    return 1;
}

/* The page directory table
   param. PDT_BaseAddr = the base address of PDT 
*/
unsigned int release_PDT_entry(unsigned int* PDT_BaseAddr, unsigned int PDT_entry) {
    if (PDT_BaseAddr == NULL) return 0;
    if (PDT_entry >= 256) return 0;
    unsigned int PFN0, PFN1;
    unsigned int cp0EntryLo0, cp0EntryLo1;
    cp0EntryLo0 = PDT_BaseAddr[(PDT_entry<<2)+2]; // 取EntryLo0项
    cp0EntryLo1 = PDT_BaseAddr[(PDT_entry<<2)+3]; // 取EntryLo0项
    PFN0 = (cp0EntryLo0 >> 6) << 10;  // 取对应的物理页起始地址
    PFN1 = (cp0EntryLo1 >> 6) << 10;
    if (cp0EntryLo0 & 2)
        kfree((unsigned int *)PFN0); // 清空物理页
        /*
    if (cp0EntryLo1 & 2)
        kfree((unsigned int *)PFN1); */// 清空物理页
    PDT_BaseAddr[(PDT_entry<<2)] = 0;   // 所有的数据项都置0
    PDT_BaseAddr[(PDT_entry<<2)+1] = 0;
    PDT_BaseAddr[(PDT_entry<<2)+2] = 0;
    PDT_BaseAddr[(PDT_entry<<2)+3] = 0;
    return 1;
}

/* 
    pages_num must be 0 or 1;  Now it is ignored.
    VPN2 has 11+8 bits.
*/

unsigned int create_PDT_entry(unsigned int ASID, unsigned int VPN2, unsigned int pages_num) {
    if (ASID > 255) return 0;
    if (PGT_BaseAddrs[ASID] == NULL) create_PGT(ASID);  // 如果没有第一级页表，则创建
    if (PGT_BaseAddrs[ASID][0][VPN2 >> 8] == NULL) create_PDT(ASID, VPN2 >> 8);
        // 如果没有第二级页表，则创建
    unsigned int* PDT_BaseAddr = PGT_BaseAddrs[ASID][0][VPN2 >> 8];
        // 取第二级页表的首地址
    unsigned int PDT_entry = VPN2 & 255;
        // 取第二级页表的数据项索引（一个数据项是4个字的）
    unsigned int* PageFrame_BaseAddr;
    unsigned int cp0EntryHi, cp0EntryLo0, cp0EntryLo1;
        // 每个页表项的值
    if (PDT_BaseAddr[PDT_entry*4] != 0)   
    // 数据项不为空，已经被占用，创建失败
    // 我们默认所有的进程号ASID都是大于0的
    // 如果cp0EntryHi这一项不为0，则ASID也一定不为0，那么如果这一项不为0则说明一定被用了
        return 0;
    PageFrame_BaseAddr = kmalloc(8192);  // 申请两个页的物理空间给用户进程使用
    cp0EntryHi  = ((unsigned int)(VPN2 << (1+PAGE_SHIFT_MMU))) | ASID;  // PAGE_SHIFT_MMU = 8
        // TLB数据项：cp0EntryHi，对应EntryHi寄存器
    cp0EntryLo0 = ((((unsigned int)PageFrame_BaseAddr & (~VPN_mask)) >> 6) & 0x1ffffc0) | 0x1a;  
        /* C = 3, D = 0, V = 1, G = 0 */
        // TLB数据项：cp0EntryLo0，对应EntryLo0寄存器
    cp0EntryLo1 = (((((unsigned int)PageFrame_BaseAddr+4096) & (~VPN_mask)) >> 6) & 0x1ffffc0) | 0x1a;
        // TLB数据项：cp0EntryLo1，对应EntryLo1寄存器
    PDT_BaseAddr[(PDT_entry<<2)] = cp0EntryHi;  
    PDT_BaseAddr[(PDT_entry<<2)+1] = 0;
    PDT_BaseAddr[(PDT_entry<<2)+2] = cp0EntryLo0;
    PDT_BaseAddr[(PDT_entry<<2)+3] = cp0EntryLo1;
    return 1;
}

unsigned int Set_valid(unsigned int ASID, unsigned int BadVAddr) {
    unsigned int PGT_entry = VAddr2PGE(BadVAddr);   /* 11 bits */
    unsigned int PDT_entry_4 = VAddr2PDE4(BadVAddr);  /* 8 bits */
    unsigned int sel = (BadVAddr & (1<<12)) > 0 ? 1 : 0; /* sel=1, select the 2nd page */
    if (PGT_BaseAddrs[ASID] == NULL) return 0;  // 检查数据的合法性
    if (PGT_BaseAddrs[ASID][0][PGT_entry] == NULL) return 0;
    if (PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4] == 0) return 0;   /* Empty */ 
    PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4 + 2 + sel] |= 2;  // 设置valid位
    return 1;
}

unsigned int Set_dirty(unsigned int ASID, unsigned int BadVAddr) {
    unsigned int PGT_entry = VAddr2PGE(BadVAddr);   /* 11 bits */
    unsigned int PDT_entry_4 = VAddr2PDE4(BadVAddr);  /* 8 bits */
    unsigned int sel = (BadVAddr & (1<<12)) > 0 ? 1 : 0; /* sel=1, select the 2nd page */
    if (PGT_BaseAddrs[ASID] == NULL) return 0;  // 检查数据的合法性
    if (PGT_BaseAddrs[ASID][0][PGT_entry] == NULL) return 0;
    if (PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4] == 0) return 0;   /* Empty */ 
    PGT_BaseAddrs[ASID][0][PGT_entry][PDT_entry_4 + 2 + sel] |= 4;  // 设置dirty位
    return 1;
}