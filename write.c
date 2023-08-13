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
	unsigned int 		bi_sector;
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
	data.bi_sector = op->bio->bi_iter.bi_sector;
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


struct bch_alloc_sectors_event_t {
	u64			start_time;

	unsigned int 		sectors;
	unsigned int 		write_point;
	unsigned int 		write_prio;
	unsigned int 		wait;

	unsigned int 		bkey_inode;

	u16			bucket_size;

	u8			nr_data_buckets;

	u64			bkey_ptrs;

};
BPF_PERF_OUTPUT(bch_alloc_sectors_event);

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
	data.nr_data_buckets = 0;

	// TODO
	data.bkey_inode = 
		((k->high >> 0) & ~(~0ULL << 20));

	data.bucket_size = c->cache->sb.bucket_size;

	// Reverse loop
	b = last_entry(&c->data_buckets);

	for (int i = 0; i < 128; ++i) {
		++data.nr_data_buckets;
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

static u64 X_PTR_OFFSET(const struct bkey *k, unsigned int i)
{ 
	const u64 *v = &k->ptr[i];
	return ((*v) >> 8) & ~(~0ULL << 43);
}

static u64 X_PTR_GEN(const struct bkey *k, unsigned int i)
{ 
	const u64 *v = &k->ptr[i];
	return ((*v) >> 0) & ~(~0ULL << 8);
}

struct __bch_submit_bbio_event_t {
	u64			start_time;

	u16			flags;
	u64			lba_on_cache_device;
	u32			bi_size;
	u64			bio_addr;
};
BPF_PERF_OUTPUT(__bch_submit_bbio_event);

int entry___bch_submit_bbio(
		struct pt_regs *ctx,
		struct bio *bio, 
		struct cache_set *c)
{
	struct __bch_submit_bbio_event_t data = {};
	struct bbio *b;

	void *__mptr = (void *)(bio);
	b = (struct bbio *)
		(__mptr - offsetof(struct bbio, bio));

	if ((bio->bi_opf & REQ_OP_MASK) != REQ_OP_WRITE) {
		return 0;
	}

	data.start_time = bpf_ktime_get_boot_ns();
	data.lba_on_cache_device = X_PTR_OFFSET(&b->key, 0);
	data.bi_size = bio->bi_iter.bi_size;
	data.bio_addr = (u64)bio;

	__bch_submit_bbio_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct submit_bio_noacct_event_t {
	u64			start_time;

	u64			bi_sector;
	u32			bi_size;
	u64			bio_addr;
};
BPF_PERF_OUTPUT(submit_bio_noacct_event);

int entry_submit_bio_noacct(
		struct pt_regs *ctx,
		struct bio *bio)
{
	struct submit_bio_noacct_event_t data = {};
	u64	bio_bpf = REQ_OP_WRITE | 
			  REQ_SYNC | REQ_META | REQ_PREFLUSH | REQ_FUA;

	if ((bio->bi_opf & REQ_OP_MASK) != REQ_OP_WRITE) {
		return 0;
	}

	if (bio->bi_opf != bio_bpf)
		return 0;

	data.start_time = bpf_ktime_get_boot_ns();
	data.bi_sector = bio->bi_iter.bi_sector;
	data.bi_size = bio->bi_iter.bi_size;
	data.bio_addr = (u64)bio;

	submit_bio_noacct_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}



struct bch_submit_bbio_event_t {
	u64			start_time;

	u64			bkey_ptrs;
	u64			bkey_dirty;
	u64			bkey_size;
	u64			inode_on_backing_device;
	u64			lba_on_backing_device;
	u64			lba_on_cache_device;
	u64			bucket_gen;

	u64			sb_nbuckets;
	u16			sb_nr_in_set;
	u16			sb_nr_this_dev;
	u16			sb_first_bucket;
	u16			sb_keys;

	unsigned long		initial_free;
	u64			bio_addr;
};
BPF_PERF_OUTPUT(bch_submit_bbio_event);

int entry_bch_submit_bbio(
		struct pt_regs *ctx,
		struct bio *bio, struct cache_set *c,
		struct bkey *k, unsigned int ptr)
{
	struct bch_submit_bbio_event_t data = {};
	struct bbio *b;

	void *__mptr = (void *)(bio);
	b = (struct bbio *)(__mptr - offsetof(struct bbio, bio));

	if ((bio->bi_opf & REQ_OP_MASK) != REQ_OP_WRITE) {
		return 0;
	}

	data.start_time = bpf_ktime_get_boot_ns();

	data.bkey_ptrs = (k->high >> 60) & ~(~0ULL << 3);
	data.bkey_dirty = (k->high >> 36) & ~(~0ULL << 1);
	data.bkey_size = (k->high >> 20) & ~(~0ULL << 16);
	data.inode_on_backing_device = (k->high >> 0) & ~(~0ULL << 20);

	data.lba_on_backing_device = k->low;
	data.lba_on_cache_device = X_PTR_OFFSET(k, 0);
	data.bucket_gen = X_PTR_GEN(k, 0);
	data.bio_addr = (u64)bio;

	data.sb_nbuckets = c->cache->sb.nbuckets;
	data.sb_nr_in_set = c->cache->sb.nr_in_set;
	data.sb_nr_this_dev = c->cache->sb.nr_this_dev;
	data.sb_first_bucket = c->cache->sb.first_bucket;
	data.sb_keys = c->cache->sb.keys;
	data.initial_free = roundup_pow_of_two(20480) >> 10;

	bch_submit_bbio_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}




struct cached_dev_write_complete_event_t {
	u64			start_time;

};
BPF_PERF_OUTPUT(cached_dev_write_complete_event);

int entry_cached_dev_write_complete(
		struct pt_regs *ctx,
		struct bio *bio, struct cache_set *c,
		struct bkey *k, unsigned int ptr)
{
	struct cached_dev_write_complete_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	cached_dev_write_complete_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}




struct bch_data_insert_endio_event_t {
	u64			start_time;

};
BPF_PERF_OUTPUT(bch_data_insert_endio_event);

int entry_bch_data_insert_endio(
		struct pt_regs *ctx,
		struct bio *bio, struct cache_set *c,
		struct bkey *k, unsigned int ptr)
{
	struct bch_data_insert_endio_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	bch_data_insert_endio_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}





struct write_dirty_event_t {
	u64			start_time;

};
BPF_PERF_OUTPUT(write_dirty_event);

int entry_write_dirty(
		struct pt_regs *ctx,
		struct closure *cl)
{
	struct write_dirty_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	write_dirty_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}



struct bch_journal_event_t {
	u64			start_time;

	u64			keylist_keys_p;
	u64			keylist_top_p;
	u64			inline_keys;
};
BPF_PERF_OUTPUT(bch_journal_event);

int entry_bch_journal(
		struct pt_regs *ctx,
		struct cache_set *c,
		struct keylist *keys,
		struct closure *parent)
{
	struct bch_journal_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	data.keylist_keys_p = (u64)keys->keys_p;
	
	data.keylist_top_p = (u64)keys->top_p;

	data.inline_keys = (u64)&keys->inline_keys;

	bch_journal_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct journal_wait_for_write_event_t {
	u64			start_time;
	u32			nkeys;

};
BPF_PERF_OUTPUT(journal_wait_for_write_event);

int entry_journal_wait_for_write(
		struct pt_regs *ctx,
		struct cache_set *c,
		unsigned int nkeys)
{
	struct journal_wait_for_write_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();
	data.nkeys = nkeys;

	journal_wait_for_write_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}


struct bch_btree_insert_event_t {
	u64			start_time;

};
BPF_PERF_OUTPUT(bch_btree_insert_event);

int entry_bch_btree_insert(
		struct pt_regs *ctx)
{
	struct bch_btree_insert_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	bch_btree_insert_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct journal_try_write_event_t {
	u64			start_time;

};
BPF_PERF_OUTPUT(journal_try_write_event);

int entry_journal_try_write(
		struct pt_regs *ctx)
{
	struct journal_try_write_event_t data = {};

	data.start_time = bpf_ktime_get_boot_ns();

	journal_try_write_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

static u32 x__set_bytes(struct jset *i, u32 k)
{
	return (sizeof(*(i)) + (i->keys) * sizeof(uint64_t));
}

static u32 x_set_blocks(struct jset *i, u32 block_bytes)
{
	u32 n = x__set_bytes(i, i->keys);
	u32 d = block_bytes;

	return ((n) + (d) - 1) / (d);
}

struct journal_write_unlocked_event_t {
	u64			start_time;

	u64			need_write;
	u64			journal_blocks_free;
	u32			block_bytes;
	u16			block_size;
	u32			sectors;

	u64			seq;
	u64			journal_bkey_ptrs;

	u64			bio_addr;
};
BPF_PERF_OUTPUT(journal_write_unlocked_event);

int entry_journal_write_unlocked(
		struct pt_regs *ctx,
		struct closure *cl)
{
	struct journal_write_unlocked_event_t data = {};
	struct cache_set *c;
	struct cache *ca;
	struct journal_write *w;
	struct bkey *k;
	struct bio *bio;

	void *__mptr = (void *)(cl);
	c = (struct cache_set *)
		(__mptr - offsetof(struct cache_set, journal.io));
	ca = c->cache;
	w = c->journal.cur;

        k = &c->journal.key;
	bio = &ca->journal.bio;

	data.start_time = bpf_ktime_get_boot_ns();
	data.block_size = (ca)->sb.block_size;			// The size of a block in sectors
	data.block_bytes = data.block_size << 9;		// The size of a block in bytes
	data.need_write = w->need_write;
	data.journal_blocks_free = x_set_blocks(w->data, data.block_bytes);
	data.sectors = x_set_blocks(w->data, data.block_bytes) * data.block_size;
	data.bio_addr = (u64)bio;

	data.seq = w->data->seq;
	data.journal_bkey_ptrs = (k->high >> 60) & ~(~0ULL << 3);

	journal_write_unlocked_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}


struct bch_bio_map_event_t {
	u64			start_time;

	u64			seq;
	u64			last_seq;
	u64			bio_addr;
};
BPF_PERF_OUTPUT(bch_bio_map_event);

int entry_bch_bio_map(
		struct pt_regs *ctx,
		struct bio *bio, 
		void *base)
{
	struct bch_bio_map_event_t data = {};
	struct jset		*d = (struct jset *)base;
	
	if (d == NULL)
		return 0;

	data.start_time = bpf_ktime_get_boot_ns();

	data.seq = d->seq;
	data.last_seq = d->last_seq;
	data.bio_addr = (u64)bio;

	bch_bio_map_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}
