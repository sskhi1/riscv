// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
    // struct spinlock lock;
    struct buf buf[NBUF];

    // Linked list of all buffers, through prev/next.
    // Sorted by how recently the buffer was used.
    // head.next is most recent, head.prev is least.
    struct buf hash_table_buckets[NBUCKETS];
    struct spinlock hash_table_bucket_locks[NBUCKETS];
} bcache;

void
binit(void) {
    struct buf *b;

    // initlock(&bcache.lock, "bcache");
    for (int i = 0; i < NBUCKETS; i++) {
        initlock(&bcache.hash_table_bucket_locks[i], "bcache_bucket_lock");
        bcache.hash_table_buckets[i].next = &bcache.hash_table_buckets[i];
        bcache.hash_table_buckets[i].prev = &bcache.hash_table_buckets[i];
    }

    // Create linked list of buffers
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.hash_table_buckets[b->blockno % NBUCKETS].next;
        b->prev = &bcache.hash_table_buckets[b->blockno % NBUCKETS];
        initsleeplock(&b->lock, "buffer");
        bcache.hash_table_buckets[b->blockno % NBUCKETS].next->prev = b;
        bcache.hash_table_buckets[b->blockno % NBUCKETS].next = b;
    }
}

void
finishResBuffer(struct buf *resBuffer, uint dev, uint blockno, int bucketIndex) {
    int lruBufferBucketIndex = resBuffer->blockno % NBUCKETS;
    resBuffer->valid = 0;
    resBuffer->dev = dev;
    resBuffer->blockno = blockno;
    resBuffer->refcnt = 1;

    if (lruBufferBucketIndex != bucketIndex) {
        resBuffer->next->prev = resBuffer->prev;
        resBuffer->prev->next = resBuffer->next;
        resBuffer->next = bcache.hash_table_buckets[bucketIndex].next;
        resBuffer->prev = &bcache.hash_table_buckets[bucketIndex];
        bcache.hash_table_buckets[bucketIndex].next->prev = resBuffer;
        bcache.hash_table_buckets[bucketIndex].next = resBuffer;
    }
    release(&bcache.hash_table_bucket_locks[lruBufferBucketIndex]);
    acquiresleep(&resBuffer->lock);
}

struct buf *
getBufferForBlock(uint dev, uint blockno, int bucketIndex) {
    struct buf *resBuffer = 0;
    int found = 0;
    int lastBucketIndex = -1;
    for (int i = 0; i < NBUCKETS && !found; i++) {
        acquire(&bcache.hash_table_bucket_locks[i]);
        if (resBuffer != 0) {
            lastBucketIndex = resBuffer->blockno % NBUCKETS;
            if (lastBucketIndex != i) {
                release(&bcache.hash_table_bucket_locks[lastBucketIndex]);
            }
        }
        struct buf *buffer = bcache.hash_table_buckets[i].next;
        while (buffer != &bcache.hash_table_buckets[i]) {
            if (buffer->refcnt == 0) {
                resBuffer = buffer;
                found = 1;
                break;
            }
            buffer = buffer->next;
        }

        if (!found)
            release(&bcache.hash_table_bucket_locks[i]);
    }
    if (lastBucketIndex != -1 && !found)
        release(&bcache.hash_table_bucket_locks[lastBucketIndex]);

    if (resBuffer == 0) panic("bget: no buffers");

    finishResBuffer(resBuffer, dev, blockno, bucketIndex);
    return resBuffer;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno) {
    struct buf *b;

    int bucketIndex = blockno % NBUCKETS;

//  for (int _ = 0; _ < 5; _++) {
    acquire(&bcache.hash_table_bucket_locks[bucketIndex]);
    b = bcache.hash_table_buckets[bucketIndex].next;
    while (b != &bcache.hash_table_buckets[bucketIndex]) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.hash_table_bucket_locks[bucketIndex]);
            acquiresleep(&b->lock);
            return b;
        }
        b = b->next;
    }
    release(&bcache.hash_table_bucket_locks[bucketIndex]);
    // }
    return getBufferForBlock(dev, blockno, bucketIndex);
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno) {
    struct buf *b;

    b = bget(dev, blockno);
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    releasesleep(&b->lock);
    acquire(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it.
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.hash_table_buckets[b->blockno % NBUCKETS].next;
        b->prev = &bcache.hash_table_buckets[b->blockno % NBUCKETS];
        bcache.hash_table_buckets[b->blockno % NBUCKETS].next->prev = b;
        bcache.hash_table_buckets[b->blockno % NBUCKETS].next = b;
    }
    release(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
}

void
bpin(struct buf *b) {
    acquire(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
    b->refcnt++;
    release(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
}

void
bunpin(struct buf *b) {
    acquire(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
    b->refcnt--;
    release(&bcache.hash_table_bucket_locks[b->blockno % NBUCKETS]);
}


