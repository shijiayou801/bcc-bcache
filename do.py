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

def print_bch_data_insert(cpu, data, size):
    event = b["bch_data_insert_event"].event(data)

    print("bch_data_insert")
    print("remaining ", bin(event.remaining))
    print("flags:", bin(event.flags))
    print("\n")


def print_bch_data_insert_start(cpu, data, size):
    event = b["bch_data_insert_start_event"].event(data)

    print("bch_data_insert_start")
    print("inode ", event.inode)
    print("bi_size ", event.bi_size)
    print("bi_opf", event.bi_opf)
    print("\n")

def print_cached_dev_write(cpu, data, size):
    event = b["cached_dev_write_event"].event(data)

    print("cached_dev_write")
    print("\n")


b["bch_data_insert_event"].open_perf_buffer(print_bch_data_insert)
b["bch_data_insert_start_event"].open_perf_buffer(print_bch_data_insert_start)
b["cached_dev_write_event"].open_perf_buffer(print_cached_dev_write)



# format output
start = 0
while 1:
    try:
        b.perf_buffer_poll()
        time.sleep(1)
    except KeyboardInterrupt:
        exit()
