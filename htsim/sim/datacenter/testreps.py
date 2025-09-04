import random
import matplotlib.pyplot as plt
import pandas as pd

# Parameters
input_port = 32
output_port = input_port
ports_queue = [0] * output_port
rounds = 5000
max_v = -1
threshold = 100  # Threshold value for deciding random or fixed routing

# Initialize lists to store queue sizes for each output port at each round
queue_sizes_over_time = [[] for _ in range(output_port)]
max_queue_over_time = []

# Function to check if the queue is above the threshold
def check_threshold(queue_size, threshold):
    return queue_size > threshold

# Simulation with threshold-based load balancing
for round_num in range(rounds):
    # Remove one packet from each output port (if any)
    for out_port in range(output_port):
        if ports_queue[out_port] > 0:
            ports_queue[out_port] -= 1

        # Check if the queue size is above the threshold
        if ports_queue[out_port] > 0 and check_threshold(ports_queue[out_port], threshold):
            # Queue above the threshold, route next packet randomly
            next_packet_port = random.randint(0, output_port - 1)
        elif ports_queue[out_port] == 0:
            # If the queue is empty, send randomly
            next_packet_port = random.randint(0, output_port - 1)
        else:
            # Queue below the threshold, route to the same output port
            next_packet_port = out_port

        # Send the next packet to the selected output port
        ports_queue[next_packet_port] += 1

        # Record the queue size for this output port
        queue_sizes_over_time[out_port].append(ports_queue[out_port])

    # Track max queue size at this round across all ports
    current_max = max(ports_queue)
    max_queue_over_time.append(current_max)
    if max_v < current_max:
        max_v = current_max

plt.figure(figsize=(12, 8))

for port in range(output_port):
    plt.plot(range(rounds), queue_sizes_over_time[port], label=f'Output Port {port}')

plt.title('Queue Size for Each Output Port Over Time')
plt.xlabel('Round')
plt.ylabel('Queue Size')
plt.legend()

# Adjust layout to avoid clipping
plt.tight_layout()

# Save the plot with higher DPI for print quality
plt.savefig("result_folder/reps_diff_ports2.png", bbox_inches='tight')
plt.savefig("result_folder/reps_diff_ports2.pdf", bbox_inches='tight')

plt.show()

# Final results
avg_queue_size = sum(sum(queue_sizes_over_time[port]) for port in range(output_port)) / (rounds * output_port)
print(f"Max queue size: {max_v}")
print(f"Average queue size: {avg_queue_size}")
