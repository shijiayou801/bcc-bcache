from __future__ import print_function
from bcc import BPF
from bcc.utils import printb
import time

program = "do.c"

# load BPF program
b = BPF(src_file = program)

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
