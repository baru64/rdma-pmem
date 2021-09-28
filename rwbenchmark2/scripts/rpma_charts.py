#!/bin/python3
import sys
import json

import matplotlib.pyplot as plt
import numpy as np

benchmark_names = [
    "wrbenchmark",
    "wsbenchmark",
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

my_results = {}
with open("results.json", "r") as f:
    my_results: dict = json.load(f)

benchmarks = {}
for memsize in memsizes:
    results = my_results[memsize]
    for benchmark in benchmark_names:
        if not benchmark in benchmarks:
            benchmarks[benchmark] = []
        benchmarks[benchmark].append(results[benchmark]["1"].get("latency")/1000.0)

# plt.rcParams.update({"font.size": 5})
def get_results(filename: str) -> dict:
    results = {}
    with open(filename, "r") as f:
        data = f.readlines()
        labels: list = None
        for line in data:
            if labels is None:
                labels = line.split(",")
                continue
            for i, value in enumerate(line.split(",")):
                if not labels[i] in results:
                    results[labels[i]] = []
                try:
                    results[labels[i]].append(int(value))
                except:
                    results[labels[i]].append(float(value))
    return results

# draw x=bs y=lat
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_lat_th1_dp1_dev_dax0.1-21-09-07-201345.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_lat_th1_dp1_dev_dax0.1-21-09-07-232454.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
y3 = benchmarks["wrbenchmark"]
y4 = benchmarks["wsbenchmark"]
y5 = benchmarks["wibenchmark"]
y6 = benchmarks["rbenchmark"]
y7 = benchmarks["wbenchmark"]
ax.bar(x-0.35, y1, color="c", width=0.10, label="RPMA-APM")
ax.bar(x-0.25, y2, color="m", width=0.10, label="RPMA-GPSPM")
ax.bar(x-0.15, y3, color="y", width=0.10, label="WR")
ax.bar(x-0.05, y4, color="r", width=0.10, label="WS")
ax.bar(x+0.05, y5, color="g", width=0.10, label="WI")
ax.bar(x+0.15, y6, color="b", width=0.10, label="R")
ax.bar(x+0.25, y7, color="orange", width=0.10, label="W")
#for i, j in zip(x, y):
#    ax.annotate(str(j), xy=(i, j+5))
ax.set_xlabel("bytes")
ax.set_ylabel("usec")
ax.set_title("all benchmarks avg latency 1 thread")
# ax.set_xscale('log')
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
# ax.grid(True)
# adjust axis
# plt.gca().margins(x=0)
# plt.gcf().canvas.draw()
# tl = plt.gca().get_xticklabels()
# maxsize = max([t.get_window_extent().width for t in tl])
# m = 0.5 # inch margin
# s = maxsize/plt.gcf().dpi*len(results["bs"])+2*m
# margin = m/plt.gcf().get_size_inches()[0]
# 
# plt.gcf().subplots_adjust(left=margin, right=1.-margin)
# plt.gcf().set_size_inches(s, plt.gcf().get_size_inches()[1])

fig.savefig("rpma_results/rpma_apm_avg_lat_th1.pdf")
