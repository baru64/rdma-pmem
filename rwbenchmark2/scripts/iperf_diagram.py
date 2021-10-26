#!/usr/bin/python3
import json

import matplotlib.pyplot as plt

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
]

# x - memsize
# y - throughput

x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    # pmem
    y = []
    for memsize in memsizes:
        with open(f"iperf3_results/iperf3-{memsize}-{threads}-pmem.json", "r") as f:
            iperfres: dict = json.load(f)
        y.append(iperfres["end"]["streams"][0]["receiver"]["bits_per_second"] / 1000000000.0)
    p1, = ax.plot(
        x, y, "*:", label="Zapis do pamięci trwałej", linewidth=2
    )
    # ssd
    y = []
    for memsize in memsizes:
        with open(f"iperf3_results/iperf3-{memsize}-{threads}-nopmem.json", "r") as f:
            iperfres: dict = json.load(f)
        y.append(iperfres["end"]["streams"][0]["receiver"]["bits_per_second"] / 1000000000.0)
    p2, = ax.plot(
        x, y, "^--", label="Zapis na dysk SSD", linewidth=2
    )
    # dram
    y = []
    for memsize in memsizes:
        with open(f"iperf3_results/iperf3-{memsize}-{threads}-nofile.json", "r") as f:
            iperfres: dict = json.load(f)
        y.append(iperfres["end"]["streams"][0]["receiver"]["bits_per_second"] / 1000000000.0)
    p3, = ax.plot(
        x, y, "x-.", label="Zapis do pamięci operacyjnej", linewidth=2
    )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Przepustowość (Gbit/s)")
    # ax.set_yscale('log')
    ax.legend()
    fig.savefig(f"diagrams/iperf3_bw_{threads}.pdf")

x = [x for x in range(len(memsizes))]
for threads in [1, 2, 4, 8]:
    fig, ax = plt.subplots(figsize=(plt.figaspect(0.5)))
    fig.x_labels = map(str, memsizes)
    # pmem
    y = []
    for memsize in memsizes:
        with open(f"iperf3_results/iperf3-{memsize}-{threads}-pmem.json", "r") as f:
            iperfres: dict = json.load(f)
        y.append(iperfres["end"]["streams"][0]["receiver"]["bits_per_second"] / 1000000000.0)
    p1, = ax.plot(
        x, y, "*:", label="Zapis do pamięci trwałej", linewidth=2
    )
    # ssd
    y = []
    for memsize in memsizes:
        with open(f"iperf3_results/iperf3-{memsize}-{threads}-nopmem.json", "r") as f:
            iperfres: dict = json.load(f)
        y.append(iperfres["end"]["streams"][0]["receiver"]["bits_per_second"] / 1000000000.0)
    p2, = ax.plot(
        x, y, "^--", label="Zapis na dysk SSD", linewidth=2
    )
    ax.set_xticks(x)
    ax.set_xticklabels(memsizes)
    ax.set_xlabel("Rozmiar zapisów (bajty)")
    ax.set_ylabel("Przepustowość (Gbit/s)")
    # ax.set_yscale('log')
    # ax.legend()
    fig.savefig(f"diagrams/iperf3_bw_fileonly_{threads}.pdf")
    # legend
    lfig = plt.figure(figsize=(3,1))
    lfig.legend([p1, p2], ['Zapis do pamięci trwałej', 'Zapis na dysk SSD'], 'center')
    lfig.savefig("diagrams/iperf_legend.pdf", bbox_inches='tight')
