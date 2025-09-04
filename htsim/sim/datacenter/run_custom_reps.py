import subprocess
import concurrent.futures
import shlex
import os
import argparse

# Script template with placeholders
link_speed = 1

script_template = "./htsim_uec -mixed_lb_traffic -sack_threshold 4000 -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 9000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -linkspeed 400000 -ecn 25 75 -q 100 -cwnd 140 > {output_dir}/{output_file}"
script_template = "./htsim_uec -failed 1 -sack_threshold 4000 -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 9000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -linkspeed 400000 -ecn 25 75 -q 100 -cwnd 140 > {output_dir}/{output_file}"
tm_template = "connection_matrices/permutation_size{size}B.cm"

message_sizes = [32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216]
""" message_sizes = [32768, 65536, 131072, 262144, 524288] """
message_sizes = [1048576, 2097152, 4194304, 8388608]
message_sizes = [262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864]


message_sizes = [1]


cc_algos = ["nscc"]
drop_strategy = ["trimming"]
os_ratio = [1]
load_balancing_algos = ["reps", "bitmap", "oblivious", "ecmp", "plb", "mp", "mprdma", "flowlet"]
num_entropies = [128, 65535]
num_entropies_reps = 2**12

""" # Parameters to loop over
load_balancing_algos = ["reps", "random", "ecmp"]  # Example algorithms
tm_files = [
    "connection_matrices/permutation_size4194304B.cm",
    "connection_matrices/permutation_size16777216B.cm"
] """

def getRunScript(dir_name, cc_algo, oversubscription, size, drop, tm_file, output_file, main_folder, lb_algo, num_entropy):
    
    """ if lb_algo == "reps":
        num_entropy = num_entropies_reps """

    cc_string = ""
    if (cc_algo == "nscc"):
        cc_algo = "-sender_cc_only"
    elif (cc_algo == "rccc"):
        cc_algo = ""
    elif (cc_algo == "both"):
        cc_algo = "-sender_cc"
    elif (cc_algo == "constant"):
        cc_algo = "-sender_cc_only -sender_cc_algo constant"

    drop_string = ""
    if (drop == "trimming"):
        drop = ""
    elif (drop == "timeout"):
        drop = "-disable_trim"

    topo_file_name = ""
    if (oversubscription == 1):
        topo_file_name = "topologies/reps/fat_tree_128_1os_3t_400g.topo"
    elif (oversubscription == 4):
        topo_file_name = "topologies/fat_tree_1024_4os.topo"
    elif (oversubscription == 8):
        topo_file_name = "topologies/fat_tree_1024_8os.topo"

    other = ""
    if (lb_algo == "mp"):
        other = "-mp_flows 8"
    
    if os.path.exists(dir_name + "/" + output_file):
        return
    command = script_template.format(other_stuff=other, algo=lb_algo, paths=num_entropy, tm_file=tm_file, cc_strat=cc_algo, trimming_strat=drop, topo_file=topo_file_name, output_file=output_file, output_dir=dir_name)
    print(f"Running command: {command}")
    return command

# Function to run a command using subprocess
def run_command(cmd):
    # Use shlex to split the command string into arguments
    """ cmd_list = shlex.split(cmd)
    # Run the command
    result = subprocess.run(cmd_list, text=True, capture_output=True)
    if result.returncode != 0:
        print(f"Command failed with error: {result.stderr}") """

    os.system(cmd)

parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
parser.add_argument('--output_folder', required=False, help='Parent output folder where to save all results', default="experiments")
args = parser.parse_args()
# Experiments Folder
if not os.path.exists(args.output_folder):
    os.makedirs(args.output_folder)
# Number of parallel tasks (degree of parallelism)
parallel_degree = 4

# Execute commands in parallel
with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
    for oversubscription in os_ratio:
        for drop in drop_strategy:
            for cc_algo in cc_algos:
                for lb_algo in load_balancing_algos:
                    for num_entropy in num_entropies:
                        for size in message_sizes:
                            # Create a directory if it does not exist at the output file path
                            dir_name = f"{args.output_folder}/"
                            dir_name += f"exp_os{oversubscription}_ccalgo{cc_algo}"
                            os.makedirs(dir_name, exist_ok=True)

                            tm_file = tm_template.format(size=size)
                            tm_file = "connection_matrices/dc.cm"
                            # Check if file does not exist
                            if not os.path.exists(tm_file):
                                cmd_to_run_cm_file = "python connection_matrices/gen_permutation.py {} {} {} {} 0 42".format(tm_file, 128, 128, size)
                                try:
                                    # Execute the command
                                    print(f"Creating CM named {cmd_to_run_cm_file}")
                                    subprocess.run(cmd_to_run_cm_file, shell=True, check=True)
                                except subprocess.CalledProcessError as e:
                                    print(f"An error occurred while running the command: {e}")

                            """ if lb_algo == "reps":
                                num_entropy = num_entropies_reps """
                            
                            output_file = f"out_cc{cc_algo}_drop{drop}_os{oversubscription}_size{size}_lb{lb_algo}_entropy{num_entropy}.txt"
                            command = getRunScript(dir_name, cc_algo, oversubscription, size, drop, tm_file, output_file, args.output_folder, lb_algo, num_entropy)
                            executor.submit(run_command, command)