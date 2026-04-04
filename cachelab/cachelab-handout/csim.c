#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "./cachelab.h"

/**
    Running cache simulator tests... [ref]
    ================================
    hits:9 misses:8 evictions:6
    hits:4 misses:5 evictions:2
    hits:2 misses:3 evictions:1
    hits:167 misses:71 evictions:67
    hits:201 misses:37 evictions:29
    hits:212 misses:26 evictions:10
    hits:231 misses:7 evictions:0
    hits:265189 misses:21775 evictions:21743
    ================================
 */

// ----------------------------------------------------
typedef struct CacheLineNode
{
    int valid;
    long tag;
    long seq;
    struct CacheLineNode *next;
} CacheLineNode;

typedef struct CacheSet
{
    CacheLineNode *line_ptr; // 指向第一个高速缓存行
} CacheSet;

typedef CacheSet **CacheTable;

CacheTable table;

// ----------------------------------------------------

long getSeqNum()
{
    static long seq_num = 0;
    return ++seq_num;
}

void printUsage(char *argv[])
{
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message\n");
    printf("  -v         Optional verbose flag\n");
    printf("  -s <s>     Number of set index bits\n");
    printf("  -E <E>     Number of lines per set\n");
    printf("  -b <b>     Number of block bits\n");
    printf("  -t <file>  Trace file\n");
}

#define ACCESS_LOAD 76
#define ACCESS_STORE 83
#define ACCESS_MODIFY 77

static int arg_s = 0;
static int arg_E = 0;
static int arg_b = 0;
static int arg_v = 0;
static int arg_m = 0;
static int instruction_seq = 1;

static int hit_num = 0,
           miss_num = 0, eviction_num = 0;

void increaseHitHum()
{
    if (arg_v)
    {
        printf("hit ");
    }
    hit_num++;
}
void increaseMissNum()
{
    if (arg_v)
    {
        printf("miss ");
    }
    miss_num++;
}
void increaseEvictionNum()
{
    if (arg_v)
    {
        printf("eviction ");
    }
    eviction_num++;
}

int getNumOfGroups()
{
    return 1 << arg_s;
}

int getNumOfLines()
{
    return arg_E;
}

long getNumInside(long address, size_t after_bits_num, size_t bits_num)
{
    long num = address >> after_bits_num;
    return num & (~(-1L << bits_num));
}

long getSetIndex(long address)
{
    return getNumInside(address, arg_b, arg_s);
}

long getBlockTag(long address)
{
    size_t after_bits_num = arg_b + arg_s;
    return getNumInside(address, arg_b + arg_s, 64 - after_bits_num);
}

void updateCacheLRU(CacheLineNode *target_ptr, long b_tag, long seq_num)
{
    target_ptr->seq = seq_num;
    target_ptr->tag = b_tag;
    target_ptr->valid = 1;
}

int checkTagExistAndUpdate(CacheSet *cache_set, long b_tag)
{
    CacheLineNode *head_ptr = cache_set->line_ptr;
    CacheLineNode *ptr = head_ptr;
    CacheLineNode *prev_ptr = NULL;
    while (ptr != NULL)
    {
        if (ptr->valid && ptr->tag == b_tag) // 如果缓存行有效，且块标记对应
        {
            increaseHitHum();
            ptr->seq = getSeqNum();
            return 1; // 表示已经命中
        }
        prev_ptr = ptr;
        ptr = ptr->next;
    }

    return 0;
}

void loadAndUpdate(CacheSet *cache_set, long b_tag)
{
    // 进入这里，肯定是已经不存在，所以触发一次miss
    increaseMissNum();

    // 判断缓冲区是否满了，满了则 eviction，否则只需load
    // 注意，二者都需要更新 LRU
    CacheLineNode *head_ptr = cache_set->line_ptr;
    CacheLineNode *ptr = head_ptr;
    CacheLineNode *target_line_ptr = ptr;

    long min_seq = ptr->seq;

    while (ptr)
    {
        if (!ptr->valid) // !ptr->valid 表示现在是空行
        {
            updateCacheLRU(ptr, b_tag, getSeqNum()); // 添加 b_tag 到该行，并开始记录 counter
            return;
        }

        if (ptr->seq < min_seq)
        {
            min_seq = ptr->seq;
            target_line_ptr = ptr;
        }

        ptr = ptr->next;
    }

    // 循环结束，没有空的，则需要将最后一个最久没有访问的移出
    increaseEvictionNum();
    updateCacheLRU(target_line_ptr, b_tag, getSeqNum()); // 用 b_tag 替换，并重新开始记录
}

void checkAndLoad(CacheSet *cache_set, long b_tag)
{
    int tagExist = checkTagExistAndUpdate(cache_set, b_tag);

    if (tagExist)
        return;

    loadAndUpdate(cache_set, b_tag);
}

void accessMemory(CacheSet *cache_set, int operation, long address)
{
    long b_tag = getBlockTag(address);

    switch (operation)
    {
    case ACCESS_LOAD:
    case ACCESS_STORE:
    {
        checkAndLoad(cache_set, b_tag);
    }
    break;

    case ACCESS_MODIFY:
    {
        checkAndLoad(cache_set, b_tag); // 第一次 Load
        checkAndLoad(cache_set, b_tag); // 第二次 store
    }
    break;
    default:
        break;
    }
    if (arg_v)
    {
        printf("\n");
    }
}

void printTable()
{
    if (!table || !arg_m)
        return;
    printf("\n");
    printf("[instruction_seq: %d] \n", instruction_seq);
    int setNums = getNumOfGroups();
    for (int i = 0; i < setNums; i++)
    {
        printf("================ Set %d================== \n", i);
        CacheSet *cache_set = table[i];
        CacheLineNode *line_node = cache_set->line_ptr;
        int line_num = 0;
        while ((line_node) != NULL)
        {
            printf("line %d: valid: %d, tag: %lx, seq: %lx \n", line_num++, line_node->valid, line_node->tag, line_node->seq);
            line_node = line_node->next;
        }
    }
    instruction_seq++;
}

int main(int argc, char *argv[])
{

    char *tracefile = NULL;

    int opt;
    // --------------------------------------------------------------------------- 获取命令行参数
    while ((opt = getopt(argc, argv, "hmvs:E:b:t:")) != -1)
    {
        switch (opt)
        {

        case 'h':
            printUsage(argv);
            return 0;

        case 'm':
            arg_m = 1;
            break;

        case 'v':
            arg_v = 1;
            break;

        case 's':
            arg_s = atoi(optarg);
            break;

        case 'E':
            arg_E = atoi(optarg);
            break;

        case 'b':
            arg_b = atoi(optarg);
            break;

        case 't':
            tracefile = optarg;
            break;

        default:
            printUsage(argv);
            return 1;
        }
    }

    // --------------------------------------------------------------------------- 申请内存
    table = malloc(getNumOfGroups() * sizeof(CacheSet *));
    if (!table)
    {
        perror("malloc table failed");
        exit(1);
    }

    for (size_t i = 0; i < getNumOfGroups(); i++)
    {
        CacheSet *cache_set = malloc(sizeof(CacheSet));

        if (!cache_set)
        {
            perror("malloc cache_set failed");
            exit(1);
        }

        CacheLineNode *line_prev = NULL;

        for (size_t j = 0; j < getNumOfLines(); j++)
        {
            CacheLineNode *line = malloc(sizeof(CacheLineNode));

            if (!line)
            {
                perror("malloc line failed");
                exit(1);
            }

            line->tag = 0;
            line->valid = 0;
            line->seq = 0;
            line->next = NULL;

            if (j == 0)
            {
                cache_set->line_ptr = line;
            }
            else
            {
                line_prev->next = line;
            }

            line_prev = line;
        }

        table[i] = cache_set;
    }

    // --------------------------------------------------------------------------- 从文件中逐行读取访问
    FILE *fp = fopen(tracefile, "r"); // 打开文件
    if (fp == NULL)
    {
        perror("fopen");
        return 1;
    }

    unsigned buf_size = 100;
    char line[buf_size];

    while (fgets(line, buf_size, fp))
    {

        char *ptr = line;
        // 跳过所有空格
        while (*ptr == ' ')
            ptr++;

        // 跳过首字母是 'I' 的行
        if (ptr[0] == 'I')
        {
            continue;
        }

        // 解析第一个字符
        char first_char = ptr[0];
        int operation_tag = (long)first_char; // 转成 long

        ptr++;

        // 跳过所有空格
        while (*ptr == ' ')
            ptr++;

        // 解析第二个字符串（假设格式固定 "S 00600aa0,1"）
        char *second_str = ptr;
        char *comma = strchr(second_str, ','); // 找到逗号
        if (!comma)
        {
            printf("输入格式错误!");
            return 1;
        }

        printTable(); // 查看操作前的状态

        char offset_char = comma[1];

        // 转成 long（16进制）
        long address = strtol(second_str, NULL, 16);

        if (arg_v)
        {
            printf("%c %lx,%c ", first_char, address, offset_char);
        }

        // 访问内存
        int group_index = getSetIndex(address);
        CacheSet *group_p = table[group_index];
        accessMemory(group_p, operation_tag, address);
    }

    // 释放内存
    for (size_t i = 0; i < getNumOfGroups(); i++)
    {
        CacheLineNode *ptr = table[i]->line_ptr;
        while (ptr != NULL)
        {
            CacheLineNode *tmp = ptr;
            ptr = ptr->next;
            free(tmp); // 先 free line
        }
        free(table[i]); // 再 free set
    }
    free(table); // 最后 free table 数组本身

    fclose(fp); // 关闭文件

    printSummary(hit_num, miss_num, eviction_num);
    return 0;
}