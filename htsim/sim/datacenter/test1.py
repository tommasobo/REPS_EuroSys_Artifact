import subprocess
import concurrent.futures
import os
import argparse

commands = ["./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo ecmp > new40e",
            "./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo ecmp > new100e",
            "./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo oblivious > new40o",
            "./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo oblivious > new100o",
            "./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo freezing > new40r",
            "./htsim_uec -connections_mapping -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo freezing > new100r"]
parallel_degree = 6

# Function to run a command using subprocess
def run_command(cmd):
    print(f"Running command: {cmd}")
    os.system(cmd)

with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
    for command in commands:
        executor.submit(run_command, command)