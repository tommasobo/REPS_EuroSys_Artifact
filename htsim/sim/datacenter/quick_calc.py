import os
import glob
import re
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import statistics
from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description='Specify arguments for the script')
parser.add_argument('--input_folder_exp', required=True, help='Input Folder for the experiment', default="experiments")
args = parser.parse_args()

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


fct_list = get_list_fct(args.input_folder_exp)

if fct_list:
    max_fct = max(fct_list)
    mean_fct = statistics.mean(fct_list)
    percentile_fct = np.percentile(fct_list, 99)

    print(f"Maximum FCT: {max_fct}")
    print(f"Mean FCT: {mean_fct}")
    print(f"99th Percentile FCT: {percentile_fct}")