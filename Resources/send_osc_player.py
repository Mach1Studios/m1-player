import time
from pythonosc import udp_client
import json
import os
import sys
from pathlib import Path

class DAWPlayheadSimulator:
    def __init__(self, helper_port=10301):
        self.helper_port = helper_port
        self.client = udp_client.SimpleUDPClient("127.0.0.1", helper_port)
        self.frame_rate = 60
        self.player_port = None
        self.is_connected = False
        self.start_time = None
       
    def register_player(self, port):
        """Register this client as a player with the helper service"""
        self.player_port = port
        self.client.send_message("/m1-addClient", [port, "player"])
        self.is_connected = True
       
    def simulate_playhead(self, start_pos=0.0, end_pos=60.0, loop=True):
        """Simulate playhead movement from start to end position"""
        current_pos = start_pos
        update_counter = 0
        self.start_time = time.time()
       
        try:
            while True:
                # Calculate time difference in milliseconds
                current_time = time.time()
                time_diff_ms = ((current_time - self.start_time) * 1.0) + start_pos * 1.0
                
                # Send time difference instead of position
                self.client.send_message("/playerPosition", [update_counter, time_diff_ms])
               
                # Send playing state (always true in this case)
                self.client.send_message("/playerIsPlaying", [update_counter, 1])
               
                # Send frame rate
                self.client.send_message("/playerFrameRate", [float(self.frame_rate)])
               
                # Update position
                current_pos += 1.0 / self.frame_rate
               
                # Handle looping
                if current_pos >= end_pos:
                    if loop:
                        current_pos = start_pos
                        self.start_time = time.time()  # Reset start time for new loop
                    else:
                        break
                       
                update_counter += 1
                print(f"playhead position: {current_pos:.2f}s (time diff: {time_diff_ms}ms)")
                time.sleep(1.0 / self.frame_rate)
               
        except KeyboardInterrupt:
            print("\nPlayhead simulation stopped")
            self.client.send_message("/playerIsPlaying", [update_counter, 0])
               
    def cleanup(self):
        """Unregister the client"""
        if self.is_connected and self.player_port:
            self.client.send_message("/m1-removeClient", [self.player_port, "player"])

def find_settings_file():
    """Find the settings.json file based on OS"""
    if sys.platform == "darwin":  # macOS
        base_path = Path("/Library/Application Support/Mach1")
    elif sys.platform == "win32":  # Windows
        base_path = Path(os.environ["PROGRAMDATA"]) / "Mach1"
    else:  # Linux or others
        base_path = Path("/usr/local/share/Mach1")
   
    return base_path / "settings.json"

def main():
    # Try to read helper port from settings
    settings_path = find_settings_file()
    helper_port = 10301  # default
   
    try:
        if settings_path.exists():
            with open(settings_path) as f:
                settings = json.load(f)
                helper_port = settings.get("helperPort", helper_port)
    except Exception as e:
        print(f"Warning: Could not read settings file: {e}")
        print(f"Using default helper port: {helper_port}")
    
    simulator = DAWPlayheadSimulator(helper_port)
   
    # Register as a player using the first available port
    for port in range(10301, 10400):
        try:
            simulator.register_player(port)
            print(f"Registered player on port {port}")
            break
        except:
            continue
   
    if not simulator.is_connected:
        print("Failed to register player")
        return
   
    try:
        # Simulate playhead movement from 0 to 60 seconds, looping
        simulator.simulate_playhead(20.0, 50.0, True)
    finally:
        simulator.cleanup()

if __name__ == "__main__":
    main()