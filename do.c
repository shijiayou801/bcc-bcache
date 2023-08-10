#include <uapi/linux/ptrace.h>
#include <linux/types.h>
#include <linux/blk_types.h>
#include <linux/container_of.h>


#include "/home/jyshi/linux-5.19/drivers/md/bcache/bcache.h"
#include "/home/jyshi/linux-5.19/drivers/md/bcache/bset.h"
#include "/home/jyshi/linux-5.19/drivers/md/bcache/btree.h"
#include "/home/jyshi/linux-5.19/drivers/md/bcache/closure.h"
#include "/home/jyshi/linux-5.19/drivers/md/bcache/request.h"


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
	s64 			remaining;
	u16              	flags;
};
BPF_PERF_OUTPUT(bch_data_insert_event);


int entry_bch_data_insert(
	struct pt_regs *ctx,
	struct closure *cl) 
{
    	struct bch_data_insert_event_t data = {};

    	data.remaining = cl->remaining.counter;

    	bch_data_insert_event.perf_submit(ctx, &data, sizeof(data));
    	return 0;
}



struct bch_data_insert_start_event_t {
	// Bio
	unsigned int		inode;
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

	data.inode = op->inode;
	data.bi_size = op->bio->bi_iter.bi_size;
	data.bi_opf = op->bio->bi_opf;

	bch_data_insert_start_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}

struct cached_dev_write_event_t {

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


	cached_dev_write_event.perf_submit(ctx, &data, sizeof(data));
	return 0;
}
