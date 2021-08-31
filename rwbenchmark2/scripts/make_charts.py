#!/bin/python3
import json

import matplotlib.pyplot as plt

all_results = {}
memsizes = ["512", "1024", "2048", "4092", "8192", "16384"]
figures = ["throughput", "latency", "jitter"]
with open("results.json", "r") as f:
    all_results: dict = json.load(f)
fig, ax = plt.subplots(len(memsizes), len(figures))
for j, memsize in enumerate(memsizes):
    results = all_results[memsize]
    for i, figure in enumerate(figures):
        for benchmark in results.keys():
            y = [] # values
            x = [] # threads
            x = sorted([k for k in results[benchmark].keys()])
            for thread in x:
                y.append(results[benchmark][thread].get(figure))
            ax[j][i].plot(x, y, label=benchmark)

        ax[j][i].set_xlabel('connections')
        ax[j][i].set_ylabel(figure)
        ax[j][i].set_title(f"{figure}: {memsize}B message")

ax[0][0].legend()
fig.tight_layout()
# fig.savefig(f"results{memsize}.pdf")
fig.savefig("results.pdf")
plt.show()
