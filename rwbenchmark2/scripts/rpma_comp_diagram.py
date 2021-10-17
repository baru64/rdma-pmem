#!/bin/python3
import json
from typing import Optional

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
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Opóźnienie (µs)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_lat_1.pdf")

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
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Opóźnienie (µs)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_lat_2.pdf")

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
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Opóźnienie (µs)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_lat_4.pdf")

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
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Opóźnienie (µs)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_lat_8.pdf")

############## draw x=bs y=bw 1 thread ###############
benchmarks = get_benchmarks("1", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th1_dp1_dev_dax0.1-21-09-07-214923.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th1_dp1_dev_dax0.1-21-09-27-124214.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
y3 = benchmarks["wrbenchmark"]
y4 = benchmarks["wsbenchmark"]
y5 = benchmarks["wibenchmark"]
y6 = benchmarks["rbenchmark"]
y7 = benchmarks["wbenchmark"]
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Przepustowość (GB/s)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_bw_1.pdf")

############## draw x=bs y=bw 2 thread ###############
benchmarks = get_benchmarks("2", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th2_dp1_dev_dax0.1-21-09-07-220829.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th2_dp1_dev_dax0.1-21-09-27-130133.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
y3 = benchmarks["wrbenchmark"]
y4 = benchmarks["wsbenchmark"]
y5 = benchmarks["wibenchmark"]
y6 = benchmarks["rbenchmark"]
y7 = benchmarks["wbenchmark"]
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Przepustowość (GB/s)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_bw_2.pdf")

############## draw x=bs y=bw 4 thread ###############
benchmarks = get_benchmarks("4", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th4_dp1_dev_dax0.1-21-09-07-222735.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th4_dp1_dev_dax0.1-21-09-27-132039.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
y3 = benchmarks["wrbenchmark"]
y4 = benchmarks["wsbenchmark"]
y5 = benchmarks["wibenchmark"]
y6 = benchmarks["rbenchmark"]
y7 = benchmarks["wbenchmark"]
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Przepustowość (GB/s)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_bw_4.pdf")

############## draw x=bs y=bw 8 thread ###############
benchmarks = get_benchmarks("8", "throughput")
results = get_results("rpma_bench_results/rpma_fio_apm_busy-wait_write_bw-bs_th8_dp1_dev_dax0.1-21-09-07-224641.csv")
results2 = get_results("rpma_bench_results/rpma_fio_gpspm_busy-wait_write_bw-bs_th8_dp1_dev_dax0.1-21-09-27-133945.csv")
fig, ax = plt.subplots(figsize=(plt.figaspect(0.4)))
x = np.arange(len(results["bs"])-2)
y1 = results["lat_avg"][:-2]
y2 = results2["lat_avg"][:-2]
y3 = benchmarks["wrbenchmark"]
y4 = benchmarks["wsbenchmark"]
y5 = benchmarks["wibenchmark"]
y6 = benchmarks["rbenchmark"]
y7 = benchmarks["wbenchmark"]
ax.plot(x, y1, color="c", label="RPMA-APM")
ax.plot(x, y2, color="m", label="RPMA-GPSPM")
ax.plot(x, y3, color="y", label="WR")
ax.plot(x, y4, color="r", label="WS")
ax.plot(x, y5, color="g", label="WI")
ax.plot(x, y6, color="b", label="R")
ax.plot(x, y7, color="orange", label="W")
ax.set_xlabel("Rozmiar zapisów (bajty)")
ax.set_ylabel("Przepustowość (GB/s)")
ax.set_xticks(x)
ax.set_xticklabels(results["bs"][:-2])
ax.legend()
fig.savefig("diagrams/rpma_cmp_bw_8.pdf")
