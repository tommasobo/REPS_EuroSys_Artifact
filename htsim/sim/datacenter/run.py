import subprocess
import concurrent.futures
import os
import argparse



parallel_degree = 2
command_list = ["./htsim_uec -sack_threshold 4000 -load_balancing_algo oblivious -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/oblivious",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo ecmp -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/ecmp",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo plb -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/plb",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo mprdma -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/mprdma",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo bitmap -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/bitmap",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo reps -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/reps",
                "./htsim_uec -sack_threshold 4000 -load_balancing_algo reps -connections_mapping -tm connection_matrices/dc_orig.cm  -end 90000 -seed 1019 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_4os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 > dc_symm1/reps_mapping"]

# Function to run a command using subprocess
def run_command(cmd):
    print(f"Running command: {cmd}")
    os.system(cmd)

with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
    for command in command_list:
        executor.submit(run_command, command)