#!/bin/python3
import matplotlib.pyplot as plt

fig, ax = plt.subplots()
x = []
y = []
ax.plot(x, y, label="benchmark")
ax.set_xlabel('x label')
ax.set_ylabel('y label')
ax.set_title("Benchmarks")
ax.legend()
