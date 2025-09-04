import os
import re
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Define a static palette for colors
static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'OPS': '#d95f02',  # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7298a',  # Pink
    'PLB': '#66a61e',  # Green
    'REPS': '#e6ab02',  # Yellow
    'MPRDMA': '#a6761d',  # Brown
    'Flowlet': '#666666'   # Gray
}

# Define the order of load balancers for the plot
lb_name_order = ["ECMP", "OPS", "Flowlet", "BitMap", "MPRDMA", "PLB", "MPTCP", "REPS"]

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
    else:
        return name_lb  # Default to original name if no match

# Regex to extract FCT from the output files
fct_pattern = re.compile(r'finished at (\d+\.?\d*)\s+flowSize')

# Directory containing the output files
output_dir = "dc_symmetric_noos"

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
for lb in ["oblivious", "ecmp", "plb", "mprdma", "bitmap", "freezing", "mp", "flowlet"]:
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

# Adjust the figure size to fit a column (7.2 inches wide)
fig, axes = plt.subplots(1, 2, figsize=(6.2, 3.2))

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
markers = ["o", "s", "D", "^", "v", "p", "*", "X"]
marker_map = {lb: markers[i] for i, lb in enumerate(lb_name_order)}

# Plot for Average FCT with unique markers and custom color palette
sns.lineplot(ax=axes[0], data=df_avg_fct, x="LoadLevel", y="AvgFCT", hue="LoadBalancer", 
             hue_order=lb_name_order,  # Ensure order of lines
             style="LoadBalancer", markers=marker_map, dashes=False, 
             linewidth=2.5, markersize=8, palette=static_color_mapping)

axes[0].set_xlabel("Load Level (%)")
axes[0].set_ylabel("Average FCT (us)")
axes[0].grid(True)

# Plot for 99th Percentile FCT with unique markers and custom color palette
sns.lineplot(ax=axes[1], data=df_percentile_99_fct, x="LoadLevel", y="Percentile99FCT", hue="LoadBalancer", 
             hue_order=lb_name_order,  # Ensure order of lines
             style="LoadBalancer", markers=marker_map, dashes=False, 
             linewidth=2.5, markersize=8, palette=static_color_mapping, alpha=0.8)

axes[1].set_xlabel("Load Level (%)")
axes[1].set_ylabel("99th Percentile FCT (us)")
axes[1].grid(True)

# Adjust the layout to prevent overlap
plt.tight_layout(rect=[0, 0, 1, 0.9])

plt.grid(True)

# Get handles and labels from the first plot for the combined legend
handles, labels = axes[0].get_legend_handles_labels()
axes[0].get_legend().remove()  # Remove the individual legend from the first plot
axes[1].get_legend().remove()  # Remove the individual legend from the second plot

# Create a single legend between the two plots at the bottom
fig.legend(handles, labels, loc='lower center', ncol=4, fontsize=12, bbox_to_anchor=(0.54, 0.82), frameon=False)

# Save the plot with high DPI for print quality
plt.savefig("result_folder/symm_dc_fct.png", bbox_inches='tight', dpi=300)
plt.savefig("result_folder/symm_dc_fct.pdf", bbox_inches='tight', dpi=300)

plt.show()
