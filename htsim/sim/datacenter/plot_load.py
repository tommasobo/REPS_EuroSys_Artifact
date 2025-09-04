import os
import re
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse

# Define a static palette for colors
static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'OPS': '#d95f02',  # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7298a',  # Pink
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
    'Adaptive RoCE': 'h'   # Gray
}

# Define the order of load balancers for the plot
lb_name_order = ["ECMP", "OPS", "Flowlet", "BitMap", "MPRDMA", "PLB", "MPTCP", "Adaptive RoCE", "REPS"]

# Function to convert the load balancer names
def update_name(name_lb):
    if name_lb == "ecmp":
        return "ECMP"
    elif name_lb == "oblivious":
        return "OPS"
    elif name_lb == "bitmap":
        return "BitMap"
    elif name_lb == "mp":
        return "MPTCP"
    elif name_lb == "plb":
        return "PLB"
    elif name_lb == "freezing" or name_lb == "reps_mapping":
        return "REPS"
    elif name_lb == "mprdma":
        return "MPRDMA"
    elif name_lb == "flowlet":
        return "Flowlet"
    elif name_lb == "nvidia":
        return "Adaptive RoCE"
    else:
        return name_lb  # Default to original name if no match

# Regex to extract FCT from the output files
fct_pattern = re.compile(r'finished at (\d+\.?\d*)\s+flowSize')

# Directory containing the output files
parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
parser.add_argument('--name', required=False, help='Name for the experiment', default="")
parser.add_argument('--save_folder', required=True, help='Save Folder for the experiment', default="")
parser.add_argument('--dont_show', action='store_true', help='Not save fig')
parser.add_argument('--filename',    default=None,
                    help='optional filename to pass to plot_symmetric')
args = parser.parse_args()
output_dir = args.input_folder_exp

# Load levels
load_levels = ["40", "60", "80", "100"]  # Representing the 40%, 60%, 80%, 100% loads

# Function to extract average and 99th percentile FCT from a file
def extract_fct_metrics(filepath):
    with open(filepath, 'r') as file:
        data = file.read()
        fct_values = [float(match.group(1)) for match in fct_pattern.finditer(data)]
        if fct_values:
            avg_fct = sum(fct_values) / len(fct_values)
            # Calculate the 99th percentile
            percentile_99th = np.percentile(fct_values, 99)
            return avg_fct, percentile_99th
        return None, None

# Lists to store the extracted data
avg_fct_data = []
percentile_99_fct_data = []

# Iterate over load balancers and load levels to read the corresponding files
for lb in ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "mp", "flowlet", "nvidia"]:
    for load in load_levels:
        # Construct the file path (e.g., dcUpdated/oblivious_40load)
        file_path = os.path.join(output_dir, f"{lb}_{load}load")
        if os.path.exists(file_path):
            avg_fct, pct_99_fct = extract_fct_metrics(file_path)
            if avg_fct is not None and pct_99_fct is not None:
                # Use the updated name for the load balancer
                lb_name = update_name(lb)
                # Append the data to the lists
                avg_fct_data.append({"LoadBalancer": lb_name, "LoadLevel": int(load), "AvgFCT": avg_fct})
                percentile_99_fct_data.append({"LoadBalancer": lb_name, "LoadLevel": int(load), "Percentile99FCT": pct_99_fct})

# Convert the lists to Pandas DataFrames
df_avg_fct = pd.DataFrame(avg_fct_data)
df_percentile_99_fct = pd.DataFrame(percentile_99_fct_data)

# Set the style and context for better readability in smaller figures

# Adjust the figure size for a single plot
fig, ax = plt.subplots(1, 1, figsize=(6, 3.65))

# Update font sizes and line widths for better readability
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Define unique markers for each load balancer
#markers = ["o", "s", "D", "^", "v", "p", "*", "X"]
#marker_map = {lb: markers[i] for i, lb in enumerate(lb_name_order)}

# Plot for Average FCT with unique markers and custom color palette
sns.lineplot(ax=ax, data=df_avg_fct, x="LoadLevel", y="AvgFCT", hue="LoadBalancer", 
             hue_order=lb_name_order,  # Ensure order of lines
             style="LoadBalancer", markers=lb_markers, dashes=False, 
             linewidth=3, markersize=16, palette=static_color_mapping, alpha=0.86)

""" sns.lineplot(ax=ax, data=df_percentile_99_fct, x="LoadLevel", y="Percentile99FCT", hue="LoadBalancer", 
             hue_order=lb_name_order,  # Ensure order of lines
             style="LoadBalancer", markers=lb_markers, dashes=False, 
             linewidth=3, markersize=14, palette=static_color_mapping, alpha=0.86) """

ax.set_xlabel("Load Level (%)", fontsize=18)
ax.set_ylabel("Average FCT (μs)", fontsize=18)
ax.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility


# Adjust the layout to prevent overlap
plt.tight_layout()

plt.ylim(0,420)

plt.xticks(fontsize=16)  # Increased x-axis tick font size
plt.yticks(fontsize=16)  # Increased y-axis tick font size
plt.grid(True)

# Get handles and labels from the plot for the legend
handles, labels = ax.get_legend_handles_labels()
ax.get_legend().remove()  # Remove the individual legend

# Create a single legend at the bottom
fig.legend(handles, labels, loc='lower center', ncol=8, fontsize=12, bbox_to_anchor=(0.54, 1.82), frameon=False)
fig.legend([],[], frameon=False)

output_name = "dc_traces"

if (args.filename is not None):
    output_name = args.filename

os.system(f"mkdir -p {args.save_folder}")

plt.savefig(f"{args.save_folder}/{output_name}.png", bbox_inches='tight')
plt.savefig(f"{args.save_folder}/{output_name}.pdf", bbox_inches='tight')

# Show the plot
if (args.dont_show == False):
    plt.show()
