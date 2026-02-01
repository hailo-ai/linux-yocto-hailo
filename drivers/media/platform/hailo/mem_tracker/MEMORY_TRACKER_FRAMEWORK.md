# Memory Tracker Framework - User Guide

## Overview

The Memory Tracker Framework provides real-time tracking and visualization of memory allocations in the Linux kernel. It tracks allocations across different memory subsystems (CMA, DMA, etc.) and provides both command-line access via debugfs and integration with Hailo SOC Profiler for visual analysis.

## Location

All memory trackers are accessible under:
```
/sys/kernel/debug/mem_trackers/
```

Each tracker has its own subdirectory. For example, the CMA tracker is at:
```
/sys/kernel/debug/mem_trackers/cma/
```

## Global Controls

At the root level (`/sys/kernel/debug/mem_trackers/`), there are two global control files:

### `session_id` (read/write)
- **Purpose**: Tracing initial state of the memory to view history while recording.
- **Read**: Returns the current session ID (0 if not set)
- **Write**: Set a new session ID (numeric value)
- **Usage**: Used by `hailo-soc-profiler` with `--new-mem-tracker-session` to mark a new tracing session

### `enable` (read/write)
- **Purpose**: Global enable/disable switch for all memory trackers
- **Read**: Returns `1` if enabled, `0` if disabled
- **Write**: Set to `1` to enable, `0` to disable
- **Default**: Enabled (1)

## Per-Tracker Debugfs Files

Each memory tracker (e.g., `cma`) provides the following files under `/sys/kernel/debug/mem_trackers/<tracker_name>/`:

### Read-Only Files

#### `summary`
Displays overall statistics and configuration:
- Total allocated/freed memory
- Current and peak allocated memory
- Allocation and free counts
- Active allocation count
- Circular buffer status (events and history)
- Current trace session ID
- Stack capture mode
- Type-specific information (e.g., CMA heap statistics)

**Example:**
```bash
cat /sys/kernel/debug/mem_trackers/cma/summary
```

### Detailed View Read-Only Files

The following files have an additional detailed view, including:
- Complete allocation details
- Stack traces (if enabled)
- Type-specific extended information (e.g., CMA heap stats)

#### `active`
Brief view of all currently active (not yet freed) allocations. Shows one line per allocation with key information.

**Format**: Type-specific, but typically includes:
- Allocation UID (unique identifier)
- Address/pointer
- Size
- Process name and PID
- Timestamp

**Example:**
```bash
cat /sys/kernel/debug/mem_trackers/cma/active  # Short one line view
cat /sys/kernel/debug/mem_trackers/cma/active_detailed  # Detailed view
```

#### `events`
Brief chronological log of all allocation and free events. Shows one line per event (ALLOC or FREE).

**Format**: Type-specific, but typically includes:
- Event type (ALLOC/FREE)
- Allocation UID
- Address
- Size
- Process name and PID
- Timestamp

**Note**: This is a circular buffer - older events are overwritten when the buffer is full.

**Example:**
```bash
cat /sys/kernel/debug/mem_trackers/cma/events  # Short one line view
cat /sys/kernel/debug/mem_trackers/cma/events_detailed  # Detailed view
```

#### `history`
Brief allocation history showing the complete lifecycle of allocations. Includes both active and freed allocations.

**Format**: Type-specific, but typically includes:
- Allocation UID
- Status (ACTIVE or FREED)
- Address
- Size
- Allocation and free timestamps (if freed)
- Duration (if freed)

**Note**: This is a circular buffer - older records are overwritten when the buffer is full.

**Example:**
```bash
cat /sys/kernel/debug/mem_trackers/cma/history  # Short one line view
cat /sys/kernel/debug/mem_trackers/cma/history_detailed  # Detailed view
```

### Read/Write Configuration Files

#### `buffer_size` (read/write)
- **Purpose**: Configure the size of circular buffers for events and history
- **Read**: Returns current buffer size (number of entries)
- **Write**: Set new buffer size (numeric value, 1-100000)
- **Default**: 1000 entries
- **Note**: Changing buffer size preserves existing data (oldest entries may be dropped if shrinking)

**Example:**
```bash
# Read current size
cat /sys/kernel/debug/mem_trackers/cma/buffer_size

# Set to 5000 entries
echo 5000 > /sys/kernel/debug/mem_trackers/cma/buffer_size
```

#### `stack_capture` (read/write)
- **Purpose**: Configure stack trace capture mode
- **Read**: Returns current mode (0, 1, or 2)
- **Write**: Set new mode
  - `0` = Disabled (best performance, no stack traces)
  - `1` = Raw addresses (small traces, allows post-processing)
  - `2` = Symbolized (large traces, human-readable)
- **Default**: 0 (disabled)

**Example:**
```bash
# Read current mode
cat /sys/kernel/debug/mem_trackers/cma/stack_capture

# Enable raw address capture
echo 1 > /sys/kernel/debug/mem_trackers/cma/stack_capture

# Enable symbolized capture
echo 2 > /sys/kernel/debug/mem_trackers/cma/stack_capture

# Disable stack capture
echo 0 > /sys/kernel/debug/mem_trackers/cma/stack_capture
```

## Hailo SOC Profiler Integration

The memory tracker framework automatically emits tracepoint events that can be captured by Hailo SOC Profiler for visual analysis. This allows you to visualize memory allocations over time in the Hailo SOC Profiler UI.

### Using hailo-soc-profiler

The easiest way to capture memory tracker data is using `hailo-soc-profiler`:

#### Basic Usage

```bash
hailo-soc-profiler mem_tracker -t 10s -o soc.trace
```

This captures memory tracker events for 10 seconds and saves to `soc.trace`.

#### Capturing Pre-Allocated Buffers

When tracing starts, allocations that were made **before** tracing began are not captured by default. To include these pre-allocated buffers in the trace, use the `--new-mem-tracker-session` flag:

```bash
hailo-soc-profiler mem_tracker -t 10s -o soc.trace --new-mem-tracker-session
```

**What this does:**
- Sets a new session ID before starting the trace
- The memory tracker detects this new session ID
- All currently active allocations are automatically emitted to the trace
- This ensures complete coverage even if tracing starts after allocations begin

**When to use:**
- When you want to see allocations that existed before tracing started
- When debugging memory leaks or tracking long-lived allocations
- When you need a complete picture of all active memory at trace start

**Example scenario:**
```bash
# Start your application (allocations happen)
./my_app &

# Wait a bit...
sleep 5

# Now trace with pre-allocated buffers included
hailo-soc-profiler mem_tracker -t 30s -o soc.trace --new-mem-tracker-session
```

### Tracepoint Events

The framework emits two types of tracepoint events:

1. **`mem_tracker_event`** - Instant ALLOC/FREE events
   - Emitted on every allocation and free operation
   - Shows as event markers on the Hailo SOC Profiler timeline

2. **`mem_tracker_hist_event`** - Allocation lifecycle events
   - Emitted when allocations are freed (complete lifecycle)
   - Shows as duration-based slices showing allocation lifetime
   - Includes both allocation and free context

### Viewing Traces

Open the generated `.trace` file in the Hailo SOC Profiler UI to visualize (http://10.41.100.153/ on Hailo's network):
- Memory allocation timeline
- Allocation durations
- Stack traces (if enabled)
- Memory pool information

## CMA Tracker Specific Files

The CMA tracker (`/sys/kernel/debug/mem_trackers/cma/`) includes two additional files:

### `heap_stats` (read-only)
Displays CMA heap statistics for all CMA areas:
- Total, used, and free pages
- Maximum chunk size
- Usage percentage

**Example:**
```bash
cat /sys/kernel/debug/mem_trackers/cma/heap_stats
```

### `heap_stats_enabled` (read/write)
- **Purpose**: Enable/disable CMA heap statistics tracking
- **Read**: Returns `1` if enabled, `0` if disabled
- **Write**: Set to `1` to enable, `0` to disable
- **Default**: Disabled (0)
- **Note**: When disabled, heap statistics are not collected or displayed, improving performance

**Example:**
```bash
# Enable heap stats tracking
echo 1 > /sys/kernel/debug/mem_trackers/cma/heap_stats_enabled

# Disable heap stats tracking
echo 0 > /sys/kernel/debug/mem_trackers/cma/heap_stats_enabled
```

## Unlimited Output

All debugfs files use the `seq_file` API, which means **there is no PAGE_SIZE limitation**. You can view all tracked data even during heavy allocation activity without truncation.

## Examples

### View current memory status
```bash
# Quick overview
cat /sys/kernel/debug/mem_trackers/cma/summary

# List all active allocations
cat /sys/kernel/debug/mem_trackers/cma/active

# See detailed allocation history
cat /sys/kernel/debug/mem_trackers/cma/history_detailed
```

### Configure for detailed debugging
```bash
# Enable stack traces (symbolized)
echo 2 > /sys/kernel/debug/mem_trackers/cma/stack_capture

# Increase buffer size to keep more history
echo 5000 > /sys/kernel/debug/mem_trackers/cma/buffer_size

# Enable CMA heap stats (CMA tracker only)
echo 1 > /sys/kernel/debug/mem_trackers/cma/heap_stats_enabled
```

### Capture trace with pre-allocated buffers
```bash
# Start your workload
./my_workload &

# Capture trace including buffers allocated before tracing started
hailo-soc-profiler mem_tracker -t 60s -o memory.trace --new-mem-tracker-session
```

### Monitor memory in real-time
```bash
# Watch active allocations update in real-time
watch -n 1 'cat /sys/kernel/debug/mem_trackers/cma/active | tail -20'

# Count active allocations
cat /sys/kernel/debug/mem_trackers/cma/active | grep -c "uid="
```

## Performance Considerations

- **Stack capture disabled (mode 0)**: Minimal overhead, best performance
- **Raw addresses (mode 1)**: Small performance impact, small trace files
- **Symbolized (mode 2)**: Higher overhead, larger trace files, human-readable
- **Heap stats (CMA)**: Additional overhead when enabled, disable if not needed
- **Buffer size**: Larger buffers use more memory but keep more history

## Troubleshooting

### No data in files
- Check if tracker is enabled: `cat /sys/kernel/debug/mem_trackers/enable`
- Verify tracker is registered: `ls /sys/kernel/debug/mem_trackers/`
- Check kernel logs: `dmesg | grep -i "memory tracker"`

### Missing pre-allocated buffers in the Hailo SOC Profiler trace
- Use `--new-mem-tracker-session` flag when starting trace
- Verify session ID changed: `cat /sys/kernel/debug/mem_trackers/session_id`

### Buffer overflow
- Increase buffer size: `echo 10000 > /sys/kernel/debug/mem_trackers/cma/buffer_size`
- Note: Oldest entries are overwritten when buffer is full
