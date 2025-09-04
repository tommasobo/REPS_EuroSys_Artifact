import os
import re
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import argparse
from matplotlib.lines import Line2D

parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
parser.add_argument('--name', required=False, help='Name for the experiment', default="")
parser.add_argument('--dont_show', action='store_true', help='Not save fig')
parser.add_argument('--save_folder', required=True, help='Save Folder for the experiment', default="")
parser.add_argument('--text', required=True, help='Name Title', default="")
parser.add_argument('--disable_legends', action='store_true', help='Disable all legends in the plot')  # Add option to disable legends
parser.add_argument('--filename',    default=None,
                    help='optional filename to pass to plot_symmetric')
args = parser.parse_args()

# Initialize the main folder path
main_folder = "{}/".format(args.input_folder_exp)

# Function to extract the last flow's finishing time
def get_last_flow_time(log_content):
    # Use regex to find all the flow finishing times
    matches = re.findall(r"sim time (\d+)", log_content)
    if matches:
        # Convert the list of strings to integers
        number_list_int = list(map(int, matches))

        # Get the highest number
        highest_number = max(number_list_int)
        return int(highest_number)  # Return the last finishing time
    return None

# Function to get fancy names for LB types
def get_fancy_name_lb(name):
    if name == "reps" or name == "freezing":
        return "REPS"
    elif name == "ecmp":
        return "ECMP"
    elif name == "mp":
        return "MPTCP"
    elif name == "plb":
        return "PLB"
    elif name == "oblivious":
        return "OPS"
    elif name == "bitmap":
        return "BitMap"
    elif name == "mprdma":
        return "MPRDMA"
    elif name == "flowlet":
        return "Flowlet"
    else:
        return name
    

def get_fancy_name_wl(name):
    if name == "bercableonepercent":
        return "BER Cable 1%"
    elif name == "berswitchonepercent":
        return "BER Switch 1%"
    elif name == "5percentfailedswitches":
        return "5% Failed\nSwitches"
    elif name == "5percentfailedcables":
        return "5% Failed\nCables"
    elif name == "failoneswitchonecable":
        return "One Failed\nSwitch/Cable"
    elif name == "5percentfailedswitchesandcables":
        return "5% Failed\nSwitches/Cables"
    elif name == "failonecable":
        return "One Failed\nCable"
    elif name == "failoneswitch":
        return "One Failed\nSwitch"
    else:
        return name

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

# Data collection list
data = []

folder_path = Path(args.input_folder_exp)

# Static color palette for LB names
static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'OPS': '#d95f02',   # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7298a',  # Pink
    'PLB': '#66a61e',   # Green
    'REPS': '#e6ab02',  # Yellow
    'MPRDMA': '#a6761d',  # Brown
    'Flowlet': '#666666'   # Gray
}

# Iterate through each subfolder in the specified folder
for subfolder in folder_path.iterdir():
    if subfolder.is_dir():
        #print(f"Subfolder: {subfolder.name}")
        if ( "degrade" in subfolder.name):
            continue
        for file in subfolder.iterdir():
            if file.is_file() and file.suffix == '.txt':
                failure_case = str(subfolder).split('_')[-1].strip()
                #print(failure_case)
                failure_case = get_fancy_name_wl(failure_case)
                
                full_path_file = str(folder_path / subfolder.name / file.name)
                match = re.search(
                    r"out_(cc\w+)_drop(\w+)_os(\d+)_size(\d+)_lb(\w+)_entropy(\d+)\.txt", 
                    os.path.basename(full_path_file)
                )
                if match:
                    lb_type = get_fancy_name_lb(match.group(5).strip())  # Apply fancy name
                    entropy_value = int(match.group(6).strip())  # Entropy value

                    if lb_type == "mp":
                        lb_type = "MP-Swift"

                    cc_name = match.group(1).replace("cc", "", 1).strip()
                    if cc_name == "both":
                        cc_name = "nscc+rccc"
                    drop_strategy = match.group(2).strip()
                    os_ratio = int(match.group(3).strip())
                    message_size = int(match.group(4).strip()) / 1024  # Convert to KiB

                    # Map drop strategy to a more readable form
                    method = "Timeout" if drop_strategy == "timeout" else "Trimming" if drop_strategy == "trimming" else None
                    
                    if method is None:
                        continue
                    
                    # Get FCT list and find max FCT
                    fct_list = get_list_fct(full_path_file)

                    if fct_list:
                        max_fct = max(fct_list)


                        if ("dc" in args.name):
                            max_fct = sum(fct_list) / len(fct_list)

                        if ("coll" in args.name):
                            with open(full_path_file, 'r') as file:
                                max_fct = get_last_flow_time(file.read())
                                #print(max_fct)

                        if len(fct_list) > 0:
                            data.append({
                                'Max FCT': max_fct, 
                                'CC Algorithm': cc_name,  
                                'Failure Mode': failure_case,
                                'Drop Detection': method,
                                'LB Type': f"{lb_type} {entropy_value}",
                                'LB Name': lb_type,  # New column for just LB Type name
                                'Entropy': entropy_value
                            })

# Convert data to a DataFrame
df = pd.DataFrame(data)

# Ensure no leading/trailing spaces
df['Failure Mode'] = df['Failure Mode'].str.strip()

# Group by 'Failure Mode' and find the 'oblivious' baseline
baseline = df[df['LB Name'].str.contains('OPS', case=False)].set_index('Failure Mode')['Max FCT']

# Calculate speedup with error handling for missing baseline values
df['Speedup'] = df.apply(lambda row: baseline.get(row['Failure Mode'], np.nan) / row['Max FCT'] if row['Failure Mode'] in baseline.index else np.nan, axis=1)

#print(df['Speedup'])
# Define markers and sizes for Entropy levels
markers = {256: 'o', 65535: 'D'}  # Circle for 256, diamond for 65535
sizes = {256: 370, 65535: 370, 4096: 370}    # Marker sizes: 140 for 256, 370 for 65535

# Create the plot
# Define a custom order for the 'Failure Mode' (Workload) y-axis
custom_order = [
    "One Failed\nCable", "One Failed\nSwitch", "One Failed\nSwitch/Cable",
    "5% Failed\nCables", "5% Failed\nSwitches", 
    "5% Failed\nSwitches/Cables", "BER Cable 1%", 
    "BER Switch 1%"
]

# Convert 'Failure Mode' to a categorical type with the specified order
df['Failure Mode'] = pd.Categorical(df['Failure Mode'], categories=custom_order, ordered=True)

# Define unique markers for each LB type
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

# Create the plot
fig = plt.figure(figsize=(5.5, 4.2))  # Adjusted figure size for better readability

# Scatterplot with color for LB Name and shape for each algorithm
sns.scatterplot(
    data=df, 
    x='Speedup', 
    y='Failure Mode', 
    hue='LB Name', 
    style='LB Name',  # Assign each LB its own marker
    palette=static_color_mapping,  # Use the color palette from earlier
    markers=lb_markers,  # Apply the unique markers for each LB type
    alpha=0.7, 
    size='Entropy', 
    sizes=sizes
)

# Customizing the plot
#print("GIVEN TEXT IS: {}".format(args.text))
plt.xlabel(f'{args.text}', fontsize=18)  # Increased x-axis label font size
plt.ylabel('Failure Mode', fontsize=18)  # Increased y-axis label font size
plt.axvline(x=1, color='grey', linestyle='--', linewidth=1.5)  # Increased line width

# Adjust font sizes for ticks
plt.xticks(fontsize=16)  # Increased x-axis tick font size
plt.yticks(fontsize=14)  # Increased y-axis tick font size

# Create custom legend handles for LB Name (color only, without 'LB Mode')
lb_name_order = ["OPS", "Flowlet", "BitMap", "MPRDMA", "PLB", "REPS"]

lb_name_handles = [Line2D([0], [0], marker=lb_markers[label], color='w', label=label, 
                          markersize=17, markeredgewidth=0,  # No edge
                          markerfacecolor=static_color_mapping.get(label, '#666666'),  # Face color from palette
                          alpha=1) 
                   for label in lb_name_order]

# Manually add the legend for LB Name without a background
""" lb_name_legend = plt.legend(
    handles=lb_name_handles,
    title=None,  # No title to remove "LB Mode"
    bbox_to_anchor=(-0.065, 1.2),
    loc='upper left',
    fontsize=17,
    title_fontsize='15',
    ncol=4,
    markerscale=1,
    frameon=False,  # Remove legend background
    handletextpad=1.5
) """
lb_name_legend = plt.legend(
    handles=lb_name_handles,
    bbox_to_anchor=(-0.71, 1.8),  # Adjusted positioning for the LB Name legend
    loc='upper left',
    fontsize=16.5,  # Increased legend font size
    title_fontsize='15',  # Increased legend title font size
    ncol=len(lb_name_order),  # Split legend into 2 columns
    markerscale=1,
    frameon=False,
    handletextpad=1.5
)
# Add the custom legend to the plot
#plt.gca().add_artist(lb_name_legend)
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

output_name = "failures"

if (args.filename is not None):
    output_name = args.filename

os.system(f"mkdir -p {args.save_folder}")

plt.savefig(f"{args.save_folder}/{output_name}.png", bbox_inches='tight')
plt.savefig(f"{args.save_folder}/{output_name}.pdf", bbox_inches='tight')

# Show the plot
if (args.dont_show == False):
    plt.show()