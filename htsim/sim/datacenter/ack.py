import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

# Function to get FCTs
def get_list_fct(name_file_to_use):
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

# Get FCT lists
fct_list_obs = get_list_fct("obs.out")
fct_list_obs_2_compression = get_list_fct("obs_2_ack.out")
fct_list_obs_4_compression = get_list_fct("obs_4_ack.out")
fct_list_obs_8_compression = get_list_fct("obs_8_ack.out")
fct_list_no_compression = get_list_fct("reps_no_ack.out")
fct_list_2_compression = get_list_fct("reps_2_ack.out")
fct_list_4_compression = get_list_fct("reps_4_ack.out")
fct_list_8_compression = get_list_fct("reps_8_ack.out")

# Define custom color palette
dark2_hex_colors = [
    '#1b9e77',  # Green
    '#d95f02',  # Orange
    '#7570b3',  # Purple
    '#e7298a',  # Pink
]

# Define colors and line styles for REPS and OPS
reps_color = dark2_hex_colors[0]  # Green
ops_color = dark2_hex_colors[1]  # Orange

# Line styles and markers for the ratios
line_styles = ['-', ':']  # REPS and OPS line styles
markers = ['o', 's', '^', 'D']  # Markers for 1:1, 2:1, 4:1, and 8:1 ratios

# Update font sizes and styles
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 10.6,
    'ytick.labelsize': 10.6,
    'legend.fontsize': 11,
    'figure.titlesize': 18,
    'grid.alpha': 0.8,
    'grid.color': '#cccccc',
    'axes.grid': True,
    'grid.linestyle': '-',  # Solid grid lines
})

# Helper function to plot CDF with markers only for the first, middle, and last points
def plot_cdf(data, label, color, linestyle, marker):
    data_sorted = np.sort(data)
    cdf = np.arange(1, len(data_sorted) + 1) / len(data_sorted)
    # Determine marker positions
    mark_indices = [len(data_sorted) - 1]
    plt.plot(
        data_sorted,
        cdf,
        label=label,
        color=color,
        linestyle=linestyle,
        marker=marker,
        markevery=mark_indices,  # Only plot markers at specified indices
        markersize=5.4,
        linewidth=2.2
    )
# Plotting the CDF
fig = plt.figure(figsize=(3.6, 3.3))

# REPS lines with markers
plot_cdf(fct_list_no_compression, "REPS - No Compression", dark2_hex_colors[0], line_styles[0], markers[0])
plot_cdf(fct_list_2_compression, "REPS - 2:1 Compression", dark2_hex_colors[1], line_styles[0], markers[1])
plot_cdf(fct_list_4_compression, "REPS - 4:1 Compression", dark2_hex_colors[2], line_styles[0], markers[2])
plot_cdf(fct_list_8_compression, "REPS - 8:1 Compression", dark2_hex_colors[3], line_styles[0], markers[3])

# OPS lines with markers
plot_cdf(fct_list_obs, "OPS - No Compression", dark2_hex_colors[0], line_styles[1], markers[0])
plot_cdf(fct_list_obs_2_compression, "OPS - 2:1 Compression", dark2_hex_colors[1], line_styles[1], markers[1])
plot_cdf(fct_list_obs_4_compression, "OPS - 4:1 Compression", dark2_hex_colors[2], line_styles[1], markers[2])
plot_cdf(fct_list_obs_8_compression, "OPS - 8:1 Compression", dark2_hex_colors[3], line_styles[1], markers[3])

plt.xlim(175,205)

plt.xlabel('Flow Completion Time (μs)', fontsize=13)
plt.ylabel('CDF', fontsize=13)

# Custom grid
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility


# Set up the legend with custom handles for colors, line styles, and markers

# Custom legend handles
legend_elements = [
    Line2D([0], [0], color=dark2_hex_colors[0], marker=markers[0], lw=2, label='1:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[1], marker=markers[1], lw=2, label='2:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[2], marker=markers[2], lw=2, label='4:1 Ratio'),
    Line2D([0], [0], color=dark2_hex_colors[3], marker=markers[3], lw=2, label='8:1 Ratio'),
    Line2D([0], [0], color='none', lw=0, label=''),  # Smaller empty line for spacing
    Line2D([0], [0], color='black', lw=2, linestyle='--', label='OPS'),
    Line2D([0], [0], color='black', lw=2, linestyle='-', label='REPS'),
]

# Create the legend
#plt.legend(handles=legend_elements, loc='lower right', bbox_to_anchor=(1.04, -0.05), fontsize=11, ncol=1, frameon=False)

# Save the plot as PNG and PDF with better formatting
plt.tight_layout()
plt.savefig('cdf_ack.png', dpi=300, bbox_inches='tight')
plt.savefig('cdf_ack.pdf', dpi=300, bbox_inches='tight')

# Show the plot
plt.show()
