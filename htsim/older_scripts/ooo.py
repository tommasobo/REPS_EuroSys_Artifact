import os
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Define the folder path
folder_path = "../sim/datacenter/psn_over_time/"  # Update this with your actual folder path

# Initialize a list to store the data from all files
data = []

# Read the data from all the files in the folder
for file_name in os.listdir(folder_path):
    if file_name.endswith('.txt'):  # Assuming the files have a .txt extension
        file_path = os.path.join(folder_path, file_name)
        diff_sequence_numbers = []
        with open(file_path, 'r') as file:
            packet_distances = []
            for line in file:
                try:
                    value = int(line.strip())
                    packet_distances.append(value)
                except ValueError:
                    print(f"Skipping non-numeric value: {line} in file: {file_name}")
                    continue
            
            # Calculate the difference between packet distances and perfect sequence
            perfect_sequence = list(range(len(packet_distances)))
            for idx, val in enumerate(perfect_sequence):
                diff_sequence_numbers.append(abs(packet_distances[idx] - val))
            
            # Print max out-of-order distance for debugging
            print(f"Max out-of-order distance in {file_name}: {max(diff_sequence_numbers)}")
            
            # Append the differences to the data list
            data.append(diff_sequence_numbers)

# Convert the list of lists into a DataFrame
df = pd.DataFrame(data)

# Compute statistics
average_distances = df.mean(axis=0)
median_distances = df.median(axis=0)
min_distances = df.min(axis=0)
max_distances = df.max(axis=0)

# Smooth the average and median lines
smoothed_average = average_distances.rolling(window=10, min_periods=1).mean()
smoothed_median = median_distances.rolling(window=10, min_periods=1).mean()

# Visualize the average and median out-of-order distance for each packet
plt.figure(figsize=(10, 6))

# Plot the smoothed average out-of-order distance (solid line, thicker)
plt.plot(smoothed_average.index, smoothed_average, linestyle='-', linewidth=2.5, color='blue', label='Smoothed Average')

# Plot the smoothed median out-of-order distance (dashed line, thicker)
plt.plot(smoothed_median.index, smoothed_median, linestyle='--', linewidth=2.5, color='blue', label='Smoothed Median')

# Fill the area between min and max with a slightly different color and mark border
plt.fill_between(average_distances.index, min_distances, max_distances, color='lightblue', alpha=0.3, edgecolor='darkblue', linewidth=0.5, label='Min-Max Range')

# Highlight the max out-of-order distance
plt.scatter(max_distances.idxmax(), max_distances.max(), color='red', label='Max Value')

plt.title('Smoothed Average and Median Out of Order Distance with Min-Max Range')
plt.xlabel('Packet Position')
plt.ylabel('Out of Order Distance')
plt.legend()
plt.grid(True)

# Save the plot to file
output_folder = "output_plots"  # Define the output folder for saving plots
os.makedirs(output_folder, exist_ok=True)  # Create the folder if it doesn't exist

# Save the plot as PNG and PDF with high DPI for quality
plt.savefig(os.path.join(output_folder, "out_of_order_distance_reps_asy.png"), dpi=300)
plt.savefig(os.path.join(output_folder, "out_of_order_distance_reps_asy.pdf"), dpi=300)

# Show the plot
plt.show()
