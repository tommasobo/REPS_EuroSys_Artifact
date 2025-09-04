import random
import matplotlib.pyplot as plt
import numpy as np

# Set random seed for reproducibility
seed_m = 408
random.seed(seed_m)
np.random.seed(seed_m)

# Parameters
input_port = 8
output_port = input_port
rounds = 2000
initial_rounds_random_routing = 1  # Number of rounds where REPS behaves randomly
threshold = 10  # Threshold for REPS

# Initialize lists to store queue sizes over time
queue_sizes_oblivious = [[] for _ in range(output_port)]
max_queue_oblivious = []
tot_sent_oblivious = 0
tot_removed_oblivious = 0

# Function to simulate oblivious spraying
def simulate_oblivious():
    global tot_sent_oblivious, tot_removed_oblivious

    # Initialize queue for oblivious load balancer
    ports_queue_oblivious = [0] * output_port

    for round_num in range(rounds):

        # Process one packet from each output port (if any)
        if round_num > 0:
            for out_port in range(output_port):
                if ports_queue_oblivious[out_port] > 0:
                    tot_removed_oblivious += 1
                ports_queue_oblivious[out_port] = max(ports_queue_oblivious[out_port] - 1, 0)

        # Oblivious: Randomly assign packets to output ports
        for _ in range(input_port):
            select_random_port = random.randint(0, output_port - 1)
            ports_queue_oblivious[select_random_port] += 1
            tot_sent_oblivious += 1

        # Track queue sizes for oblivious
        for out_port in range(output_port):
            queue_sizes_oblivious[out_port].append(ports_queue_oblivious[out_port])

        # Track max queue size for oblivious
        max_queue_oblivious.append(max(ports_queue_oblivious))

# Function to simulate REPS load balancer with consistent packet removal
def simulate_reps(reps_every):
    # Initialize/reset REPS variables
    ports_queue_reps = [0] * output_port
    queue_sizes_reps = [[] for _ in range(output_port)]
    max_queue_reps = []
    tot_sent_reps = 0
    tot_removed_reps = 0
    local_round = 0

    for round_num in range(rounds):
        decisions = []
        direction = []

        # First, remove one packet from each output port if there's any
        if round_num > 0:
            for out_port in range(output_port):
                if ports_queue_reps[out_port] > 0:
                    tot_removed_reps += 1
                ports_queue_reps[out_port] = max(ports_queue_reps[out_port] - 1, 0)

        # Make decisions for REPS routing
        for out_port in range(output_port):
            if round_num < initial_rounds_random_routing or local_round % reps_every != 0:
                # Route randomly if in the initial random rounds or in non-REPS rounds
                next_packet_port = random.randint(0, output_port - 1)
                decisions.append("Random")
                direction.append(next_packet_port)
            else:
                # Threshold-based routing decision
                if ports_queue_reps[out_port] >= threshold:
                    next_packet_port = random.randint(0, output_port - 1)  # Route randomly
                    decisions.append("RandomTH")
                    direction.append(next_packet_port)
                elif ports_queue_reps[out_port] == 0:
                    next_packet_port = random.randint(0, output_port - 1)  # Send randomly
                    decisions.append("Random")
                    direction.append(next_packet_port)
                else:
                    next_packet_port = out_port  # Route to the same output port
                    decisions.append("Same")
                    direction.append(next_packet_port)

        # Send the next packet to the selected output port
        for out_port in range(output_port):
            ports_queue_reps[direction[out_port]] += 1
            tot_sent_reps += 1

        # Record the queue size for this output port
        for out_port in range(output_port):
            queue_sizes_reps[out_port].append(ports_queue_reps[out_port])

        # Track max queue size for REPS
        max_queue_reps.append(max(ports_queue_reps))

        local_round += 1

    return queue_sizes_reps, max_queue_reps

# Run the oblivious simulation once
simulate_oblivious()

# Plotting the results
fig, axes = plt.subplots(3, 1, figsize=(5.5, 5.0))  # Create a 3x1 grid of plots
reps_every_values = [2, 4, 8]
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

# Set font sizes
plt.rcParams.update({
    'axes.titlesize': 13.2,
    'axes.labelsize': 18,
    'xtick.labelsize': 25,
    'ytick.labelsize': 25,
    'legend.fontsize': 15,
    'figure.titlesize': 15
})

for i, reps_every in enumerate(reps_every_values):
    queue_sizes_reps, max_queue_reps = simulate_reps(reps_every)
    
    # Plot for oblivious packet spraying
    for port in range(output_port):
        axes[i].plot(range(rounds), queue_sizes_oblivious[port], 
                     label=f'Oblivious Output Port {port}', 
                     color=dark2_hex_colors[1], linestyle='--', linewidth=2.5)
    
    # Plot for REPS load balancer
    for port in range(output_port):
        axes[i].plot(range(rounds), queue_sizes_reps[port], 
                     label=f'REPS (reps_every={reps_every}) Output Port {port}', 
                     color=dark2_hex_colors[0], linestyle='-', linewidth=2.5)

    # Plot threshold as a horizontal dotted line
    axes[i].axhline(y=threshold, color='k', linestyle=':', linewidth=3, label=f'Threshold {threshold}')
    
    # Set title for each subplot
    axes[i].set_title(f"Recycled Ball every {reps_every} ACKs", fontsize=14.5)
    
    # Keep x-axis label only for the last (bottom) plot
    if i == len(reps_every_values) - 1:
        axes[i].set_xlabel('Balls-into-bins Round', fontsize=13.5)

    axes[i].tick_params(axis='x', labelsize=13)  # Increase x-tick label size
    axes[i].tick_params(axis='y', labelsize=13)  # Increase x-tick label size
    axes[i].grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Set a common y-axis label for all plots
fig.text(0.04, 0.5, 'Queue Size (Pkts)', va='center', rotation='vertical', fontsize=13.5)

# Adjust layout to avoid overlap
plt.tight_layout(rect=[0.05, 0, 1, 1])  # Adjust layout to fit common y-axis label
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Save the plot (ensure the folder exists or comment out these lines)
plt.savefig("result_folder/reps_every_comparison.png", bbox_inches='tight')
plt.savefig("result_folder/reps_every_comparison.pdf", bbox_inches='tight')

plt.show()

# Final results
max_queue_oblivious_overall = max(max_queue_oblivious)
avg_queue_oblivious = sum(sum(queue_sizes_oblivious[port]) for port in range(output_port)) / (rounds * output_port)

print(f"Oblivious Max Queue Size: {max_queue_oblivious_overall}")
print(f"Oblivious Average Queue Size: {avg_queue_oblivious}")
