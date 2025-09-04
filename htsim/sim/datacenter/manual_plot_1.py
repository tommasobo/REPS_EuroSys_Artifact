import numpy as np
import matplotlib.pyplot as plt

# Data for the bar plot
x_labels = ['Switch setup-1', 'Switch setup-2']
reps = [86.5, 96.75]  # Example data for REPS
ops = [96.8, 93.3]  # Example data for OPS

# Min and Max placeholders (you will replace with actual values)
reps_min = [86.3, 96.71]  # Example min values for REPS
reps_max = [86.9, 96.76]  # Example max values for REPS
ops_min = [96.6, 93.0]  # Example min values for OPS
ops_max = [96.9, 93.6]  # Example max values for OPS

x = np.arange(len(x_labels))  # The label locations
width = 0.4  # The width of the bars

# Calculate error values (difference between max and the value itself, and value and min)
reps_errors = [np.array(reps) - np.array(reps_min), np.array(reps_max) - np.array(reps)]
ops_errors = [np.array(ops) - np.array(ops_min), np.array(ops_max) - np.array(ops)]

# Color palette (dark2-like)
dark2_hex_colors = ['#1b9e77', '#d95f02', '#7570b3', '#e7298a']

# Update font sizes and styles based on previous code
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 10.6,
    'ytick.labelsize': 10.6,
    'legend.fontsize': 11,
    'figure.titlesize': 16,
    'grid.alpha': 0.8,
    'grid.color': '#cccccc',
    'axes.grid': True,
})

static_color_mapping = {
    'ECMP': '#1b9e77',  # Green
    'OPS': '#d95f02',  # Orange
    'BitMap': '#7570b3',  # Blue
    'MPTCP': '#e7298a',  # Pink
    'PLB': '#66a61e',  # Green
    'REPS': '#e6ab02',  # Yellow
    'MPRDMA': '#a6761d',  # Brown
    'Flowlet': '#666666',   # Gray
    'Adaptive RoCE': '#1f78b4'   # Gray
}

# Create the plot
fig, ax = plt.subplots(figsize=(3.6, 3.0))

# Plot bars for REPS and OPS with error bars
bars_ops = ax.bar(x - width/2, ops, width, label='OPS', color=static_color_mapping["OPS"], linewidth=2.2)
bars_reps = ax.bar(x + width/2, reps, width, label='REPS', color=static_color_mapping["REPS"], linewidth=2.2)

# Add error bars (min and max values) with the same color as the bars and thicker error lines
ax.errorbar(x - width/2, ops, yerr=ops_errors, fmt='none', ecolor="black", capsize=5, elinewidth=3)
ax.errorbar(x + width/2, reps, yerr=reps_errors, fmt='none', ecolor="black", capsize=5, elinewidth=3)

# Add a horizontal dashed line at y=100
ax.axhline(y=100, color='gray', linestyle='--', linewidth=1.5)

# Set labels and title
ax.set_ylabel('Avg. Per-Flow Goodput (Gbps)', fontsize=12.8)
ax.set_xlabel('Switch Setup', fontsize=13.5)

# Set x-tick labels with rotation for clarity
ax.set_xticks(x)
ax.set_xticklabels(x_labels)

# Custom grid and legend
plt.ylim(0, 120)
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)

# Save the plot
plt.tight_layout()
plt.savefig('custom1.png', dpi=300, bbox_inches='tight')
plt.savefig('custom1.pdf', dpi=300, bbox_inches='tight')

# Display the plot
plt.show()
