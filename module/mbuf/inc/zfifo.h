#ifndef _ZFIFO_H_
#define _ZFIFO_H_

#include <errno.h>
#include <pthread.h>
#include <time.h>

typedef struct _ZFIFO_ ZFIFO;
typedef struct _ZFIFO_DESC_ ZFIFO_DESC;
typedef struct _ZFIFO_FRAME_ ZFIFO_FRAME;

struct _ZFIFO_
{
    char *name;
    void *buf;
    int buf_size;

    int write_offset;            // 写指针偏移量：当前写入位置在buf中的字节偏移
    unsigned long long next_seq; // 下一个数据帧的序列号，用于帧的有序管理
    ZFIFO_FRAME *frame_head;     // 数据帧链表头指针：指向最早的有效数据帧
    ZFIFO_FRAME *frame_tail;     // 数据帧链表尾指针：指向最新的有效数据帧
    ZFIFO_FRAME *flag_frame;     // 最新的关键帧

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    /* reader list and writer */
    ZFIFO_DESC *reader_list; // 读者描述符链表头：管理所有已打开的读端
    ZFIFO_DESC *writer_desc; // 写者描述符：当前活跃的写端

    unsigned int wrap_waste_events;      // 回绕写入导致浪费的发生次数
    unsigned int last_wrap_waste_bytes;  // 最近一次回绕浪费的尾部字节数
    int frame_count;                     // 当前帧链表节点总数
};

struct _ZFIFO_DESC_
{
    ZFIFO *zfifo;
    ZFIFO_FRAME *cursor;         // 当前读写游标：指向该读写端待处理的数据帧
    ZFIFO_FRAME *locked_frame;   // 锁定帧指针：临时锁定的帧（如peek操作时的帧）
    unsigned long long wait_seq; // 等待的序列号：读端期望读取的下一个帧序列号

    int is_writer;
    ZFIFO_DESC *next; /* linked list for readers */

    struct timespec lock_ts;     /* 首次锁定帧时 */
    unsigned char evicted;      /* 读者超时被驱逐 */
};

typedef struct _NODE_HEADER_
{
    int index;        // 节点索引：在FIFO缓冲区中的逻辑索引
    int size;         // 节点数据长度：当前节点包含的实际业务数据字节数
} NODE_HEADER;

typedef struct _ZFIFO_NODE_
{
    void *base;
    int len;
} ZFIFO_NODE;

ZFIFO *zfifo_init(const char *zfifo_name, int size);
void zfifo_uninit(ZFIFO *zfifo);

ZFIFO_DESC *zfifo_open_reader(ZFIFO *fifo);
ZFIFO_DESC *zfifo_open_writer(ZFIFO *fifo);

void zfifo_close(ZFIFO_DESC *zfifo_desc);
int zfifo_set_newest_frame(ZFIFO_DESC *zfifo_desc);
int zfifo_set_oldest_frame(ZFIFO_DESC *zfifo_desc);

int zfifo_writev(ZFIFO_DESC *zfifo_desc, const ZFIFO_NODE *iov, int iovcnt, int flag);

/* shallow peek: 不拷贝数据，仅返回内部 buffer 的地址（must call zfifo_read_release afterward） */
int zfifo_peek(ZFIFO_DESC *zfifo_desc, ZFIFO_NODE *iov, int iovcnt, int timeout);
/* release: 推进 reader 指针（对应之前 peek 返回的数据），释放后该区域可以被覆盖 */
int zfifo_read_release(ZFIFO_DESC *zfifo_desc, int iovcnt);

int zfifo_get_info(ZFIFO *zfifo, int *remain_percent);

#endif
