from __future__ import print_function
from bcc import BPF
from bcc.utils import printb
import time

program = """
#include <uapi/linux/ptrace.h>
#include <linux/types.h>
#include <linux/blk_types.h>
#include <linux/container_of.h>

#include "/home/jyshi/dev/linux-hwe-5.19-5.19.0/drivers/md/bcache/bset.h"
#include "/home/jyshi/dev/linux-hwe-5.19-5.19.0/drivers/md/bcache/closure.h"
#include "/home/jyshi/dev/linux-hwe-5.19-5.19.0/drivers/md/bcache/bcache.h"
#include "/home/jyshi/dev/linux-hwe-5.19-5.19.0/drivers/md/bcache/request.h"

struct data_t {
    char comm[16];

    s64 remaining;

    unsigned int		inode;

    unsigned int        bi_size;

    unsigned int        bi_opf;
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

	op = ((struct data_insert_op *)(__mptr - offsetof(struct data_insert_op, cl)));

    struct data_t data = {};

    data.remaining = cl->remaining.counter;
    data.inode = op->inode;
    data.bi_size = op->bio->bi_iter.bi_size;
    data.bi_opf = op->bio->bi_opf;

    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}

"""

# load BPF program
b = BPF(text=program)

b.attach_kprobe(event="bch_data_insert", fn_name="entry_bch_data_insert")
b.attach_kprobe(event="bch_data_insert_start", fn_name="entry_bch_data_insert_start")


def print_events(cpu, data, size):
    event = b["events"].event(data)

    print("%s " % (event.comm))
    print(bin(event.remaining))
    print(event.inode)
    print(event.bi_size)
    print(bin(event.bi_opf))
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
