#!/bin/python3
import sys
import json
from typing import Optional
import subprocess
from multiprocessing import Process
from time import sleep
from pathlib import Path

# build_path = "/home/vagrant/host/rwbenchmark2/build"
build_path = "/home/inf126145/code/rdma-pmem/rwbenchmark2/build"
benchmark_secs = str(60)


def client(program: str, node: str, serveraddr: str, memsize: str, threadnum: int):
    """Runs client part of benchmark"""
    args = [
        "ssh",
        node,
        f"{build_path}/{program}",
        "-s",
        serveraddr,
        "-S",
        memsize,
        "-v",
        "-c",
        str(threadnum),
        "-t",
        benchmark_secs
    ]
    output = subprocess.check_output(args=args, stderr=sys.stderr)
    result = output.decode("utf-8").strip().split(";")
    print(f"result program: {program} th: {threadnum}: ", result)
    RESULTS = None

    file_path = Path("./results.json")
    if not file_path.is_file():
        # create file
        with open("results.json", "w+") as f:
            json.dump({}, f)

    with open("results.json", "r") as f:
        RESULTS = json.load(f)
        if not mem_size in RESULTS:
            RESULTS[mem_size] = {}
        if not program in RESULTS[mem_size]:
            RESULTS[mem_size][program] = {}
        if program == "wsbenchmark_send_lat":
            RESULTS[mem_size][program][threadnum] = {
                "ops": int(result[0]),
                "latency": int(result[1]),
                "jitter": int(result[2]),
                "throughput": float(result[3]),
                "send_latency": int(result[4]),
                "send_jitter": int(result[5]),
            }
        else:
            RESULTS[mem_size][program][threadnum] = {
                "ops": int(result[0]),
                "latency": int(result[1]),
                "jitter": int(result[2]),
                "throughput": float(result[3]),
            }

    with open("results.json", "w") as f:
        json.dump(RESULTS, f)


def server(
    program: str,
    node: str,
    serveraddr: str,
    memsize: str,
    threadnum: int,
    pmem: Optional[str] = "/dev/dax0.1",
):
    """Runs server part of benchmark"""
    args = [
        "ssh",
        node,
        f"{build_path}/{program}",
        "-b",
        serveraddr,
        "-S",
        memsize,
        "-c",
        str(threadnum),
        "--pmem",
        pmem
    ]
    if program in ["wbenchmark", "rbenchmark"]:
        args = [
            "ssh",
            node,
            f"{build_path}/{program}",
            "-b",
            serveraddr,
            "-S",
            memsize,
            "-c",
            str(threadnum)
        ]
    subprocess.run(args=args, stdout=sys.stdout, stderr=sys.stderr)


benchmarks = ["wrbenchmark", "wsbenchmark_send_lat", "wibenchmark", "rbenchmark", "wbenchmark"]
mem_sizes = ["256", "512", "1024", "2048", "4096", "8192", "12288", "16384", "20480", "24576", "32768", "65536"]
# benchmarks = ["rwbenchmark"]

if __name__ == "__main__":
    client_node = "pmem-4"
    server_node = "pmem-3"
    server_addr = "10.10.0.123"

    for mem_size in mem_sizes:
        for program in benchmarks:
            for threadnum in [1, 2, 4, 8, 12, 16]:
                clientproc = Process(
                    target=client,
                    args=(program, client_node, server_addr, mem_size, threadnum),
                )
                serverproc = Process(
                    target=server,
                    args=(program, server_node, server_addr, mem_size, threadnum),
                )

                serverproc.start()
                sleep(0.1)
                clientproc.start()

                clientproc.join()
                serverproc.kill()
