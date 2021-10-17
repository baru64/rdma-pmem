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
    "wrbenchmark": "GPRRM",
    "wsbenchmark": "ARRM",
    "wibenchmark": "SGPRRM",
    "wbenchmark": "RDMA WRITE"
}
memsizes = [
    "10",
    "50",
    "100",
    "200",
    "400",
    "600",
    "800",
    "1000",
    "1200",
    "1400",
    "1500",
    "1600",
]
titles = {
    "throughput": "throughput",
    "latency": "avg. latency",
    "jitter": "avg. jitter",
}
labels = {"throughput": "GB/s", "latency": "ns", "jitter": "ns"}
with open("results.json", "r") as f:
    all_results: dict = json.load(f)

# all_results[memsize][benchmark][threads]

# x - memsize
# y - latency
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["latency"] / 1000.0)
        ax.plot(
            x, y, ".:", label=benchmark_labels[benchmark], markersize=10, linewidth=1
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Opóźnienie (µs)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/sp_lat_diagram_{threads}.pdf")

# x - memsize
# y - throughput
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["throughput"]*8.0)
        ax.plot(
            x, y, ".:", label=benchmark_labels[benchmark], markersize=10, linewidth=1
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Przepustowość (Gbit/s)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/sp_thr_diagram_{threads}.pdf")

# x - memsize
# y - jitter
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["jitter"] / 1000.0)
        ax.plot(
            x, y, ".:", label=benchmark_labels[benchmark], markersize=10, linewidth=1
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Jitter (µs)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/sp_jit_diagram_{threads}.pdf")
