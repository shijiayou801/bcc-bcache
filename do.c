#include <uapi/linux/ptrace.h>
#include <linux/types.h>
#include <linux/blk_types.h>
#include <linux/container_of.h>


#include "../linux-5.19/drivers/md/bcache/bcache.h"
#include "../linux-5.19/drivers/md/bcache/bset.h"
#include "../linux-5.19/drivers/md/bcache/btree.h"
#include "../linux-5.19/drivers/md/bcache/closure.h"
#include "../linux-5.19/drivers/md/bcache/request.h"


struct search {
	/* Stack frame for bio_complete */
	struct closure		cl;

	struct bbio		bio;
	struct bio		*orig_bio;
	struct bio		*cache_miss;
	struct bcache_device	*d;

	unsigned int		insert_bio_sectors;
	unsigned int		recoverable:1;
	unsigned int		write:1;
	unsigned int		read_dirty_data:1;
	unsigned int		cache_missed:1;

	struct block_device	*orig_bdev;
	unsigned long		start_time;

	struct btree_op		op;
	struct data_insert_op	iop;
};

struct bch_data_insert_event_t {
	u64             start_time;

	s64 			remaining;
};
BPF_PERF_OUTPUT(bch_data_insert_event);


int entry_bch_data_insert(
	struct pt_regs *ctx,
	struct closure *cl) 
{
    	struct bch_data_insert_event_t data = {};
	struct data_insert_op *op;

	data.start_time = bpf_ktime_get_boot_ns();
    	data.remaining = cl->remaining.counter;

    	bch_data_insert_event.perf_submit(ctx, &data, sizeof(data));
    	return 0;
}



struct bch_data_insert_start_event_t {
	u64			start_time;

	u16			flags;

	// Bio
	unsigned int		inode;	 // The inode of the device
	unsigned int		bi_size;
	unsigned int		bi_opf;
};
BPF_PERF_OUTPUT(bch_data_insert_start_event);

int entry_bch_data_insert_start(
	struct pt_regs *ctx,
	struct closure *cl)
{
	struct bch_data_insert_start_event_t data = {};
	struct data_insert_op *op;

	void *__mptr = (void *)(cl);
	op = (struct data_insert_op *)
		(__mptr - offsetof(struct data_insert_op, cl));

	data.start_time = bpf_ktime_get_boot_ns();

	data.inode = op->inode;
	data.bi_size = op->bio->bi_iter.bi_size;
	data.bi_opf = op->bio->bi_opf;

	data.flags = op->flags;

	bch_data_insert_start_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct cached_dev_write_event_t {
	u64			start_time;
	int			has_dirty;
};
BPF_PERF_OUTPUT(cached_dev_write_event);

int entry_cached_dev_write(
	struct pt_regs *ctx,
    	struct cached_dev *dc,
    	struct search *s)
{
	struct cached_dev_write_event_t data = {};
 	struct closure *cl = &s->cl;
	struct bio *bio = &s->bio.bio;
	struct bkey start = KEY(dc->disk.id, bio->bi_iter.bi_sector, 0);
	struct bkey end = KEY(dc->disk.id, bio_end_sector(bio), 0);

	data.start_time = bpf_ktime_get_boot_ns();
	data.has_dirty = dc->has_dirty.counter;

	cached_dev_write_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct bch_keylist_realloc_event_t {
	u64			start_time;

	u64			u64s;
};
BPF_PERF_OUTPUT(bch_keylist_realloc_event);

int entry___bch_keylist_realloc(
	struct pt_regs *ctx,
	struct keylist *l,
	unsigned int u64s)
{
	struct bch_keylist_realloc_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();
	data.u64s = u64s;

	bch_keylist_realloc_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}


struct open_bucket {
	struct list_head	list;
	unsigned int		last_write_point;
	unsigned int		sectors_free;
	BKEY_PADDED(key);
};

struct bch_alloc_sectors_event_t {
	u64			start_time;

	unsigned int 		sectors;
	unsigned int 		write_point;
	unsigned int 		write_prio;
	unsigned int 		wait;

	unsigned int 		bkey_inode;
	u64			bkey_offset;

	u16			bucket_size;

	u8			nr_buckets;
};
BPF_PERF_OUTPUT(bch_alloc_sectors_event);


static struct open_bucket *get_data(struct list_head *head)
{
	struct open_bucket *b;

	void *__mptr = (void *)(head);
	b = ((struct open_bucket *)(__mptr - offsetof(struct open_bucket, list)));

	return b;
}

static struct open_bucket *last_entry(struct list_head *list_head)
{
	return get_data(list_head->prev);
}

static bool entry_is_head(
		struct list_head *list_head,
		struct open_bucket *b)
{
	return &b->list == list_head;
}

static struct open_bucket *prev_entry(struct open_bucket *b)
{
	return get_data(b->list.prev);
}


BPF_ARRAY(bucket_inode, u32, 128);

/*
 * Allocates some space in the cache to write to, and k to point to the newly
 * allocated space, and updates KEY_SIZE(k) and KEY_OFFSET(k) (to point to the
 * end of the newly allocated space).
 *
 * May allocate fewer sectors than @sectors, KEY_SIZE(k) indicates how many
 * sectors were actually allocated.
 *
 * If s->writeback is true, will not fail.
 */
// param
// @sectors: the number of sectors the user requested to write
//
//
// 
int entry_bch_alloc_sectors(
		struct pt_regs *ctx,
		struct cache_set *c,
		struct bkey *k,
		unsigned int sectors,
		unsigned int write_point,
		unsigned int write_prio,
		bool wait)
{
	struct open_bucket *b;
	struct bch_alloc_sectors_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();
	data.sectors = sectors;
	data.write_point = write_point;
	data.write_prio = write_prio;
	data.wait = wait;
	data.nr_buckets = 0;

	// TODO
	data.bkey_inode = 
		((k->high >> 0) & ~(~0ULL << 20));

	// The Bkey's offset is [dc->sb.data_offset] + user's sector's offset
	// If user writes at offset 10 * 512,
	// then Bkey's offset is at [dc->sb.data_offset] + 10
	// data_offset is printed to be 16
	data.bkey_offset = k->low;

	data.bucket_size = c->cache->sb.bucket_size;

	// Reverse loop
	b = last_entry(&c->data_buckets);
	for (int i = 0; i < 128; ++i) {
		++data.nr_buckets;

		//u32 *val = bucket_inode.lookup(&i);
		//lock_xadd(val, b->key);

	     	b = prev_entry(b);
		if (entry_is_head(&c->data_buckets, b)) {
			break;
		}
	}

	bch_alloc_sectors_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}



struct cached_dev_submit_bio_event_t {
	u64			start_time;

	u32			bio_opf;

	
	u64			cached_dev_data_offset;
	u32			bcache_device_inode;
};
BPF_PERF_OUTPUT(cached_dev_submit_bio_event);

int entry_cached_dev_submit_bio(
		struct pt_regs *ctx,
		struct bio *bio)
{
	struct cached_dev_submit_bio_event_t data = {};
	struct block_device *orig_bdev = bio->bi_bdev;
	struct bcache_device *d = orig_bdev->bd_disk->private_data;
	struct cached_dev *dc;

	void *__mptr = (void *)(d);
	dc = (struct cached_dev *)
		(__mptr - offsetof(struct cached_dev, disk));

	// Right now, we are only interested in write requests
	if ((bio->bi_opf & REQ_OP_MASK) != REQ_OP_WRITE) {
		return 0;
	}

	data.start_time = bpf_ktime_get_boot_ns();
	data.bio_opf = bio->bi_opf;
	data.cached_dev_data_offset = dc->sb.data_offset;
	data.bcache_device_inode = d->id;

	cached_dev_submit_bio_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}



struct bch_bucket_alloc_set_t {
	u64			start_time;
};
BPF_PERF_OUTPUT(bch_bucket_alloc_set_event);

int entry__bch_bucket_alloc_set(
		struct pt_regs *ctx,
		struct cache_set *c, unsigned int reserve,
		struct bkey *k, bool wait)
{
	struct bch_bucket_alloc_set_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	bch_bucket_alloc_set_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}



struct bch_data_insert_keys_event_t {
	u64			start_time;

	u16			flags;
};
BPF_PERF_OUTPUT(bch_data_insert_keys_event);

int entry_bch_data_insert_keys(
		struct pt_regs *ctx,
		struct closure *cl)
{
	struct bch_data_insert_keys_event_t data = {};
	struct data_insert_op *op;

	
	void *__mptr = (void *)(cl);
	op = (struct data_insert_op *)
		(__mptr - offsetof(struct data_insert_op, cl));

	data.start_time = bpf_ktime_get_boot_ns();
	data.flags = op->flags;

	bch_data_insert_keys_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}


struct __bch_submit_bbio_event_t {
	u64			start_time;

	u16			flags;
};
BPF_PERF_OUTPUT(__bch_submit_bbio_event);

int entry___bch_submit_bbio(
		struct pt_regs *ctx,
		struct bio *bio, 
		struct cache_set *c)
{
	struct __bch_submit_bbio_event_t data = {};
	struct data_insert_op *op;

	if ((bio->bi_opf & REQ_OP_MASK) != REQ_OP_WRITE) {
		return 0;
	}

	data.start_time = bpf_ktime_get_boot_ns();
	data.flags = op->flags;

	__bch_submit_bbio_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}
