#!/usr/bin/env python3
"""
CW HID / Digispark CDC - WPM Configuration Test Script

Auto-detects the Digispark CDC COM port on Windows/Linux/macOS,
allows setting WPM (Words Per Minute), querying speed, and running test sequences.

Usage:
    python test_wpm.py                # Auto-detect port and launch interactive terminal
    python test_wpm.py COM3           # Use specific COM port
    python test_wpm.py --set 25       # Set WPM to 25
    python test_wpm.py --test         # Run automated test suite
"""

import sys
import time
import argparse

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("[!] Error: 'pyserial' module is required.")
    print("    Install it with: pip install pyserial")
    sys.exit(1)


def find_digispark_port():
    """Auto-detect the Digispark CDC COM port."""
    ports = list(serial.tools.list_ports.comports())
    
    # Check VID/PID 0x16c0 or description containing Digispark
    for p in ports:
        if p.vid == 0x16c0 or "Digispark" in (p.description or ""):
            return p.device

    # Fallback to any non-standard COM port
    for p in ports:
        if p.device and p.device.upper() not in ("COM1", "COM2"):
            print(f"[*] Candidate port found: {p.device} ({p.description})")
            return p.device

    return None


def send_command(ser, cmd, timeout=1.5):
    """Send command string and collect responses."""
    ser.reset_input_buffer()
    cmd_str = f"{cmd.strip()}\r\n"
    ser.write(cmd_str.encode("ascii"))
    ser.flush()

    time.sleep(0.1)
    start = time.time()
    lines = []

    while time.time() - start < timeout:
        if ser.in_waiting > 0:
            line = ser.readline().decode("ascii", errors="replace").strip()
            if line:
                lines.append(line)
                if any(kw in line for kw in ["OK:", "ERROR:", "Current WPM:"]):
                    break
        else:
            time.sleep(0.05)

    return lines


def run_automated_test(port_name):
    """Execute automated test suite."""
    print(f"\n=============================================")
    print(f"  Testing Digispark CDC WPM Feature ({port_name})")
    print(f"=============================================\n")

    try:
        ser = serial.Serial(port_name, 9600, timeout=1)
    except Exception as e:
        print(f"[!] Unable to open port {port_name}: {e}")
        return False

    time.sleep(0.5)

    # Step 1: Query current speed
    print("[1] Querying current WPM...")
    res = send_command(ser, "?")
    for r in res:
        print(f"    <- {r}")

    # Step 2: Set WPM to 25
    test_wpm = 25
    print(f"\n[2] Setting WPM to {test_wpm}...")
    res = send_command(ser, str(test_wpm))
    for r in res:
        print(f"    <- {r}")

    # Step 3: Verify speed
    print("\n[3] Verifying stored WPM...")
    res = send_command(ser, "wpm")
    for r in res:
        print(f"    <- {r}")

    ser.close()
    print("\n[+] Automated test complete.\n")
    return True


def interactive_terminal(port_name):
    """Interactive console to query and set WPM."""
    print(f"\nConnected to {port_name}. Type WPM (5-60), '?' for status, or 'q' to quit.")
    print("--------------------------------------------------------------------------------")

    try:
        ser = serial.Serial(port_name, 9600, timeout=1)
    except Exception as e:
        print(f"[!] Unable to open port {port_name}: {e}")
        return

    time.sleep(0.5)

    # Initial query
    res = send_command(ser, "?")
    for r in res:
        print(f"<- {r}")

    while True:
        try:
            cmd = input("WPM> ").strip()
            if not cmd:
                continue
            if cmd.lower() in ("q", "quit", "exit"):
                break

            res = send_command(ser, cmd)
            for r in res:
                print(f"<- {r}")

        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"[!] Error: {e}")
            break

    ser.close()
    print("\n[+] Disconnected.")


def main():
    parser = argparse.ArgumentParser(description="Digispark CW HID WPM Test Script")
    parser.add_argument("port", nargs="?", help="COM port (e.g. COM3)")
    parser.add_argument("--set", type=int, help="Set WPM to given integer (5-60) and exit")
    parser.add_argument("--test", action="store_true", help="Run automated test suite")
    args = parser.parse_args()

    port = args.port
    if not port:
        print("[*] Auto-detecting Digispark CDC COM port...")
        port = find_digispark_port()
        if not port:
            print("[!] Digispark CDC port not automatically found.")
            print("    Usage: python test_wpm.py COMx")
            sys.exit(1)
        print(f"[+] Device detected on {port}")

    if args.set:
        try:
            ser = serial.Serial(port, 9600, timeout=1)
            time.sleep(0.5)
            res = send_command(ser, str(args.set))
            for r in res:
                print(r)
            ser.close()
        except Exception as e:
            print(f"[!] Error: {e}")
            sys.exit(1)
    elif args.test:
        run_automated_test(port)
    else:
        interactive_terminal(port)


if __name__ == "__main__":
    main()
