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
    "../artifact_results/fig_4_asymmetric",
    "../artifact_results/fig_4_asymmetric/data",
    "../artifact_results/fig_4_asymmetric/plots"
]:
    os.makedirs(directory, exist_ok=True)

os.chdir("../htsim/sim/datacenter")
os.system("python3 run_all_asy.py --save_folder ../../../artifact_results/fig_4_asymmetric/plots/ --filename microbenchmarks")
os.system("python3 run_all_asy.py --save_folder ../../../artifact_results/fig_4_asymmetric/plots/ --filename dc")
os.system("python3 run_all_asy.py --save_folder ../../../artifact_results/fig_4_asymmetric/plots/ --filename ai")

