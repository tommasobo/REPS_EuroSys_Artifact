import re
import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib import gridspec
import matplotlib

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Ensure output directories exist
for directory in [
    "../artifact_results",
    "../artifact_results/fig_2_symmetric",
    "../artifact_results/fig_2_symmetric/data",
    "../artifact_results/fig_2_symmetric/plots"
]:
    os.makedirs(directory, exist_ok=True)

os.chdir("../htsim/sim/datacenter")
os.system("python3 run_all_symm.py --save_folder ../../../artifact_results/fig_2_symmetric/plots/ --filename microbenchmarks")
os.system("python3 run_all_symm.py --save_folder ../../../artifact_results/fig_2_symmetric/plots/ --filename dc")
os.system("python3 run_all_symm.py --save_folder ../../../artifact_results/fig_2_symmetric/plots/ --filename ai")

