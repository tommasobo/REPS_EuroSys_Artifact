import os
import re
import numpy as np
import matplotlib.pyplot as plt

# Path to the folder containing the files
folder_path = 'dcHuan'

# Updated regular expression patterns to extract FCT and flowSize
fct_pattern = re.compile(r'finished at (\d+\.?\d*)\s+flowSize')
flow_size_pattern = re.compile(r'flowSize (\d+)')

# Dictionary to hold FCTs for each file
file_fcts = {}

# Iterate over files in the directory
for filename in os.listdir(folder_path):
    file_path = os.path.join(folder_path, filename)
    
    fcts = []
    flow_sizes = []

    if ("100" not in file_path):
        continue
    # Read the file
    with open(file_path, 'r') as file:
        content = file.read()
        
        # Find all FCTs and flow sizes in the file
        fcts = fct_pattern.findall(content)
        flow_sizes = flow_size_pattern.findall(content)

        # Convert FCTs and flow sizes
        fcts = list(map(float, fcts))
        flow_sizes = list(map(int, flow_sizes))
        filename = filename.split('_')[0]

        # Store FCTs in dictionary (no size filtering)
        print(f"Analyzing file: {file_path} - Num Len {len(fcts)}")
        if fcts:
            file_fcts[filename] = fcts

# Function to compute ECDF
def ecdf(data):
    sorted_data = np.sort(data)
    y = np.arange(1, len(sorted_data) + 1) / len(sorted_data)
    return sorted_data, y

# Plot setup for ECDFs and 99th percentile CDFs
fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.6))  # Smaller size for column width

# Increase font sizes for better readability
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

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

# Store legend elements for combined legend later
handles = []
labels = []

# Plot ECDFs on the left subplot
for i, (filename, fcts) in enumerate(file_fcts.items()):
    x, y = ecdf(fcts)
    line, = axs[0].step(x, y, where='post', label=filename, 
                        color=dark2_hex_colors[i % len(dark2_hex_colors)], linewidth=2.5)
    handles.append(line)
    labels.append(filename)

axs[0].set_xlabel('FCT (s)')
axs[0].set_ylabel('ECDF')
axs[0].set_title('CDF')
axs[0].grid(True)

# Plot CDFs for the worst 1% FCTs on the right subplot, but no legend here
for i, (filename, fcts) in enumerate(file_fcts.items()):
    percentile_99th = np.percentile(fcts, 99)
    # Filter data to consider only values above the 99th percentile
    fcts_above_99th = [fct for fct in fcts if fct >= percentile_99th]
    x, y = ecdf(fcts_above_99th)
    axs[1].step(x, y, where='post', 
                label=f"{filename} (99th+ %)", 
                color=dark2_hex_colors[i % len(dark2_hex_colors)], 
                linewidth=2.5)

axs[1].set_xlabel('FCT (s)')
axs[1].set_ylabel('CDF 99th+ Percentile')
axs[1].set_title('CDF (99th+ Percentile)')
axs[1].grid(True)

# Adjust layout to avoid clipping
plt.tight_layout(rect=[0, 0, 1, 0.9])  # Adjust layout to make room for the legend

# Move the legend to the bottom center of the whole figure
fig.legend(handles, labels, loc='lower center', ncol=4, fontsize=12, bbox_to_anchor=(0.55, 0.2))

# Save the plot with higher DPI for print quality
plt.savefig("result_folder/fct_ecdf_99th_cdf_single_legend_bottom_center.png", bbox_inches='tight', dpi=300)
plt.savefig("result_folder/fct_ecdf_99th_cdf_single_legend_bottom_center.pdf", bbox_inches='tight', dpi=300)

plt.show()
