#!/usr/bin/python3
import json

import matplotlib.pyplot as plt

# plt.rcParams.update({"font.size": 13})

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
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.6)))
    fig.x_labels = map(str, memsizes)
    plots = []
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["latency"] / 1000.0)
        p, = ax.plot(
            x, y, benchmark_styles[benchmark], markersize=10, linewidth=2
        )
        plots.append(p)
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Opóźnienie (µs)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/lat_diagram_{threads}.pdf", bbox_inches='tight')
    # legend
    lfig = plt.figure(figsize=(3,1))
    lfig.legend(plots, legend_labels, 'center')
    lfig.savefig("diagrams/legend.pdf")

# x - memsize
# y - throughput
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.6)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["throughput"]*8.58993)
        ax.plot(
            x, y, benchmark_styles[benchmark], markersize=10, linewidth=2
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Przepustowość (Gbit/s)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/thr_diagram_{threads}.pdf", bbox_inches='tight')

# x - memsize
# y - jitter
x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.6)))
    fig.x_labels = map(str, memsizes)
    for benchmark in benchmark_names:
        # get y
        y = []
        for memsize in memsizes:
            y.append(all_results[memsize][benchmark][str(threads)]["jitter"] / 1000.0)
        ax.plot(
            x, y, benchmark_styles[benchmark], markersize=10, linewidth=2
        )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Jitter (µs)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/jit_diagram_{threads}.pdf", bbox_inches='tight')

# ACK VS NO ACK
# x - memsize
# y - latency

with open("sample_results/no_ack_results.json", "r") as f:
    no_ack_results: dict = json.load(f)

with open("sample_results/finalO3.json", "r") as f:
    ack_results: dict = json.load(f)

x = [x for x in range(len(memsizes))]
for threads in [1]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    # wrbenchmark no ack
    y = []
    for memsize in memsizes:
        y.append(no_ack_results[memsize]['wrbenchmark'][str(threads)]["latency"] / 1000.0)
    p1, = ax.plot(
        x, y, ".:", label="ARRM bez oczekiwania na RDMA Write", linewidth=1
    )
    # wrbenchmark ack
    y = []
    for memsize in memsizes:
        y.append(ack_results[memsize]['wrbenchmark'][str(threads)]["latency"] / 1000.0)
    p2, = ax.plot(
        x, y, "^--", label="ARRM z oczekiwaniem na RDMA Write", linewidth=1
    )
    # wsbenchmark no ack
    y = []
    for memsize in memsizes:
        y.append(no_ack_results[memsize]['wsbenchmark'][str(threads)]["latency"] / 1000.0)
    p3, = ax.plot(
        x, y, "x:", label="GPRRM bez oczekiwania na RDMA Write", linewidth=1
    )
    # wsbenchmark ack
    y = []
    for memsize in memsizes:
        y.append(ack_results[memsize]['wsbenchmark'][str(threads)]["latency"] / 1000.0)
    p4, = ax.plot(
        x, y, "*--", label="GPRRM z oczekiwaniem na RDMA Write", linewidth=1
    )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Opóźnienie (µs)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/ack_vs_no_ack.pdf")
