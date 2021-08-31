#!/bin/python3
import json

import matplotlib.pyplot as plt

all_results = {}
figures = ["throughput", "latency", "jitter"]
with open("results.json", "r") as f:
    all_results: dict = json.load(f)
results = all_results["1024"]
for figure in figures:
    fig, ax = plt.subplots()
    for benchmark in results.keys():
        y = [] # values
        x = [] # threads
        x = sorted([k for k in results[benchmark].keys()])
        for thread in x:
            y.append(results[benchmark][thread].get(figure))
        ax.plot(x, y, label=benchmark)

    ax.set_xlabel('threads')
    ax.set_ylabel(figure)
    ax.set_title(figure)
    ax.legend()
    # fig.savefig(f"{figure}.png")

plt.show()
