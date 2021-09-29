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

def get_benchmarks(threads: str, field: str) -> dict:
    benchmarks = {}
    for memsize in memsizes:
        results = my_results[memsize]
        for benchmark in benchmark_names:
            if not benchmark in benchmarks:
                benchmarks[benchmark] = []
            if field in ("latency", "jitter"):
                benchmarks[benchmark].append(results[benchmark][threads].get(field)/1000.0)
            elif field == "throughput":
                benchmarks[benchmark].append(results[benchmark][threads].get(field)*8.0)
            else:
                benchmarks[benchmark].append(results[benchmark][threads].get(field))
    return benchmarks


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

# draw x=bs y=lat 1 thread
benchmarks = get_benchmarks("1", "latency")
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
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_avg_lat_th1.pdf")

# draw x=bs y=lat 2 threads
benchmarks = get_benchmarks("2", "latency")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_lat_th2_dp1_dev_dax0.1-21-09-07-203259.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_lat_th2_dp1_dev_dax0.1-21-09-07-234400.csv")
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
ax.set_title("all benchmarks avg latency 2 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_avg_lat_th2.pdf")

############ draw x=bs y=lat 4 threads ###############3
benchmarks = get_benchmarks("4", "latency")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_lat_th4_dp1_dev_dax0.1-21-09-07-205205.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_lat_th4_dp1_dev_dax0.1-21-09-08-000306.csv")
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
ax.set_title("all benchmarks avg latency 4 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_avg_lat_th4.pdf")

############## draw x=bs y=lat 8 threads ###############
benchmarks = get_benchmarks("8", "latency")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_lat_th8_dp1_dev_dax0.1-21-09-07-211110.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_lat_th8_dp1_dev_dax0.1-21-09-08-002212.csv")
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
ax.set_title("all benchmarks avg latency 8 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_avg_lat_th8.pdf")

############## draw x=bs y=bw 1 thread ###############
benchmarks = get_benchmarks("1", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th1_dp1_dev_dax0.1-21-09-07-214923.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th1_dp1_dev_dax0.1-21-09-27-124214.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["bw_avg"][:-2]
y2 = results2["bw_avg"][:-2]
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
ax.set_ylabel("GB/s")
ax.set_title("all benchmarks throughput 1 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_throughput_th1.pdf")

############## draw x=bs y=bw 2 thread ###############
benchmarks = get_benchmarks("2", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th2_dp1_dev_dax0.1-21-09-07-220829.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th2_dp1_dev_dax0.1-21-09-27-130133.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["bw_avg"][:-2]
y2 = results2["bw_avg"][:-2]
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
ax.set_ylabel("GB/s")
ax.set_title("all benchmarks throughput 2 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_throughput_th2.pdf")

############## draw x=bs y=bw 4 thread ###############
benchmarks = get_benchmarks("4", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th4_dp1_dev_dax0.1-21-09-07-222735.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th4_dp1_dev_dax0.1-21-09-27-132039.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["bw_avg"][:-2]
y2 = results2["bw_avg"][:-2]
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
ax.set_ylabel("GB/s")
ax.set_title("all benchmarks throughput 4 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_throughput_th4.pdf")

############## draw x=bs y=bw 8 thread ###############
benchmarks = get_benchmarks("8", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th8_dp1_dev_dax0.1-21-09-07-224641.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th8_dp1_dev_dax0.1-21-09-27-133945.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["bw_avg"][:-2]
y2 = results2["bw_avg"][:-2]
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
ax.set_ylabel("GB/s")
ax.set_title("all benchmarks throughput 8 threads")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"])
ax.legend()
ax.grid(axis="y")

fig.savefig("rpma_results/all_throughput_th8.pdf")
