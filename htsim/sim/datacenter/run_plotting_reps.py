import os
import glob
import re
import seaborn as sns
import statistics
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import argparse


parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
parser.add_argument('--link_speed_exp', required=True, help='Link Speed for the experiment (Mbps)', default=100000)
parser.add_argument('--size_topology_exp', required=True, help='Topology Size for the experiment', default=128)
args = parser.parse_args()


# Initialize the main folder path
main_folder = "{}/{}/{}/".format(args.input_folder_exp, args.link_speed_exp, args.size_topology_exp)
main_folder = "{}/".format(args.input_folder_exp)

subfolder_pattern = "exp_os1_ccalgo*"
link_speed_used = int(int(args.link_speed_exp) / 1000) # Convert to Gbps

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

# Iterate through each subfolder
for subfolder in glob.glob(os.path.join(main_folder, subfolder_pattern)):
    # Iterate through each .txt file in the subfolder
    for txt_file in glob.glob(os.path.join(subfolder, "*.txt")):
        # Extract information from the filename
        match = re.search(
            r"out_(cc\w+)_drop(\w+)_os(\d+)_size(\d+)_lb(\w+)_entropy(\d+)\.txt", 
            os.path.basename(txt_file)
        )
        if match:
            lb_type = match.group(5)  # Load balancer type
            """ if lb_type == "ecmp" or lb_type == "plb" or lb_type == "mp":
                continue  """
            entropy_value = int(match.group(6))  # Entropy value

            """ if (entropy_value == 65535) and (lb_type == "mp" or lb_type == "ecmp" or lb_type == "plb"):
                continue """

            if lb_type == "mp":
                lb_type = "MP-Swift"

            cc_name = match.group(1).replace("cc", "", 1)
            if cc_name == "both":
                cc_name = "nscc+rccc"
            drop_strategy = match.group(2)
            os_ratio = int(match.group(3))
            message_size = int(match.group(4)) / 1024  # Convert to KiB
            
            
            # Map drop strategy to a more readable form
            if drop_strategy == "timeout":
                method = "Timeout"
            elif drop_strategy == "trimming":
                method = "Trimming"
            else:
                continue
            
            # Get FCT list and find max FCT
            fct_list = get_list_fct(txt_file)
            print(f"Processing file: {txt_file}")

            if fct_list:
                max_fct = max(fct_list)
                #max_fct = statistics.mean(fct_list)
                #max_fct = np.percentile(fct_list, 99)

                # Print the message size and max FCT for debugging
                print(f"Message Size: {message_size}, Max FCT: {max_fct}")

                ideal_time = ((message_size * 8 * 1024) / (link_speed_used / os_ratio)) / 1000 + 10  # 10 Gbps link speed
                
                runtime_ratio = max_fct / ideal_time
                if runtime_ratio < 1:
                    runtime_ratio = 1

                print(f"CC: {cc_name}, Drop: {method}, Size: {message_size}, LB: {lb_type}, Entropy: {entropy_value}, Max FCT: {max_fct}, Ideal Time: {ideal_time}, Runtime Ratio: {runtime_ratio}")

                # Append to data
                data.append({
                    'Message Size (KiB)': message_size, 
                    'Max FCT': max_fct, 
                    'CC Algorithm': cc_name,  
                    'Drop Detection': method,
                    'LB Type & Entropy': f"{lb_type} {entropy_value}"
                })

# Convert data to a DataFrame
df = pd.DataFrame(data)

# Set up a custom color palette, assigning specific colors to each CC algorithm
""" palette = {
    "mixed 32": "#fa841a",
    "mixed 64": "#38a63f",
    "mixed 128": "#95a0a7",
    "mixed 256": "#a8558d",
    "mixed 1024": "blue",
    "mixed 60000": "#154c79",
    "reps 32": "green",
    "reps 64": "red",
    "reps 128": "black",
    "reps 256": "yellow",
    "reps 1024": "#1f77b4",
    "reps 60000": "#abdbe3",
    "oblivious 32": "#873e23",
    "oblivious 64": "#76b5c5",
    "oblivious 128": "#eab676",
    "oblivious 256": "#e28743",
    "oblivious 1024": "#eeeee4",
    "oblivious 60000": "#1e81b0",
} 

palette = {
    "oblivious 32": "#fa841a",
    "oblivious 64": "#38a63f",
    "oblivious 128": "#95a0a7",
    "oblivious 256": "#a8558d",
    "oblivious 1024": "blue",
    "oblivious 60000": "#154c79",
    "reps 32": "green",
    "reps 64": "red",
    "reps 128": "black",
    "reps 256": "yellow",
    "reps 1024": "#1f77b4",
    "reps 60000": "#abdbe3",
    "mixed 32": "#873e23",
    "mixed 64": "#76b5c5",
    "mixed 128": "#eab676",
    "mixed 256": "#e28743",
    "mixed 1024": "#eeeee4",
    "mixed 60000": "#1e81b0",
}"""

# Seaborn plot
print(df)
plt.figure(figsize=(10, 6))
ax = sns.lineplot(
    data=df, 
    x='Message Size (KiB)', 
    y='Max FCT', 
    hue='LB Type & Entropy', 
    style='CC Algorithm', 
    markers=True, 
    linewidth=2.5  # Increase line width
)

# Set x-axis to log scale with base 2
plt.xscale('log', base=2)

# Manually set x-ticks to the powers of 2
xticks = df['Message Size (KiB)'].unique()
xticks.sort()
xtick_labels = [f"{int(x)} KiB" if x < 1024 else f"{int(x/1024)} MiB" for x in xticks]

plt.xticks(xticks, labels=xtick_labels)

# Customize the plot
plt.xlabel('Message Size')
plt.ylabel('Ratio to Ideal Completion Time')
plt.grid(True)

# Adjust the legend: Remove "Trimming" entries and set custom order
handles, labels = plt.gca().get_legend_handles_labels()

# Create a dictionary to map labels to handles
label_handle_map = dict(zip(labels, handles))

# Filter out "Trimming" entries and sort by entropy
filtered_labels = [label for label in labels if 'Trimming' not in label]
filtered_handles = [label_handle_map[label] for label in filtered_labels]

# Function to extract entropy value for sorting
def extract_entropy(label):
    match = re.search(r'(\d+)', label)
    return int(match.group(1)) if match else float('inf')  # Default to inf if no match

# Sort labels and handles by entropy
sorted_labels = sorted(filtered_labels, key=extract_entropy)
sorted_handles = [filtered_handles[filtered_labels.index(label)] for label in sorted_labels]
del sorted_labels[-3:]
del sorted_handles[-3:]

plt.legend(sorted_handles, sorted_labels)

# Add annotations at the last point of each line
grouped = df.groupby(['LB Type & Entropy', 'CC Algorithm'])

for name, group in grouped:
    # Sort the group by 'Message Size (KiB)' to ensure correct ordering
    group_sorted = group.sort_values(by='Message Size (KiB)')
    
    # Get the maximum x-value (last point in sorted order)
    last_row = group_sorted.iloc[-1]
    last_x = last_row['Message Size (KiB)']
    last_y = last_row['Max FCT']
    
    # Print to debug the last x and y values
    print(f"Annotating for {name}: x={last_x}, y={last_y}")
    
    #f"{name[0]} ({name[1]})",          # Text to display

    # Annotate the last point with the name
    ax.annotate(
        f"{name[0]}",          # Text to display
        xy=(last_x, last_y),               # Position of the annotation
        xytext=(5, 0),                     # Offset text position slightly to avoid overlap
        textcoords='offset points',        # Use offset points for positioning
        horizontalalignment='left',        # Align text to the left
        verticalalignment='center',        # Center text vertically
        size='small', 
        weight='semibold'
    )

plt.savefig(os.path.join(main_folder, "runtime_plot81.png"), bbox_inches='tight')
plt.savefig(os.path.join(main_folder, "runtime_plot81.pdf"), bbox_inches='tight')
plt.show()
