#!/bin/python3
import sys
import json

import matplotlib.pyplot as plt
import numpy as np

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
y = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
ax.bar(x-0.15, y, color="m", width=0.25, label="APM")
ax.bar(x+0.15, y2, color="y", width=0.25, label="GPSPM")
#for i, j in zip(x, y):
#    ax.annotate(str(j), xy=(i, j+5))
ax.set_xlabel("bytes")
ax.set_ylabel("ns")
ax.set_title("RPMA - avg latency 1 thread")
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
