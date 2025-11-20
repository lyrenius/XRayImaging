import os
import glob
from datetime import datetime
from typing import List, Tuple

# Define the directory path and file pattern constants
LOG_DIR = "../logs/batch_testing"
STDOUT_PATTERN = "stdout.*"
STDERR_PATTERN = "stderr."
EXCLUSION_TEXT = "Text file busy"


def is_file_excluded(stdout_filepath: str) -> bool:
    """
    Checks if the corresponding stderr file contains the exclusion text.
    
    Args:
        stdout_filepath: The full path to the stdout log file.

    Returns:
        True if the corresponding stderr file exists and contains the 
        exclusion text, False otherwise.
    """
    # 1. Determine the corresponding stderr filename
    stdout_filename = os.path.basename(stdout_filepath)
    # This assumes a naming convention like stdout.N and stderr.N
    # We strip "stdout." and replace it with "stderr."
    if not stdout_filename.startswith("stdout."):
        # If the filename doesn't follow the expected pattern, we can't reliably find stderr
        return False
        
    stderr_filename = stdout_filename.replace("stdout.", STDERR_PATTERN)
    
    # 2. Construct the full path to the stderr file
    log_directory = os.path.dirname(stdout_filepath)
    stderr_filepath = os.path.join(log_directory, stderr_filename)

    # 3. Check if the stderr file exists and contains the text
    if os.path.exists(stderr_filepath):
        try:
            with open(stderr_filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                if EXCLUSION_TEXT in content:
                    print(f"-> EXCLUDING {stdout_filename}: Found '{EXCLUSION_TEXT}' in {stderr_filename}.")
                    return True
        except FileNotFoundError:
            # Should not happen due to os.path.exists check, but safe to include
            pass
        except Exception as e:
            print(f"Warning: Error reading {stderr_filename} for exclusion check: {e}")
            
    return False


def get_runtime_for_file(filepath: str) -> Tuple[float, str]:
    """
    Reads a file to extract timestamps from line 2 and line 5, 
    calculates the difference (runtime) in milliseconds, and returns it.
    
    Returns:
        A tuple of (runtime_ms, filepath)
        Returns (float('inf'), filepath) if an error occurs.
    """
    # Line indices (0-indexed) for the two required timestamps
    START_LINE_INDEX = 2  # Line 2
    END_LINE_INDEX = 5    # Line 5
    
    # The format string for the timestamp inside the file:
    TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"
    
    timestamps = {} # To store the datetime objects

    try:
        # Read all necessary lines at once
        with open(filepath, 'r') as f:
            lines = f.readlines()

        # 1. Check if the file has enough lines
        if len(lines) < END_LINE_INDEX + 1:
            print(f"Warning: File {os.path.basename(filepath)} is too short (needs at least {END_LINE_INDEX + 1} lines). Skipping.")
            return (float('inf'), filepath)

        # 2. Extract and parse the start and end timestamps
        for index, key in [(START_LINE_INDEX, 'start'), (END_LINE_INDEX, 'end')]:
            line = lines[index].strip()
            
            if line.startswith('['):
                end_index = line.find(']')
                if end_index != -1:
                    timestamp_str = line[1:end_index] 
                else:
                    print(f"Error: Could not find closing bracket on line {index + 1} in {os.path.basename(filepath)}.")
                    return (float('inf'), filepath)
            else:
                print(f"Error: Line {index + 1} does not start with '[' in {os.path.basename(filepath)}.")
                return (float('inf'), filepath)
            
            try:
                dt_object = datetime.strptime(timestamp_str, TIMESTAMP_FORMAT)
                timestamps[key] = dt_object
            except ValueError as e:
                print(f"Error parsing date/time on line {index + 1} in {os.path.basename(filepath)}: {e}.")
                return (float('inf'), filepath)

        # 3. Calculate the Runtime
        start_dt = timestamps['start']
        end_dt = timestamps['end']
        
        # Calculate the timedelta and convert to total milliseconds
        time_delta = end_dt - start_dt
        runtime_ms = time_delta.total_seconds() * 1000.0
        
        return (runtime_ms, filepath)

    except FileNotFoundError:
        print(f"Error: File not found: {os.path.basename(filepath)}. Skipping.")
        return (float('inf'), filepath)
    except Exception as e:
        print(f"An unexpected error occurred while processing {os.path.basename(filepath)}: {e}. Skipping.")
        return (float('inf'), filepath)

# --- Main Execution Block ---

# Construct the full path pattern to search for
search_path = os.path.join(LOG_DIR, STDOUT_PATTERN)

# 1. Use glob.glob to find all matching stdout files
all_stdout_files = glob.glob(search_path)

if not all_stdout_files:
    print(f"\n❌ Error: No files matching '{STDOUT_PATTERN}' found in directory '{LOG_DIR}'.")
    print("Please ensure the directory exists and contains matching log files.")
else:
    print(f"\n--- Processing {len(all_stdout_files)} potential files in {LOG_DIR} ---")
    
    # 2. Calculate the runtime for each valid file
    all_runtimes = []
    
    for f in all_stdout_files:
        # Apply the new exclusion condition
        if is_file_excluded(f):
            continue
            
        runtime_ms, filepath = get_runtime_for_file(f)
        
        # Only include valid runtimes (not float('inf') for errors)
        if runtime_ms != float('inf'):
            all_runtimes.append((runtime_ms, filepath))

    if not all_runtimes:
        print("\n❌ No valid runtimes were calculated after filtering and error checking.")
    else:
        # 3. Sort the results based on the millisecond runtime
        sorted_results = sorted(all_runtimes, key=lambda x: x[0])

        print("\n--- Results Sorted by Internal Runtime (Shortest to Longest) ---")
        print(f"Total valid runs: {len(sorted_results)}")
        print("Filename\t\tRuntime (ms)")
        print("---------------------------------------")

        for runtime_ms, filename in sorted_results:
            # Use os.path.basename() to print just the filename
            print(f"{os.path.basename(filename):<15}\t{runtime_ms:>.3f}")