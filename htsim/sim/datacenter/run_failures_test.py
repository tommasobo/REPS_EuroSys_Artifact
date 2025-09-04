import subprocess
import concurrent.futures
import shlex
import os
import argparse

# Script template with placeholders
link_speed = 400 * 1000

#script_template = "./htsim_uec -mixed_lb_traffic -sack_threshold 4000 -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 9000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -linkspeed 400000 -ecn 25 75 -q 100 -cwnd 140 > {output_dir}/{output_file}"
script_template = "./htsim_uec -connections_mapping -sack_threshold 4000 {failures_input} {fail_type} {fail_part} -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 90000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -enable_fi -linkspeed 400000 -ecn {kmin} {kmax} -q {queue} -cwnd {cwnd} > {output_dir}/{output_file}"
tm_template = "connection_matrices/permutation_size{size}B.cm"

message_sizes = [4194304*2]

cc_algos = ["mprdma"]
drop_strategy = ["trimming"]
os_ratio = [1]
load_balancing_algos = ["reps", "freezing", "oblivious", "flowlet"]
load_balancing_algos = ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "flowlet", "mp"]
load_balancing_algos = ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "flowlet", "mp"]
#load_balancing_algos = ["oblivious"]

num_entropies = [4096]
num_entropies_bitmap = 256
num_entropies_reps = 2**12

failure_modes = ["degrade_one_cable.txt", "ber_cable_one_percent.txt", "fail_one_switch.txt", "fail_one_cable.txt"]
failure_modes = ["degrade_one_cable.txt", "fail_one_cable.txt", "fail_one_switch_one_cable.txt", "10_percent_failed_cables.txt", "10_percent_failed_switches_and_cables.txt", "10_percent_failed_switches.txt", "10_percent_us-cs-cables.txt", "50_percent_failed_cables.txt", "ber_cable_one_percent.txt", "fail_new_cable_every_100us_for_50us.txt", "fail_one_cable_for_300ms.txt", "packet_drop_rate_10^-4.txt", "periodically_fail_one_cable.txt"]
failure_modes = ["fail_one_switch_one_cable.txt", "fail_one_cable.txt", "fail_one_switch.txt", "10_percent_failed_cables.txt", "10_percent_failed_switches_and_cables.txt", "10_percent_failed_switches.txt", "ber_cable_one_percent.txt", "ber_switch_one_percent.txt"]

def getBDP(link_speed, tiers, link_delay):
    link_speed = link_speed / 1000
    network_rtt = link_delay * (tiers * 2 * 2) + (tiers * 2 * (4096 * 8 / link_speed))
    print(f"Network RTT: {network_rtt}")
    return network_rtt * link_speed / 8 / 4096

def getRunScript(dir_name, cc_algo, oversubscription, size, drop, tm_file, output_file, main_folder, lb_algo, num_entropy, failure_type):

    cc_string = ""
    if (cc_algo == "nscc"):
        cc_algo = "-sender_cc_only"
    elif (cc_algo == "rccc"):
        cc_algo = ""
    elif (cc_algo == "both"):
        cc_algo = "-sender_cc"
    elif (cc_algo == "constant"):
        cc_algo = "-sender_cc_only -sender_cc_algo constant"
    elif (cc_algo == "mprdma"):
        cc_algo = "-sender_cc_only -sender_cc_algo mprdma"

    drop_string = ""
    if (drop == "trimming"):
        drop = ""
    elif (drop == "timeout"):
        drop = "-disable_trim"

    topo_file_name = ""
    if (oversubscription == 1):
        topo_file_name = "topologies/reps/fat_tree_128_1os_2t_400g.topo"
    elif (oversubscription == 4):
        topo_file_name = "topologies/fat_tree_1024_4os.topo"
    elif (oversubscription == 8):
        topo_file_name = "topologies/fat_tree_1024_8os.topo"

    other = ""
    if (lb_algo == "mp"):
        other = "-mp_flows 8"

    bdp = getBDP(link_speed, 2, 1000)

    queue_size = int(bdp)
    ecn_min = int(bdp / 4)
    ecn_max = int(bdp * 3 / 4)
    
    if os.path.exists(dir_name + "/" + output_file):
        return
    
    fail_part = ""
    failure_param = "-failures_input"
    if (failure_type == "degrade_one_cable.txt"):
        failure_type = ""
        fail_part = "-failed 1"
        failure_param = ""

    if (failure_type != ""):
        failure_type = "../failures_input/" + failure_type

    command = script_template.format(failures_input=failure_param, other_stuff=other, fail_part=fail_part, queue=int(queue_size*1.5), kmin=ecn_min, kmax=ecn_max, cwnd=queue_size*1.5, fail_type=failure_type, algo=lb_algo, paths=num_entropy, tm_file=tm_file, cc_strat=cc_algo, trimming_strat=drop, topo_file=topo_file_name, output_file=output_file, output_dir=dir_name)
    print(f"Running command: {command}")
    return command

# Function to run a command using subprocess
def run_command(cmd):
    os.system(cmd)

parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
parser.add_argument('--output_folder', required=False, help='Parent output folder where to save all results', default="experiments")
args = parser.parse_args()
# Experiments Folder
if not os.path.exists(args.output_folder):
    os.makedirs(args.output_folder)
# Number of parallel tasks (degree of parallelism)
parallel_degree = 8

# Execute commands in parallel
with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
    for oversubscription in os_ratio:
        for drop in drop_strategy:
            for cc_algo in cc_algos:
                for lb_algo in load_balancing_algos:
                    for failure_type in failure_modes:
                        for num_entropy in num_entropies:
                            for size in message_sizes:
                                # Create a directory if it does not exist at the output file path
                                dir_name = f"{args.output_folder}/"
                                tmp_fail = failure_type.replace(".txt", "")
                                tmp_fail = tmp_fail.replace("_", "")
                                dir_name += f"exp_os{oversubscription}_ccalgo{cc_algo}_failure_{tmp_fail}"
                                os.makedirs(dir_name, exist_ok=True)

                                tm_file = tm_template.format(size=size)
                                tm_file = "connection_matrices/load10.cm"
                                # Check if file does not exist
                                if not os.path.exists("connection_matrices/" + tm_file):
                                    cmd_to_run_cm_file = "python connection_matrices/gen_permutation.py {} {} {} {} 0 42".format(tm_file, 128, 128, size)
                                    try:
                                        # Execute the command
                                        print(f"Creating CM named {cmd_to_run_cm_file}")
                                        subprocess.run(cmd_to_run_cm_file, shell=True, check=True)
                                    except subprocess.CalledProcessError as e:
                                        print(f"An error occurred while running the command: {e}")

                                if (lb_algo == "bitmap"):
                                    num_entropy = num_entropies_bitmap
                                
                                output_file = f"out_cc{cc_algo}_drop{drop}_os{oversubscription}_size{size}_lb{lb_algo}_entropy{num_entropy}.txt"
                                command = getRunScript(dir_name, cc_algo, oversubscription, size, drop, tm_file, output_file, args.output_folder, lb_algo, num_entropy, failure_type)
                                executor.submit(run_command, command)