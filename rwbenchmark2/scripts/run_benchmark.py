#!/bin/python3
import sys
import json
from typing import Optional
import subprocess
from multiprocessing import Process

RESULTS = {}

def client(program: str, node: str, serveraddr: str, memsize: str):
    """Runs client part of benchmark"""
    for threadnum in [1]:
        args = ["ssh", node, f"build/{program}", "-s", serveraddr, "-S", memsize, "-v", "-c", str(threadnum)]
        output = subprocess.check_output(args=args, stderr=sys.stderr)
        result = output.decode('utf-8').split(';')
        print(f"result th:{program} th:{threadnum}: ", result)
        if not RESULTS[program]:
            RESULTS[program] = {}
        RESULTS[program][threadnum] = {
            "ops": int(result[0]),
            "latency": int(result[1]),
            "jitter": int(result[2]),
            "throughput": int(result[3])
        }


def server(
    program: str,
    node: str,
    serveraddr: str,
    memsize: str,
    pmem: Optional[str] = "/dev/dax0.0",
):
    """Runs server part of benchmark"""
    for threadnum in [1]:
        args = [
            "ssh",
            node,
            f"build/{program}",
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

if __name__ == "__main__":
    client_node = "pmem-2"
    server_node = "pmem-3"
    mem_size = 1024
    server_addr = "192.168.33.11"

    for program in benchmarks:
        clientproc = Process(
            target=client, args=(program, client_node, server_addr, mem_size)
        )
        serverproc = Process(
            target=server, args=(program, server_node, server_addr, mem_size)
        )

        clientproc.start()
        serverproc.start()

        clientproc.join()
        serverproc.join()

    print(RESULTS)
    with open("results.json", "+w") as f:
        json.dump(RESULTS, f)
