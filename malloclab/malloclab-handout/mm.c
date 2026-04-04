/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */            // line:vm:mm:beginconst
#define DSIZE 8                                                      /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */ // line:vm:mm:endconst
#define MIN_BLOCK_SIZE (DSIZE * 2)                                   /* 最小块的 size，包含两个指针(8 * 2)和头尾(4 * 2)，指针的占用后面会用作内容 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // line:vm:mm:get
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1) // line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)                      // line:vm:mm:hdrp
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // line:vm:mm:nextblkp
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // line:vm:mm:prevblkp

/**
 * (char **)(bp) 将 bp 视为一个指向指针变量的指针，再通过 *bp 得到bp指向内容的引用
 * char * 在32位机器是 4 字节
 */
#define REFER_PRED(bp) *((char **)(bp))
#define REFER_SUCC(bp) *((char **)((char *)(bp) + WSIZE))

#define PREV_FREE_PTR(bp) REFER_PRED(bp)
#define NEXT_FREE_PTR(bp) REFER_SUCC(bp)

/* $end mallocmacros */

/**
 * debug variables
 */
static int debug_info = 0;

/* Global variables */
static int o_index = 5;
static char *heap_listp = 0;     /* Pointer to the first byte of head */
static char *free_list_head = 0; /* Pointer to head */

static void link_two_free_blk(void *prev, void *succ)
{
    if (prev)
    {
        REFER_SUCC(prev) = succ;
    }
    if (succ)
    {
        REFER_PRED(prev) = prev;
    }
}

static void link_three_free_blk(void *prev, void *mid, void *succ)
{
    REFER_PRED(mid) = prev;
    REFER_SUCC(mid) = succ;

    // 更新前后模块对当前模块的指针指向
    if (prev)
    {
        REFER_SUCC(prev) = mid; // 更新 pred 模块的 next 指针
    }
    if (succ)
    {
        REFER_PRED(succ) = mid; // 更新 next 模块的 pred 指针
    }
}

static int is_epilogue_header(void *head_ptr)
{
    // 检查是否是最后一个word
    return GET_SIZE(HDRP(head_ptr)) == 0 && GET_ALLOC(HDRP(head_ptr)) == 1;
}

static int rest_size_bigger_valid_or_zero(int rest_size)
{
    if (rest_size % DSIZE)
    {
        printf("[ERROR]: rest_size_bigger_valid_or_zero \n");
        exit(1);
    }
    return (rest_size == 0) || (rest_size >= MIN_BLOCK_SIZE);
}

/**
 * mm_check - return 1 if consistent, else return 0
 * • Is every block in the free list marked as free?
 * • Are there any contiguous free blocks that somehow escaped coalescing?
 * • Is every free block actually in the free list?
 * • Do the pointers in the free list point to valid free blocks?
 * • Do any allocated blocks overlap?
 * • Do the pointers in a heap block point to valid heap addresses?
 */

static void mm_check()
{
    printf("----------------------- mm_check ----------------------- \n");
    // • Is every free block actually in the free list ?
    void *head_ptr = free_list_head;
    int i = 0;
    while (i < 20)
    {
        if (is_epilogue_header(head_ptr))
        {
            printf("[addr]: %p, [size]: %d, [allocated]: %d \n", head_ptr, GET_SIZE(HDRP(head_ptr)), GET_ALLOC(HDRP(head_ptr)));
            break;
        }
        printf("[addr]: %p, [size]: %d, [allocated]: %d \n", head_ptr, GET_SIZE(HDRP(head_ptr)), GET_ALLOC(HDRP(head_ptr)));
        head_ptr = NEXT_BLKP(head_ptr);
        i++;
    }
    printf("----------------------- mm_check ----------------------- \n\n");
}

static void log_memory()
{
    int type = -1;
    size_t malloc_size = 0;

    if (!debug_info)
        return;

    char type_str[20];

    switch (type)
    {
    case 1:
        strcpy(type_str, "free");
        break;
    case 2:
        strcpy(type_str, "realloc");
        break;
    default:
        strcpy(type_str, "malloc");
        break;
    }

    void *ptr = free_list_head;
    // printf("------------------------ [type]: %s ", type_str);
    printf("------------------------ memory map");

    if (type == 0)
    {
        printf("[malloc_size]: %d", malloc_size);
    }
    printf("------------------------ \n");
    printf("[heap_size]: %d [heap_start_ad]: %p [heap_end_ad]: %p [free_list_head]: %p \n\n", mem_heapsize(), mem_heap_lo(), mem_heap_hi(), free_list_head);

    while (ptr)
    {
        printf("[blk addr]: %p  [blk size]: %u [blk pred]: %p [blk succ]: %p \n", ptr, GET_SIZE(HDRP(ptr)), PREV_FREE_PTR(ptr), NEXT_FREE_PTR(ptr));
        ptr = NEXT_FREE_PTR(ptr);
    }

    printf("-- \n\n");

    mm_check();

    fflush(stdout);
}

static void init_block_size(void *bp, size_t asize, int allocated)
{
    int safe_allocated = allocated && 1;
    PUT(HDRP(bp), PACK(asize, safe_allocated));
    PUT(FTRP(bp), PACK(asize, safe_allocated));
}

/**
 * init_block_status - 更新 bp 指向block的状态
 */
static void init_block_status(void *bp, size_t asize, int allocated)
{
    init_block_size(bp, asize, allocated);

    REFER_SUCC(bp) = 0;
    REFER_PRED(bp) = 0;
}

/**
 * insert_free_blk - insert a free blk to free list
 * 用于将一个新产生的空闲块（且没有相邻块是空闲块）插入到空闲链表中
 */
static void insert_free_blk_by_find(void *bp)
{

    void *head_ptr = (void *)free_list_head; // 指向序言块的地址

    if (bp < (void *)head_ptr)
    {
        fprintf(stderr, "[ERROR_SONG]-insert: unbelievable \n");
        exit(1);
    }

    while (NEXT_FREE_PTR(head_ptr))
    {
        void *next_free_blkp = NEXT_FREE_PTR(head_ptr);

        if ((bp > head_ptr) && (bp < next_free_blkp))
        {
            REFER_SUCC(head_ptr) = bp;
            REFER_PRED(next_free_blkp) = bp;

            REFER_PRED(bp) = head_ptr;
            REFER_SUCC(bp) = next_free_blkp;
            return;
        }

        head_ptr = NEXT_FREE_PTR(head_ptr);
    }

    // 走到这里说明 bp 比空闲链表中所有都要大，所以插在末尾
    REFER_SUCC(head_ptr) = bp;

    REFER_PRED(bp) = head_ptr;
    REFER_SUCC(bp) = 0;
}

/**
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 * maintain explicit free list
 * 进入这个 bp 表示都是free的
 */
static void *coalesce(void *bp)
{
    if (REFER_SUCC(free_list_head) == 0)
    {

        // debug_info &&printf("coalesce case 0 \n");

        // head next 不指向任何内容，则认为当前没有空闲块，所以将 bp 放入链中
        REFER_SUCC(free_list_head) = bp;

        REFER_PRED(bp) = free_list_head;
        REFER_SUCC(bp) = 0;
        return bp;
    }

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t asize = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        // debug_info &&printf("coalesce case 1 \n");

        /* Case 1 */
        insert_free_blk_by_find(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    {
        // debug_info &&printf("coalesce case 2 \n");

        /* Case 2 */
        // bp 即为 结果bp，将后面空闲块的指针信息拷贝到当前块，同时要更新前后空闲块对当前块的指针指向
        char *next_blkp = NEXT_BLKP(bp);
        char *nl_prev_free_ptr = PREV_FREE_PTR(next_blkp); // 指向下一个blk 的上一个free block
        char *nl_next_free_ptr = NEXT_FREE_PTR(next_blkp); // 指向下一个blk 的下一个free block

        asize += GET_SIZE(HDRP(next_blkp));

        init_block_status(bp, asize, 0);

        link_three_free_blk(nl_prev_free_ptr, bp, nl_next_free_ptr);
    }

    else if (!prev_alloc && next_alloc)
    {
        // debug_info &&printf("coalesce 3 \n");

        /* Case 3 */
        // PREV_BLKP(bp)是 结果bp，和prev 合并，直接应用 prev 的指针内容，无需更新空闲链表
        asize += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(asize, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(asize, 0));
        bp = PREV_BLKP(bp);
    }

    else
    {
        // debug_info &&printf("coalesce case 4 \n");

        /* Case 4 */
        // 更新空闲链表，合并 prev 和 next，应用 prev 的前后指针，并将 next 从空闲链表中删除
        char *prev_free_blkp = PREV_BLKP(bp);
        char *next_free_blkp = NEXT_BLKP(bp);

        char *next_next_free_blk_ptr = NEXT_FREE_PTR(next_free_blkp); // 指向下一个blk 的下一个 free block
        char *prev_prev_free_blk_ptr = PREV_FREE_PTR(prev_free_blkp);

        asize += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                 GET_SIZE(FTRP(NEXT_BLKP(bp)));

        bp = PREV_BLKP(bp);
        init_block_status(bp, asize, 0);

        link_three_free_blk(prev_prev_free_blk_ptr, bp, next_next_free_blk_ptr);
    }

    return bp;
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
    // debug_info &&printf("❗extend_heap occur! \n");
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    init_block_status(bp, size, 0);

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    void *res_p = coalesce(bp); // line:vm:mm:returnblock

    return res_p;
}

void log_operation_index()
{
    if (!debug_info)
        return;

    printf("❗index: %d \n", o_index++);

    log_memory();
}

/**
 * place
 * bp 指向要放置的模块，asize 已经是调整过的大小（已经 8 字节对齐）
 */
void place(void *bp, size_t applied_size)
{
    unsigned int block_size = GET_SIZE(HDRP(bp));

    if (applied_size > block_size)
    {
        fprintf(stderr, "[ERROR_SONG]-place: place size is too small. \n");
        return;
    }

    // ❗❗❗下面两个指针可能为空
    char *prev_free_blkp = PREV_FREE_PTR(bp); // 指向上一个 free block
    char *next_free_blkp = NEXT_FREE_PTR(bp); // 指向下一个 free block
    // printf("place : %p %p \n", prev_free_blkp, next_free_blkp);

    unsigned int rest_space_size = block_size - applied_size; // 前后都是 8 字节对齐，结果也是 8 字节对齐

    // ❗判断剩余空间是否可以容纳下一个空闲块，如果不可以就不要分割
    if (rest_space_size >= MIN_BLOCK_SIZE)
    {
        // 分割
        // 前半部分，脱离空闲链表，用以分配
        init_block_status(bp, applied_size, 1);

        // 后半部分
        char *splitted_block_ptr = (char *)bp + applied_size; // applied_size 已经表示要申请的字节大小了
        init_block_status(splitted_block_ptr, rest_space_size, 0);

        // update free list
        REFER_PRED(splitted_block_ptr) = prev_free_blkp;
        REFER_SUCC(splitted_block_ptr) = next_free_blkp;

        if (prev_free_blkp)
        {
            REFER_SUCC(prev_free_blkp) = splitted_block_ptr;
        }
        if (next_free_blkp)
        {
            REFER_PRED(next_free_blkp) = splitted_block_ptr;
        }
    }
    else
    {
        // 不分割，直接分配，将当前 block 从 free list 中移出
        // 注意要直接使用 block_size, 因为没有进行分割
        init_block_status(bp, block_size, 1);

        if (prev_free_blkp)
        {
            REFER_SUCC(prev_free_blkp) = next_free_blkp;
        }
        if (next_free_blkp)
        {
            REFER_PRED(next_free_blkp) = prev_free_blkp;
        }
    }
}

/**
 * find_fit
 * 在显示空闲链表中查找存在满足的空闲块
 */
void *find_fit(size_t asize)
{
    char *head_ptr = NEXT_FREE_PTR(free_list_head);

    while (head_ptr)
    {
        // 在链表中查找合适模块，采用首次匹配机制
        if (GET_SIZE(HDRP(head_ptr)) >= asize)
            return head_ptr;
        head_ptr = NEXT_FREE_PTR(head_ptr);
    }

    return NULL;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    /* Create the initial empty heap */
    // MIN_BLOCK_SIZE 用于构造 head ，WSIZE-1 用于头部对齐，WSIZE-2 用于设置 Epilogue header
    if ((heap_listp = mem_sbrk(MIN_BLOCK_SIZE + WSIZE * 2)) == (void *)-1) // line:vm:mm:begininit
        return -1;
    unsigned int prologue_size = MIN_BLOCK_SIZE; // 包含两个指针 (2 * 4)和头尾块 (2 * 4)
    // free_list_head 指向序言的 pred 地址
    // heap_listp 始终指向第一个内存池的第一个字节
    free_list_head = heap_listp + (2 * WSIZE);

    PUT(heap_listp, 0);
    init_block_status(free_list_head, prologue_size, 1);

    PUT(heap_listp + (5 * WSIZE), PACK(0, 1)); /* Epilogue header */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    if (heap_listp == 0)
    {
        mm_init();
    }

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = MIN_BLOCK_SIZE;
    else
        asize = ALIGN(size + (DSIZE)); // 申请块大小本身 + 头尾，然后再 ALIGN

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {                     // line:vm:mm:findfitcall
        place(bp, asize); // line:vm:mm:findfitplace
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if (bp == 0)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0)
    {
        mm_init();
    }

    init_block_size(bp, size, 0); // 只设置头信息，不影响内容

    coalesce(bp);
}

int check_ptr_is_valid(void *bp)
{

    if (bp >= mem_heap_hi() || bp <= mem_heap_lo())
        return 0;

    void *head_ptr = free_list_head;

    while (!(is_epilogue_header(head_ptr)))
    {
        if (head_ptr == bp && GET_ALLOC(HDRP(head_ptr)))
            return 1;
        head_ptr = NEXT_BLKP(head_ptr);
    }

    return 0;
}

/*
 * mm_realloc
 */
void *mm_realloc(void *p, size_t size)
{

    if (p == NULL)
    {
        return mm_malloc(size);
    }

    if (!check_ptr_is_valid(p))
    {
        return NULL;
    }

    if (size == 0)
    {
        mm_free(p);
        return NULL;
    }

    void *old_ptr = p;

    void *ptr = p;
    size_t asize = ALIGN(size + DSIZE); // 补充头尾，然后查看实际需要的大小

    size_t block_size = GET_SIZE(HDRP(ptr));
    size_t content_size = block_size - DSIZE;

    // 当前块无法满足分配
    // 检查相邻：合并空白之后，如果还无法满足，则查找空白链表或重新分配
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

    // rest_size_bigger_valid_or_zero(rest_size_new) == 0 说明：
    // 申请内容和当前块内容要么相等，要么相减后可以分割

    if (prev_alloc && next_alloc)
    {
        int rest_size_new = block_size - asize;
        if (rest_size_bigger_valid_or_zero(rest_size_new)) // 相等，或者 block_size - asize 支持分割
        {
            // debug_info &&printf("mm_realloc case 1 %d \n", rest_size_new);

            if (rest_size_new == 0)
                // 直接分配
                // 申请的块大小和当前块大小完全一致
                return old_ptr;

            init_block_size(ptr, asize, 1); // 不要覆盖内容

            void *tofree_ptr = (char *)ptr + asize;
            init_block_status(tofree_ptr, rest_size_new, 0);

            coalesce(tofree_ptr); // 分割剩下的现在前后都是 allocated，进入之后会分
            return ptr;
        }
    }
    else if (prev_alloc && !next_alloc)
    {
        size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        int rest_size_new = next_blk_size + block_size - asize;

        if (rest_size_bigger_valid_or_zero(rest_size_new))
        {
            // debug_info &&printf("mm_realloc case 2 %d \n", rest_size_new);

            char *next_blkp = NEXT_BLKP(ptr);
            char *nl_prev_free_ptr = PREV_FREE_PTR(next_blkp); // 指向下一个blk 的上一个free block
            char *nl_next_free_ptr = NEXT_FREE_PTR(next_blkp); // 指向下一个blk 的下一个free block

            init_block_size(ptr, asize, 1); // 重新分配

            if (rest_size_new == 0)
            {
                link_two_free_blk(nl_prev_free_ptr, nl_next_free_ptr);
            }
            else
            {
                // 分割
                void *tofree_ptr = (char *)ptr + asize;
                init_block_status(tofree_ptr, rest_size_new, 0);

                link_three_free_blk(nl_prev_free_ptr, tofree_ptr, nl_next_free_ptr);
            }

            return ptr;
        }
    }
    else if (!prev_alloc && next_alloc)
    {
        size_t prev_blk_size = GET_SIZE(HDRP(PREV_BLKP(ptr)));
        int rest_size_new = prev_blk_size + block_size - asize;

        if (rest_size_bigger_valid_or_zero(rest_size_new))
        {
            // debug_info &&printf("mm_realloc case 3 %d \n", rest_size_new);

            void *prev_blk = PREV_BLKP(ptr);
            char *pl_prev_free_ptr = PREV_FREE_PTR(prev_blk); // 指向上一个blk 的上一个free block
            char *pl_next_free_ptr = NEXT_FREE_PTR(prev_blk); // 指向上一个blk 的下一个free block

            memcpy(prev_blk, ptr, content_size);
            ptr = prev_blk;

            init_block_size(ptr, asize, 1);

            if (rest_size_new == 0)
            {
                link_two_free_blk(pl_prev_free_ptr, pl_next_free_ptr);
            }
            else
            {
                void *tofree_ptr = ptr + asize;
                init_block_status(tofree_ptr, rest_size_new, 0);

                link_three_free_blk(pl_prev_free_ptr, tofree_ptr, pl_next_free_ptr);
            }

            return ptr;
        }
    }
    else
    {

        // 两边都是 free blk
        size_t prev_blk_size = GET_SIZE(HDRP(PREV_BLKP(ptr)));
        size_t next_blk_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        int rest_size_new = prev_blk_size + next_blk_size + block_size - asize;

        // debug_info &&printf("mm_realloc case 4 %d \n", rest_size_new);

        if (rest_size_bigger_valid_or_zero(rest_size_new))
        {
            char *prev_free_blkp = PREV_BLKP(ptr);
            char *next_free_blkp = NEXT_BLKP(ptr);

            char *next_next_free_blk_ptr = NEXT_FREE_PTR(next_free_blkp); // 指向下一个blk 的下一个 free block
            char *prev_prev_free_blk_ptr = PREV_FREE_PTR(prev_free_blkp);

            memcpy(prev_free_blkp, ptr, content_size);
            ptr = prev_free_blkp;

            init_block_size(ptr, asize, 1);

            if (rest_size_new == 0)
            {
                // 直接分配
                link_two_free_blk(prev_prev_free_blk_ptr, next_next_free_blk_ptr);
            }
            else
            {
                // 分割
                void *tofree_ptr = ptr + asize;
                init_block_status(tofree_ptr, rest_size_new, 0);

                link_three_free_blk(prev_prev_free_blk_ptr, tofree_ptr, next_next_free_blk_ptr);
            }

            return ptr;
        }
    }

    // 以上如果都无法处理，则表示需要寻找新的模块了 (上述表示，当前的块无论合并与否，都无法支持完全分配或者分配后再分割)
    // 所以寻找足够大的空闲块
    // 注意内容复制到新的 block 之后，注意回收旧的 block
    /* Search the free list for a fit */
    if ((ptr = find_fit(asize)) != NULL)
    {
        // debug_info &&printf("mm_realloc case 5 %d \n", asize);

        // 分配
        place(ptr, asize);
        memcpy(ptr, old_ptr, content_size);

        // 回收
        mm_free(old_ptr);
        return ptr;
    }

    /* No fit found. Get more memory and place the block */
    size_t extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    // debug_info &&printf("mm_realloc case 6 %d \n", asize);

    // 分配
    place(ptr, asize);
    memcpy(ptr, old_ptr, content_size);

    // 回收
    mm_free(old_ptr);

    return ptr;
}
