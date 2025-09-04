import argparse
import re
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
import os

def parse_rtt_values(name_file_to_use):
    """
    Extracts the finished-at runtime values from the file.
    """
    temp_list = []
    try:
        with open(name_file_to_use) as file:
            for line in file:
                pattern = r"RTT: (\d+) - FlowSize (\d+)"
                match = re.search(pattern, line)
                if match:
                    rtt = float(int(match.group(1))/1000/1000)
                    flow_size = int(int(match.group(2)) / 1000)
                    temp_list.append(rtt)
                    """ if (flow_size == 12288):
                        temp_list.append(rtt) """
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    
    return temp_list

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
                    """ if (actual_fct < 50):
                        temp_list.append(actual_fct) """
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    return temp_list

def filter_percentile(values, percentile):
    # Calculate the percentile threshold
    threshold = np.percentile(values, percentile)
    # Filter values above the threshold
    return [v for v in values if v >= threshold]

def plot_rtt_distribution(rtt_values_1, rtt_values_2, bins, plot_percentile, plot_type, output_folder, not_show):
    # If the percentile option is enabled, filter values based on the 99th percentile
    if plot_percentile:
        rtt_values_1 = filter_percentile(rtt_values_1, 99)
        rtt_values_2 = filter_percentile(rtt_values_2, 99)

    fig = plt.figure(figsize=(5, 3.9))

    # Select the appropriate plot type
    if plot_type == 'histogram':
        sns.histplot(rtt_values_1, kde=True, color='blue', bins=bins, label="Freeze after 150us", alpha=0.6)
        sns.histplot(rtt_values_2, kde=True, color='red', bins=bins, label="No Freeze", alpha=0.6)
        plt.xlabel("RTT (microseconds)")
        plt.ylabel("Frequency")

    elif plot_type == 'boxplot':
        sns.boxplot(data=[rtt_values_1, rtt_values_2], palette=["blue", "red"])
        plt.xticks([0, 1], ["Freeze after 150us", "No Freeze"])
        plt.xlabel("File")
        plt.ylabel("RTT (microseconds)")

    elif plot_type == 'violin':
        sns.violinplot(data=[rtt_values_1, rtt_values_2], palette=["blue", "red"])
        plt.xticks([0, 1], ["Freeze after 150us", "No Freeze"])
        plt.xlabel("File")
        plt.ylabel("RTT (microseconds)")

    elif plot_type == 'ecdf':
        sns.ecdfplot(rtt_values_1, label="Freeze after 150us", linewidth=2, color="#1b9e77")
        axes = sns.ecdfplot(rtt_values_2, label="No Freeze", linewidth=2, color="#d95f02")
        axes.tick_params(axis='x', labelsize=13)  # Increase x-tick label size
        axes.tick_params(axis='y', labelsize=13)  # Increase x-tick label size
        plt.xlabel("RTT (microseconds)", fontsize=12)
        plt.ylabel("CDF", fontsize=12)
        plt.legend()

    elif plot_type == 'strip':
        sns.stripplot(data=[rtt_values_1, rtt_values_2], jitter=True, palette=["blue", "red"])
        plt.xticks([0, 1], ["Freeze after 150us", "No Freeze"])
        plt.xlabel("File")
        plt.ylabel("RTT (microseconds)")

    elif plot_type == 'swarm':
        sns.swarmplot(data=[rtt_values_1, rtt_values_2], palette=["blue", "red"])
        plt.xticks([0, 1], ["Freeze after 150us", "No Freeze"])
        plt.xlabel("File")
        plt.ylabel("RTT (microseconds)")

    else:
        print(f"Unknown plot type: {plot_type}. Please choose from 'histogram', 'boxplot', 'violin', 'ecdf', 'strip', or 'swarm'.")
        return
    plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

    plt.legend()
    if plot_percentile:
        plt.title("RTT Distribution -- 99th percentile")
        plt.savefig(os.path.join(output_folder, "rtt_distr_percentile.png"), bbox_inches='tight')
        plt.savefig(os.path.join(output_folder, "rtt_distr_percentile.pdf"), bbox_inches='tight')
    else:
        plt.title("RTT Distribution")
        plt.savefig(os.path.join(output_folder, "rtt_distr.png"), bbox_inches='tight')
        plt.savefig(os.path.join(output_folder, "rtt_distr.pdf"), bbox_inches='tight')
    if not not_show:
        plt.show()

def main():
    # Setting up argparse
    parser = argparse.ArgumentParser(description="Plot RTT Distribution from two text files")
    parser.add_argument('--file1', type=str, required=True, help='The path to the first text file containing RTT values')
    parser.add_argument('--file2', type=str, required=True, help='The path to the second text file containing RTT values')
    parser.add_argument('--output_folder', type=str, default="", required=False, help='The path to the output folder')
    parser.add_argument('--bins', type=int, default=10, help='The number of bins (buckets) to use in the histogram')
    parser.add_argument('--percentile', action='store_true', help='If set, plots only RTT values at the 99th percentile or higher')
    parser.add_argument('--no_show', action='store_true', help='If set, do not show the plot')
    parser.add_argument('--plot_type', type=str, default='histogram', help="The type of plot to display: 'histogram', 'boxplot', 'violin', 'ecdf', 'strip', or 'swarm'")
    
    args = parser.parse_args()
    
    # Parse the RTT values from both files
    rtt_values_1 = parse_rtt_values(args.file1)
    rtt_values_2 = parse_rtt_values(args.file2)


    """ fct1 = get_list_fct(args.file1)
    fct2 = get_list_fct(args.file2) """
    """ 
    print("Max FCT1: ", max(fct1))
    print("Max FCT2: ", max(fct2))

    print("Mean FCT1: ", sum(fct1) / len(fct1))
    print("Mean FCT2: ", sum(fct2) / len(fct2)) """
    
    # Plot the RTT distribution comparison with the specified number of bins, percentile filtering, and plot type
    plot_rtt_distribution(rtt_values_1, rtt_values_2, args.bins, args.percentile, args.plot_type, args.output_folder, args.no_show)

if __name__ == "__main__":
    main()
