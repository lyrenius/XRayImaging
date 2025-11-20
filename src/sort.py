import os
import glob
from datetime import datetime
from typing import List, Tuple, Optional

# Define the directory path and file pattern constants
LOG_DIR = "../logs/batch_testing"
STDOUT_PATTERN = "stdout.*"
STDERR_PATTERN = "stderr."
EXCLUSION_TEXT = "Text file busy"
# The maximum length to print for the stderr first line
MAX_STDERR_PREVIEW_LEN = 60 


def get_stderr_preview(stdout_filepath: str) -> Optional[str]:
    """
    Finds the corresponding stderr file and returns its first line.
    
    Returns:
        The first line of the stderr file (or None if not found/error).
    """
    # 1. Determine the corresponding stderr filename
    stdout_filename = os.path.basename(stdout_filepath)
    if not stdout_filename.startswith("stdout."):
        return None
        
    stderr_filename = stdout_filename.replace("stdout.", STDERR_PATTERN)
    
    # 2. Construct the full path to the stderr file
    log_directory = os.path.dirname(stdout_filepath)
    stderr_filepath = os.path.join(log_directory, stderr_filename)

    # 3. Read the first line of the stderr file
    if os.path.exists(stderr_filepath):
        try:
            with open(stderr_filepath, 'r', encoding='utf-8', errors='ignore') as f:
                first_line = f.readline().strip()
                # Truncate for clean output
                if len(first_line) > MAX_STDERR_PREVIEW_LEN:
                    return first_line[:MAX_STDERR_PREVIEW_LEN - 3] + "..."
                return first_line
        except Exception as e:
            # Handle potential reading errors but return None
            # print(f"Warning: Error reading {stderr_filename}: {e}")
            return f"[ERROR reading file: {e}]"
            
    return "[No corresponding stderr file]"


def is_file_excluded(stdout_filepath: str) -> bool:
    """
    Checks if the corresponding stderr file contains the exclusion text.
    """
    stdout_filename = os.path.basename(stdout_filepath)
    if not stdout_filename.startswith("stdout."):
        return False
        
    stderr_filename = stdout_filename.replace("stdout.", STDERR_PATTERN)
    log_directory = os.path.dirname(stdout_filepath)
    stderr_filepath = os.path.join(log_directory, stderr_filename)

    if os.path.exists(stderr_filepath):
        try:
            with open(stderr_filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                if EXCLUSION_TEXT in content:
                    print(f"-> EXCLUDING {stdout_filename}: Found '{EXCLUSION_TEXT}' in {stderr_filename}.")
                    return True
        except Exception:
            # Assume not excluded if there's a read error
            pass
            
    return False


def get_runtime_for_file(filepath: str) -> Tuple[float, str]:
    """
    Reads a file to extract timestamps from line 2 and line 5, 
    calculates the difference (runtime) in milliseconds, and returns it.
    
    Returns:
        A tuple of (runtime_ms, filepath)
        Returns (float('inf'), filepath) if an error occurs.
    """
    START_LINE_INDEX = 2  # Line 2
    END_LINE_INDEX = 5    # Line 5
    TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S.%f"
    timestamps = {} 

    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()

        if len(lines) < END_LINE_INDEX + 1:
            print(f"Warning: File {os.path.basename(filepath)} is too short. Skipping.")
            return (float('inf'), filepath)

        # Extract and parse start/end timestamps
        for index, key in [(START_LINE_INDEX, 'start'), (END_LINE_INDEX, 'end')]:
            line = lines[index].strip()
            if line.startswith('['):
                end_index = line.find(']')
                timestamp_str = line[1:end_index] 
            else:
                print(f"Error: Line {index + 1} format incorrect in {os.path.basename(filepath)}. Skipping.")
                return (float('inf'), filepath)
            
            timestamps[key] = datetime.strptime(timestamp_str, TIMESTAMP_FORMAT)

        # Calculate the Runtime
        time_delta = timestamps['end'] - timestamps['start']
        runtime_ms = time_delta.total_seconds() * 1000.0
        
        return (runtime_ms, filepath)

    except Exception as e:
        print(f"An error occurred while processing {os.path.basename(filepath)}: {e}. Skipping.")
        return (float('inf'), filepath)

# --- Main Execution Block ---

search_path = os.path.join(LOG_DIR, STDOUT_PATTERN)
all_stdout_files = glob.glob(search_path)

if not all_stdout_files:
    print(f"\n❌ Error: No files matching '{STDOUT_PATTERN}' found in directory '{LOG_DIR}'.")
else:
    print(f"\n--- Processing {len(all_stdout_files)} potential files in {LOG_DIR} ---")
    
    # List to store (runtime_ms, filepath, stderr_preview)
    all_results = []
    
    for f in all_stdout_files:
        # 1. Apply the exclusion condition
        if is_file_excluded(f):
            continue
            
        # 2. Calculate runtime
        runtime_ms, filepath = get_runtime_for_file(f)
        
        # 3. Retrieve stderr preview
        stderr_preview = get_stderr_preview(filepath)
        
        # Only include valid runtimes
        if runtime_ms != float('inf'):
            all_results.append((runtime_ms, filepath, stderr_preview))

    if not all_results:
        print("\n❌ No valid runtimes were calculated after filtering and error checking.")
    else:
        # 4. Sort the results based on the millisecond runtime (shortest to longest)
        sorted_results = sorted(all_results, key=lambda x: x[0])

        print("\n--- Results Sorted by Internal Runtime (Shortest to Longest) ---")
        print(f"Total valid runs: {len(sorted_results)}")
        print("-" * 100)
        # Use simple print statements for columns due to variable length of stderr text
        print(f"{'Filename':<15} | {'Runtime (ms)':>15} | First Stderr Line (Max {MAX_STDERR_PREVIEW_LEN} chars)")
        print("-" * 100)

        for runtime_ms, filename, stderr_preview in sorted_results:
            # Use os.path.basename() to print just the filename
            basename = os.path.basename(filename)
            print(f"{basename:<15} | {runtime_ms:>15.3f} | {stderr_preview}")
        
        print("-" * 100)