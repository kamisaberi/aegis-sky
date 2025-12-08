import mmap
import struct
import time
import os
import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# CONFIG MATCHING C++ HEADER
BRIDGE_NAME = "/aegis_bridge_v1"
BRIDGE_SIZE = 1024 * 1024 * 64
HEADER_SIZE = 64 # Adjust based on struct alignment
POINT_SIZE = 24  # 5 floats (x,y,z,vel,snr) + 1 uint32 (id) = 24 bytes

def read_bridge():
    # 1. Open Shared Memory
    try:
        fd = os.open("/dev/shm" + BRIDGE_NAME, os.O_RDONLY)
        buf = mmap.mmap(fd, BRIDGE_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)
    except FileNotFoundError:
        print("Error: Simulator is not running! Run ./aegis_sim first.")
        sys.exit(1)

    return buf

def update(frame):
    buf.seek(0)
    
    # 2. Read Header
    # struct BridgeHeader { uint32 magic; uint64 frame; double time; uint32 count; uint32 state; ... }
    # We unpack standard 64-bit alignment logic, might need tweaking depending on C++ compiler padding
    header_data = buf.read(HEADER_SIZE)
    # I = uint32, Q = uint64, d = double
    magic, frame_id, sim_time, num_points, state = struct.unpack('<IQdII', header_data[:28])

    plt.cla()
    plt.xlim(-500, 500)
    plt.ylim(0, 1000) # Radar is usually looking forward (Z+)
    plt.title(f"AEGIS RADAR FEED | T={sim_time:.2f}s | Targets: {num_points}")
    plt.grid(True)

    if num_points > 0:
        # 3. Read Radar Points
        # Offset to Data (Sizeof Header)
        buf.seek(HEADER_SIZE)
        points_data = buf.read(num_points * POINT_SIZE)
        
        # Parse into Numpy
        # struct: float x, y, z, vel, snr; uint32 id
        dt = np.dtype([('x', 'f4'), ('y', 'f4'), ('z', 'f4'), ('vel', 'f4'), ('snr', 'f4'), ('id', 'u4')])
        arr = np.frombuffer(points_data, dtype=dt)

        # 4. Plot
        # Color based on Velocity (Red = Closing In, Blue = Moving Away)
        colors = ['red' if v < 0 else 'blue' for v in arr['vel']]
        sizes = [max(10, s * 2) for s in arr['snr']] # Size by Signal Strength

        plt.scatter(arr['x'], arr['z'], c=colors, s=sizes)
        
        # Annotate
        for p in arr:
            lbl = f"ID:{p['id']}\n{p['y']:.1f}m"
            plt.text(p['x'], p['z'], lbl, fontsize=8)

buf = read_bridge()
fig = plt.figure()
ani = FuncAnimation(fig, update, interval=50) # 20 FPS
plt.show()