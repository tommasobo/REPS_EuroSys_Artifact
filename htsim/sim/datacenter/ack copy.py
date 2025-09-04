import os
import re
import numpy as np
import matplotlib.pyplot as plt

# Function to get FCTs
def get_list_fct(name_file_to_use):
    """
    Extracts the finished-at runtime values from the file.
    """
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

# Run the simulations permutation_size262144B.cm permutation_size8388608B.cm
string_obs = "./htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo oblivious > obs.out"
sting_obs_2_compression = "./htsim_uec -sack_threshold 8192 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo oblivious > obs_2_ack.out"
string_obs_4_compression = "./htsim_uec -sack_threshold 16384 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo oblivious > obs_4_ack.out"
string_obs_8_compression = "./htsim_uec -sack_threshold 32766 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo oblivious > obs_8_ack.out"

string_no_compression = "./htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo freezing > reps_no_ack.out"
string_2_compression = "./htsim_uec -sack_threshold 8192 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo freezing > reps_2_ack.out"
string_4_compression = "./htsim_uec -sack_threshold 16384 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo freezing > reps_4_ack.out"
string_8_compression = "./htsim_uec -sack_threshold 32766 -end 90000 -seed 20 -paths 60000 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/permutation_size262144B.cm -load_balancing_algo freezing > reps_8_ack.out"

os.system(string_obs)
os.system(sting_obs_2_compression)
os.system(string_obs_4_compression)
os.system(string_obs_8_compression)
os.system(string_no_compression)
os.system(string_2_compression)
os.system(string_4_compression)
os.system(string_8_compression)


# Get FCT lists
fct_list_obs = get_list_fct("obs.out")
fct_list_obs_2_compression = get_list_fct("obs_2_ack.out")
fct_list_obs_4_compression = get_list_fct("obs_4_ack.out")
fct_list_obs_8_compression = get_list_fct("obs_8_ack.out")

fct_list_no_compression = get_list_fct("reps_no_ack.out")
fct_list_2_compression = get_list_fct("reps_2_ack.out")
fct_list_4_compression = get_list_fct("reps_4_ack.out")
fct_list_8_compression = get_list_fct("reps_8_ack.out")

# Helper function to plot CDF with custom styles
def plot_cdf(data, label, color, linestyle):
    data_sorted = np.sort(data)
    cdf = np.arange(1, len(data_sorted) + 1) / len(data_sorted)
    plt.plot(data_sorted, cdf, label=label, color=color, linestyle=linestyle)

# Plotting the CDF
plt.figure(figsize=(10, 6))

# Define styles
reps_color = 'blue'
ops_color = 'green'

# REPS with different line styles for compression ratios
plot_cdf(fct_list_no_compression, "REPS - No Compression", reps_color, '-')
plot_cdf(fct_list_2_compression, "REPS - 2:1 Compression", reps_color, '--')
plot_cdf(fct_list_4_compression, "REPS - 4:1 Compression", reps_color, '-.')
plot_cdf(fct_list_8_compression, "REPS - 8:1 Compression", reps_color, ':')

# OPS with different line styles for compression ratios
plot_cdf(fct_list_obs, "OPS - No Compression", ops_color, '-')
plot_cdf(fct_list_obs_2_compression, "OPS - 2:1 Compression", ops_color, '--')
plot_cdf(fct_list_obs_4_compression, "OPS - 4:1 Compression", ops_color, '-.')
plot_cdf(fct_list_obs_8_compression, "OPS - 8:1 Compression", ops_color, ':')

plt.xlabel('Flow Completion Time (us)')
plt.ylabel('CDF')
plt.title('CDF of Flow Completion Times for Different Compression Levels (REPS vs OPS)')
plt.legend(loc='lower right')
plt.grid(True)

# Save the plot as PNG and PDF
plt.savefig('cdf_plot_fail.png', dpi=300)
plt.savefig('cdf_plot_fail.pdf', dpi=300)

# Show the plot
plt.show()
