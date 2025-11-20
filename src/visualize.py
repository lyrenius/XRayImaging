import matplotlib.pyplot as plt
import numpy as np

# Define the expected size of the 2D data
LEN = 512

# Initialize the list to store the 2D data
data: list[list[float]] = []

# --- 1. Read the Data ---
try:
    with open("data/detected_ratio.txt", "r") as f:
        for i in range(LEN):
            # Read a line, strip whitespace, and split by spaces
            line = f.readline().strip()
            if not line:
                # Handle case where file ends unexpectedly
                print(f"Warning: File ended after reading {i} rows, expected {LEN}.")
                break
                
            # Convert the space-separated strings to floats
            ratio_row = list(map(float, line.split()))
            
            # Append the row to the data list
            data.append(ratio_row)

except FileNotFoundError:
    print("Error: The file 'data/detection_info.txt' was not found. Please ensure the path is correct.")
    exit()
except Exception as e:
    print(f"An error occurred during file processing: {e}")
    exit()

# --- 2. Convert to NumPy Array for Plotting ---
# Matplotlib prefers NumPy arrays for efficient plotting, especially for image-like data.
data_array = np.array(data)

# --- 3. Plot the Heat Map ---
plt.figure(figsize=(8, 6)) # Set the size of the plot

# Use the imshow function to create a 2D heat map.
# The data is treated as an image where the value of each element maps to a color.
image = plt.imshow(data_array, 
                   cmap='Reds',  # 'viridis' is a common and perceptually uniform colormap
                   vmax=8.0,      # Set maximum data value for color scaling
                   interpolation='nearest', # <-- Ensures blocky, distinct data points
                   aspect='equal')         # <-- Ensures data points look square

# Add a color bar to show the mapping between data values and colors
plt.colorbar(image, label='Data Value')

# Add titles and labels
plt.title('2D Heat Map of Detection Data')
plt.xlabel('Column Index')
plt.ylabel('Row Index')

# Display the plot
plt.savefig('data/detected_ratio.png')

print(f"Successfully read and plotted a 2D array of shape: {data_array.shape}")