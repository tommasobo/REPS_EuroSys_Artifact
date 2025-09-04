import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os
import re

# Configuration: set SRC range, folder path, and time bucket size in microseconds
src_min = 0
src_max = 7
folder_path = '../sim/datacenter/pipe/'
time_bucket_size_us = 10  # Time bucket size in microseconds (configurable)

# Assume each packet is a fixed size in bits (you can adjust this as needed)
packet_size_bits = 4096 * 8  # 4096 bytes per packet

# Function to extract SRC from filename
def extract_src(filename):
    match = re.search(r'SRC(\d+)', filename)
    if match:
        return int(match.group(1))
    return None

# Process each file
def process_file(file_path):
    # Read the file, assuming CSV format with no header
    df = pd.read_csv(file_path, header=None, names=['time_ns', 'received', 'id'])
    
    # Convert nanoseconds to microseconds
    df['time_us'] = df['time_ns'] / 1e3
    
    # Define the time buckets based on the configurable time bucket size
    time_buckets = np.arange(0, df['time_us'].max() + time_bucket_size_us, time_bucket_size_us)
    
    # Calculate packet counts for each bucket
    df['time_bucket'] = pd.cut(df['time_us'], bins=time_buckets, labels=time_buckets[:-1])
    
    # Group by time bucket to count packets received in each interval
    utilization = df.groupby('time_bucket').size()
    
    # Convert packet counts to Gbps: utilization * packet_size_bits / time interval (in seconds)
    utilization_gbps = (utilization * packet_size_bits) / 1e9 / (time_bucket_size_us / 1e6)  # Convert microseconds to seconds
    
    return utilization_gbps

# Collect utilization data for all files that match the SRC range
utilizations = {}
all_utilizations_df = pd.DataFrame()

# List of marker styles for differentiation between lines
markers = ['o', 'v', '^', '<', '>', 's', 'p', '*', 'D', 'X', 'h']

for i, filename in enumerate(os.listdir(folder_path)):
    file_path = os.path.join(folder_path, filename)
    src = extract_src(filename)
    if src is not None and src_min <= src <= src_max:
        utilization = process_file(file_path)
        marker_style = markers[i % len(markers)]  # Cycle through the marker styles
        utilizations[filename] = (utilization, marker_style)
        
        # Combine each utilization into a single DataFrame for calculating the average
        all_utilizations_df[filename] = utilization

# Calculate the average utilization across all lines
average_utilization = all_utilizations_df.mean(axis=1)

# Plot the utilization for each file (link) in Gbps
plt.figure(figsize=(10, 6))
for filename, (utilization, marker_style) in utilizations.items():
    plt.plot(utilization.index, utilization.values, label=filename, linewidth=2, marker=marker_style)  # Add markers

# Plot the average utilization line
plt.plot(average_utilization.index, average_utilization.values, label='Average Utilization', linewidth=3, linestyle='--', color='black')

plt.xlabel(f'Time (us, bucket size = {time_bucket_size_us} us)')  # X-axis in microseconds, showing the bucket size
plt.ylabel('Utilization (Gbps)')
plt.title('Link Utilization Over Time (SRC 0-7)')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("utilsave5.png", bbox_inches='tight')
plt.savefig("utilsave5.pdf", bbox_inches='tight')
# Show the plot
plt.show()
