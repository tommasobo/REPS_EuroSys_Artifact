import re
import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib import gridspec

# Constants
LINK_SPEED_Gbps = 400  # Link speed in Gbps
PACKET_SIZE_BYTES = 4096  # Packet size in bytes
TIME_INTERVAL_NS = 10000  # Time interval in nanoseconds

# Convert Gbps to Bytes per nanosecond
LINK_CAPACITY_BYTES_PER_NS = (LINK_SPEED_Gbps * 1e9) / 8 / 1e9
ls_number = 0
# Adjusted regex pattern to extract output port

pattern = r"LS0->US(\d+)"
# Input file path
file_path = '../sim/datacenter/port/portSwitch_LowerPod_{}_.txt'.format(ls_number)
queue_folder = '../sim/datacenter/queueSize/'

# Read and parse the port utilization data
data = []
with open(file_path, 'r') as file:
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

# Plotting the utilization and queue size
fig = plt.figure(figsize=(10, 7))  # Adjusted the height to 7

# Use GridSpec to adjust subplot heights
gs = gridspec.GridSpec(2, 1, height_ratios=[2, 1])  # 2:1 ratio for top and bottom plots

# Plot utilization (top)
ax1 = plt.subplot(gs[0])
for output_port in unique_ports:
    port_utilization = utilization[utilization['output_port'] == output_port]
    sns.lineplot(data=port_utilization, x='time_bin', y='utilization', ax=ax1, color=port_colors[str(output_port)], marker='o')

ax1.set_xlabel('Time (microseconds)')
ax1.set_ylabel('Output port arrival rate compared to ideal (%)')

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
ax1.legend(unique_handles, unique_labels, loc='upper left')

# Add horizontal dotted lines at specific values
ax2.axhline(31*4096 / 1e3, color='gray', linestyle=':', linewidth=2.5)  # Convert to KB
ax2.axhline(120*4096 / 1e3, color='gray', linestyle=':', linewidth=2.5)  # Convert to KB

# Display grid
ax1.grid(True)

plt.title('Output port arrival rate and Queue Size per Output Port')

# Plot the barplot below the time-series plot
ax3 = plt.subplot(gs[1])
# Calculate overall utilization for each output port (no time buckets)
port_counts = df['output_port'].value_counts().reset_index()
port_counts.columns = ['output_port', 'usage_count']

# Convert output_port column to numeric for sorting
port_counts['output_port'] = pd.to_numeric(port_counts['output_port'])

# Sort the barplot x-axis by port number
port_counts = port_counts.sort_values(by='output_port')

# Plot the overall port utilization as a barplot
sns.barplot(data=port_counts, x='output_port', y='usage_count', palette=[port_colors[str(port)] for port in port_counts['output_port']])

plt.title('Overall Port Utilization')
plt.xlabel('Output Port')
plt.ylabel('Usage Count')
plt.grid(True)

plt.tight_layout()  # Adjust layout to prevent overlap
plt.savefig("combined_plot2_obs.png", bbox_inches='tight')
plt.savefig("combined_plot2_obs.pdf", bbox_inches='tight')
plt.show()
