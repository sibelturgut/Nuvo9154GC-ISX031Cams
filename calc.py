import re
import statistics
from collections import defaultdict
from decimal import Decimal

def analyze_camera_logs(log_file_path):
    # Dictionary to group timestamps by frame number
    # Key: frame_number (int), Value: list of timestamps (float)
    frame_groups = defaultdict(list)

    # Regex pattern to capture the frame number and the exact floating-point timestamp
    log_pattern = re.compile(r"Recv#(\d+).*Timestamp:\s*([\d.]+)")

    print(f"Reading and parsing log file: {log_file_path}...")
    
    try:
        with open(log_file_path, 'r') as file:
            for line_num, line in enumerate(file, 1):
                match = log_pattern.search(line)
                if match:
                    frame_num = int(match.group(1))
                    timestamp = Decimal(match.group(2))
                    frame_groups[frame_num].append(timestamp)
    except FileNotFoundError:
        print(f"ERROR: The file '{log_file_path}' was not found. Please check the path.")
        return

    # List to hold the maximum difference found in each frame (converted to ms)
    max_gaps_per_frame_ms = []

    for frame_num, timestamps in frame_groups.items():
        if frame_num == 1:
            continue
        # We need at least 2 cameras to calculate a gap/skew for a given frame
        if len(timestamps) >= 2:
            # Calculate the absolute difference between the earliest and latest timestamp (in seconds)
            max_sec_diff = max(timestamps) - min(timestamps)
            # Convert seconds to milliseconds
            max_ms_diff = float(max_sec_diff * Decimal('1000.0'))
            max_gaps_per_frame_ms.append(max_ms_diff)

    if not max_gaps_per_frame_ms:
        print("ERROR: No valid synchronous frames (with 2 or more cameras) were found.")
        return

    # Calculate metrics
    mean_gap = statistics.mean(max_gaps_per_frame_ms)
    std_dev_gap = statistics.stdev(max_gaps_per_frame_ms) if len(max_gaps_per_frame_ms) > 1 else 0.0
    worst_case_gap = max(max_gaps_per_frame_ms)
    best_case_gap = min(max_gaps_per_frame_ms)

    # Output Format
    print("\n=========================================================")
    print("             CROSS-CAMERA METRIC ANALYSIS REPORT         ")
    print("=========================================================")
    print(f" Unique Frames Grouped  : {len(frame_groups)}")
    print(f" Valid Frames Analyzed  : {len(max_gaps_per_frame_ms)} (frames with >= 2 cams)")
    print(f" Mean Max Gap           : {mean_gap:.6f} ms")
    print(f" Standard Deviation (σ) : {std_dev_gap:.6f} ms")
    print(f" Worst-Case Max Skew    : {worst_case_gap:.6f} ms")
    print(f" Best-Case Min Skew     : {best_case_gap:.6f} ms")
    print("=========================================================\n")

if __name__ == "__main__":
    # Change 'my_terminal_output.txt' to the name of your text/log file
    log_file_name = "hourlonglog_inreport.txt" 
    analyze_camera_logs(log_file_name)
