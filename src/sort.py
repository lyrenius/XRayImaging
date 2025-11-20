import os
import glob
from datetime import datetime
from typing import List, Tuple

def calculate_time_differences(file_list: List[str]) -> List[Tuple[str, float]]:
    """
    Reads the first timestamp from each file, calculates the millisecond
    difference between consecutive files, and returns a list of (filename, difference).
    
    The difference for the very first file is calculated against a zero baseline (0.0).
    """
    if not file_list:
        return []

    # The format string for the timestamp inside the file:
    TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"
    
    # List to store (timestamp, filename)
    timestamp_data = []

    print(f"--- Processing {len(file_list)} files ---")

    for filename in file_list:
        try:
            with open(filename, 'r') as f:
                # Read only the first line
                first_line = f.readline().strip()
            
            # Extract the timestamp string
            if first_line.startswith('['):
                end_index = first_line.find(']')
                if end_index != -1:
                    # e.g., "2025-11-20 17:10:08.554"
                    timestamp_str = first_line[1:end_index] 
                else:
                    print(f"Warning: Could not find closing bracket in file {filename}. Skipping.")
                    continue
            else:
                print(f"Warning: First line in file {filename} does not start with '['. Skipping.")
                continue

            # Parse the timestamp
            try:
                dt_object = datetime.strptime(timestamp_str, TIMESTAMP_FORMAT)
                timestamp_data.append((dt_object, filename))
            except ValueError as e:
                print(f"Error parsing date/time in file {filename} ('{timestamp_str}'): {e}. Skipping.")
                continue

        except FileNotFoundError:
            print(f"Error: File not found: {filename}. Skipping.")
        except Exception as e:
            print(f"An unexpected error occurred while processing {filename}: {e}. Skipping.")


    # Sort the files by their **actual** timestamp chronologically
    timestamp_data.sort(key=lambda x: x[0])
    
    # List to store (filename, difference in ms)
    differences_ms = []
    
    previous_dt = None

    for i, (current_dt, filename) in enumerate(timestamp_data):
        if i == 0:
            # The first chronologically sorted file has a 0.0 ms delta
            difference_ms = 0.0
        else:
            # Calculate the time difference (timedelta object)
            time_delta = current_dt - previous_dt
            
            # Convert the timedelta to total milliseconds
            difference_ms = time_delta.total_seconds() * 1000.0
            
        differences_ms.append((filename, difference_ms))
        
        # Update the previous timestamp for the next iteration
        previous_dt = current_dt

    return differences_ms

# --- Main Execution Block ---

# 1. Define the directory path relative to the script's location
LOG_DIR = "../logs/batch_testing"
FILE_PATTERN = "stdout.*"

# Construct the full path pattern to search for
search_path = os.path.join(LOG_DIR, FILE_PATTERN)

# 2. Use glob.glob to find all matching files
input_files = glob.glob(search_path)

if not input_files:
    print(f"\n❌ Error: No files matching '{FILE_PATTERN}' found in directory '{LOG_DIR}'.")
    print("Please ensure the directory exists and contains matching log files.")
else:
    # 3. Calculate the differences
    results = calculate_time_differences(input_files)

    # 4. Sort the results based on the millisecond difference (the delta)
    sorted_results = sorted(results, key=lambda x: x[1])

    print("\n--- Results Sorted by Millisecond Difference (Delta between chronological files) ---")
    print("Filename\t\tDifference (ms)")
    print("---------------------------------------")

    for filename, diff_ms in sorted_results:
        # Use os.path.basename() to print just the filename, not the full path
        # Use f-string formatting to align and limit decimal places
        print(f"{os.path.basename(filename):<15}\t{diff_ms:>.3f}")