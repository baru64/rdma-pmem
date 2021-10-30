#!/usr/bin/python3
import json
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np

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
def get_results(filename: str) -> dict:
    results = {}
    with open(filename, "r") as f:
        data = f.readlines()
        labels: Optional[list] = None
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

# x - memsize
# y - latency
x = [x for x in range(len(memsizes))]
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_lat_th1_dp1_dev_dax0.1-21-09-07-201345.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_lat_th1_dp1_dev_dax0.1-21-09-07-232454.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
ax.plot(x, y1, ".:", label="RPMA-APM")
ax.plot(x, y2, "d:", label="RPMA-GPSPM")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Opóźnienie (µs)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig(f"diagrams/rpma_lat_diagram_1.pdf")
