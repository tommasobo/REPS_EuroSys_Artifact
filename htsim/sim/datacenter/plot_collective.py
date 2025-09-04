import os
import re
import matplotlib.pyplot as plt
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

# Define the desired order for load balancers in the bars
lb_name_order = ["ECMP", "OPS", "Flowlet", "BitMap", "MPRDMA", "PLB", "MPTCP", "Adaptive RoCE", "REPS"]

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

# Function to map the load balancer names
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

# Function to update the x labels based on custom logic
def update_label(label):
    if label == 'alltoall4':
        return 'AlltoAll\n(n=4)'
    elif label == 'alltoall8':
        return 'AlltoAll\n(n=8)'
    elif label == 'alltoall1':
        return 'AlltoAll\n(n=1)'
    elif label == 'alltoall2':
        return 'AlltoAll\n(n=2)'
    elif label == 'alltoall16':
        return 'AlltoAll\n(n=16)'
    elif label == 'allreduce':
        return 'Ring\nAllRed.'
    elif label == 'allreduce_t':
        return 'Buttefly\nAllRed.'
    else:
        return label  # Return the original label if no match is found

parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
parser.add_argument('--name', required=False, help='Name for the experiment', default="")
parser.add_argument('--save_folder', required=True, help='Save Folder for the experiment', default="")
parser.add_argument('--dont_show', action='store_true', help='Not save fig')
parser.add_argument('--filename',    default=None,
                    help='optional filename to pass to plot_symmetric')
args = parser.parse_args()
main_folder = args.input_folder_exp

# Dictionary to store results
collective_data = {}

# Traverse through subfolders (collectives)
for collective in os.listdir(main_folder):
    collective_path = os.path.join(main_folder, collective)
    
    if os.path.isdir(collective_path):
        load_balancer_times = {}
        
        # Traverse through each log file for load balancers
        for file_name in os.listdir(collective_path):
            file_path = os.path.join(collective_path, file_name)
            
            if "reps" in file_name:
                continue
            
            # Read the content of the log file
            with open(file_path, 'r') as file:
                #print(file_path)
                # "reduce" not in file_path
                if (True):
                    log_content = file.read()
                    last_time = get_last_flow_time(log_content) / 1000
                    
                    if last_time is not None:
                        file_name = update_name(file_name)
                        load_balancer_times[file_name] = last_time
        
        collective_data[collective] = load_balancer_times

# Define the desired order for the x-tick labels
desired_order = ['alltoall1', 'alltoall2', 'alltoall4', 'alltoall8', 'alltoall16', 'allreduce', 'allreduce_t']
desired_order = ['alltoall4', 'alltoall8', 'alltoall16', 'allreduce', 'allreduce_t']

# Reorder the collective data to match the desired order
ordered_collective_data = {k: collective_data[k] for k in desired_order if k in collective_data}

# Create the bar plot
collectives = list(ordered_collective_data.keys())

# Prepare the plot
fig, ax = plt.subplots(figsize=(5.5, 3.5))

# Set font sizes
plt.rcParams.update({
    'axes.titlesize': 14.5,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Define bar width and position for each load balancer
bar_width = 0.1
index = range(len(collectives))

# Plot each load balancer's finishing times following the lb_name_order
for i, lb_name in enumerate(lb_name_order):
    times = [ordered_collective_data[c].get(lb_name, 0) for c in collectives]
    color = static_color_mapping.get(lb_name, '#000000')  # Default to black if not found
    ax.bar([x + i * bar_width for x in index], times, bar_width, label=lb_name, color=color)

# Apply updated labels to the x-ticks in the correct order
updated_x_labels = [update_label(label) for label in collectives]
ax.set_xticks([x + bar_width * (len(lb_name_order) / 2) for x in index])
ax.set_xticklabels(updated_x_labels)  # Rotate labels if needed

# Add labels and title
ax.set_ylabel('Collective Runtime (ms)', fontsize=17)
#ax.legend(ncol=4, loc='lower center', bbox_to_anchor=(0.50, 0.95), frameon=False)

plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility
plt.xticks(fontsize=16)  # Increased x-axis tick font size
plt.yticks(fontsize=16)  # Increased y-axis tick font size
# Adjust layout to avoid clipping
plt.tight_layout()

output_name = "collectives"

if (args.filename is not None):
    output_name = args.filename

os.system(f"mkdir -p {args.save_folder}")

plt.savefig(f"{args.save_folder}/{output_name}.png", bbox_inches='tight')
plt.savefig(f"{args.save_folder}/{output_name}.pdf", bbox_inches='tight')

# Show the plot
if (args.dont_show == False):
    plt.show()
