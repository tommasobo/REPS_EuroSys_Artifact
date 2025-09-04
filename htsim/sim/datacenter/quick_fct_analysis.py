import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import re
import argparse
import os

# Function to parse the log file and extract flow size and FCT
def parse_log_file(file_name):
    data = []
    # Updated regex to account for the correct order of 'finished at' and 'flowSize'
    flow_pattern = re.compile(r"finished\sat\s([\d]+\.[\d]+).*?flowSize\s(\d+)")
    
    file_size = os.path.getsize(file_name)
    
    # Jump to a specific part of the file (3/4th of the file)
    with open(file_name, 'r') as file:
        file.seek((file_size * 3) // 4)  # Go to three-quarters into the file
        for line in file:
            match = flow_pattern.search(line)
            if match:
                fct = float(match.group(1))
                flow_size = int(match.group(2))
                data.append({"flow_size": flow_size, "fct": fct})
    
    # Convert the parsed data into a pandas DataFrame
    return pd.DataFrame(data)

# Function to create plots with a specified number of bins, supporting multiple files for comparison
def create_comparison_plots(filenames, num_bins):
    # Parse all files and store them in a list
    dataframes = [parse_log_file(filename) for filename in filenames]
    
    # Step 1: Calculate the common range of flow sizes across all files
    min_flow_size = min(df['flow_size'].min() for df in dataframes)
    max_flow_size = max(df['flow_size'].max() for df in dataframes)
    bin_size = (max_flow_size - min_flow_size) / num_bins
    
    # Create bin edges based on the calculated bin size
    bin_edges = np.arange(min_flow_size, max_flow_size + bin_size, bin_size)

    # Prepare the figure with one extra column for overall statistics
    fig, axes = plt.subplots(2, num_bins + 1, figsize=(24, 10))  # 2 rows, num_bins + 1 columns

    # For each file, compute statistics and plot side-by-side for comparison
    for i, df in enumerate(dataframes):
        # Create bins based on flow size
        df['bin'] = pd.cut(df['flow_size'], bins=bin_edges, include_lowest=True)

        # Step 2: Compute statistics for each bin
        avg_fct_per_bin = df.groupby('bin')['fct'].mean()
        percentile_99_fct_per_bin = df.groupby('bin')['fct'].quantile(0.99)

        # Plot Avg FCT for each bin, placing bars side by side for comparison
        for j, (bin_label, avg_fct) in enumerate(avg_fct_per_bin.items()):
            ax = axes[0, j]
            ax.bar(i, avg_fct, label=f'File {i+1}')
            ax.set_title(f'Avg FCT for {bin_label}')
            ax.set_ylabel('Avg FCT')
            ax.set_xlabel('File Comparison')

        # Plot 99th percentile FCT for each bin, placing bars side by side for comparison
        for j, (bin_label, pct_99_fct) in enumerate(percentile_99_fct_per_bin.items()):
            ax = axes[1, j]
            ax.bar(i, pct_99_fct, label=f'File {i+1}')
            ax.set_title(f'99th Percentile FCT for {bin_label}')
            ax.set_ylabel('99th Percentile FCT')
            ax.set_xlabel('File Comparison')

        # Step 3: Compute overall average and 99th percentile FCT for all flow sizes
        overall_avg_fct = df['fct'].mean()
        overall_pct_99_fct = df['fct'].quantile(0.99)

        # Add the overall avg FCT to the last column of the first row
        ax = axes[0, -1]
        ax.bar(i, overall_avg_fct, label=f'File {i+1}')
        ax.set_title('Overall Avg FCT')
        ax.set_ylabel('Avg FCT')
        ax.set_xlabel('File Comparison')

        # Add the overall 99th percentile FCT to the last column of the second row
        ax = axes[1, -1]
        ax.bar(i, overall_pct_99_fct, label=f'File {i+1}')
        ax.set_title('Overall 99th Percentile FCT')
        ax.set_ylabel('99th Percentile FCT')
        ax.set_xlabel('File Comparison')

    # Add legends to the plots
    for ax in axes.flatten():
        ax.legend()

    plt.tight_layout()

    plt.savefig("fcts100.png", bbox_inches='tight', dpi=300)
    plt.savefig("fcts100.pdf", bbox_inches='tight', dpi=300)

    plt.show()

# Main function to handle command-line arguments and run the script
def main():
    parser = argparse.ArgumentParser(description="Visualize Avg and 99th Percentile FCT for different flow size bins across multiple files.")
    parser.add_argument('filenames', type=str, nargs='+', help="Log files to be parsed and visualized (up to 3 files)")
    parser.add_argument('--num_bins', type=int, default=5, help="Number of bins (default: 5)")
    args = parser.parse_args()

    if len(args.filenames) > 3:
        print("Please provide up to 3 filenames.")
        return

    # Create and display the comparison plots
    create_comparison_plots(args.filenames, args.num_bins)

if __name__ == "__main__":
    main()
