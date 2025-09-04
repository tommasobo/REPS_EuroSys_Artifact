import os
import re
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
from matplotlib.lines import Line2D
import argparse

parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
args = parser.parse_args()

# Initialize the main folder path
main_folder = "{}/".format(args.input_folder_exp)

def get_fancy_name_lb(name):
    if name == "reps":
        return "REPS"
    elif name == "ecmp":
        return "ECMP"
    elif name == "mp":
        return "MPTCP"
    elif name == "plb":
        return "PLB"
    elif name == "oblivious":
        return "RPS"
    elif name == "bitmap":
        return "BitMap"
    elif name == "mprdma":
        return "MPRDMA"
    elif name == "flowlet":
        return "Flowlet"
    else:
        return name

# Function to get FCTs, now only including bg_traffic = 0
def get_list_fct(name_file_to_use):
    """
    Extracts the finished-at runtime values from the file and filters out background traffic (bg_traffic = 1).
    """
    temp_list = []
    print("MATCHING")
    try:
        with open(name_file_to_use) as file:
             for line in file:
                # Regex pattern to capture finished at time and bg traffic value
                pattern = r"finished at (\d+\.\d+).*bg traffic (\d+)"
                pattern = r"finished at (\d+)"
                match = re.search(pattern, line)
                
                if match:
                    actual_fct = float(match.group(1))
                    temp_list.append(actual_fct)
                    """ bg_traffic = int(match.group(2))
                    if bg_traffic == 1:  # Only include non-background traffic
                        temp_list.append(actual_fct) """
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    return temp_list

# Data collection list
data = []

folder_path = Path(args.input_folder_exp)

print(folder_path)

# Iterate through each subfolder in the specified folder
for file in folder_path.iterdir():
    if file.is_file() and file.suffix == '.txt':
        print(f"  - {file.name}")
        full_path_file = str(folder_path / file.name)

        # r"out_(workload\w+)_(cc\w+)_drop(\w+)_os(\d+)_size(\d+)_lb(\w+)_entropy(\d+)_bg(\w+)\.txt",

        match = re.search(
            r"out_(workload\w+)_(cc\w+)_drop(\w+)_os(\d+)_size(\d+)_lb(\w+)_entropy(\d+)_bg(\w+)\.txt", 
            os.path.basename(full_path_file)
        )
        if match:
            workload = match.group(1)  # Load balancer type
            lb_type = match.group(6).strip()  # Load balancer type
            lb_type = get_fancy_name_lb(lb_type)
            entropy_value = int(match.group(7).strip())  # Entropy value
            bg_mode = match.group(8)  # Load balancer type
            """ bg_mode = "" """
            if bg_mode == "on":
                continue

            """ if lb_type == "ECMP" and entropy_value == 65535:
                continue """
            
            if entropy_value != 65535:
                continue

            if lb_type == "mp":
                lb_type = "MP-Swift"

            cc_name = match.group(2).replace("cc", "", 1).strip()
            if cc_name == "both":
                cc_name = "nscc+rccc"
            drop_strategy = match.group(3).strip()
            os_ratio = int(match.group(4).strip())
            message_size = int(match.group(5).strip()) / 1024  # Convert to KiB
            
            # Map drop strategy to a more readable form
            method = "Timeout" if drop_strategy == "timeout" else "Trimming" if drop_strategy == "trimming" else None
            
            if method is None:
                continue
            
            print("MATCHING {}".format(full_path_file))
            # Get FCT list and find max FCT, with background traffic filtered out
            fct_list = get_list_fct(full_path_file)
            if fct_list:
                max_fct = max(fct_list)
                print("appending2 {} max {}".format(full_path_file, max_fct))
                

                # Append to data
                data.append({
                    'Max FCT': max_fct, 
                    'CC Algorithm': cc_name,  
                    'Workload': "{}_{}_{}".format(workload, message_size, bg_mode),
                    'Drop Detection': method,
                    'LB Type': f"{lb_type} {entropy_value}",
                    'LB Name': lb_type,  # New column for just LB Type name
                    'Entropy': entropy_value
                })

# Convert data to a DataFrame
df = pd.DataFrame(data)
print(df)

# Ensure no leading/trailing spaces
df['Workload'] = df['Workload'].str.strip()

# Group by 'Workload' and find the 'oblivious' baseline
baseline = df[df['LB Name'].str.contains('ECMP', case=False)].set_index('Workload')['Max FCT']
print(baseline)

# Check if all Workloads are in the baseline
missing_modes = [mode for mode in df['Workload'].unique() if mode not in baseline.index]
if missing_modes:
    print("Missing Workloads in baseline:", missing_modes)

# Calculate speedup with error handling for missing baseline values
df['Speedup'] = df.apply(lambda row: baseline.get(row['Workload'], np.nan) / row['Max FCT'] if row['Workload'] in baseline.index else np.nan, axis=1)


# Define a static palette for colors
static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'RPS': '#d95f02',  # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7298a',  # Pink
    'PLB': '#66a61e',  # Green
    'REPS': '#e6ab02',  # Yellow
    'MPRDMA': '#a6761d',  # Brown
    'Flowlet': '#666666'   # Gray
}

lb_markers = {
    'ECMP': 'o',      # Circle
    'RPS': 's',       # Square
    'BitMap': 'v',    # Triangle down
    'MPTCP': '^',     # Triangle up
    'PLB': 'P',       # Plus
    'REPS': 'X',      # X
    'MPRDMA': 'D',    # Diamond
    'Flowlet': '*',   # Star
}

# Extract unique LB Names and create a palette using the static mapping
unique_lb_names = df['LB Name'].unique()
palette = {lb_name: static_color_mapping.get(lb_name, '#666666') for lb_name in unique_lb_names}  # Default to gray if not found

# Define markers and sizes for Entropy levels
#markers = {256: 'o', 65535: 'D'}  # Circle for 256, diamond for 65535
sizes = {256: 140, 65535: 370}    # Marker sizes: 140 for 256, 370 for 65535

# Define a function to rename y-axis labels based on internal logic
def rename_y_axis_labels(label):
    """
    Custom function to rename y-axis labels based on internal logic.
    Modify this function as needed.
    """
    if "workloadincast32_4096.0" in label:
        return "Incast 32:1 4MiB"
    elif "workloadincast32_8192.0" in label:
        return "Incast 32:1 8MiB"
    elif "workloadincast32_16384.0" in label:
        return "Incast 32:1 16MiB"
    elif "workloadincast8_4096.0" in label:
        return "Incast 8:1 4MiB"
    elif "workloadincast8_8192.0" in label:
        return "Incast 8:1 8MiB"
    elif "workloadincast8_16384.0" in label:
        return "Incast 8:1 16MiB"
    elif "workloadperm_8192.0_off" in label:
        return "Perm. 8MiB"
    elif "workloadperm_4096.0_off" in label:
        return "Perm. 4MiB"
    elif "workloadperm_16384.0_off" in label:
        return "Perm. 16MiB"
    elif "workloadtornado_4096.0_off" in label:
        return "Tornado 4MiB"
    elif "workloadtornado_8192.0_off" in label:
        return "Tornado 8MiB"
    elif "workloadperm_8192.0" in label:
        return "Perm. (BG) 8MiB"
    elif "workloadperm_4096.0_on" in label:
        return "Perm. (BG) 4MiB"
    elif "workloadtornado_4096.0_on" in label:
        return "Tornado (BG) 4MiB"
    elif "workloadtornado_8192.0_on" in label:
        return "Tornado (BG) 8MiB"
    elif "workloadtornado_16384.0_off" in label:
        return "Tornado 16MiB"
    elif "workloadincast32_4096.0" in label:
        return "Incast 32:1 4MiB"
    elif "workloadincast32_8192.0" in label:
        return "Incast 32:1 8MiB"
    elif "workloadincast32_16384.0" in label:
        return "Incast 32:1 16MiB"
    elif "workloadincast8_4096.0" in label:
        return "Incast 8:1 4MiB"
    elif "workloadincast8_8192.0" in label:
        return "Incast 8:1 8MiB"
    elif "workloadincast8_16384.0" in label:
        return "Incast 8:1 16MiB"
    elif "workloadperm_8192.0" in label:
        return "Perm. 8MiB"
    elif "workloadperm_4096.0" in label:
        return "Perm. 4MiB"
    elif "workloadperm_16384.0" in label:
        return "Perm. 16MiB"
    elif "workloadtornado_4096.0" in label:
        return "Tornado 4MiB"
    elif "workloadtornado_8192.0" in label:
        return "Tornado 8MiB"
    elif "workloadperm_8192.0" in label:
        return "Perm. (BG) 8MiB"
    elif "workloadperm_4096.0" in label:
        return "Perm. (BG) 4MiB"
    elif "workloadtornado_4096.0" in label:
        return "Tornado (BG) 4MiB"
    elif "workloadtornado_8192.0" in label:
        return "Tornado (BG) 8MiB"
    elif "workloadtornado_16384.0" in label:
        return "Tornado 16MiB"
    else:
        return label

# Define the order for the y-axis
order = ["I. 8:1 4MiB", "I. 8:1 8MiB", "I. 32:1 4MiB", "I. 32:1 8MiB", 
         "P. 4MiB", "P. 8MiB", "P. (BG) 4MiB", "P. (BG) 8MiB", "T. 4MiB", "T. 8MiB", "T. (BG) 4MiB", "T. (BG) 8MiB"]
# Symmetrir cun
order = ["I. 8:1 4MiB", "I. 8:1 8MiB", "I. 8:1 16MiB", "I. 32:1 4MiB", "I. 32:1 8MiB", "I. 32:1 16MiB",
         "P. 4MiB", "P. 8MiB", "P. 16MiB", "T. 4MiB", "T. 8MiB", "T. 16MiB"]
order = ["I. 32:1 4MiB", "I. 32:1 8MiB", "I. 32:1 16MiB",
         "P. 4MiB", "P. 8MiB", "P. 16MiB", "T. 4MiB", "T. 8MiB", "T. 16MiB"]
# Map workload names to the order list
df['Workload'] = pd.Categorical(df['Workload'].apply(rename_y_axis_labels), categories=order, ordered=True)

# Create the plot
plt.figure(figsize=(5.5, 4.5))  # Adjusted figure size for better readability
print(df)
sizes = {256: 140, 65535: 400}    # Marker sizes: 140 for 256, 370 for 65535

# Scatterplot with color for LB Name and shape for Entropy
sns.scatterplot(
    data=df, 
    x='Speedup', 
    y='Workload', 
    hue='LB Name', 
    style='LB Name', 
    palette=palette, 
    markers=lb_markers, 
    alpha=0.7, 
    size='Entropy', 
    sizes=sizes)

# Customizing the plot
#plt.title('Speedup Compared to ECMP Baseline', fontsize=22)  # Increased title font size
plt.xlabel('Speedup vs ECMP', fontsize=18)  # Increased x-axis label font size
plt.ylabel('Workload', fontsize=18)  # Increased y-axis label font size
plt.axvline(x=1, color='grey', linestyle='--', linewidth=1.5)  # Increased line width

# Apply renaming function to y-axis labels
current_labels = plt.gca().get_yticks()
new_labels = order  # Use the order list for labels
plt.gca().set_yticks([order.index(label) for label in df['Workload'].cat.categories])
plt.gca().set_yticklabels(new_labels, fontsize=16)  # Increased y-axis tick font size

# Create custom legend handles for LB Name (color only)
lb_name_order = ["ECMP", "RPS", "Flowlet", "BitMap", "MPRDMA", "PLB", "MPTCP", "REPS"]
lb_name_handles = [Line2D([0], [0], marker=lb_markers[label], color='w', label=label, 
                         markersize=18, markerfacecolor=static_color_mapping.get(label, '#666666'), alpha=0.7) 
                   for label in lb_markers]

# Create legend handles for Entropy markers (symbols only)
entropy_handles = [
    Line2D([0], [0], marker='o', color='w', label='256', 
            markersize=15, markerfacecolor='black', alpha=0.7),
    Line2D([0], [0], marker='D', color='w', label='65535', 
            markersize=19.5, markerfacecolor='black', alpha=0.7)
]

# Manually add the legend for LB Name and Entropy Size
# Add the LB Name legend
lb_name_legend = plt.legend(
    handles=lb_name_handles,
    bbox_to_anchor=(-0.385, 1.22),  # Adjusted positioning for the LB Name legend
    loc='upper left',
    fontsize=17,  # Increased legend font size
    title_fontsize='15',  # Increased legend title font size
    ncol=4,  # Split legend into 2 columns
    markerscale=1,
    frameon=False,
    handletextpad=1.5
)

# Add the Entropy Size legend
""" entropy_legend = plt.legend(
    handles=entropy_handles,
    title='EVG Size',
    bbox_to_anchor=(0.725, 1.01),  # Adjusted positioning for the Entropy Size legend
    loc='upper left',
    fontsize=17,  # Increased legend font size
    title_fontsize='15',  # Increased legend title font size
    ncol=1,  # Split legend into 2 columns
    markerscale=1,
    handletextpad=1.5
) """

# Add both legends to the plot
plt.gca().add_artist(lb_name_legend)
""" plt.gca().add_artist(entropy_legend)
 """
plt.grid(True)

# Adjust font sizes for ticks
plt.xticks(fontsize=16)  # Increased x-axis tick font size
plt.yticks(fontsize=16)  # Increased y-axis tick font size


""" plt.ylabel('')  # Empty string removes the y-axis title
plt.gca().set_yticklabels([])  # Set the y-axis labels to an empty list
plt.gca().get_legend().remove() """


plt.savefig("reps/asy8.png", bbox_inches='tight')
plt.savefig("reps/asy8.pdf", bbox_inches='tight')

# Show the plot
plt.show()
