#!/usr/bin/python3
import json

import matplotlib.pyplot as plt

all_results = {}
benchmark_names = [
    "wrbenchmark",
    "wsbenchmark",
    "wibenchmark",
    "wbenchmark"
]
benchmark_labels = {
    "wrbenchmark": "ARRM",
    "wsbenchmark": "GPRRM",
    "wibenchmark": "iGPRRM",
    "wbenchmark": "RDMA WRITE"
}
memsizes = [
    "256",
    "512",
    "1024",
    "2048",
    "4096",
    "8192",
    "12288",
    "16384",
    "20480",
    "24576",
    "32768",
    "65536",
]
titles = {
    "throughput": "throughput",
    "latency": "avg. latency",
    "jitter": "avg. jitter",
}
labels = {"throughput": "GB/s", "latency": "ns", "jitter": "ns"}
threads = ["1", "2", "4", "8", "12"]
with open("results.json", "r") as f:
    all_results: dict = json.load(f)

# all_results[memsize][benchmark][threads]

# x - memsize
# y - latency
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    # get y
    y = []
    for memsize in memsizes:
        y.append(all_results[memsize]["wsbenchmark"][str(threads)]["latency"] / 1000.0)
    ax.plot(
        x, y, ".:", label="razem", markersize=10, linewidth=1
    )
    y = []
    for memsize in memsizes:
        y.append(all_results[memsize]["wsbenchmark"][str(threads)]["send_latency"] / 1000.0)
    ax.plot(
        x, y, "d:", label="RDMA SEND", markersize=10, linewidth=1
    )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Opóźnienie (µs)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/send_lat_diagram_{threads}.pdf")

# todo ws vs wi send latency
