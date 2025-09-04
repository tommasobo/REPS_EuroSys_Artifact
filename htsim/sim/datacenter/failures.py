import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

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

# Get FCT lists
file_size = 33554432
ideal_fct_0 = (file_size * 8 / 400 / 1000) + 20 # 8MB file size

fail_0_run = "./htsim_uec -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_0.out"
fail_10_run = "./htsim_uec -failures_input ../failures_input/10_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_10.out"
fail_20_run = "./htsim_uec -failures_input ../failures_input/20_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_20.out"
fail_30_run = "./htsim_uec -failures_input ../failures_input/30_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_30.out"
fail_40_run = "./htsim_uec -failures_input ../failures_input/40_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_40.out"
fail_50_run = "./htsim_uec -failures_input ../failures_input/50_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo freezing -exit_freeze 100000000 > fail_50.out"

plb_fail_0_run = "./htsim_uec -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_0.out"
plb_fail_10_run = "./htsim_uec -failures_input ../failures_input/10_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_10.out"
plb_fail_20_run = "./htsim_uec -failures_input ../failures_input/20_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_20.out"
plb_fail_30_run = "./htsim_uec -failures_input ../failures_input/30_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_30.out"
plb_fail_40_run = "./htsim_uec -failures_input ../failures_input/40_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 44 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_40.out"
plb_fail_50_run = "./htsim_uec -failures_input ../failures_input/50_percent_failed_cables.txt -sack_threshold 4000 -end 90000 -seed 440 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_1024_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/perm_n1024_s33554432.cm -load_balancing_algo plb > plb_fail_50.out"


""" os.system(fail_0_run)
os.system(fail_10_run)
os.system(fail_20_run)
os.system(fail_30_run)
os.system(fail_40_run)
os.system(fail_50_run)

os.system(plb_fail_0_run)
os.system(plb_fail_10_run)
os.system(plb_fail_20_run)
os.system(plb_fail_30_run)
os.system(plb_fail_40_run)
os.system(plb_fail_50_run) """

failure_0 = get_list_fct("fail_0.out")
failure_10 = get_list_fct("fail_10.out")
failure_20 = get_list_fct("fail_20.out")
failure_30 = get_list_fct("fail_30.out")
failure_40 = get_list_fct("fail_40.out")
failure_50 = get_list_fct("fail_50.out")

plb_failure_0 = get_list_fct("plb_fail_0.out")
plb_failure_10 = get_list_fct("plb_fail_10.out")
plb_failure_20 = get_list_fct("plb_fail_20.out")
plb_failure_30 = get_list_fct("plb_fail_30.out")
plb_failure_40 = get_list_fct("plb_fail_40.out")
plb_failure_50 = get_list_fct("plb_fail_50.out")

max_fcts = [max(failure_0), max(failure_10), max(failure_20), max(failure_30), max(failure_40), max(failure_50)]
plb_max_fcts= [max(plb_failure_0), max(plb_failure_10), max(plb_failure_20), max(plb_failure_30), max(plb_failure_40), max(plb_failure_50)]
ideal_fcts = [ideal_fct_0, ideal_fct_0 * (10/9), ideal_fct_0 * (10/8), ideal_fct_0 * (10/7), ideal_fct_0 * (10/6), ideal_fct_0 * (10/5)]
x_axis = [0, 10, 20, 30, 40, 50]

# Define custom color palette
dark2_hex_colors = [
    '#1b9e77',  # Green
    '#d95f02',  # Orange
    '#7570b3',  # Purple
    '#e7298a',  # Pink
]

# Define colors and line styles for REPS and OPS
reps_color = dark2_hex_colors[0]  # Green
ops_color = dark2_hex_colors[1]  # Orange

# Line styles and markers for the ratios
line_styles = ['-', '--']  # REPS and OPS line styles
markers = ['o', 's', '^', 'D']  # Markers for 1:1, 2:1, 4:1, and 8:1 ratios

# Update font sizes and styles
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 11,
    'figure.titlesize': 18,
    'grid.alpha': 0.8,
    'grid.color': '#cccccc',
    'axes.grid': True,
    'grid.linestyle': '-',  # Solid grid lines
})

# Helper function to plot CDF with custom styles and markers
def plot_cdf(data, label, color, linestyle, marker):
    data_sorted = np.sort(data)
    cdf = np.arange(1, len(data_sorted) + 1) / len(data_sorted)
    plt.plot(data_sorted, cdf, label=label, color=color, linestyle=linestyle, marker=marker, markevery=0.1, markersize=4.4, linewidth=2.2)

# Plotting the CDF
fig = plt.figure(figsize=(6.5, 2.7))

static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'OPS': '#d95f02',  # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7698a',  # Pink
    'PLB': '#66a61e',  # Green
    'REPS': '#e6ab02',  # Yellow
    'MPRDMA': '#a6761d',  # Brown
    'Flowlet': '#666666',   # Gray
    'Adaptive RoCE': '#1f78b4'   # Gray
}


lb_markers = {
    'ECMP': 'o',      # Circle
    'OPS': 's',       # Square
    'BitMap': 'v',    # Triangle down
    'MPTCP': '^',     # Triangle up
    'PLB': 'P',       # Plus
    'REPS': 'X',      # X
    'MPRDMA': 'D',    # Diamond
    'Flowlet': '*',   # Star
}

# REPS lines with markers
plt.plot(x_axis, max_fcts, label="Max FCT", color=static_color_mapping['REPS'], marker=lb_markers['REPS'], linestyle='-', linewidth=2.2, markersize=7.2)
plt.plot(x_axis, plb_max_fcts, label="Max FCT", color=static_color_mapping['PLB'], marker=lb_markers['PLB'], linestyle='-', linewidth=2.2, markersize=7.2)
plt.plot(x_axis, ideal_fcts, label="Max FCT", color=static_color_mapping['MPTCP'], marker=lb_markers['MPTCP'], linestyle='-', linewidth=2.2, markersize=7.2)


# Annotate slowdown on the plot
for i, (x, max_fct, ideal_fct) in enumerate(zip(x_axis, max_fcts, ideal_fcts)):
    slowdown = (max_fct / ideal_fct - 1) * 100
    if (slowdown < 0):
        slowdown = 1

    if (i == 0):
        x = x + 3
        max_fct = max_fct + 60
        
    max_fct = max_fct + 80
    plt.text(x, max_fct, f"{slowdown:.0f}%", fontsize=10, ha='right', va='bottom', color=static_color_mapping['REPS'])

# Annotate slowdown on the plot
for i, (x, max_fct, ideal_fct) in enumerate(zip(x_axis, plb_max_fcts, ideal_fcts)):
    slowdown = (max_fct / ideal_fct - 1) * 100
    if (slowdown < 0):
        slowdown = 1
    if (i == 0):
        x = x + 4
        max_fct = max_fct + 150
    if (i == 5):
        x = x + 2.5
        max_fct = max_fct - 700
    plt.text(x, max_fct + 70, f"{slowdown:.0f}%", fontsize=10, ha='right', va='bottom', color=static_color_mapping['PLB'])


plt.xlabel('Network Cables Failure Percentage (%)', fontsize=13)
plt.ylabel('Max FCT (μs)', fontsize=13)

# Custom grid
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Custom legend handles
legend_elements = [
    Line2D([0], [0], color=dark2_hex_colors[0], marker=markers[0], lw=2, label='1:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[1], marker=markers[1], lw=2, label='2:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[2], marker=markers[2], lw=2, label='4:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[3], marker=markers[3], lw=2, label='8:1 Ratio'),
    Line2D([0], [0], color='none', lw=0, label=''),  # Smaller empty line for spacing
    Line2D([0], [0], color='black', lw=2, linestyle='--', label='OPS'),
    Line2D([0], [0], color='black', lw=2, linestyle='-', label='REPS'),
]

# Create the legend
plt.legend(handles=legend_elements, loc='lower right', bbox_to_anchor=(1.04, -0.05), fontsize=11, ncol=1, frameon=False)

# Save the plot as PNG and PDF with better formatting
plt.tight_layout()
plt.savefig('failures.png', dpi=300, bbox_inches='tight')
plt.savefig('failures.pdf', dpi=300, bbox_inches='tight')

# Show the plot
plt.show()
