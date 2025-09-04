import numpy as np
import matplotlib.pyplot as plt

# Data for the bar plot
x_labels = ['OPS', 'REPS']
reps = [28142/1000]  # Example data for REPS
ops = [1470000/1000]  # Example data for OPS

# Min and Max placeholders (you will replace with actual values)
reps_min = [19000/1000]  # Example min values for REPS
reps_max = [39000/1000]  # Example max values for REPS
ops_min = [903000/1000]  # Example min values for OPS
ops_max = [2200000/1000]  # Example max values for OPS

x = np.arange(len(x_labels))  # The label locations
width = 0.4  # The width of the bars

# Combine the data into one array
values = [ops[0], reps[0]]  # OPS and REPS in a single list

# Calculate error values (difference between max and the value itself, and value and min)
errors = [
    [ops[0] - ops_min[0], reps[0] - reps_min[0]],  # Min errors
    [ops_max[0] - ops[0], reps_max[0] - reps[0]]   # Max errors
]

# Color palette (dark2-like)
dark2_hex_colors = ['#1b9e77', '#d95f02', '#7570b3', '#e7298a']

# Static color mapping (you can update if needed)
static_color_mapping = {
    'OPS': '#d95f02',  # Orange
    'REPS': '#e6ab02',  # Yellow
}

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

# Create the plot
fig, ax = plt.subplots(figsize=(3.85, 3.0))

# Plot single bars for OPS and REPS
bars = ax.bar(x, values, color=[static_color_mapping["OPS"], static_color_mapping["REPS"]], linewidth=2.2)

# Add error bars (min and max values) with black error bars and thicker lines
ax.errorbar(x, values, yerr=errors, fmt='none', ecolor='black', capsize=5, elinewidth=3)

# Add a horizontal dashed line at y=100
#ax.axhline(y=75, color='gray', linestyle='--', linewidth=1.5)

# Set labels and title
ax.set_ylabel(r'# Packet Drops (10$^3$)', fontsize=14)
ax.set_xlabel('Load Balancing Algorithm', fontsize=13.5)

# Set x-tick labels with rotation for clarity
ax.set_xticks(x)
ax.set_xticklabels(x_labels)

# Custom grid and legend
#plt.ylim(0, 100)
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)

# Save the plot
plt.tight_layout()
plt.savefig('custom4.png', dpi=300, bbox_inches='tight')
plt.savefig('custom4.pdf', dpi=300, bbox_inches='tight')

# Display the plot
plt.show()
