#!/usr/bin/env python3
"""
Covert channel experiment using strided pages
Measures bandwidth and error rates using every 32nd page of a large file
"""

import subprocess
import time
import csv
import sys
import random
from datetime import datetime

# Configuration
RUNTIME = "runc"
TARGET_FILE = "/workspace/rand0.bin"
PAGE_STRIDE = 32
IMAGE_PATH = "union-buster:latest"
CONTAINER_NAMES = ["sender_container", "receiver_container"]
OUTPUT_FILE = "stride_channel_results.csv"
NUM_REPETITIONS = 3
MESSAGE_LENGTH = 1024  # Number of bits per message (configurable)
CYCLE_THRESHOLD = 100000
NUM_RANDOM_PATTERNS = 5  # Number of random patterns to test
RANDOM_SEED = 42  # For reproducibility (set to None for truly random)

def generate_random_patterns(num_patterns, message_length):
    """Generate random bit patterns"""
    if RANDOM_SEED is not None:
        random.seed(RANDOM_SEED)
    patterns = []
    for _ in range(num_patterns):
        pattern = ''.join(random.choice('01') for _ in range(message_length))
        patterns.append(pattern)
    return patterns

def run_command(cmd, capture_output=True, check=True):
    """Run a shell command"""
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=capture_output,
            text=True,
            check=check
        )
        return result.stdout.strip() if capture_output else None
    except subprocess.CalledProcessError as e:
        if check:
            print(f"Command failed: {cmd}")
            print(f"Error: {e.stderr}")
            raise
        return None

def docker_exec(container, command):
    """Execute command in Docker container"""
    cmd = f"sudo docker exec {container} {command}"
    return run_command(cmd)

def clear_page_cache():
    """Clear the system page cache"""
    run_command("sync", capture_output=False)
    run_command("echo 1 | sudo tee /proc/sys/vm/drop_caches > /dev/null 2>&1")

def setup_containers():
    """Setup Docker containers"""
    print("Setting up containers...")
    for name in CONTAINER_NAMES:
        # Remove existing container
        run_command(f"sudo docker rm --force {name}", check=False)
        
        # Start new container
        print(f"  Starting container: {name}")
        container_id = run_command(
            f"sudo docker run --runtime={RUNTIME} -d --name {name} {IMAGE_PATH}"
        )
        print(f"    {container_id}")
    
    print("\nContainers running:")
    run_command(
        f"sudo docker container ls | grep -E '{CONTAINER_NAMES[0]}|{CONTAINER_NAMES[1]}'",
        capture_output=False
    )

def cleanup_containers():
    """Remove Docker containers"""
    print("Cleaning up containers...")
    for name in CONTAINER_NAMES:
        run_command(f"sudo docker rm --force {name}", check=False)

def calculate_bit_errors(sent, received):
    """Calculate number of bit errors between two binary strings"""
    if len(sent) != len(received):
        return len(sent)  # Complete mismatch
    
    errors = sum(1 for i in range(len(sent)) if sent[i] != received[i])
    return errors

def send_pattern(pattern):
    """Send a pattern by priming cache"""
    cmd = f"/workspace/sender_stride {TARGET_FILE} {pattern}"
    output = docker_exec(CONTAINER_NAMES[0], cmd)
    
    # Parse CSV output from sender
    # Format: page_size,filename,bit_pattern,num_bits,pages_primed,stride,open_cycles,open_ns,avg_read_cycles,avg_read_ns,total_cycles,total_ns
    fields = output.split(',')
    if len(fields) >= 12:
        total_cycles = fields[10]
        total_ns = fields[11]
        return total_cycles, total_ns
    else:
        return "0", "0"

def receive_pattern():
    """Receive pattern by detecting cached pages"""
    cmd = f"/workspace/receiver_stride {TARGET_FILE} {MESSAGE_LENGTH} {CYCLE_THRESHOLD}"
    output = docker_exec(CONTAINER_NAMES[1], cmd)
    
    # Parse CSV output
    fields = output.split(',')
    if len(fields) >= 13:
        received = fields[11]  # bit_pattern field
        cached_count = fields[4]
        avg_cycles = fields[8]
        min_cycles = fields[6]
        max_cycles = fields[7]
        cycle_values = fields[12].strip()  # individual cycle times (space-separated)
        return received, cached_count, avg_cycles, min_cycles, max_cycles, cycle_values
    else:
        return "?" * MESSAGE_LENGTH, "0", "0", "0", "0", ""

def run_experiment():
    """Run the main experiment"""
    experiment_start = time.time()
    
    # Generate random patterns
    patterns = generate_random_patterns(NUM_RANDOM_PATTERNS, MESSAGE_LENGTH)
    
    print("Starting strided page covert channel experiment...")
    print("Configuration:")
    print(f"  Target file: {TARGET_FILE}")
    print(f"  Page stride: {PAGE_STRIDE}")
    print(f"  Message length: {MESSAGE_LENGTH} bits")
    print(f"  Repetitions: {NUM_REPETITIONS}")
    print(f"  Random patterns: {NUM_RANDOM_PATTERNS}")
    print(f"  Random seed: {RANDOM_SEED if RANDOM_SEED is not None else 'None (truly random)'}")
    print(f"  Output file: {OUTPUT_FILE}")
    print()
    
    # Display the random patterns
    print("Generated random patterns:")
    for i, pattern in enumerate(patterns, 1):
        print(f"  Pattern {i}: {pattern}")
    print()
    
    # Setup containers
    setup_containers()
    
    print(f"\nTesting {len(patterns)} different patterns, {NUM_REPETITIONS} repetitions each")
    print("=" * 60)
    
    # Initialize CSV file
    with open(OUTPUT_FILE, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            'experiment_start_time',
            'pattern',
            'repetition',
            'received_pattern',
            'bit_errors',
            'duration_ms',
            'send_cycles',
            'send_ns',
            'cached_count',
            'avg_cycles',
            'min_cycles',
            'max_cycles',
            'cycle_values'
        ])
    
    total_transmissions = len(patterns) * NUM_REPETITIONS
    transmission_count = 0
    
    # Run experiments
    for pattern in patterns:
        print(f"\nPattern: {pattern}")
        
        for rep in range(1, NUM_REPETITIONS + 1):
            iteration_start = time.time()
            transmission_count += 1
            
            print(f"  Rep {rep:3d}/{NUM_REPETITIONS}: ", end='', flush=True)
            
            # Clear page cache
            clear_page_cache()
            
            # Send pattern (prime cache) and get timing
            send_cycles, send_ns = send_pattern(pattern)
            
            # Small delay
            time.sleep(0.05)
            
            # Receive pattern (detect cached pages)
            received, cached_count, avg_cycles, min_cycles, max_cycles, cycle_values = receive_pattern()
            
            # Calculate bit errors
            bit_errors = calculate_bit_errors(pattern, received)
            
            iteration_end = time.time()
            duration_ms = (iteration_end - iteration_start) * 1000
            
            # Parse individual cycle values for display
            cycle_list = cycle_values.split() if cycle_values else []
            
            # Status output
            if bit_errors == 0:
                status = "✓"
            else:
                status = f"✗ ({bit_errors} errors)"
            
            # Show latency range and send time in output
            latency_info = f"[{min_cycles}-{max_cycles} cycles]" if min_cycles and max_cycles else ""
            send_ms = float(send_ns) / 1_000_000  # Convert ns to ms
            send_info = f"send:{send_ms:.2f}ms"
            print(f"[{received}] {status} {duration_ms:.2f}ms ({send_info}) {latency_info} [{transmission_count}/{total_transmissions}]")
            
            # Log to CSV
            with open(OUTPUT_FILE, 'a', newline='') as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow([
                    experiment_start,
                    pattern,
                    rep,
                    received,
                    bit_errors,
                    f"{duration_ms:.2f}",
                    send_cycles,
                    send_ns,
                    cached_count,
                    avg_cycles,
                    min_cycles,
                    max_cycles,
                    cycle_values
                ])
            
            # Small delay between iterations
            time.sleep(0.05)
    
    experiment_end = time.time()
    total_duration = experiment_end - experiment_start
    
    print()
    print("=" * 60)
    print("Experiment complete!")
    print(f"Total duration: {total_duration:.2f}s")
    print(f"Total transmissions: {total_transmissions}")
    print(f"Results saved to: {OUTPUT_FILE}")
    print("=" * 60)
    
    # Cleanup
    cleanup_containers()
    
    print()
    print("Analysis:")
    print(f"  Run './plot_stride_channel.py {OUTPUT_FILE}' to analyze results")

if __name__ == "__main__":
    try:
        run_experiment()
    except KeyboardInterrupt:
        print("\n\nExperiment interrupted by user")
        cleanup_containers()
        sys.exit(1)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback
        traceback.print_exc()
        cleanup_containers()
        sys.exit(1)
