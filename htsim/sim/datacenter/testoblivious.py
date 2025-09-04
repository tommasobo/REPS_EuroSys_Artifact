import random
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd

input_port = 32
output_port = input_port
ports_queue = [0] * output_port
rounds = 100000
max_v = -1

# Initialize lists to store queue sizes for each output port at each round
queue_sizes_over_time = [[] for _ in range(output_port)]

for round_num in range(rounds):
    # Randomly assign packets to output ports
    for _ in range(input_port):
        select_random_port = random.randint(0, output_port - 1)
        ports_queue[select_random_port] += 1

    # Process one packet from each output port (if any)
    for out_port in range(output_port):
        ports_queue[out_port] = max(ports_queue[out_port] - 1, 0)
        # Record the queue size for this output port
        queue_sizes_over_time[out_port].append(ports_queue[out_port])

    # Track max queue size
    current_max = max(ports_queue)
    if max_v < current_max:
        max_v = current_max

# Convert the list to a DataFrame for Seaborn plotting
df_list = []
for port in range(output_port):
    df_list.extend(pd.DataFrame({
        'Round': range(1, rounds + 1),
        'Queue Size': queue_sizes_over_time[port],
        'Output Port': port
    }).values)

df = pd.DataFrame(df_list, columns=['Round', 'Queue Size', 'Output Port'])

# Plot using Seaborn
plt.figure(figsize=(12, 6))
sns.lineplot(x='Round', y='Queue Size', hue='Output Port', data=df, palette='tab20', legend='full')
plt.title('Queue Size for Each Output Port Over Time')
plt.xlabel('Round')
plt.ylabel('Queue Size')


# Adjust layout to avoid clipping
plt.tight_layout()

# Save the plot with higher DPI for print quality
plt.savefig("result_folder/oblivious_diff_ports3.png", bbox_inches='tight')
plt.savefig("result_folder/oblivious_diff_ports3.pdf", bbox_inches='tight')


plt.show()

# Final results
avg_queue_size = sum(sum(queue_sizes_over_time[port]) for port in range(output_port)) / (rounds * output_port)
print(f"Max queue size: {max_v}")
print(f"Average queue size: {avg_queue_size}")
