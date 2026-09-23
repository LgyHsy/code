#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_mutex.h"
#include "zfifo.h"

#define ZFIFO_READER_TIMEOUT_MS 1000
#define ZFIFO_MAX_FRAMES 200
#define ZFIFO_DIFF_TIME_MS(start, end) \
    (((end)->tv_sec - (start)->tv_sec) * 1000L + ((end)->tv_nsec - (start)->tv_nsec) / (1000 * 1000))

struct _ZFIFO_FRAME_
{
    unsigned long long seq;
    int offset;
    int len;
    int is_flag;
    int ref_count;
    struct _ZFIFO_FRAME_ *prev;
    struct _ZFIFO_FRAME_ *next;
};

static int zfifo_calc_len(const ZFIFO_NODE *iov, int iovcnt)
{
    unsigned long long total_len = 0;
    int i;

    if (iov == NULL || iovcnt <= 0)
    {
        return -1;
    }

    for (i = 0; i < iovcnt; i++)
    {
        unsigned long long add = sizeof(NODE_HEADER);

        if (iov[i].len < 0)
        {
            return -1;
        }

        add += (unsigned int)iov[i].len;
        total_len += add;
        if (total_len > (unsigned long long)INT_MAX)
        {
            return -1;
        }
    }

    return (int)total_len;
}

static int zfifo_write_node(void *buf, const ZFIFO_NODE *iov, int iovcnt)
{
    char *dst = (char *)buf;
    int offset = 0;
    int i;

    for (i = 0; i < iovcnt; i++)
    {
        NODE_HEADER node_hdr;

        node_hdr.index = i;
        node_hdr.size = iov[i].len;

        memcpy(dst + offset, &node_hdr, sizeof(node_hdr));
        offset += (int)sizeof(node_hdr);

        if (iov[i].len > 0)
        {
            if (iov[i].base == NULL)
            {
                __ERR("zfifo_write_node: null base for len=%d\n", iov[i].len);
                return -1;
            }
            memcpy(dst + offset, iov[i].base, iov[i].len);
            offset += iov[i].len;
        }
    }

    return offset;
}

static int zfifo_read_node(const void *buf, int frame_len, ZFIFO_NODE *iov, int iovcnt)
{
    const char *src = (const char *)buf;
    int offset = 0;
    int i;

    for (i = 0; i < iovcnt; i++)
    {
        NODE_HEADER node_hdr;

        if (offset + (int)sizeof(node_hdr) > frame_len)
        {
            __ERR("zfifo_read_node: broken node header offset=%d frame_len=%d\n", offset, frame_len);
            return -1;
        }

        memcpy(&node_hdr, src + offset, sizeof(node_hdr));
        offset += (int)sizeof(node_hdr);

        if (node_hdr.size < 0 || offset + node_hdr.size > frame_len)
        {
            __ERR("zfifo_read_node: broken node size=%d offset=%d frame_len=%d\n", node_hdr.size, offset, frame_len);
            return -1;
        }

        iov[i].base = (void *)(src + offset);
        iov[i].len = node_hdr.size;

        offset += node_hdr.size;
    }

    return offset;
}

// 不重叠 = 区间 1 完全在区间 2 左边 或 区间 1 完全在区间 2 右边
static int zfifo_ranges_overlap(int offset1, int len1, int offset2, int len2)
{
    int end1 = offset1 + len1;
    int end2 = offset2 + len2;

    return !(end1 <= offset2 || end2 <= offset1);
}

// 按序列号找帧, 从最新帧往前找 效率最高
static ZFIFO_FRAME *zfifo_find_first_from_seq_locked(ZFIFO *zfifo, unsigned long long seq)
{
    ZFIFO_FRAME *frame = zfifo->frame_head;

    while (frame != NULL)
    {
        if (frame->seq >= seq)
        {
            return frame;
        }
        frame = frame->next;
    }

    return NULL;
}

static ZFIFO_FRAME *zfifo_first_frame_locked(ZFIFO_DESC *zfifo_desc)
{
    if (zfifo_desc->cursor != NULL)
    {
        return zfifo_desc->cursor;
    }

    return zfifo_find_first_from_seq_locked(zfifo_desc->zfifo, zfifo_desc->wait_seq);
}

static ZFIFO_FRAME *zfifo_first_flag_frame_locked(ZFIFO_DESC *zfifo_desc)
{
    ZFIFO_FRAME *frame = zfifo_first_frame_locked(zfifo_desc);

    while (frame != NULL && frame->is_flag == 0)
    {
        frame = frame->next;
    }

    return frame;
}

static void zfifo_release_locked_frame_only(ZFIFO_DESC *zfifo_desc)
{
    if (zfifo_desc->locked_frame != NULL)
    {
        if (zfifo_desc->locked_frame->ref_count > 0)
        {
            zfifo_desc->locked_frame->ref_count--;
        }
        zfifo_desc->locked_frame = NULL;
    }
}

static void zfifo_detach_reader_locked(ZFIFO_DESC *desc)
{
    ZFIFO_DESC **pp = &desc->zfifo->reader_list;

    while (*pp != NULL)
    {
        if (*pp == desc)
        {
            *pp = desc->next;
            break;
        }
        pp = &((*pp)->next);
    }

    zfifo_release_locked_frame_only(desc);
    desc->evicted = 1;
}

static int zfifo_evict_stale_readers_locked(ZFIFO *zfifo, int timeout_ms)
{
    ZFIFO_DESC *reader;
    ZFIFO_DESC *next;
    struct timespec ts_now;
    int count = 0;

    anj_mw_get_cputime_ms(&ts_now);

    for (reader = zfifo->reader_list; reader != NULL; reader = next)
    {
        next = reader->next;

        if (reader->is_writer || reader->evicted)
        {
            continue;
        }

        if (reader->locked_frame != NULL &&
            ZFIFO_DIFF_TIME_MS(&reader->lock_ts, &ts_now) >= timeout_ms)
        {
            zfifo_detach_reader_locked(reader);
            __ERR("zfifo evict reader %p fifo=%s\n", reader, reader->zfifo->name);
            count++;
        }
    }

    return count;
}

static void zfifo_consume_locked_frame(ZFIFO_DESC *zfifo_desc, ZFIFO_FRAME *frame)
{
    ZFIFO_FRAME *next = frame->next;

    zfifo_release_locked_frame_only(zfifo_desc);
    zfifo_desc->cursor = next;
    zfifo_desc->wait_seq = frame->seq + 1;
}

// 当某个帧被删除时，遍历所有读端描述符，将指向该待删除帧的读端，重新指向该帧的下一个帧
static void zfifo_repoint_readers_locked(ZFIFO *zfifo, ZFIFO_FRAME *victim)
{
    ZFIFO_DESC *reader = zfifo->reader_list;
    ZFIFO_FRAME *replacement = victim->next;

    while (reader != NULL)
    {
        if (reader->cursor == victim)
        {
            reader->cursor = replacement;
            if (replacement == NULL)
            {
                reader->wait_seq = victim->seq + 1;
            }
        }
        reader = reader->next;
    }
}

// 从最新帧开始往前找I帧
static void zfifo_refresh_flag_frame_locked(ZFIFO *zfifo)
{
    ZFIFO_FRAME *frame = zfifo->frame_tail;

    while (frame != NULL && frame->is_flag == 0)
    {
        frame = frame->prev;
    }

    zfifo->flag_frame = frame;
}

/* 删除一个指定的帧，从链表移除，回收内存，并且保证所有读者不会指向野指针 */
static void zfifo_remove_frame_locked(ZFIFO *zfifo, ZFIFO_FRAME *frame)
{
    int refresh_flag = (zfifo->flag_frame == frame);

    zfifo_repoint_readers_locked(zfifo, frame);

    if (frame->prev != NULL)
    {
        frame->prev->next = frame->next;
    }
    else
    {
        zfifo->frame_head = frame->next;
    }

    if (frame->next != NULL)
    {
        frame->next->prev = frame->prev;
    }
    else
    {
        zfifo->frame_tail = frame->prev;
    }

    if (refresh_flag)
    {
        zfifo_refresh_flag_frame_locked(zfifo);
    }

    if (zfifo->frame_count > 0)
    {
        zfifo->frame_count--;
    }
    anj_mw_free(frame);
}

// 在写入新数据前，遍历所有还没被读完的帧，检查要写入的区域是否和它们重叠
static int zfifo_prepare_overwrite_locked(ZFIFO *zfifo, int offset, int len)
{
    ZFIFO_FRAME *frame = zfifo->frame_head;

    while (frame != NULL)
    {
        ZFIFO_FRAME *next = frame->next;
        if (zfifo_ranges_overlap(offset, len, frame->offset, frame->len))
        {
            if (frame->ref_count > 0)
            {
                return -1;
            }
            else
            {
                zfifo_remove_frame_locked(zfifo, frame);
            }
        }
        frame = next;
    }

    frame = zfifo->frame_head;
    while (frame != NULL)
    {
        ZFIFO_FRAME *next = frame->next;

        if (zfifo_ranges_overlap(offset, len, frame->offset, frame->len))
        {
            zfifo_remove_frame_locked(zfifo, frame);
        }
        frame = next;
    }

    return 0;
}

static int zfifo_alloc_offset_locked(ZFIFO *zfifo, int frame_len)
{
    int tail_side_size = zfifo->buf_size - zfifo->write_offset;
    int head_side_size = zfifo->write_offset;
    int offset;
    int wrap_waste_bytes = 0;

    if (frame_len > zfifo->buf_size)
    {
        return -1;
    }

    if (frame_len <= tail_side_size)
    {
        offset = zfifo->write_offset;
    }
    else
    {
        // 尾部不够写 直接回到缓冲区最开头从头写 （宁愿浪费一点尾部空间，也要保证帧连续）
        offset = 0;
        wrap_waste_bytes = tail_side_size;
    }

    if (zfifo_prepare_overwrite_locked(zfifo, offset, frame_len) != 0)
    {
        return -1;
    }

    if (frame_len <= tail_side_size)
    {
        zfifo->write_offset = offset + frame_len;
    }
    else if (frame_len <= head_side_size)
    {
        zfifo->write_offset = frame_len;
        if (wrap_waste_bytes > 0)
        {
            zfifo->wrap_waste_events++;
            zfifo->last_wrap_waste_bytes = wrap_waste_bytes;
        }
    }
    else
    {
        return -1;
    }

    return offset;
}

static void zfifo_append_frame_locked(ZFIFO *zfifo, ZFIFO_FRAME *frame)
{
    frame->prev = zfifo->frame_tail;
    frame->next = NULL;

    if (zfifo->frame_tail != NULL)
    {
        zfifo->frame_tail->next = frame;
    }
    else
    {
        zfifo->frame_head = frame;
    }

    zfifo->frame_tail = frame;
    zfifo->frame_count++;
    if (frame->is_flag)
    {
        zfifo->flag_frame = frame;
    }
}

static int zfifo_make_timespec(int timeout, struct timespec *ts)
{
    unsigned long long now_ms;

    if (timeout <= 0 || ts == NULL)
    {
        return -1;
    }

    now_ms = anj_mw_get_cputime_ms(NULL);
    now_ms += (unsigned long long)timeout;
    return anj_mw_msecond_to_timespec(ts, now_ms);
}

static int zfifo_lock_frame_locked(ZFIFO_DESC *zfifo_desc, int timeout, int flag_only, ZFIFO_FRAME **out_frame)
{
    ZFIFO *zfifo = zfifo_desc->zfifo;
    struct timespec ts;
    int ret = 0;
    int timed = (zfifo_make_timespec(timeout, &ts) == 0);

    if (out_frame == NULL)
    {
        return -1;
    }

    for (;;)
    {
        ZFIFO_FRAME *frame;

        if (zfifo_desc->locked_frame != NULL)
        {
            frame = zfifo_desc->locked_frame;
        }
        else
        {
            frame = flag_only ? zfifo_first_flag_frame_locked(zfifo_desc) : zfifo_first_frame_locked(zfifo_desc);
            if (frame != NULL && flag_only)
            {
                zfifo_desc->cursor = frame;
                zfifo_desc->wait_seq = frame->seq;
            }
            if (frame != NULL)
            {
                zfifo_desc->locked_frame = frame;
                frame->ref_count++;
                anj_mw_get_cputime_ms(&zfifo_desc->lock_ts);
            }
        }

        if (frame != NULL)
        {
            *out_frame = frame;
            return 1;
        }

        if (!timed)
        {
            return 0;
        }

        ret = pthread_cond_timedwait(&zfifo->cond, &zfifo->mutex, &ts);
        if (ret == ETIMEDOUT)
        {
            return 0;
        }
    }
}

static void zfifo_reset_desc_locked(ZFIFO_DESC *zfifo_desc, ZFIFO_FRAME *frame)
{
    zfifo_release_locked_frame_only(zfifo_desc);
    zfifo_desc->cursor = frame;
    if (frame != NULL)
    {
        zfifo_desc->wait_seq = frame->seq;
    }
    else
    {
        zfifo_desc->wait_seq = zfifo_desc->zfifo->next_seq;
    }
}

static ZFIFO_DESC *zfifo_desc_create(ZFIFO *fifo, int is_writer)
{
    ZFIFO_DESC *zfifo_desc;

    zfifo_desc = (ZFIFO_DESC *)anj_mw_malloc(sizeof(ZFIFO_DESC));
    if (zfifo_desc == NULL)
    {
        __ERR("anj_mw_malloc failed\n");
        return NULL;
    }
    memset(zfifo_desc, 0, sizeof(ZFIFO_DESC));

    zfifo_desc->zfifo = fifo;
    zfifo_desc->is_writer = is_writer;

    anj_mutex_lock(&fifo->mutex);
    zfifo_desc->cursor = fifo->frame_head;
    zfifo_desc->wait_seq = (fifo->frame_head != NULL) ? fifo->frame_head->seq : fifo->next_seq;
    anj_mutex_unlock(&fifo->mutex);

    return zfifo_desc;
}

ZFIFO *zfifo_init(const char *zfifo_name, int size)
{
    ZFIFO *zfifo;
    size_t name_len;

    if (zfifo_name == NULL || size <= 0)
    {
        __ERR("zfifo_init param error\n");
        return NULL;
    }

    zfifo = (ZFIFO *)anj_mw_malloc(sizeof(ZFIFO));
    if (zfifo == NULL)
    {
        __ERR("anj_mw_malloc failed\n");
        return NULL;
    }
    memset(zfifo, 0, sizeof(ZFIFO));

    name_len = strlen(zfifo_name);
    zfifo->name = (char *)anj_mw_malloc(name_len + 1);
    if (zfifo->name == NULL)
    {
        anj_mw_free(zfifo);
        __ERR("anj_mw_malloc failed\n");
        return NULL;
    }
    snprintf(zfifo->name, name_len + 1, "%s", zfifo_name);

    zfifo->buf = anj_mw_malloc(size);
    if (zfifo->buf == NULL)
    {
        anj_mw_free(zfifo->name);
        anj_mw_free(zfifo);
        __ERR("anj_mw_malloc failed\n");
        return NULL;
    }

    zfifo->buf_size = size;
    zfifo->write_offset = 0;
    zfifo->next_seq = 1;
    anj_mutex_create(&zfifo->mutex, 0);

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&zfifo->cond, &attr);
    pthread_condattr_destroy(&attr);

    __DBG("zfifo:%p,data:%p,size:%d\n", zfifo, zfifo->buf, zfifo->buf_size);
    return zfifo;
}

void zfifo_uninit(ZFIFO *zfifo)
{
    if (zfifo == NULL)
    {
        return;
    }

    while (zfifo->reader_list != NULL)
    {
        ZFIFO_DESC *reader = zfifo->reader_list;
        zfifo->reader_list = reader->next;
        anj_mw_free(reader);
    }

    if (zfifo->writer_desc != NULL)
    {
        anj_mw_free(zfifo->writer_desc);
        zfifo->writer_desc = NULL;
    }

    while (zfifo->frame_head != NULL)
    {
        ZFIFO_FRAME *frame = zfifo->frame_head;
        zfifo->frame_head = frame->next;
        anj_mw_free(frame);
    }

    pthread_cond_destroy(&zfifo->cond);
    anj_mutex_destroy(&zfifo->mutex);

    if (zfifo->buf != NULL)
    {
        anj_mw_free(zfifo->buf);
    }
    if (zfifo->name != NULL)
    {
        anj_mw_free(zfifo->name);
    }

    anj_mw_free(zfifo);
}

ZFIFO_DESC *zfifo_open_reader(ZFIFO *fifo)
{
    ZFIFO_DESC *desc;

    if (fifo == NULL)
    {
        return NULL;
    }

    desc = zfifo_desc_create(fifo, 0);
    if (desc == NULL)
    {
        return NULL;
    }

    anj_mutex_lock(&fifo->mutex);
    desc->next = fifo->reader_list;
    fifo->reader_list = desc;
    anj_mutex_unlock(&fifo->mutex);

    return desc;
}

ZFIFO_DESC *zfifo_open_writer(ZFIFO *fifo)
{
    ZFIFO_DESC *desc;

    if (fifo == NULL)
    {
        return NULL;
    }

    anj_mutex_lock(&fifo->mutex);
    if (fifo->writer_desc != NULL)
    {
        anj_mutex_unlock(&fifo->mutex);
        __ERR("writer already exists\n");
        return NULL;
    }
    anj_mutex_unlock(&fifo->mutex);

    desc = zfifo_desc_create(fifo, 1);
    if (desc == NULL)
    {
        return NULL;
    }

    anj_mutex_lock(&fifo->mutex);
    fifo->writer_desc = desc;
    anj_mutex_unlock(&fifo->mutex);

    return desc;
}

void zfifo_close(ZFIFO_DESC *zfifo_desc)
{
    if (zfifo_desc == NULL)
    {
        return;
    }

    anj_mutex_lock(&zfifo_desc->zfifo->mutex);
    if (zfifo_desc->is_writer)
    {
        if (zfifo_desc->zfifo->writer_desc == zfifo_desc)
        {
            zfifo_desc->zfifo->writer_desc = NULL;
        }
    }
    else if (!zfifo_desc->evicted)
    {
        ZFIFO_DESC **pp = &zfifo_desc->zfifo->reader_list;

        while (*pp != NULL)
        {
            if (*pp == zfifo_desc)
            {
                *pp = zfifo_desc->next;
                break;
            }
            pp = &((*pp)->next);
        }

        zfifo_release_locked_frame_only(zfifo_desc);
    }
    else
    {
        zfifo_release_locked_frame_only(zfifo_desc);
    }
    anj_mutex_unlock(&zfifo_desc->zfifo->mutex);

    anj_mw_free(zfifo_desc);
}

int zfifo_writev(ZFIFO_DESC *zfifo_desc, const ZFIFO_NODE *iov, int iovcnt, int flag)
{
    ZFIFO *zfifo;
    ZFIFO_FRAME *frame;
    int frame_len;
    int offset;
    int written;

    if (zfifo_desc == NULL || iov == NULL || !zfifo_desc->is_writer)
    {
        __ERR("zfifo_writev_plus param error\n");
        return -1;
    }

    frame_len = zfifo_calc_len(iov, iovcnt);
    if (frame_len <= 0)
    {
        __ERR("zfifo_writev_plus calc_len failed\n");
        return -1;
    }

    frame = (ZFIFO_FRAME *)anj_mw_malloc(sizeof(ZFIFO_FRAME));
    if (frame == NULL)
    {
        __ERR("anj_mw_malloc failed\n");
        return -1;
    }
    memset(frame, 0, sizeof(ZFIFO_FRAME));

    zfifo = zfifo_desc->zfifo;
    anj_mutex_lock(&zfifo->mutex);

    while (zfifo->frame_count >= ZFIFO_MAX_FRAMES &&
           zfifo->frame_head != NULL &&
           zfifo->frame_head->ref_count == 0)
    {
        zfifo_remove_frame_locked(zfifo, zfifo->frame_head);
    }

    offset = zfifo_alloc_offset_locked(zfifo, frame_len);
    if (offset < 0)
    {
        zfifo_evict_stale_readers_locked(zfifo, ZFIFO_READER_TIMEOUT_MS);
        offset = zfifo_alloc_offset_locked(zfifo, frame_len);
    }
    if (offset < 0)
    {
        anj_mutex_unlock(&zfifo->mutex);
        anj_mw_free(frame);
        __ERR("zfifo write fail: overwrite active frame %s len=%d\n", zfifo->name, frame_len);
        return 0;
    }

    written = zfifo_write_node((char *)zfifo->buf + offset, iov, iovcnt);
    if (written != frame_len)
    {
        anj_mutex_unlock(&zfifo->mutex);
        anj_mw_free(frame);
        __ERR("zfifo_write_node failed written=%d expect=%d\n", written, frame_len);
        return -1;
    }

    frame->seq = zfifo->next_seq++;
    frame->offset = offset;
    frame->len = frame_len;
    frame->is_flag = (flag != 0);
    zfifo_append_frame_locked(zfifo, frame);

    pthread_cond_broadcast(&zfifo->cond);
    anj_mutex_unlock(&zfifo->mutex);

    return written;
}

int zfifo_peek(ZFIFO_DESC *zfifo_desc, ZFIFO_NODE *iov, int iovcnt, int timeout)
{
    ZFIFO_FRAME *frame;
    int ret;

    if (zfifo_desc == NULL || iov == NULL || zfifo_desc->is_writer || zfifo_desc->evicted)
    {
        return -1;
    }

    anj_mutex_lock(&zfifo_desc->zfifo->mutex);
    ret = zfifo_lock_frame_locked(zfifo_desc, timeout, 0, &frame);
    if (ret <= 0)
    {
        anj_mutex_unlock(&zfifo_desc->zfifo->mutex);
        return ret;
    }

    ret = zfifo_read_node((char *)zfifo_desc->zfifo->buf + frame->offset, frame->len, iov, iovcnt);
    if (ret < 0)
    {
        zfifo_release_locked_frame_only(zfifo_desc);
        anj_mutex_unlock(&zfifo_desc->zfifo->mutex);
        return -1;
    }

    anj_mutex_unlock(&zfifo_desc->zfifo->mutex);
    return ret;
}

int zfifo_read_release(ZFIFO_DESC *zfifo_desc, int iovcnt)
{
    ZFIFO_FRAME *frame;
    int ret;

    (void)iovcnt;

    if (zfifo_desc == NULL || zfifo_desc->is_writer || zfifo_desc->evicted)
    {
        return -1;
    }

    anj_mutex_lock(&zfifo_desc->zfifo->mutex);
    frame = zfifo_desc->locked_frame;
    // 容错处理 找没锁的帧进行释放 防止崩溃
    if (frame == NULL)
    {
        frame = zfifo_first_frame_locked(zfifo_desc);
        if (frame == NULL)
        {
            anj_mutex_unlock(&zfifo_desc->zfifo->mutex);
            return 0;
        }

        zfifo_desc->locked_frame = frame;
        frame->ref_count++;
    }

    ret = frame->len;
    zfifo_consume_locked_frame(zfifo_desc, frame);
    anj_mutex_unlock(&zfifo_desc->zfifo->mutex);

    return ret;
}

int zfifo_set_newest_frame(ZFIFO_DESC *zfifo_desc)
{
    ZFIFO_FRAME *frame;

    if (zfifo_desc == NULL || zfifo_desc->is_writer || zfifo_desc->evicted)
    {
        return -1;
    }

    anj_mutex_lock(&zfifo_desc->zfifo->mutex);
    frame = zfifo_desc->zfifo->flag_frame;
    if (frame == NULL)
    {
        frame = zfifo_desc->zfifo->frame_tail;
    }
    zfifo_reset_desc_locked(zfifo_desc, frame);
    anj_mutex_unlock(&zfifo_desc->zfifo->mutex);

    return 0;
}

int zfifo_set_oldest_frame(ZFIFO_DESC *zfifo_desc)
{
    if (zfifo_desc == NULL || zfifo_desc->is_writer || zfifo_desc->evicted)
    {
        return -1;
    }

    anj_mutex_lock(&zfifo_desc->zfifo->mutex);
    zfifo_reset_desc_locked(zfifo_desc, zfifo_desc->zfifo->frame_head);
    anj_mutex_unlock(&zfifo_desc->zfifo->mutex);

    return 0;
}

int zfifo_get_reader_offset_locked(ZFIFO_DESC *zfifo_desc, int *offset)
{
    ZFIFO_FRAME *frame = NULL;

    if (zfifo_desc == NULL || offset == NULL || zfifo_desc->is_writer)
    {
        return -1;
    }

    if (zfifo_desc->locked_frame != NULL)
    {
        *offset = zfifo_desc->locked_frame->offset;
        return 0;
    }

    if (zfifo_desc->cursor != NULL)
    {
        *offset = zfifo_desc->cursor->offset;
        return 0;
    }

    frame = zfifo_first_frame_locked(zfifo_desc);
    *offset = (frame != NULL) ? frame->offset : zfifo_desc->zfifo->write_offset;
    return 0;
}

static int zfifo_calc_remain_percent_locked(ZFIFO *zfifo)
{
    ZFIFO_DESC *reader;
    int max_used_bytes = 0;

    if (zfifo == NULL || zfifo->buf_size <= 0)
    {
        return -1;
    }

    for (reader = zfifo->reader_list; reader != NULL; reader = reader->next)
    {
        int read_offset = zfifo->write_offset;
        int used_bytes;

        if (zfifo_get_reader_offset_locked(reader, &read_offset) != 0)
        {
            read_offset = zfifo->write_offset;
        }

        if (zfifo->write_offset >= read_offset)
        {
            used_bytes = zfifo->write_offset - read_offset;
        }
        else
        {
            used_bytes = zfifo->buf_size - (read_offset - zfifo->write_offset);
        }

        if (used_bytes < 0)
        {
            used_bytes = 0;
        }
        if (used_bytes > zfifo->buf_size)
        {
            used_bytes = zfifo->buf_size;
        }

        if (used_bytes > max_used_bytes)
        {
            max_used_bytes = used_bytes;
        }
    }

    return ((zfifo->buf_size - max_used_bytes) * 100) / zfifo->buf_size;
}

int zfifo_get_info(ZFIFO *zfifo, int *remain_percent)
{
    if (zfifo == NULL || remain_percent == NULL)
    {
        return -1;
    }

    anj_mutex_lock(&zfifo->mutex);
    *remain_percent = zfifo_calc_remain_percent_locked(zfifo);

    anj_mutex_unlock(&zfifo->mutex);

    return 0;
}