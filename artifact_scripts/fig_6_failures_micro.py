import re
import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib import gridspec
import matplotlib

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Ensure output directories exist
for directory in [
    "../artifact_results",
    "../artifact_results/fig_6_failures_micro",
    "../artifact_results/fig_6_failures_micro/data",
    "../artifact_results/fig_6_failures_micro/plots"
]:
    os.makedirs(directory, exist_ok=True)

# Constants
LINK_SPEED_Gbps = 400  # Link speed in Gbps
PACKET_SIZE_BYTES = 4096  # Packet size in bytes
TIME_INTERVAL_NS = 20000  # Time interval in nanoseconds

# Convert Gbps to Bytes per nanosecond
LINK_CAPACITY_BYTES_PER_NS = (LINK_SPEED_Gbps * 1e9) / 8 / 1e9
ls_number = 0
# Adjusted regex pattern to extract output port

os.system("../htsim/sim/datacenter/htsim_uec -failed 42 -exit_freeze 100000000 -save_data_folder ../artifact_results/fig_6_failures_micro/raw_output -sack_threshold 4000 -end 90000 -seed 5 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_3t_400g.topo -linkspeed 400000 -ecn 20 80 -q 100 -cwnd 151 -load_balancing_algo freezing -log_link  -tm ../htsim/sim/datacenter/connection_matrices/test_symm32.cm -collect_data > out.tmp")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/port/portSwitch_LowerPod_0_.txt ../artifact_results/fig_6_failures_micro/data/portSwitch_LowerPod_0_.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US0.txt ../artifact_results/fig_6_failures_micro/data/queueSizeLS0-\>US0.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US1.txt ../artifact_results/fig_6_failures_micro/data/queueSizeLS0-\>US1.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US2.txt ../artifact_results/fig_6_failures_micro/data/queueSizeLS0-\>US2.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US3.txt ../artifact_results/fig_6_failures_micro/data/queueSizeLS0-\>US3.txt")

os.system("../htsim/sim/datacenter/htsim_uec -failed 42 -save_data_folder ../artifact_results/fig_6_failures_micro/raw_output -sack_threshold 4000 -end 90000 -seed 5 -paths 65535 -sender_cc_only -sender_cc_algo mprdma -topo ../htsim/sim/datacenter/topologies/reps/fat_tree_128_1os_3t_400g.topo -linkspeed 400000 -ecn 20 80 -q 100 -cwnd 151 -load_balancing_algo oblivious -log_link -tm ../htsim/sim/datacenter/connection_matrices/test_symm32.cm -collect_data > out.tmp")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/port/portSwitch_LowerPod_0_.txt ../artifact_results/fig_6_failures_micro/data/obsportSwitch_LowerPod_0_.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US0.txt ../artifact_results/fig_6_failures_micro/data/obsqueueSizeLS0-\>US0.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US1.txt ../artifact_results/fig_6_failures_micro/data/obsqueueSizeLS0-\>US1.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US2.txt ../artifact_results/fig_6_failures_micro/data/obsqueueSizeLS0-\>US2.txt")
os.system("cp ../artifact_results/fig_6_failures_micro/raw_output/queueSize/queueSizeLS0-\>US3.txt ../artifact_results/fig_6_failures_micro/data/obsqueueSizeLS0-\>US3.txt")


pattern = r"LS0->US(\d+)"
# Input file path
file_pathreps = '../artifact_results/fig_6_failures_micro/data/portSwitch_LowerPod_{}_.txt'.format(ls_number)
file_pathobs = '../artifact_results/fig_6_failures_micro/data/obsportSwitch_LowerPod_{}_.txt'.format(ls_number)
queue_folder = '../artifact_results/fig_6_failures_micro/data/'


# Read and parse the port utilization data
data = []
with open(file_pathobs, 'r') as file:
    for line in file:
        timestamp_ns_str = line.split(',', 1)[0]
        try:
            timestamp_ns = int(timestamp_ns_str)
        except ValueError:
            continue
        
        port_info_match = re.search(pattern, line)
        if port_info_match:
            output_port = port_info_match.group(1)
            data.append((timestamp_ns, output_port))

# Create a DataFrame for port utilization
df = pd.DataFrame(data, columns=['timestamp_ns', 'output_port'])

# Bin the timestamps into the specified intervals
df['time_bin'] = (df['timestamp_ns'] // TIME_INTERVAL_NS) * TIME_INTERVAL_NS

# Calculate utilization for each output port and time bin
utilization = df.groupby(['time_bin', 'output_port']).size().reset_index(name='packets')
utilization['utilization'] = (utilization['packets'] * PACKET_SIZE_BYTES * 8) / (TIME_INTERVAL_NS ) 

# Generate the complete grid of time bins and output ports

all_time_bins = pd.Series(df['time_bin'].unique(), name='time_bin')

all_output_ports = pd.Series(df['output_port'].unique(), name='output_port')

time_port_grid = pd.merge(all_time_bins, all_output_ports, how='cross')



# Merge with the actual utilization data and fill missing values with zero

utilization = pd.merge(time_port_grid, utilization, on=['time_bin', 'output_port'], how='left')

utilization['packets'] = utilization['packets'].fillna(0)

utilization['utilization'] = utilization['utilization'].fillna(0)

# Find and load relevant queue files
queue_data = {}
for output_port in df['output_port'].unique():
    queue_file = os.path.join(queue_folder, f'obsqueueSizeLS{ls_number}->US{output_port}.txt')
    if os.path.exists(queue_file):
        queue_df = pd.read_csv(queue_file, header=None, names=['timestamp_ns', 'queue_size_bytes'])
        # Convert bytes to kilobytes
        queue_df['queue_size_bytes'] = queue_df['queue_size_bytes'] * LINK_SPEED_Gbps / 8 / 1e3
        queue_data[output_port] = queue_df

# Define custom color palette
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

# Ensure we have enough colors for the number of unique ports
unique_ports = sorted(df['output_port'].unique())
if len(unique_ports) > len(dark2_hex_colors):
    raise ValueError("Number of unique ports exceeds number of available colors")

# Map ports to colors
port_colors = {str(port): dark2_hex_colors[i] for i, port in enumerate(unique_ports)}

# Set font sizes
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Plotting the utilization and queue size
fig = plt.figure(figsize=(7, 5.2))  # Adjusted the height to 7

# Use GridSpec to adjust subplot heights
gs = gridspec.GridSpec(2, 1, height_ratios=[1, 1])  # 2:1 ratio for top and bottom plots

# Plot utilization (top)
ax1 = plt.subplot(gs[0])
for output_port in unique_ports:
    port_utilization = utilization[utilization['output_port'] == output_port]
    sns.lineplot(data=port_utilization, x='time_bin', y='utilization', ax=ax1, color=port_colors[str(output_port)], marker='o')

ax1.set_ylabel('Output Port Utilization (Gbps)')

# Update x-axis to display time in microseconds
ax1.set_xticks(ax1.get_xticks())  # Ensure that the current ticks are used
ax1.set_xticklabels([f'{int(x / 1e3)}' for x in ax1.get_xticks()])

# Create a secondary y-axis for the queue sizes
ax2 = ax1.twinx()
for output_port, q_data in queue_data.items():
    sns.lineplot(data=q_data, x='timestamp_ns', y='queue_size_bytes', ax=ax2, color=port_colors[str(output_port)], alpha=0.7)

ax2.set_ylabel('Queue Size (KB)')

# Set axis limits and grids
ax1.set_ylim(0, 450)
# ax2.set_ylim(0, 650)  # Adjusted y-limit for KB

# Create a combined legend
# Create handles and labels for both utilization and queue sizes
handles = []
labels = []
for port in unique_ports:
    # Create a combined label
    combined_label = f'Queue/Port {port}'
    
    # Add handles for the utilization plot
    line_handle = plt.Line2D([0], [0], color=port_colors[str(port)], lw=2)
    handles.append(line_handle)
    labels.append(combined_label)
    
    # Add handles for the queue size plot
    handles.append(line_handle)
    labels.append(combined_label)

# Remove duplicate labels
unique_labels = list(dict.fromkeys(labels))
unique_handles = [handles[labels.index(label)] for label in unique_labels]

# Apply the legend with combined labels
""" ax1.legend(unique_handles, unique_labels, loc='upper left') """

# Add horizontal dotted lines at specific values
ax2.axhline(20*4096 / 1e3, color='gray', linestyle=':', linewidth=3)  # Convert to KB
ax2.axhline(80*4096 / 1e3, color='gray', linestyle=':', linewidth=3)  # Convert to KB

# Display grid
ax1.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility


# Plot the barplot below the time-series plot
ax3 = plt.subplot(gs[1])
# Calculate overall utilization for each output port (no time buckets)


# Read and parse the port utilization data
data = []
with open(file_pathreps, 'r') as file:
    for line in file:
        timestamp_ns_str = line.split(',', 1)[0]
        try:
            timestamp_ns = int(timestamp_ns_str)
        except ValueError:
            continue
        
        port_info_match = re.search(pattern, line)
        if port_info_match:
            output_port = port_info_match.group(1)
            data.append((timestamp_ns, output_port))

# Create a DataFrame for port utilization
df = pd.DataFrame(data, columns=['timestamp_ns', 'output_port'])

# Bin the timestamps into the specified intervals
df['time_bin'] = (df['timestamp_ns'] // TIME_INTERVAL_NS) * TIME_INTERVAL_NS

# Calculate utilization for each output port and time bin
utilization = df.groupby(['time_bin', 'output_port']).size().reset_index(name='packets')
utilization['utilization'] = (utilization['packets'] * PACKET_SIZE_BYTES * 8) / (TIME_INTERVAL_NS) 

# Generate the complete grid of time bins and output ports

all_time_bins = pd.Series(df['time_bin'].unique(), name='time_bin')

all_output_ports = pd.Series(df['output_port'].unique(), name='output_port')

time_port_grid = pd.merge(all_time_bins, all_output_ports, how='cross')



# Merge with the actual utilization data and fill missing values with zero

utilization = pd.merge(time_port_grid, utilization, on=['time_bin', 'output_port'], how='left')

utilization['packets'] = utilization['packets'].fillna(0)

utilization['utilization'] = utilization['utilization'].fillna(0)
# Find and load relevant queue files
queue_data = {}
for output_port in df['output_port'].unique():
    queue_file = os.path.join(queue_folder, f'queueSizeLS{ls_number}->US{output_port}.txt')
    if os.path.exists(queue_file):
        queue_df = pd.read_csv(queue_file, header=None, names=['timestamp_ns', 'queue_size_bytes'])
        # Convert bytes to kilobytes
        queue_df['queue_size_bytes'] = queue_df['queue_size_bytes'] * LINK_SPEED_Gbps / 8 / 1e3
        queue_data[output_port] = queue_df

# Define custom color palette
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

# Ensure we have enough colors for the number of unique ports
unique_ports = sorted(df['output_port'].unique())
if len(unique_ports) > len(dark2_hex_colors):
    raise ValueError("Number of unique ports exceeds number of available colors")

# Map ports to colors
port_colors = {str(port): dark2_hex_colors[i] for i, port in enumerate(unique_ports)}

# Set font sizes
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Plot utilization (top)
ax3 = plt.subplot(gs[1])
for output_port in unique_ports:
    port_utilization = utilization[utilization['output_port'] == output_port]
    sns.lineplot(data=port_utilization, x='time_bin', y='utilization', ax=ax3, color=port_colors[str(output_port)], marker='o')

ax3.set_xlabel('Time (microseconds)')
ax3.set_ylabel('Output Port Utilization (Gbps)')

# Update x-axis to display time in microseconds
ax3.set_xticks(ax3.get_xticks())  # Ensure that the current ticks are used
ax3.set_xticklabels([f'{int(x / 1e3)}' for x in ax3.get_xticks()])

# Create a secondary y-axis for the queue sizes
ax4 = ax3.twinx()
for output_port, q_data in queue_data.items():
    sns.lineplot(data=q_data, x='timestamp_ns', y='queue_size_bytes', ax=ax4, color=port_colors[str(output_port)], alpha=0.7)

ax4.set_ylabel('Queue Size (KB)')

# Set axis limits and grids
ax3.set_ylim(0, 450)
# ax4.set_ylim(0, 650)  # Adjusted y-limit for KB

# Create a combined legend
# Create handles and labels for both utilization and queue sizes
handles = []
labels = []
for port in unique_ports:
    # Create a combined label
    combined_label = f'Queue/Port {port}'
    
    # Add handles for the utilization plot
    line_handle = plt.Line2D([0], [0], color=port_colors[str(port)], lw=2)
    handles.append(line_handle)
    labels.append(combined_label)
    
    # Add handles for the queue size plot
    handles.append(line_handle)
    labels.append(combined_label)

# Remove duplicate labels
unique_labels = list(dict.fromkeys(labels))
unique_handles = [handles[labels.index(label)] for label in unique_labels]

# Apply the legend with combined labels
""" ax3.legend(unique_handles, unique_labels, loc='upper left')
 """
# Add horizontal dotted lines at specific values
ax4.axhline(20*4096 / 1e3, color='gray', linestyle=':', linewidth=3)  # Convert to KB
ax4.axhline(80*4096 / 1e3, color='gray', linestyle=':', linewidth=3)  # Convert to KB

ax1.set_zorder(ax2.get_zorder() + 1)  # ax1 on top
ax1.patch.set_visible(False)  # Make ax1 transparent to ensure ax2's features are visible

ax3.set_zorder(ax4.get_zorder() + 1)  # ax1 on top
ax3.patch.set_visible(False)  # Make ax1 transparent to ensure ax2's features are visible

ax1.set_xlabel('')
ax2.set_ylabel('')  # Remove the label, will set it centrally later
ax4.set_ylabel('')  # Remove the label, will set it centrally later
ax3.set_ylabel('')  # Remove the label, will set it centrally later
ax1.set_ylabel('')  # Remove the label, will set it centrally later


fig.text(0.99, 0.5, 'Queue Size (KB)', ha='center', va='center', rotation='vertical', fontsize=14)
fig.text(0, 0.5, 'Output Port Utilization (Gbps)', ha='center', va='center', rotation='vertical', fontsize=14)

ax1.set_title('Oblivious Packet Spraying')  # For the upper plot
ax3.set_title('REPS')  # For the lower plot

# Display grid
ax3.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility


plt.tight_layout()  # Adjust layout to prevent overlap
plt.savefig("../artifact_results/fig_6_failures_micro/plots/failures_micro.png", bbox_inches='tight')
plt.savefig("../artifact_results/fig_6_failures_micro/plots/failures_micro.pdf", bbox_inches='tight')
