import subprocess
import concurrent.futures
import shlex
import os
import argparse

# Script template with placeholders

script_template = "./htsim_uec -sack_threshold 4000 -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 9000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -linkspeed 400000 -ecn 25 75 -q 100 -cwnd 140 > {output_dir}/{output_file}"
script_template = "./htsim_uec -connections_mapping {trim} {enable_bg_option} {failed_num} {asy_string} -sack_threshold 4000 -load_balancing_algo {algo} -tm {tm_file} {other_stuff} -end 90000 -seed 42 -paths {paths} {cc_strat} {trimming_strat} -topo {topo_file} -enable_qa_gate -linkspeed 400000 -ecn {kmin} {kmax} -q {queue} -cwnd {cwnd} > {output_dir}/{output_file}"

topo_tiers = 2
link_speed = 400 * 1000
message_sizes = [int(8388608/2), int(16777216/2), int(16777216)]
cc_algos = ["mprdma"]
drop_strategy = ["trimming"]
os_ratio = [1]
load_balancing_algos = ["reps", "bitmap", "oblivious", "ecmp"]
load_balancing_algos = ["freezing", "bitmap", "oblivious", "ecmp", "plb", "mp", "mprdma", "flowlet", "nvidia"]

num_entropies = [65535]
files_input = []

def getBDP(link_speed, tiers, link_delay):
    link_speed = link_speed / 1000
    network_rtt = link_delay * (tiers * 2 * 2) + (tiers * 2 * (4096 * 8 / link_speed))
    return network_rtt * link_speed / 8 / 4096

def getRunScript(dir_name, enable_bg, is_trim, skip_asy, num_failed, size_topo, cc_algo, oversubscription, size, drop, tm_file, output_file, main_folder, lb_algo, num_entropy):
    
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
        topo_file_name = f"topologies/reps/fat_tree_{size_topo}_1os_{topo_tiers}t_400g.topo"
    elif (oversubscription == 4):
        topo_file_name = "topologies/fat_tree_1024_4os.topo"
    elif (oversubscription == 8):
        topo_file_name = "topologies/fat_tree_1024_8os.topo"

    other = ""
    if (lb_algo == "mp"):
        other = "-mp_flows 8 "

    if ("BG" in tm_file):
        other += "-mixed_lb_traffic"

    bdp = getBDP(link_speed, topo_tiers, 1000)

    queue_size = int(bdp)
    ecn_min = int(bdp / 4)
    ecn_max = int(bdp * 3 / 4)

    trim_option = ""
    failed_liks_option = ""
    if (is_trim == "off"):
        trim_option = "-disable_trim"
    if (int(num_failed) > 0):
        failed_liks_option = "-failed {}".format(num_failed)

    if (lb_algo == "bitmap"):
        num_entropy = 256

    skip_s = ""
    if (skip_asy == "on"):
        skip_s = "-skip_asy"

    enable_bg_s = ""
    if (enable_bg == "on"):
        enable_bg_s += "-mixed_lb_traffic"
    
    command = script_template.format(trim=trim_option, enable_bg_option=enable_bg_s, asy_string=skip_s, failed_num=failed_liks_option, other_stuff=other, queue=int(queue_size*1.0), kmin=ecn_min, kmax=ecn_max, cwnd=int(queue_size*1.5),  algo=lb_algo, paths=num_entropy, tm_file=tm_file, cc_strat=cc_algo, trimming_strat=drop, topo_file=topo_file_name, output_file=output_file, output_dir=dir_name)
    #print(f"Running command: {command}")
    return command

# Function to run a command using subprocess
def run_command(cmd):
    os.system(cmd)


def get_msg_size(name_file):
    parts = name_file.split('_s')
    number_part = 0
    # Check if '_d' was found and there is something after it
    if len(parts) > 1:
        # Take the part after '_d' and split it by the dot to isolate the number
        if ("incast" in name_file):
            number_part = parts[1].split('_d')[0]
        else:
            number_part = parts[1].split('.')[0]
    return number_part

def get_short_name(name_file):
    if ("perm" in name_file):
        return "perm"
    elif ("tornado" in name_file):
        return "tornado"
    elif ("incast" in name_file):
        # Split the string by '_d'
        parts = name_file.split('_d')
        number_part = 0
        # Check if '_d' was found and there is something after it
        if len(parts) > 1:
            # Take the part after '_d' and split it by the dot to isolate the number
            number_part = parts[1].split('.')[0]
        return "incast{}".format(number_part)

def create_tm_files(message_size, type, topo_size, incast_degree=0):
    # Check if file does not exist
    cmd_to_run_cm_file = ""
    if (type == "incast"):
        cmd_to_run_cm_file = "python connection_matrices/gen_incast.py {} {} {} {} 0 42 1".format("connection_matrices/incast_n{}_s{}_d{}.cm".format(topo_size, message_size, incast_degree), topo_size, incast_degree, message_size)
        files_input.append("connection_matrices/incast_n{}_s{}_d{}.cm".format(topo_size, message_size, incast_degree))
    elif (type == "permutation"):   
        cmd_to_run_cm_file = "python connection_matrices/gen_permutation.py {} {} {} {} 0 42".format("connection_matrices/perm_n{}_s{}.cm".format(topo_size, message_size), topo_size, topo_size, message_size)
        files_input.append("connection_matrices/perm_n{}_s{}.cm".format(topo_size, message_size))
    elif (type == "tornado"):
        cmd_to_run_cm_file = "python connection_matrices/gen_tornado.py {} {} {} {} 0 42".format("connection_matrices/tornado_n{}_s{}.cm".format(topo_size, message_size), topo_size, topo_size, message_size)
        files_input.append("connection_matrices/tornado_n{}_s{}.cm".format(topo_size, message_size))
    elif (type == "permutation_bg"):   
        cmd_to_run_cm_file = "python connection_matrices/gen_permutation.py {} {} {} {} 0 42".format("connection_matrices/permBG_n{}_s{}.cm".format(topo_size, message_size), topo_size, topo_size, message_size)
        files_input.append("connection_matrices/permBG_n{}_s{}.cm".format(topo_size, message_size))
    elif (type == "tornado_bg"):
        cmd_to_run_cm_file = "python connection_matrices/gen_tornado.py {} {} {} {} 0 42".format("connection_matrices/tornadoBG_n{}_s{}.cm".format(topo_size, message_size), topo_size, topo_size, message_size)
        files_input.append("connection_matrices/tornadoBG_n{}_s{}.cm".format(topo_size, message_size))
    try:
        # Execute the command
        #print(f"Creating CM named {cmd_to_run_cm_file}")
        subprocess.run(cmd_to_run_cm_file, shell=True, check=True)
    except subprocess.CalledProcessError as e:
        #print(f"An error occurred while running the command: {e}")
        a = 1

def main():

    parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
    parser.add_argument('--output_folder', required=False, help='Parent output folder where to save all results', default="experiments")
    parser.add_argument('--trim', required=False, help='Trim Active Or not', default="on")
    parser.add_argument('--size_topo', type=int, required=True, help='Size Topo', default=128)
    parser.add_argument('--asymmetry_links', required=False, help='Num downgraded links', default=0)
    parser.add_argument('--skip_asy', required=False, help='', default="off")
    parser.add_argument('--enable_bg', required=False, help='', default="off")

    args = parser.parse_args()

    for msg_size in message_sizes:
        create_tm_files(msg_size, "permutation", args.size_topo)
        create_tm_files(msg_size, "tornado", args.size_topo)
        """ create_tm_files(msg_size, "permutation_bg", args.size_topo)
        create_tm_files(msg_size, "tornado_bg", args.size_topo) """
        create_tm_files(msg_size, "incast", args.size_topo, 8)

    
    # Experiments Folder
    if not os.path.exists(args.output_folder):
        os.makedirs(args.output_folder)
    # Number of parallel tasks (degree of parallelism)
    parallel_degree = 6

    #print(len(files_input))
    # Execute commands in parallel
    with concurrent.futures.ThreadPoolExecutor(max_workers=parallel_degree) as executor:
        for file in files_input:
            for oversubscription in os_ratio:
                for drop in drop_strategy:
                    for cc_algo in cc_algos:
                        for lb_algo in load_balancing_algos:
                            for num_entropy in num_entropies:
                                # Create a directory if it does not exist at the output file path
                                dir_name = f"{args.output_folder}/"
                                os.makedirs(dir_name, exist_ok=True)

                                short_name = get_short_name(file)
                                size_name = get_msg_size(file)
                                if ("BG" in file):
                                    bg_on = "on"
                                else:
                                    bg_on = "off"
                                
                                output_file = f"out_workload{short_name}_cc{cc_algo}_drop{drop}_os{oversubscription}_size{size_name}_lb{lb_algo}_entropy{num_entropy}_bg{bg_on}.txt"
                                command = getRunScript(dir_name, args.enable_bg, args.trim, args.skip_asy, args.asymmetry_links, args.size_topo, cc_algo, oversubscription, size_name, drop, file, output_file, args.output_folder, lb_algo, num_entropy)
                                executor.submit(run_command, command)

if __name__ == "__main__":
    main()