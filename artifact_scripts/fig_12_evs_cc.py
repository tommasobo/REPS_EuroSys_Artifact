import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import matplotlib
import concurrent.futures

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Ensure output directories exist
for directory in [
    "../artifact_results",
    "../artifact_results/fig_12_evs_cc",
    "../artifact_results/fig_12_evs_cc/data",
    "../artifact_results/fig_12_evs_cc/plots"
]:
    os.makedirs(directory, exist_ok=True)

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
string_obs_32 = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 32 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_32.out"
string_obs_256 = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 256 -sender_cc_only -sender_cc_algo mprdma  -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_256.out"
string_obs_65= "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_65.out"

string_reps_32 = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 32 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_32.out"
string_reps_256 = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 256 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_256.out"
string_reps_65 = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_65.out"

# Run all simulation commands in parallel with up to 4 threads
commands = [
    string_obs_32, string_obs_256, string_obs_65,
    string_reps_65, string_reps_32, string_reps_256
]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    executor.map(os.system, commands)

# Get FCT lists
fct_list_obs_dctcp = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_32.out")
fct_list_obs_eqds = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_256.out")
fct_list_obs_nscc = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_65.out")

fct_list_reps_dctcp = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_32.out")
fct_list_reps_eqds = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_256.out")
fct_list_reps_nscc = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_65.out")
# Get FCT lists (already done before)

# Define custom color palette (inspired by your other file)
dark2_hex_colors = [
    '#1b9e77',  # Green
    '#d95f02',  # Orange
    '#7570b3',  # Purple
    '#e7298a',  # Pink
    '#66a61e',  # Light Green
    '#e6ab02',  # Yellow
    '#a6761d',  # Brown
    '#666666'   # Gray
]

# Define colors and line styles for REPS and OPS
reps_color = dark2_hex_colors[0]  # Green
ops_color = dark2_hex_colors[1]  # Orange

# Line styles
line_styles = ['-', ':', '-.', ':']

# Update font sizes and styles
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 11,
    'ytick.labelsize': 11,
    'legend.fontsize': 11,
    'figure.titlesize': 18,
    'grid.alpha': 0.8,
    'grid.color': '#cccccc',
    'axes.grid': True,
    'grid.linestyle': '-',  # Solid grid lines
})
markers = ['o', 's', '^', 'D']  # Markers for 1:1, 2:1, 4:1, and 8:1 ratios

# Helper function to plot CDF with custom styles
def plot_cdf(data, label, color, linestyle, marker):
    data_sorted = np.sort(data)
    cdf = np.arange(1, len(data_sorted) + 1) / len(data_sorted)
    # Determine marker positions
    mark_indices = [len(data_sorted) - 1]

    line_width = 2.2
    if (linestyle == ':'):
        line_width = 3.0
    else:
        line_width = 2.1

    plt.plot(
        data_sorted,
        cdf,
        label=label,
        color=color,
        linestyle=linestyle,
        marker=marker,
        markevery=mark_indices,  # Only plot markers at specified indices
        markersize=5.5,
        linewidth=line_width
    )
# Plotting the CDF
fig = plt.figure(figsize=(3.6, 3.2))

# REPS lines
plot_cdf(fct_list_obs_dctcp, "OPS - DCTCP", dark2_hex_colors[2], line_styles[1], markers[0])
plot_cdf(fct_list_obs_eqds, "OPS - EQDS", dark2_hex_colors[3], line_styles[1], markers[1])
plot_cdf(fct_list_obs_nscc, "OPS - NSCC", dark2_hex_colors[4], line_styles[1], markers[2])

# OPS lines
plot_cdf(fct_list_reps_dctcp, "REPS - DCTCP", dark2_hex_colors[2], line_styles[0], markers[0])
plot_cdf(fct_list_reps_eqds, "REPS - EQDS", dark2_hex_colors[3], line_styles[0], markers[1])
plot_cdf(fct_list_reps_nscc, "REPS - NSCC", dark2_hex_colors[4], line_styles[0], markers[2])

plt.xlabel('Flow Completion Time (μs)', fontsize=13)
plt.ylabel('CDF', fontsize=13)

# Custom grid
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Custom legend handles
legend_elements = [
    
    Line2D([0], [0], color=dark2_hex_colors[2],  marker=markers[0], lw=2, label='32 EVs'),
    Line2D([0], [0], color=dark2_hex_colors[3], marker=markers[1],lw=2, label='256 EVs'),
    Line2D([0], [0], color=dark2_hex_colors[4], marker=markers[2],lw=2, label='64K EVs'),
    Line2D([0], [0], color='none', lw=0, label=''),  # Smaller empty line for spacing

    Line2D([0], [0], color='black', lw=3, linestyle=':', label='OPS'),
    Line2D([0], [0], color='black', lw=2, linestyle='-', label='REPS'),
]

# Create the legend
plt.legend(handles=legend_elements, loc='lower right', bbox_to_anchor=(1, -0.41), fontsize=11, ncol=6, frameon=False)
plt.xlim([170, 260])
plt.ylim([0, 1.05])
# Save the plot as PNG and PDF with better formatting
plt.tight_layout()
plt.savefig('../artifact_results/fig_12_evs_cc/plots/evs.png', dpi=300, bbox_inches='tight')
plt.savefig('../artifact_results/fig_12_evs_cc/plots/evs.pdf', dpi=300, bbox_inches='tight')



# Run the simulations permutation_size262144B.cm permutation_size8388608B.cm
string_obs_dctcp = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_dctcp.out"
string_obs_eqds = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_eqds.out"
sting_obs_nscc= "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo nscc -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo oblivious > ../artifact_results/fig_12_evs_cc/data/obs_nscc.out"

string_reps_dctcp = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_dctcp.out"
string_reps_eqds = "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_eqds.out"
sting_reps_nscc= "../htsim/sim/datacenter/htsim_uec -sack_threshold 4000 -end 90000 -seed 20 -paths 65535 -sender_cc_only -sender_cc_algo nscc -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm ../htsim/sim/datacenter/connection_matrices/perm_n128_s8388608.cm -load_balancing_algo reps > ../artifact_results/fig_12_evs_cc/data/reps_nscc.out"

# Run all remaining simulation commands in parallel with up to 4 threads
commands2 = [
    string_obs_dctcp, string_obs_eqds, sting_obs_nscc,
    string_reps_dctcp, string_reps_eqds, sting_reps_nscc
]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    executor.map(os.system, commands2)

# Get FCT lists
fct_list_obs_dctcp = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_dctcp.out")
fct_list_obs_eqds = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_eqds.out")
fct_list_obs_nscc = get_list_fct("../artifact_results/fig_12_evs_cc/data/obs_nscc.out")

fct_list_reps_dctcp = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_dctcp.out")
fct_list_reps_eqds = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_eqds.out")
fct_list_reps_nscc = get_list_fct("../artifact_results/fig_12_evs_cc/data/reps_nscc.out")
# Get FCT lists (already done before)

# Define custom color palette (inspired by your other file)
dark2_hex_colors = [
    '#1b9e77',  # Green
    '#d95f02',  # Orange
    '#7570b3',  # Purple
    '#e7298a',  # Pink
    '#66a61e',  # Light Green
    '#e6ab02',  # Yellow
    '#a6761d',  # Brown
    '#666666'   # Gray
]

# Define colors and line styles for REPS and OPS
reps_color = dark2_hex_colors[0]  # Green
ops_color = dark2_hex_colors[1]  # Orange

# Line styles
line_styles = ['-', ':', '-.', ':']

# Update font sizes and styles
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 11,
    'ytick.labelsize': 11,
    'legend.fontsize': 11,
    'figure.titlesize': 18,
    'grid.alpha': 0.8,
    'grid.color': '#cccccc',
    'axes.grid': True,
    'grid.linestyle': '-',  # Solid grid lines
})
markers = ['o', 's', '^', 'D']  # Markers for 1:1, 2:1, 4:1, and 8:1 ratios

# Helper function to plot CDF with custom styles
def plot_cdf(data, label, color, linestyle, marker):
    data_sorted = np.sort(data)
    cdf = np.arange(1, len(data_sorted) + 1) / len(data_sorted)
    # Determine marker positions
    mark_indices = [len(data_sorted) - 1]

    line_width = 2.2
    if (linestyle == ':'):
        line_width = 3.0
    else:
        line_width = 2.1

    plt.plot(
        data_sorted,
        cdf,
        label=label,
        color=color,
        linestyle=linestyle,
        marker=marker,
        markevery=mark_indices,  # Only plot markers at specified indices
        markersize=5.5,
        linewidth=line_width
    )
# Plotting the CDF
fig = plt.figure(figsize=(3.6, 3.2))

# REPS lines
plot_cdf(fct_list_obs_dctcp, "OPS - DCTCP", dark2_hex_colors[2], line_styles[1], markers[0])
plot_cdf(fct_list_obs_eqds, "OPS - EQDS", dark2_hex_colors[3], line_styles[1], markers[1])
plot_cdf(fct_list_obs_nscc, "OPS - NSCC", dark2_hex_colors[4], line_styles[1], markers[2])

# OPS lines
plot_cdf(fct_list_reps_dctcp, "REPS - DCTCP", dark2_hex_colors[2], line_styles[0], markers[0])
plot_cdf(fct_list_reps_eqds, "REPS - EQDS", dark2_hex_colors[3], line_styles[0], markers[1])
plot_cdf(fct_list_reps_nscc, "REPS - NSCC", dark2_hex_colors[4], line_styles[0], markers[2])

plt.xlabel('Flow Completion Time (us)', fontsize=13)
plt.ylabel('CDF', fontsize=13)

# Custom grid
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

plt.ylim(0, 1.05)


# Custom legend handles
legend_elements = [
    Line2D([0], [0], color=dark2_hex_colors[2],  marker=markers[0], lw=2, label='DCTCP'),
    Line2D([0], [0], color=dark2_hex_colors[3],  marker=markers[1], lw=2, label='EQDS'),
    Line2D([0], [0], color=dark2_hex_colors[4],  marker=markers[2], lw=2, label='INTERNAL'),
    Line2D([0], [0], color='none', lw=0, label=''),  # Smaller empty line for spacing
    Line2D([0], [0], color='black', lw=3, linestyle=':', label='OPS'),
    Line2D([0], [0], color='black', lw=2, linestyle='-', label='REPS'),
]

# Create the legend
#plt.legend(handles=legend_elements, loc='lower right', bbox_to_anchor=(0.51, 0.35), fontsize=11, ncol=1, frameon=False)
plt.legend(handles=legend_elements, loc='lower right', bbox_to_anchor=(1.04, -0.36), fontsize=11, ncol=6, frameon=False)

# Save the plot as PNG and PDF with better formatting
plt.tight_layout()
plt.savefig('../artifact_results/fig_12_evs_cc/plots/cc.png', dpi=300, bbox_inches='tight')
plt.savefig('../artifact_results/fig_12_evs_cc/plots/cc.pdf', dpi=300, bbox_inches='tight')
