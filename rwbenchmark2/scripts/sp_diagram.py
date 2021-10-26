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
benchmark_styles = {
    "wrbenchmark": "*:",
    "wsbenchmark": "^--",
    "wibenchmark": "x--",
    "wbenchmark": ".-."
}
legend_labels = [
    "Zapisy metodą ARRM",
    "Zapisy metodą GPRRM",
    "Zapisy metodą iGPRRM",
    "Zapisy do pamięci DRAM"
]
labels = {"throughput": "GB/s", "latency": "ns", "jitter": "ns"}
with open("results.json", "r") as f:
    all_results: dict = json.load(f)

# all_results[memsize][benchmark][threads]

# x - memsize
# y - latency
x = [x for x in range(len(memsizes))]
for threads in [1]: #, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.65)))
    fig.x_labels = map(str, memsizes)
    plots = []
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["latency"] / 1000.0)
        p, = ax.plot(
            x, y, benchmark_styles[benchmark], label=benchmark_labels[benchmark], markersize=10, linewidth=2
        )
        plots.append(p)
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Opóźnienie (µs)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/sp_lat_diagram_{threads}.pdf")
    # legend
    lfig = plt.figure(figsize=(3,1))
    lfig.legend(plots, legend_labels, 'center')
    lfig.savefig("diagrams/sp_legend.pdf")

# x - memsize
# y - throughput
x = [x for x in range(len(memsizes))]
for threads in [1]: #, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.65)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["throughput"]*8.0)
        ax.plot(
            x, y, benchmark_styles[benchmark], label=benchmark_labels[benchmark], markersize=10, linewidth=2
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Przepustowość (Gbit/s)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/sp_thr_diagram_{threads}.pdf")

# x - memsize
# y - jitter
x = [x for x in range(len(memsizes))]
for threads in [1]: #, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.65)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["jitter"] / 1000.0)
        ax.plot(
            x, y, benchmark_styles[benchmark], label=benchmark_labels[benchmark], markersize=10, linewidth=2
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Jitter (µs)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/sp_jit_diagram_{threads}.pdf")
