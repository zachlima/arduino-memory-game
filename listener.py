"""
Reads game-over results from the Arduino over USB serial and forwards
each one to the local leaderboard server.

The Arduino sends each result as "player,score" on its own line,
e.g. "2,5" means Player 2 survived 5 rounds.

Setup:
    pip install pyserial requests

Run this while you play. It posts a new result every time you lose,
reconnects if the board is unplugged, and queues results if the server
is down.
"""

import glob
import time
from collections import deque

import requests
import serial

# macOS changes the port name depending on the USB socket, so search for it
PORT_PATTERNS = ["/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/cu.wchusbserial*"]
FALLBACK_PORT = "/dev/cu.usbmodem1101"

BAUD_RATE = 9600
SERVER_URL = "http://localhost:3000/scores"
POST_TIMEOUT = 5  # without this, a stalled server would block serial reads
RECONNECT_DELAY = 3

# undelivered results, retried after the next successful post
pending = deque(maxlen=100)


def find_port():
    for pattern in PORT_PATTERNS:
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    return FALLBACK_PORT


def send(player, score):
    """Try to deliver one result.

    True means done with it: either saved, or refused outright so there
    is no point retrying. False means unreachable, so queue it.
    """
    try:
        response = requests.post(
            SERVER_URL,
            json={"player": player, "score": score},
            timeout=POST_TIMEOUT,
        )
    except requests.RequestException:
        print("  Could not reach the server. Queued for retry.")
        return False

    if response.ok:
        return True

    print(f"  Server rejected it ({response.status_code}). Discarding.")
    return True


def flush_pending():
    while pending:
        player, score = pending[0]
        if not send(player, score):
            return  # still unreachable; keep the rest queued
        pending.popleft()
        print(f"  Delivered queued result (Player {player}, score {score}).")


def handle_line(line):
    parts = line.split(",")
    if len(parts) != 2 or not all(part.isdigit() for part in parts):
        return

    player, score = int(parts[0]), int(parts[1])
    print(f"Game over. Player {player}, score {score}. Sending...")

    if send(player, score):
        print("  Saved.")
        flush_pending()
    else:
        pending.append((player, score))


def main():
    print("Listening for scores. Press Ctrl+C to stop.")

    while True:
        port = find_port()
        try:
            print(f"Connecting to {port}...")
            with serial.Serial(port, BAUD_RATE, timeout=1) as ser:
                time.sleep(2)  # let the Arduino reset after opening the port
                print("Connected.")
                flush_pending()

                while True:
                    line = ser.readline().decode(errors="ignore").strip()
                    if line:
                        handle_line(line)

        except (serial.SerialException, OSError) as err:
            print(f"Serial problem ({err}). Retrying in {RECONNECT_DELAY}s...")
            time.sleep(RECONNECT_DELAY)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
        if pending:
            print(f"{len(pending)} result(s) were never delivered.")
