import os
import re
import numpy as np
import matplotlib.pyplot as plt

# Function to get FCTs
def get_list_fct(name_file_to_use):
    temp_list = []
    try:
        with open(name_file_to_use) as file:
            for line in file:
                pattern = r"finished at (\d+)"
                match = re.search(pattern, line)
                if match:
                    actual_fct = float(match.group(1))
                    temp_list.append(actual_fct)
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    return temp_list

# Data extraction
ack_ratios = [1, 2, 4, 8, 16]


# Get FCT lists
string_obs_1 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > obs_1.out"
string_obs_2 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 8000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > obs_2.out"
string_obs_4 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 16000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > obs_4.out"
string_obs_8 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 32000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > obs_8.out"
string_obs_16 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 64000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > obs_16.out"

string_reps_1 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo freezing > reps_1.out"
string_reps_2 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 8000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo freezing > reps_2.out"
string_reps_4 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 16000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo freezing > reps_4.out"
string_reps_8 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 32000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo freezing > reps_8.out"
string_reps_16 = "./htsim_uec -failures_input ../failures_input/8_percent_failed_cables.txt -sack_threshold 64000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n128_s8388608.cm -load_balancing_algo freezing > reps_16.out"

os.system(string_obs_1)
os.system(string_obs_2)
os.system(string_obs_4)
os.system(string_obs_8)
os.system(string_obs_16)
os.system(string_reps_1)
os.system(string_reps_2)
os.system(string_reps_4)
os.system(string_reps_8)
os.system(string_reps_16)

# OBS FCTs
fct_list_obs = [
    max(get_list_fct("obs_1.out")),
    max(get_list_fct("obs_2.out")),
    max(get_list_fct("obs_4.out")),
    max(get_list_fct("obs_8.out")),
    max(get_list_fct("obs_16.out")),
]

# REPS FCTs
fct_list_reps = [
    max(get_list_fct("reps_1.out")),
    max(get_list_fct("reps_2.out")),
    max(get_list_fct("reps_4.out")),
    max(get_list_fct("reps_8.out")),
    max(get_list_fct("reps_16.out")),
]

# Plot
plt.figure(figsize=(8, 6))
plt.plot(ack_ratios, fct_list_obs, label="OPS", marker="o", linestyle="-", color="blue")
plt.plot(ack_ratios, fct_list_reps, label="REPS", marker="s", linestyle="--", color="orange")

# Formatting
plt.xscale("log")  # Use logarithmic scale for consistent spacing
plt.xlabel("ACK Ratio", fontsize=14)
plt.ylabel("Max Flow Completion Time", fontsize=14)
plt.title("Max Flow Completion Time vs ACK Ratio", fontsize=16)
plt.xticks(ack_ratios, labels=[str(r) for r in ack_ratios], fontsize=12)  # Ensure correct tick labels
plt.yticks(fontsize=12)

# Simplified legend
plt.legend(loc="upper left", fontsize=12)

# Save and show
plt.tight_layout()
plt.savefig("fail_max_fct_vs_ack_ratio.png", dpi=300)
plt.savefig("fail_max_fct_vs_ack_ratio.pdf", dpi=300)
plt.show()
