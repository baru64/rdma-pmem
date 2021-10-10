#!/bin/python3
from typing import Optional
from time import ctime

import psutil

# build_path = "/home/vagrant/host/rwbenchmark2/build"
build_path = "/home/inf126145/code/rdma-pmem/rwbenchmark2/build"
benchmark_secs = str(60)


def find_process_by_name(name: str) -> Optional[psutil.Process]:
    "Return a list of processes matching 'name'."
    process = None
    for p in psutil.process_iter(['name']):
        if p.info['name'] == name:
            process = p
    return process


def monitor_benchmark(program: str, label: str):
    process = None
    while process is None:
        process = find_process_by_name(program)

    # monitor thing here
    with open(f"./info_{program}_{label}.txt", "w") as f:
        f.write("t,cpu_percent,rss\n")
        while True:
            try:
                cpu = process.cpu_percent()
                memory_info = process.memory_info()
                f.write(f"{ctime()},{cpu},{memory_info['rss']}")
            except psutil.NoSuchProcess:
                break

