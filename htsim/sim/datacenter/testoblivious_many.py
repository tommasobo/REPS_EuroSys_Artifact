import numpy as np
import matplotlib.pyplot as plt

# Optimized simulation for packet spraying with batch processing
def simulate_packet_spraying_optimized(num_ports, rounds, batch_size=5000):
    # Initialize queues for each output port
    queues = np.zeros(num_ports)
    max_queue_sizes = []

    for batch in range(0, rounds, batch_size):
        for _ in range(batch_size):
            # Send m packets (one for each input port) to random output ports
            random_ports = np.random.randint(0, num_ports, size=num_ports)
            np.add.at(queues, random_ports, 0.99)

            # Remove one packet from each output port if it exists
            queues = np.maximum(queues - 1, 0)
        
        # Record the maximum queue size after each batch
        max_queue_sizes.append(np.max(queues))
    
    return max_queue_sizes

# Running multiple trials to get the average maximum queue size
def simulate_packet_spraying_multiple_trials(num_ports, rounds, trials=44, batch_size=50):
    all_max_queue_sizes = np.zeros((trials, rounds // batch_size))
    
    for trial in range(trials):
        #print(f"Running trial {trial} for {num_ports} ports")
        max_queue_sizes = simulate_packet_spraying_optimized(num_ports, rounds, batch_size=batch_size)
        all_max_queue_sizes[trial, :] = max_queue_sizes
    
    # Calculate the average of the maximum queue sizes over all trials
    avg_max_queue_sizes = np.mean(all_max_queue_sizes, axis=0)
    
    return avg_max_queue_sizes

# Simulation parameters
rounds = 1000  # Number of rounds to simulate
trials = 500  # Number of trials to average over
port_numbers = [4, 8, 16, 32, 64, 128]  # Different numbers of input/output ports

# Plot the average maximum queue size over time for different port numbers
plt.figure(figsize=(7, 2.65))  # Adjusted the height to 7
# Set font sizes
plt.rcParams.update({
    'axes.titlesize': 14.5,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 11.5,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Define custom color palette
dark2_hex_colors = [
    '#1b9e77',  # Green
    '#d95f02',  # Orange
    '#7570b3',  # Purple
    '#e7298a',  # Pink
    '#66a61e',  # Light Green
    '#e6ab02',  # Yellow
    '#a6761d',  # Brown
    '#666666'   # Gray
]

# Define marker styles
markers = ['o', 's', 'D', '^', 'v', 'p', '*']
# Define line styles
line_styles = ['-']

for i, num_ports in enumerate(port_numbers):
    avg_max_queue_sizes = simulate_packet_spraying_multiple_trials(num_ports, rounds, trials)
    plt.plot(np.linspace(0, rounds, len(avg_max_queue_sizes)), avg_max_queue_sizes, 
             label=f'{num_ports} output ports', linewidth=2.5, 
             color=dark2_hex_colors[i % len(dark2_hex_colors)], 
             marker=markers[i % len(markers)], 
             linestyle=line_styles[i % len(line_styles)], 
             markersize=8,  # Increase marker size for better visibility
             markevery=int(len(avg_max_queue_sizes) / 5))  # Show marker every 5% of points

plt.xlabel('Balls-into-bins Round')
plt.ylabel('Avg. Max. Queue\nSize (Pkts)')
plt.legend(ncol=2, loc='upper left')
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Adjust layout to avoid clipping
plt.tight_layout()

# Save the plot with higher DPI for print quality
plt.savefig("result_folder/oblivious_diff_ports3.png", bbox_inches='tight')
plt.savefig("result_folder/oblivious_diff_ports3.pdf", bbox_inches='tight')

plt.show()
