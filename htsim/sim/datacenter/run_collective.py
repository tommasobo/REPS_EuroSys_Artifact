import subprocess
import concurrent.futures
import argparse
import os

parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
parser.add_argument('--output_folder', required=False, help='Parent output folder where to save all results', default="experiments")
parser.add_argument('--trim', required=False, help='Trim Active Or not', default="on")
parser.add_argument('--size_topo', type=int, required=True, help='Size Topo', default=128)
parser.add_argument('--asymmetry_links', type=int, required=False, help='Num downgraded links', default=0)
args = parser.parse_args()

trim_s = ""
if (args.trim == "off"):
    trim_s = "-disable_trim"
else:
    trim_s = ""

failed_s = ""
if (args.asymmetry_links != 0):
    failed_s = f"-failed {args.asymmetry_links}"

topology_size = int(args.size_topo)


# Parallel degree (number of threads)
parallel_degree = 7

# Base command template
base_command = "./htsim_uec -connections_mapping -failures_input ../failures_input/5_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 1019 -paths 1024 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151"
base_command = "./htsim_uec {trim} {failed} -connections_mapping -down_ratio {ratio} -sack_threshold 4000 -end 90000 -seed 1019 -paths {paths} -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_{size}_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151"


# Load balancing algorithms
load_balancing_algos = ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "flowlet", "mp"]
load_balancing_algos = ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "flowlet", "mp", "nvidia"]

# Traffic matrix options (with .cm extensions)
traffic_matrices = [
    "connection_matrices/alltoall4.cm",
    "connection_matrices/alltoall8.cm",
    "connection_matrices/alltoall16.cm",
    "connection_matrices/allreduce.cm", 
    "connection_matrices/allreduce_t.cm", 
]

""" traffic_matrices = [
    "connection_matrices/allreduce.cm", 
    "connection_matrices/allreduce_t.cm", 
] """

# Output directory (updated to dc_load)
output_dir = args.output_folder
if not os.path.exists(output_dir):
    os.makedirs(output_dir)
os.system(f"rm -rf {output_dir}/*")

# Function to run a command using subprocess
def run_command(cmd):
    #print(f"Running command: {cmd}")
    subprocess.run(cmd, shell=True, check=True)

# Dynamically generate and run commands
with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
    # Loop over each traffic matrix and load balancing algorithm
    for tm in traffic_matrices:   

        collective_name = tm.split('/')[-1].split('.')[0]
        collective_out = output_dir + "/" + collective_name
        os.makedirs(collective_out)

        for algo in load_balancing_algos:
            # Build the command string dynamically
            output_file = f"{collective_out}/{algo}"
            extra = ""
            if (algo == "mp"):
                extra = "-mp_flows 8"
            evs = 0
            if (algo == "nvidia"):
                evs = 256
            else:
                evs = 65535

            add_ratio = 0.5
            if (collective_name == "allreduce"):
                add_ratio = 0.25

            base_command = base_command.format(trim=trim_s, failed=failed_s, paths=evs, size=topology_size, ratio=add_ratio)
            command = f"{base_command} {extra} -tm {tm} -load_balancing_algo {algo} > {output_file}"
            executor.submit(run_command, command)