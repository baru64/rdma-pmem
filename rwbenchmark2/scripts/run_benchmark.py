#!/bin/python3
from multiprocessing import Process
from subprocess import Popen

def client(program: str):
    """ Runs client part of benchmark """
    pass

def server(program: str):
    """ Runs server part of benchmark """
    pass


benchmarks = ["rwbenchmark", "wsbenchmark", "wibenchmark"]

# TODO: run benchmarks, get output and write it combined

if __name__ == "__main__":
    for program in benchmarks:
        clientproc = Process(target=client, args=(program))
        serverproc = Process(target=server, args=(program))

        clientproc.start()
        serverproc.start()

        clientproc.join()
        serverproc.join()
