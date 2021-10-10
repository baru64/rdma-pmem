#!/bin/python3
import sys
import json
from typing import Optional
import subprocess
from multiprocessing import Process
from time import sleep
from pathlib import Path
from time import ctime

import psutil

benchmark_secs = str(30)


def client(node: str, serveraddr: str, memsize: str, threadnum: int):
    """Runs iperf client"""
    args = [
        "ssh",
        node,
        "iperf3",
        "-c",
        serveraddr,
        "-l",
        memsize,
        "-P",
        str(threadnum),
        "-t",
        benchmark_secs,
        "-f",
        "m",  # format mbits
        "--logfile",
        f"iperf3_client_log_{memsize}_{threadnum}.txt"
    ]
    subprocess.run(args=args, stdout=sys.stdout, stderr=sys.stderr)


def server(
    node: str,
    serveraddr: str,
    memsize: str,
    threadnum: int
):
    """Runs iperf server"""
    args = [
        "ssh",
        node,
        f"iperf3",
        "-B",
        serveraddr,
        "-f",
        "m",  # format mbits
        "--logfile",
        f"iperf3_server_log_{memsize}_{threadnum}.txt",
        "-F",
        f"/pmem0/iperf_test/iperf3_data"
    ]
    subprocess.run(args=args, stdout=sys.stdout, stderr=sys.stderr)


# mem_sizes = ["256", "512", "1024", "2048", "4096", "8192", "12288", "16384", "20480", "24576", "32768", "65536"]
mem_sizes = ["256"]

if __name__ == "__main__":
    client_node = "pmem-4"
    server_node = "pmem-3"
    server_addr = "10.10.0.123"

    for mem_size in mem_sizes:
        # for threadnum in [1, 2, 4, 8, 12, 16]:
        for threadnum in [1]:
            clientproc = Process(
                target=client,
                args=(client_node, server_addr, mem_size, threadnum),
            )
            serverproc = Process(
                target=server,
                args=(server_node, server_addr, mem_size, threadnum),
            )

            serverproc.start()
            sleep(0.1)
            clientproc.start()

            clientproc.join()
            serverproc.kill()
