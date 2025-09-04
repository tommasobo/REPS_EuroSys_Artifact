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
    "../artifact_results/fig_5_bg_traffic",
    "../artifact_results/fig_5_bg_traffic/data",
    "../artifact_results/fig_5_bg_traffic/plots"
]:
    os.makedirs(directory, exist_ok=True)

os.chdir("../htsim/sim/datacenter")
os.system("python3 run_bg.py --save_folder ../../../artifact_results/fig_5_bg_traffic/plots/ --filename main_traffic")
os.system("python3 run_bg.py --save_folder ../../../artifact_results/fig_5_bg_traffic/plots/ --filename bg_traffic --bg_type 1")