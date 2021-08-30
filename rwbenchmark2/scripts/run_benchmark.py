#!/bin/python3
from multiprocessing import Process
from subprocess import Popen

def client(program: str, node: str):
    """ Runs client part of benchmark """
    # TODO: Popen ssh pmem-node program client_args
    pass

def server(program: str, node: str):
    """ Runs server part of benchmark """
    # TODO: Popen ssh pmem-node program server_args
    pass


benchmarks = ["rwbenchmark", "wsbenchmark", "wibenchmark"]

# TODO: run benchmarks, get output and write it combined

if __name__ == "__main__":
    client_node = "pmem-2"
    server_node = "pmem-3"
    for program in benchmarks:
        clientproc = Process(target=client, args=(program,client_node))
        serverproc = Process(target=server, args=(program,server_node))

        clientproc.start()
        serverproc.start()

        clientproc.join()
        serverproc.join()
