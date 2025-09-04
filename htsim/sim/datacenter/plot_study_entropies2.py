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

# Define the thresholds
thresholds = [0.05, 0.1, 0.2, 0.4, 0.8]

# Data structure to hold the probability data
probability_data = {'entropy': [], 'threshold': [], 'probability': []}

# Calculate the probability of exceeding each threshold for each entropy
for entropy_value in sorted(df['entropy'].unique()):
    subset = df[df['entropy'] == entropy_value]
    total_count = len(subset)
    
    for threshold in thresholds:
        count_exceeding = (subset['load_imbalance'] > threshold).sum()
        probability = count_exceeding / total_count if total_count > 0 else 0
        probability_data['entropy'].append(entropy_value)
        probability_data['threshold'].append(threshold)
        probability_data['probability'].append(probability)

# Convert the probability data into a pandas DataFrame
probability_df = pd.DataFrame(probability_data)

# Plot using Seaborn
plt.figure(figsize=(12, 8))
sns.lineplot(x='entropy', y='probability', hue='threshold', data=probability_df, marker='o')

plt.title('Probability of Exceeding Load Imbalance Thresholds')
plt.xlabel('Number of Entropies')
plt.ylabel('Probability')
plt.xscale('log')
plt.ylim(0, 1)  # Set y-axis from 0 to 1 for probability
plt.legend(title='Threshold', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.grid(True)

# Set x-ticks to show all entropy values explicitly
plt.gca().set_xticks(sorted(df['entropy'].unique()))
plt.gca().set_xticklabels([f'{int(tick)}' for tick in sorted(df['entropy'].unique())], rotation=0)

plt.savefig("result_folder/entropy_study2.png", bbox_inches='tight')
plt.savefig("result_folder/entropy_study2.pdf", bbox_inches='tight')

plt.show()
