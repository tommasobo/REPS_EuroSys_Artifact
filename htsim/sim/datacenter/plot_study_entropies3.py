import os
import re
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import argparse

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

                    '''
                    For Lukas: probably you can ignore the code above this as it 
                    is just reading the data from the file and extracting the
                    relevant information. The code below is where the actual
                    calculation and plotting is done.
                    '''

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

# Group by entropy to calculate mean, 2.5th percentile, and 97.5th percentile load imbalance
summary_df = df.groupby('entropy')['load_imbalance'].agg(
    mean='mean',
    perc_2_5=lambda x: x.quantile(0.025),
    perc_97_5=lambda x: x.quantile(0.975)
).reset_index()

# Sort the summary_df by entropy to ensure proper order
summary_df = summary_df.sort_values('entropy')

# Plot using Seaborn
plt.figure(figsize=(12, 8))
sns.lineplot(x='entropy', y='mean', data=summary_df, marker='o', label='Average Load Imbalance', color=static_color_mapping['ECMP'])

# Plotting the shaded region for 2.5th-97.5th percentile range
plt.fill_between(summary_df['entropy'], summary_df['perc_2_5'], summary_df['perc_97_5'], color=static_color_mapping['ECMP'], alpha=0.2, label='95% Percentile Range')

# Annotate the average values
for i in range(summary_df.shape[0]):
    plt.text(summary_df['entropy'].iloc[i], summary_df['mean'].iloc[i], f'{summary_df["mean"].iloc[i]:.2f}', 
             ha='center', va='bottom', fontsize=10, color='black')
    
# Use log scale for the x-axis
plt.xscale('log')

# Set x-ticks to show all entropy values explicitly
plt.gca().set_xticks(summary_df['entropy'])
plt.gca().set_xticklabels([f'{int(tick)}' for tick in summary_df['entropy']], rotation=0)

plt.title('Load Imbalance vs. Number of Entropies with 1000 Random Seeds')
plt.xlabel('Number of Entropies')
plt.ylabel('Load Imbalance')
plt.legend()
plt.grid(True)

plt.savefig("result_folder/entropy_study1.png", bbox_inches='tight')
plt.savefig("result_folder/entropy_study1.pdf", bbox_inches='tight')

plt.show()
