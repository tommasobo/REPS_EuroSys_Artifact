import os
import re
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import argparse
import numpy as np

# Argument parser for input folder
parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment')
parser.add_argument('--conns', required=True, help='Number conns for the experiment')

args = parser.parse_args()
folder_path = args.input_folder_exp

# Regex pattern to extract the number of entropies and seeds from the filename
entropy_pattern = re.compile(r'entropy(\d+)_seed(\d+)')

# Regex pattern to extract the port number
port_pattern = re.compile(r'Port: compqueue\(.+\)LS\d+->US(\d+)\(')

# Data structure to hold the data
data = {'entropy': [], 'seed': [], 'load_imbalance': []}

# Fixed value of n (number of bins)
n = 32

# Loop through all the files in the folder
for filename in os.listdir(folder_path):
    if filename.endswith(".txt"):  # Process only .txt files
        filepath = os.path.join(folder_path, filename)
        
        if os.path.getsize(filepath) > 0:
            with open(filepath, 'r') as file:
                file_content = file.read()
                
                # Find all port matches in the file
                ports = port_pattern.findall(file_content)
                ports = [int(port) for port in ports]
                
                # Extract the number of entropies and seed from the filename
                entropy_match = entropy_pattern.search(filename)
                if entropy_match:
                    m = int(entropy_match.group(1)) * int(args.conns)
                    seed = int(entropy_match.group(2))
                    
                    # Calculate l(m, n) as the max usage of any port
                    if ports:
                        l_m_n = max(ports.count(port) for port in set(ports))
                    else:
                        l_m_n = 0  # Handle cases with no ports found
                    
                    # Calculate load imbalance
                    load_imbalance = l_m_n / (m / n)
                    load_imbalance -= 1  # Subtract 1 to get the load imbalance

                    # Store the results in the data structure
                    data['entropy'].append(int(m/int(args.conns)))
                    data['seed'].append(seed)
                    data['load_imbalance'].append(load_imbalance)

# Convert the data into a pandas DataFrame
df = pd.DataFrame(data)

# Define color mapping (for future flexibility)
static_color_mapping = {
    'RPS': '#d95f02',  # Orange
}

# Group by entropy to calculate mean, min, and max load imbalance
summary_df = df.groupby('entropy')['load_imbalance'].agg(
    mean='mean',
    perc_2_5=lambda x: x.quantile(0.025),
    perc_97_5=lambda x: x.quantile(0.975)
).reset_index()

# Sort the summary_df by entropy to ensure proper order
summary_df = summary_df.sort_values('entropy')

# Adjust font sizes and figure size to fit a single column in a scientific paper
plt.figure(figsize=(3.5, 2.5))  # Adjust to fit in a single column (3.5 inches wide)

# Increase line width and marker size for better visibility
sns.lineplot(x='entropy', y='mean', data=summary_df, marker='o', markersize=7, linewidth=2, color=static_color_mapping['RPS'], label='Average Load Imbalance')

# Plotting the shaded region for 95% percentile range
plt.fill_between(summary_df['entropy'], summary_df['perc_2_5'], summary_df['perc_97_5'], color=static_color_mapping['RPS'], alpha=0.2, label='95% Percentile Range')

# Annotate the average values (optional)
""" for i in range(summary_df.shape[0]):
    plt.text(summary_df['entropy'].iloc[i], summary_df['mean'].iloc[i], f'{summary_df["mean"].iloc[i]:.2f}', 
             ha='center', va='bottom', fontsize=8, color='black')
 """

# Annotate only every 2nd point to avoid clutter, and adjust the annotations to avoid overlap
for i in range(0, summary_df.shape[0], 1):  # Annotate every 2nd point
    offset_y = 0.11
    offset_x = 3.35
    if (i < 4):
        offset_y = 0.2
        offset_x = 0.05
    if (i==1):
        offset_x = 11.05

    offset_x = 0
    offset_y = 0
    if (i < 4):
        offset_y = 0.015
        offset_x = 3.5
    plt.text(
        summary_df['entropy'].iloc[i]+offset_x, 
        summary_df['mean'].iloc[i]+offset_y, 
        f'{summary_df["mean"].iloc[i]:.2f}', 
        ha='center', 
        va='bottom', 
        fontsize=9.1, 
        color='black', 
        rotation=45  # Add slight rotation to avoid overlap
    )


# Set the x-axis to log scale (base 2)
plt.xscale('log', base=2)

# Set x-ticks to show only where entropy values are present (actual values, not log-transformed)
plt.gca().set_xticks(summary_df['entropy'])

# Display x-tick labels as the exponent part (e.g., 14 for 2^14)
plt.gca().set_xticklabels([f'{int(round(np.log2(val)))}' for val in summary_df['entropy']], rotation=0)

# Add explanation to the legend to clarify the "2^x" representation
plt.legend()

# Increase font sizes for readability
#plt.title('Load Imbalance vs. EVG Size', fontsize=12, weight='bold')
plt.xlabel('EVS Size (2^x)', fontsize=12)
plt.ylabel('Load Imbalance', fontsize=12)
plt.xticks(fontsize=10.5)
plt.yticks(fontsize=10.5)

# Adjust grid for better readability
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Adjust layout to avoid clipping
plt.tight_layout()

# Save the plot with higher DPI for print quality
plt.savefig("result_folder/entropy_study32.png", bbox_inches='tight', dpi=400)
plt.savefig("result_folder/entropy_study32.pdf", bbox_inches='tight')

plt.show()
