import random
import matplotlib.pyplot as plt
import numpy as np
import matplotlib
import concurrent.futures
import os

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# Ensure output directories exist
for directory in [
    "../artifact_results",
    "../artifact_results/fig_14_ballsbins",
    "../artifact_results/fig_14_ballsbins/data",
    "../artifact_results/fig_14_ballsbins/plots"
]:
    os.makedirs(directory, exist_ok=True)
# Set random seed for reproducibility
seed_m = 408
random.seed(seed_m)
np.random.seed(seed_m)

# Parameters
input_port = 8
output_port = input_port
rounds = 200
initial_rounds_random_routing = 2  # Number of rounds where REPS behaves randomly
threshold = 5  # Threshold for REPS
reps_every = 1  # Number of rounds to run REPS

tot_sent_oblivious = 0
tot_sent_reps = 0
tot_removed_reps = 0
tot_removed_oblivious = 0

# Initialize queues for oblivious and REPS load balancers
ports_queue_oblivious = [0] * output_port
ports_queue_reps = [0] * output_port

# Initialize lists to store queue sizes over time
queue_sizes_oblivious = [[] for _ in range(output_port)]
queue_sizes_reps = [[] for _ in range(output_port)]
max_queue_oblivious = []
max_queue_reps = []

# Function to simulate oblivious spraying
def simulate_oblivious():
    global tot_sent_oblivious, tot_removed_oblivious  # Use global variables
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

# Function to simulate REPS load balancer
def simulate_reps():

    local_round = 0
    global tot_sent_reps, tot_removed_reps  # Use global variables
    for round_num in range(rounds):
        decisions = []
        direction = []
        
        for out_port in range(output_port):
            # During the initial rounds, route randomly for REPS
            if round_num < initial_rounds_random_routing or local_round % reps_every != 0:
                next_packet_port = random.randint(0, output_port - 1)  # Send randomly
                decisions.append("Random")
                direction.append(next_packet_port)
            else:
                # After the initial rounds, use threshold-based decision-making
                if ports_queue_reps[out_port] >= threshold:
                    next_packet_port = random.randint(0, output_port - 1)  # Route randomly
                    decisions.append("RandomTH")
                    direction.append(next_packet_port)
                    if ports_queue_reps[out_port] > 0:
                        tot_removed_reps += 1
                    ports_queue_reps[out_port] = max(ports_queue_reps[out_port] - 1, 0)
                elif ports_queue_reps[out_port] == 0:
                    next_packet_port = random.randint(0, output_port - 1)  # Send randomly
                    decisions.append("Random")
                    direction.append(next_packet_port)
                elif ports_queue_reps[out_port] < threshold:
                    next_packet_port = out_port  # Route to the same output port
                    decisions.append("Same")
                    direction.append(next_packet_port)
                    if ports_queue_reps[next_packet_port] > 0:
                        tot_removed_reps += 1
                    ports_queue_reps[next_packet_port] = max(ports_queue_reps[next_packet_port] - 1, 0)

        # Send next packet to the selected output port
        for out_port in range(output_port):
            ports_queue_reps[direction[out_port]] += 1
            tot_sent_reps += 1


            if round_num < initial_rounds_random_routing and round_num > 0:
                if (ports_queue_reps[out_port] > 0):
                    tot_removed_reps += 1
                ports_queue_reps[out_port] = max(ports_queue_reps[out_port] - 1, 0)
            elif (local_round % reps_every != 0):
                if (ports_queue_reps[out_port] > 0):
                    tot_removed_reps += 1
                ports_queue_reps[out_port] = max(ports_queue_reps[out_port] - 1, 0)

            # Record the queue size for this output port
        for out_port in range(output_port):
            queue_sizes_reps[out_port].append(ports_queue_reps[out_port])
        
        # Track max queue size for REPS
        max_queue_reps.append(max(ports_queue_reps))

        local_round += 1

# Run simulations
simulate_oblivious()
simulate_reps()

# Plotting the results
plt.figure(figsize=(7, 2.6))  # Adjusted the height to 7

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

# Set font sizes (similar to the oblivious plot settings)
plt.rcParams.update({
    'axes.titlesize': 16,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 12,
    'figure.titlesize': 18
})

# Plot for oblivious packet spraying
for port in range(output_port):
    plt.plot(range(rounds), queue_sizes_oblivious[port], 
             label=f'Oblivious Output Port {port}', 
             color=dark2_hex_colors[1], linestyle='--', linewidth=2.5)

# Plot for REPS load balancer
for port in range(output_port):
    plt.plot(range(rounds), queue_sizes_reps[port], 
             label=f'REPS Output Port {port}', 
             color=dark2_hex_colors[0], linestyle='-', linewidth=2.5)

# Plot threshold as a horizontal dotted line
plt.axhline(y=threshold, color='k', linestyle=':', linewidth=3, label=f'Threshold {threshold}')

plt.xlabel('Balls-into-bins Round')
plt.ylabel('Queue Size (Pkts)')
plt.legend(ncol=2)
plt.grid(True, which='both', linestyle=':', linewidth=0.75, alpha=0.75)  # Dotted lines with reduced visibility

# Adjust layout to avoid clipping
plt.tight_layout()
plt.legend('', frameon=False)

# Save the plot (ensure the folder exists or comment out these lines)
plt.savefig("../artifact_results/fig_14_ballsbins/plots/oblivious_vs_reps_balls_bins_rounds.png", bbox_inches='tight')
plt.savefig("../artifact_results/fig_14_ballsbins/plots/oblivious_vs_reps_balls_bins_rounds.pdf", bbox_inches='tight')


# Final results
max_queue_oblivious_overall = max(max_queue_oblivious)
avg_queue_oblivious = sum(sum(queue_sizes_oblivious[port]) for port in range(output_port)) / (rounds * output_port)

max_queue_reps_overall = max(max_queue_reps)
avg_queue_reps = sum(sum(queue_sizes_reps[port]) for port in range(output_port)) / (rounds * output_port)

""" print(f"Oblivious Max Queue Size: {max_queue_oblivious_overall}")
print(f"Oblivious Average Queue Size: {avg_queue_oblivious}")

print(f"REPS Max Queue Size: {max_queue_reps_overall}")
print(f"REPS Average Queue Size: {avg_queue_reps}")

print(f"Total packets sent by Oblivious: {tot_sent_oblivious}")
print(f"Total packets sent by REPS: {tot_sent_reps}")
print(f"Total packets removed by Oblivious: {tot_removed_oblivious}")
print(f"Total packets removed by REPS: {tot_removed_reps}")
 """