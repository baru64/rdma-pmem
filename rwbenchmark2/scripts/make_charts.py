#!/bin/python3
import json

import matplotlib.pyplot as plt

plt.rcParams.update({"font.size": 5})

all_results = {}
benchmark_names = [
    "wrbenchmark",
    "wsbenchmark_send_lat",
    "wibenchmark",
    "rbenchmark",
    "wbenchmark",
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
figures = ["throughput", "latency", "jitter"]
wsbenchmark_send_lat_figures = [
    "throughput",
    "latency",
    "jitter",
    "send_latency",
    "send_jitter",
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

# wsbenchmark_send_lat jitter
for thread in threads:
    fig, ax = plt.subplots()
    x = [int(m) for m in memsizes]
    y1 = []  # jitter
    y2 = []  # send_jitter
    for memsize in memsizes:
        results = all_results[memsize]
        y1.append(results["wsbenchmark_send_lat"][thread].get("jitter"))
        y2.append(results["wsbenchmark_send_lat"][thread].get("send_jitter"))
    ax.plot(x, y1, "-o", label="total jitter")
    ax.plot(x, y2, "-o", label="send jitter")
    for i, j in zip(x, y1):
        ax.annotate(str(j), xy=(i, j))
    for i, j in zip(x, y2):
        ax.annotate(str(j), xy=(i, j))
    ax.set_xlabel("bytes")
    ax.set_ylabel("ns")
    ax.set_title(f"wsbenchmark total jitter vs send jitter: {thread} connection(s)")
    ax.legend()
    ax.grid(True)
    fig.savefig(f"results/wsbenchmark_send_lat_jitter_memsize{thread}.pdf")

# wsbenchmark_send_lat latency
for thread in threads:
    fig, ax = plt.subplots()
    x = [int(m) for m in memsizes]
    y1 = []  # latency
    y2 = []  # send_latency
    for memsize in memsizes:
        results = all_results[memsize]
        y1.append(results["wsbenchmark_send_lat"][thread].get("latency"))
        y2.append(results["wsbenchmark_send_lat"][thread].get("send_latency"))
    ax.plot(x, y1, "-o", label="total latency")
    ax.plot(x, y2, "-o", label="send latency")
    for i, j in zip(x, y1):
        ax.annotate(str(j), xy=(i, j))
    for i, j in zip(x, y2):
        ax.annotate(str(j), xy=(i, j))
    ax.set_xlabel("bytes")
    ax.set_ylabel("ns")
    ax.set_title(f"wsbenchmark total latency vs send latency: {thread} connection(s)")
    ax.legend()
    ax.grid(True)
    fig.savefig(f"results/wsbenchmark_send_lat_latency_memsize{thread}.pdf")

# x = memsize
for thread in threads:
    for figure in figures:
        benchmarks = {}
        for memsize in memsizes:
            results = all_results[memsize]
            for benchmark in benchmark_names:
                if not benchmark in benchmarks:
                    benchmarks[benchmark] = []
                benchmarks[benchmark].append(results[benchmark][thread].get(figure))

        fig, ax = plt.subplots()
        for benchmark, y in benchmarks.items():
            x = [int(m) for m in memsizes]
            ax.plot(x, y, "-o", label=benchmark)
            for i, j in zip(x, y):
                ax.annotate(str(j), xy=(i, j))
            ax.set_xlabel("bytes")
            ax.set_ylabel(labels[figure])
            ax.set_title(f"{titles[figure]}: {thread} connection(s)")
            ax.legend()
            ax.grid(True)
        fig.tight_layout()
        fig.savefig(f"results/{figure}_memsize{thread}.pdf")

# x = connections
for j, memsize in enumerate(memsizes):
    fig, ax = plt.subplots(1, len(figures), figsize=plt.figaspect(0.3))
    results = all_results[memsize]
    for i, figure in enumerate(figures):
        for benchmark in results.keys():
            y = []  # values
            x = []  # threads
            x = sorted([int(k) for k in results[benchmark].keys()])
            for thread in x:
                y.append(results[benchmark][str(thread)].get(figure))
            ax[i].plot(x, y, "-o", label=benchmark)
            for k, l in zip(x, y):
                ax[i].annotate(str(l), xy=(k, l))
            ax[i].grid(True)

        ax[i].set_xlabel("connections")
        ax[i].set_ylabel(labels[figure])
        ax[i].set_title(f"{titles[figure]}: {memsize}B message")

    ax[0].legend()
    fig.tight_layout()
    fig.savefig(f"results/results{memsize}.pdf")
    # plt.show()
