from __future__ import print_function
from bcc import BPF
from bcc.utils import printb
import time

program = """
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


struct data_t {
    char comm[16];

    // Closure
    s64 remaining;

    // Bio
    unsigned int		inode;
    unsigned int        bi_size;
    unsigned int        bi_opf;

    // Search
    //     data_insert_op
    u16              flags;
};
BPF_PERF_OUTPUT(events);


int entry_bch_data_insert(
    struct pt_regs *ctx,
    struct closure *cl) 
{
    struct data_t data = {};

    bpf_get_current_comm(&data.comm, sizeof(data.comm));

    data.remaining = cl->remaining.counter;

    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}

int entry_bch_data_insert_start(
    struct pt_regs *ctx,
    struct closure *cl) 
{
    struct data_insert_op *op;

    void *__mptr = (void *)(cl);

	op = (struct data_insert_op *)(__mptr - offsetof(struct data_insert_op, cl));

    struct data_t data = {};

    data.remaining = cl->remaining.counter;
    data.inode = op->inode;
    data.bi_size = op->bio->bi_iter.bi_size;
    data.bi_opf = op->bio->bi_opf;

    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}


int entry_cached_dev_write(
    struct pt_regs *ctx,
    struct cached_dev *dc,
    struct search *s)
{

    struct closure *cl = &s->cl;
	struct bio *bio = &s->bio.bio;
	struct bkey start = KEY(dc->disk.id, bio->bi_iter.bi_sector, 0);
	struct bkey end = KEY(dc->disk.id, bio_end_sector(bio), 0);

    struct data_t data = {};

    data.flags = s->iop.flags;

    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}

"""

# load BPF program
b = BPF(text=program)

b.attach_kprobe(event="bch_data_insert", fn_name="entry_bch_data_insert")
b.attach_kprobe(event="bch_data_insert_start", fn_name="entry_bch_data_insert_start")
b.attach_kprobe(event="cached_dev_write", fn_name="entry_cached_dev_write")

###bch_writeback_add

def print_events(cpu, data, size):
    event = b["events"].event(data)

    print("%s " % (event.comm))
    print(bin(event.remaining))
    print(event.inode)
    print(event.bi_size)
    print(bin(event.bi_opf))
    print("flags:", bin(event.flags))
    print("\n")

b["events"].open_perf_buffer(print_events)

# format output
start = 0
while 1:
    try:
        b.perf_buffer_poll()
        time.sleep(1)
    except KeyboardInterrupt:
        exit()
