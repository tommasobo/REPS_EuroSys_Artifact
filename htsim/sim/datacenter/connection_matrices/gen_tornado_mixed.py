#!/usr/bin/env python

# Generate a permutation traffic matrix.
# python gen_pemutation.py <nodes> <conns> <flowsize> <extrastarttime>
# Parameters:
# <nodes>   number of nodes in the topology
# <conns>    number of active connections
# <flowsize>   size of the flows in bytes
# <extrastarttime>   How long in microseconds to space the start times over (start time will be random in between 0 and this time).  Can be a float.
# <randseed>   Seed for random number generator, or set to 0 for random seed

import os
import sys
from random import seed, shuffle
#print(sys.argv)
if len(sys.argv) != 6:
    print("Usage: python gen_pemutation.py <filename> <nodes> <flowsize> <extrastarttime> <randseed>")
    sys.exit()
filename = sys.argv[1]
nodes = int(sys.argv[2])
flowsize = int(sys.argv[3])
extrastarttime = float(sys.argv[4])
randseed = int(sys.argv[5])

""" print("Nodes: ", nodes)
print("Flowsize: ", flowsize, "bytes")
print("ExtraStartTime: ", extrastarttime, "us")
print("Random Seed ", randseed) """

count_normal = 0
count_bg = 0
for i in range(nodes):
    if (i % 10 == 0):
        count_bg += 1
    else:
        count_normal += 1

print("Normal connections: ", count_normal)
print("Background connections: ", count_bg)
conns = count_normal + (count_bg * 20)

start_times_bg = []
num_pkt = int(flowsize / 4096)
num_bg_pkts = int(num_pkt / 20 / 2)
expected_completion_time = flowsize * 8 / 400
intervals = int(expected_completion_time / 20)
for n in range(num_bg_pkts):
    start_times_bg.append(n * intervals)
print("Connections: ", conns)

f = open(filename, "w")
print("Nodes", nodes, file=f)
print("Connections", conns, file=f)

srcs = []
dsts = []
for n in range(nodes):
    srcs.append(n)
    dsts.append(n)
if randseed != 0:
    seed(randseed)

counter = 0
# eliminate any duplicates - a node should not send to itself
for n in range(nodes):
    if (n < nodes / 2):
        if (n % 10 == 0):
            for i in range(20):
                out = str(n) + "->" + str( int(n + nodes / 2)) + " id " + str(counter+1) + " start " + str(int(start_times_bg[i] * 1000)) + " size " + str(3*4096)
                counter += 1
                print(out, file=f)
        else:
            out = str(n) + "->" + str( int(n + nodes / 2)) + " id " + str(counter+1) + " start " + str(int(extrastarttime * 1000)) + " size " + str(flowsize)
            counter += 1
            print(out, file=f)
    else:
        if (n % 10 == 0):
            for i in range(20):
                out = str(n) + "->" + str( int(n - nodes / 2)) + " id " + str(counter+1) + " start " + str(int(start_times_bg[i] * 1000)) + " size " + str(3*4096)
                counter += 1
                print(out, file=f)
        else:
            out = str(n) + "->" + str( int(n - nodes / 2)) + " id " + str(counter+1) + " start " + str(int(extrastarttime * 1000)) + " size " + str(flowsize)
            counter += 1
            print(out, file=f)

f.close()
