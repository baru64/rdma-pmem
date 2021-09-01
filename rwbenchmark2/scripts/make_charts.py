#!/bin/python3
import json

import matplotlib.pyplot as plt

all_results = {}
memsizes = ["512", "1024", "2048", "4096", "8192", "16384", "32768", "65536"]
figures = ["throughput", "latency", "jitter"]
titles = {"throughput": "throughput", "latency": "avg. latency", "jitter": "avg. jitter"}
labels = {"throughput": "GB/s", "latency": "ns", "jitter": "ns"}
with open("results.json", "r") as f:
    all_results: dict = json.load(f)

for thread in ["1", "2", "4", "8", "12"]:
    for figure in figures:
        benchmarks = {}
        for memsize in memsizes:
            results = all_results[memsize]
            for benchmark in results.keys():
                if not benchmark in benchmarks:
                    benchmarks[benchmark] = []
                benchmarks[benchmark].append(results[benchmark][thread].get(figure))

        fig, ax = plt.subplots()
        for benchmark, y in benchmarks.items():
            x = [int(m) for m in memsizes]
            ax.plot(x, y, label=benchmark)
            ax.set_xlabel('bytes')
            ax.set_ylabel(labels[figure])
            ax.set_title(f"{titles[figure]}: {thread} connection(s)")
            ax.legend()
        fig.tight_layout()
        fig.savefig(f"results/{figure}_memsize{thread}.pdf")

for j, memsize in enumerate(memsizes):
    fig, ax = plt.subplots(1, len(figures))
    results = all_results[memsize]
    for i, figure in enumerate(figures):
        for benchmark in results.keys():
            y = [] # values
            x = [] # threads
            x = sorted([int(k) for k in results[benchmark].keys()])
            for thread in x:
                y.append(results[benchmark][str(thread)].get(figure))
            ax[i].plot(x, y, label=benchmark)

        ax[i].set_xlabel('connections')
        ax[i].set_ylabel(labels[figure])
        ax[i].set_title(f"{titles[figure]}: {memsize}B message")

    ax[0].legend()
    fig.tight_layout()
    fig.savefig(f"results/results{memsize}.pdf")
    # plt.show()
