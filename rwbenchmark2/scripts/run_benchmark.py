#!/bin/python3
import sys
import json
from typing import Optional
import subprocess
from multiprocessing import Process
from time import sleep
from pathlib import Path

build_path = "/home/vagrant/host/rwbenchmark2/build"
benchmark_secs = str(10)


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
        benchmark_secs,
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
        if not program in RESULTS:
            RESULTS[program] = {}
        RESULTS[program][threadnum] = {
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
    pmem: Optional[str] = "/dev/dax0.0",
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
        pmem,
    ]
    subprocess.run(args=args, stdout=sys.stdout, stderr=sys.stderr)


benchmarks = ["rwbenchmark", "wsbenchmark", "wibenchmark"]
# benchmarks = ["rwbenchmark"]

if __name__ == "__main__":
    client_node = "node1"
    server_node = "node2"
    mem_size = str(1024)
    server_addr = "192.168.33.11"

    for program in benchmarks:
        for threadnum in [1, 2, 4]:
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
