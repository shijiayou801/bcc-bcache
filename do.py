from __future__ import print_function
from bcc import BPF
from bcc.utils import printb
import time
from datetime import datetime

program = "do.c"

# load BPF program
b = BPF(src_file = program)



b.attach_kprobe(event="bch_data_insert", 
                fn_name="entry_bch_data_insert")
b.attach_kprobe(event="bch_data_insert_start", 
                fn_name="entry_bch_data_insert_start")
b.attach_kprobe(event="cached_dev_write", 
                fn_name="entry_cached_dev_write")
b.attach_kprobe(event="__bch_keylist_realloc", 
                fn_name="entry___bch_keylist_realloc")
b.attach_kprobe(event="cached_dev_submit_bio", 
                fn_name="entry_cached_dev_submit_bio")
b.attach_kprobe(event="__bch_bucket_alloc_set", 
                fn_name="entry__bch_bucket_alloc_set")
b.attach_kprobe(event="bch_alloc_sectors", 
                fn_name="entry_bch_alloc_sectors")
b.attach_kprobe(event="bch_data_insert_keys",
                fn_name="entry_bch_data_insert_keys")
b.attach_kprobe(event="__bch_submit_bbio",
                fn_name="entry___bch_submit_bbio")
b.attach_kprobe(event="submit_bio_noacct",
                fn_name="entry_submit_bio_noacct")


def print_bch_data_insert(cpu, data, size):
    event = b["bch_data_insert_event"].event(data)

    print("bch_data_insert")
    print("time ", event.start_time / (1e6))
    print("remaining ", bin(event.remaining))
    print("\n")

def print_bch_data_insert_start(cpu, data, size):
    event = b["bch_data_insert_start_event"].event(data)

    print("bch_data_insert_start")
    print("time ", event.start_time / (1e6))
    print("inode ", event.inode)
    print("bi_size ", event.bi_size)
    print("bi_opf ", bin(event.bi_opf))

    print("data_insert_op flags ", bin(event.flags))
    print("\n")

def print_cached_dev_write(cpu, data, size):
    event = b["cached_dev_write_event"].event(data)

    print("cached_dev_write")
    print("time ", event.start_time / (1e6))
    print("dc->has_dirty ", event.has_dirty)
    print("\n")

def print_bch_keylist_realloc(cpu, data, size):
    event = b["bch_keylist_realloc_event"].event(data)

    print("bch_keylist_realloc")
    print("time ", event.start_time / (1e6))
    print("u64s ", event.u64s)
    print("\n")

def print_bch_alloc_sectors(cpu, data, size):
    event = b["bch_alloc_sectors_event"].event(data)

    print("bch_alloc_sectors")
    print("time ", event.start_time / (1e6))
    print("sectors ", event.sectors)
    print("write_point ", event.write_point)
    print("write_prio ", event.write_prio)
    print("wait ", event.wait)
    print("bkey_inode ", event.bkey_inode)
    print("bkey_offset ", event.bkey_offset)
    print("bucket_size %dKB" % (event.bucket_size * 512.0 / 1024))
    print("nr_buckets ", event.nr_buckets)
    print("\n")

def print_cached_dev_submit_bio(cpu, data, size):
    event = b["cached_dev_submit_bio_event"].event(data)

    print("\n")
    print("cached_dev_submit_bio")
    print("time ", event.start_time / (1e6))
    print("bio_opf ", bin(event.bio_opf))
    print("cached_dev_data_offset ", event.cached_dev_data_offset)
    print("bcache_device_inode ", event.bcache_device_inode)
    print("\n")


def print_bch_bucket_alloc_set(cpu, data, size):
    event = b["bch_bucket_alloc_set_event"].event(data)

    print("bch_data_bucket_alloc_set")
    print("time ", event.start_time / (1e6))
    print("\n")


def print_bch_data_insert_keys(cpu, data, size):
    event = b["bch_data_insert_keys_event"].event(data)

    print("bch_data_insert_keys")
    print("time ", event.start_time / (1e6))
    print("flags ", bin(event.flags))
    print("\n")

def print___bch_submit_bbio_event(cpu, data, size):
    event = b["__bch_submit_bbio_event"].event(data)

    print("__bch_submit_bbio")
    print("time ", event.start_time / (1e6))
    print("bkey_offset", event.bkey_offset)
    print("bi_size", event.bi_size)
    print("bio_addr ", hex(event.bio_addr))
    print("\n")


def print_submit_bio_noacct(cpu, data, size):
    event = b["submit_bio_noacct_event"].event(data)

    """
    print("submit_bio_noacct")
    print("time ", event.start_time / (1e6))
    print("bi_sector ", event.bi_sector)
    print("bi_size ", event.bi_size)
    print("bio_addr ", hex(event.bio_addr))
    print("\n")"""

b["cached_dev_submit_bio_event"].open_perf_buffer(print_cached_dev_submit_bio)
b["cached_dev_write_event"].open_perf_buffer(print_cached_dev_write)
b["bch_data_insert_event"].open_perf_buffer(print_bch_data_insert)
b["bch_data_insert_start_event"].open_perf_buffer(print_bch_data_insert_start)
b["bch_keylist_realloc_event"].open_perf_buffer(print_bch_keylist_realloc)
b["bch_alloc_sectors_event"].open_perf_buffer(print_bch_alloc_sectors)
b["bch_bucket_alloc_set_event"].open_perf_buffer(print_bch_bucket_alloc_set)
b["bch_data_insert_keys_event"].open_perf_buffer(print_bch_data_insert_keys)
b["__bch_submit_bbio_event"].open_perf_buffer(print___bch_submit_bbio_event)
b["submit_bio_noacct_event"].open_perf_buffer(print_submit_bio_noacct)




# format output
start = 0
while 1:
    try:
        b.perf_buffer_poll()
        time.sleep(1)
    except KeyboardInterrupt:
        exit()
