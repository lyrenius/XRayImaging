import os
import glob
from datetime import datetime
from typing import List, Tuple

def get_runtime_for_file(filepath: str) -> Tuple[float, str]:
    """
    Reads a file to extract timestamps from line 2 and line 5, 
    calculates the difference (runtime) in milliseconds, and returns it.
    
    Returns:
        A tuple of (runtime_ms, filepath)
        Returns (float('inf'), filepath) if an error occurs.
    """
    # Line indices (0-indexed) for the two required timestamps
    START_LINE_INDEX = 2  # Line 2: [2025-11-20 17:10:08.554] Running...
    END_LINE_INDEX = 5    # Line 5: [2025-11-20 17:10:08.607] Checking...
    
    # The format string for the timestamp inside the file:
    TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"
    
    timestamps = {} # To store the datetime objects

    try:
        # Read all necessary lines at once
        with open(filepath, 'r') as f:
            lines = f.readlines()

        # 1. Check if the file has enough lines
        if len(lines) < END_LINE_INDEX + 1:
            print(f"Warning: File {filepath} is too short (needs at least {END_LINE_INDEX + 1} lines). Skipping.")
            return (float('inf'), filepath)

        # 2. Extract and parse the start and end timestamps
        for index, key in [(START_LINE_INDEX, 'start'), (END_LINE_INDEX, 'end')]:
            line = lines[index].strip()
            
            if line.startswith('['):
                end_index = line.find(']')
                if end_index != -1:
                    timestamp_str = line[1:end_index] 
                else:
                    print(f"Error: Could not find closing bracket on line {index + 1} in {filepath}.")
                    return (float('inf'), filepath)
            else:
                print(f"Error: Line {index + 1} does not start with '[' in {filepath}.")
                return (float('inf'), filepath)
            
            try:
                timestamps[key] = datetime.strptime(timestamp_str, TIMESTAMP_FORMAT)
            except ValueError as e:
                print(f"Error parsing date/time on line {index + 1} in {filepath}: {e}.")
                return (float('inf'), filepath)

        # 3. Calculate the Runtime
        start_dt = timestamps['start']
        end_dt = timestamps['end']
        
        # Ensure the end time is after the start time
        if end_dt < start_dt:
             print(f"Warning: End time ({end_dt}) is before start time ({start_dt}) in {filepath}. Runtime will be negative.")
        
        time_delta = end_dt - start_dt
        
        # Convert the timedelta to total milliseconds
        runtime_ms = time_delta.total_seconds() * 1000.0
        
        return (runtime_ms, filepath)

    except FileNotFoundError:
        print(f"Error: File not found: {filepath}. Skipping.")
        return (float('inf'), filepath)
    except Exception as e:
        print(f"An unexpected error occurred while processing {filepath}: {e}. Skipping.")
        return (float('inf'), filepath)

# --- Main Execution Block ---

# 1. Define the directory path and file pattern
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
    print(f"\n--- Processing {len(input_files)} files in {LOG_DIR} ---")
    
    # 3. Calculate the runtime for each file
    # results is a list of tuples: [(runtime_ms, filepath), ...]
    all_runtimes = []
    for f in input_files:
        runtime_ms, filepath = get_runtime_for_file(f)
        # Only include valid runtimes (not float('inf') for errors)
        if runtime_ms != float('inf'):
            all_runtimes.append((runtime_ms, filepath))

    if not all_runtimes:
        print("\n❌ No valid runtimes were calculated from the files.")
    else:
        # 4. Sort the results based on the millisecond runtime (the first element of the tuple)
        # This sorts from shortest runtime to longest runtime (ascending).
        sorted_results = sorted(all_runtimes, key=lambda x: x[0])

        print("\n--- Results Sorted by Internal Runtime (Shortest to Longest) ---")
        print("Filename\t\tRuntime (ms)")
        print("---------------------------------------")

        for runtime_ms, filename in sorted_results:
            # Use os.path.basename() to print just the filename
            print(f"{os.path.basename(filename):<15}\t{runtime_ms:>.3f}")