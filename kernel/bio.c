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
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf *hashmap[BIOHASHSZ];
  struct spinlock hashmap_bucket_lock[BIOHASHSZ];
} bcache;

struct hashmap_iter {
  struct buf **cur_bucket;
  struct buf *cur_buf;
  struct buf **prev;
};

struct hashmap_iter hashmap_end() {
  struct hashmap_iter end = {0, 0, 0};
  return end;
}

struct hashmap_iter hashmap_begin(struct buf *hashmap[]) {
  for (int i = 0; i < BIOHASHSZ; i++)
  {
    if (hashmap[i]) {
      struct hashmap_iter res = {
        &hashmap[i], 
        hashmap[i], 
        &hashmap[i]
      };
      return res;
    }
  }
  return hashmap_end();
}

int is_hashmap_end(struct hashmap_iter iter) {
  if (iter.cur_bucket == 0) {
    return 1;
  }
  return 0;
}

struct hashmap_iter hashmap_next(struct hashmap_iter iter, struct buf *hashmap[]) {
  if (is_hashmap_end(iter))
    return iter;

  iter.prev = &iter.cur_buf->next;
  iter.cur_buf = iter.cur_buf->next;
  if (iter.cur_buf) {
    return iter;
  }

  for (struct buf **bucket = iter.cur_bucket + 1; bucket < hashmap + BIOHASHSZ; bucket++)
  {
    if (*bucket) {
      struct hashmap_iter res = {
        bucket,
        *bucket, 
        bucket
      };
      return res;
    }
  }
  return hashmap_end();
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.hashmap[0] = &bcache.buf[0];
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->blockno = 0;
    b->refcnt = 0;
    b->next = (b == bcache.buf + NBUF - 1) ? (struct buf *)0 : b + 1;
    initsleeplock(&b->lock, "buffer");
  }

  for (int i = 1; i < BIOHASHSZ; i++) {
    bcache.hashmap[i] = 0;
  }

  for (int i = 0; i < BIOHASHSZ; i++) {
    initlock(&bcache.hashmap_bucket_lock[i], "bcache.bucket");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  struct spinlock * bucket_lock = &bcache.hashmap_bucket_lock[blockno % BIOHASHSZ];

  acquire(bucket_lock);
  for (b = bcache.hashmap[blockno % BIOHASHSZ]; b != 0; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(bucket_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(bucket_lock);

  acquire(&bcache.lock);
  struct hashmap_iter victim = hashmap_end();
  uint victim_tick = -1;
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (struct hashmap_iter iter = hashmap_begin(bcache.hashmap); !is_hashmap_end(iter); iter = hashmap_next(iter, bcache.hashmap)) {
    b = iter.cur_buf;
    if (b->refcnt == 0) {
      if (b->tick < victim_tick) {
        victim = iter;
        victim_tick = b->tick;
      }
    }
  }
  if (is_hashmap_end(victim))
    panic("bget: no buffers");

  uint old_blockno = victim.cur_buf->blockno;



  acquire(&bcache.hashmap_bucket_lock[blockno % BIOHASHSZ]);
  if (blockno % BIOHASHSZ != old_blockno % BIOHASHSZ)
    acquire(&bcache.hashmap_bucket_lock[old_blockno % BIOHASHSZ]);
  victim.cur_buf->dev = dev;
  victim.cur_buf->blockno = blockno;
  victim.cur_buf->valid = 0;
  victim.cur_buf->refcnt = 1;

  *victim.prev = victim.cur_buf->next;
  victim.cur_buf->next = bcache.hashmap[blockno % BIOHASHSZ];
  bcache.hashmap[blockno % BIOHASHSZ] = victim.cur_buf;
  if (blockno % BIOHASHSZ != old_blockno % BIOHASHSZ)
    release(&bcache.hashmap_bucket_lock[old_blockno % BIOHASHSZ]);
  release(&bcache.hashmap_bucket_lock[blockno % BIOHASHSZ]);
  release(&bcache.lock);
  acquiresleep(&victim.cur_buf->lock);
  return victim.cur_buf;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    acquire(&tickslock);
    b->tick = ticks;
    release(&tickslock);
  }

  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


