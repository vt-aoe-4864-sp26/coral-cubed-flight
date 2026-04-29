#!/usr/bin/env python3
"""
cdh_img_demo.py — CDH Image Demo PC tool

Send an image to CDH over UART using TAB/OpenLST framing,
then read it back and reconstruct the file to verify persistence.

Usage:
    python3 cdh_img_demo.py send <port> <image_file>
    python3 cdh_img_demo.py recv <port> <output_file>
    python3 cdh_img_demo.py demo <port> <image_file>   # send + power cycle prompt + recv

Examples:
    python3 cdh_img_demo.py send COM5 photo.jpg
    python3 cdh_img_demo.py recv COM5 recovered.jpg
    python3 cdh_img_demo.py demo COM5 photo.jpg
"""

import sys
import os
import struct
import time
import serial
import serial.tools.list_ports

# ── TAB constants ─────────────────────────────────────────────────────
SYNC          = bytes([0x22, 0x69])
DST_CDH       = 0x0001
SRC_PC        = 0x0002
CHUNK_SIZE    = 64
IMAGE_ID      = 0x00000001

CMD_IMG_CHUNK = 0x50
CMD_IMG_START = 0x51
CMD_IMG_END   = 0x52
CMD_IMG_REQ   = 0x53
CMD_IMG_ACK   = 0x54

seq = 0


# ── CRC-16/CCITT ──────────────────────────────────────────────────────
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
        crc &= 0xFFFF
    return crc


# ── Build TAB message ─────────────────────────────────────────────────
def build_tab(cmd: int, data: bytes) -> bytes:
    global seq
    inner = struct.pack("<BHHH", len(data) + 6, DST_CDH, SRC_PC, seq & 0xFFFF)
    inner += bytes([cmd]) + data
    seq += 1
    crc = crc16(inner)
    return SYNC + inner + struct.pack("<H", crc)


# ── Parse received TAB message ────────────────────────────────────────
def parse_tab(port: serial.Serial, timeout: float = 5.0) -> dict | None:
    """Read one TAB message from port. Returns dict or None on timeout."""
    deadline = time.time() + timeout
    buf = bytearray()

    # Hunt for sync
    while time.time() < deadline:
        b = port.read(1)
        if not b:
            continue
        if b[0] == 0x22:
            b2 = port.read(1)
            if b2 and b2[0] == 0x69:
                buf = bytearray([0x22, 0x69])
                break

    if len(buf) < 2:
        return None

    # Read length
    lb = port.read(1)
    if not lb:
        return None
    inner_len = lb[0]
    buf.append(inner_len)

    # Read rest
    rest = port.read(inner_len + 2)
    if len(rest) < inner_len + 2:
        return None
    buf.extend(rest)

    # Verify CRC
    rx_crc   = struct.unpack_from("<H", buf, len(buf) - 2)[0]
    calc_crc = crc16(bytes(buf[2:-2]))
    if rx_crc != calc_crc:
        print(f"  CRC fail rx={rx_crc:04X} calc={calc_crc:04X}")
        return None

    cmd      = buf[9]
    data     = bytes(buf[10:-2])
    return {"cmd": cmd, "data": data, "raw": bytes(buf)}


# ── Wait for ACK ──────────────────────────────────────────────────────
def wait_ack(port: serial.Serial, expected_chunk: int, timeout: float = 3.0) -> bool:
    msg = parse_tab(port, timeout)
    if msg is None:
        print(f"  timeout waiting for ACK chunk {expected_chunk}")
        return False
    if msg["cmd"] != CMD_IMG_ACK:
        print(f"  unexpected cmd 0x{msg['cmd']:02X}")
        return False
    if len(msg["data"]) < 3:
        return False
    chunk_idx = struct.unpack_from("<H", msg["data"], 0)[0]
    status    = msg["data"][2]
    if status != 0:
        print(f"  NAK chunk {chunk_idx} status={status}")
        return False
    return True


# ── List available serial ports ───────────────────────────────────────
def list_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available ports:")
    for p in ports:
        print(f"  {p.device:12s}  {p.description}")


# ── SEND ──────────────────────────────────────────────────────────────
def send_image(port_name: str, image_path: str):
    if not os.path.exists(image_path):
        print(f"File not found: {image_path}")
        return False

    ext = os.path.splitext(image_path)[1].lower()
    fmt = 1 if ext in (".jpg", ".jpeg") else 2 if ext == ".png" else 0

    with open(image_path, "rb") as f:
        raw = f.read()

    total   = len(raw)
    n_chunks = (total + CHUNK_SIZE - 1) // CHUNK_SIZE
    print(f"\nSending: {image_path}")
    print(f"  Size   : {total} bytes")
    print(f"  Chunks : {n_chunks}")
    print(f"  Format : {'jpeg' if fmt == 1 else 'png' if fmt == 2 else 'raw'}")
    print(f"  Port   : {port_name}\n")

    with serial.Serial(port_name, 115200, timeout=1) as port:
        time.sleep(0.5)

        # IMG_START
        start_data = struct.pack("<IIHB", IMAGE_ID, total, CHUNK_SIZE, fmt)
        port.write(build_tab(CMD_IMG_START, start_data))
        print("Sent IMG_START...", end=" ", flush=True)
        if not wait_ack(port, 0xFFFF):
            print("FAILED")
            return False
        print("ACK")

        # IMG_CHUNKs
        for i, off in enumerate(range(0, total, CHUNK_SIZE)):
            chunk = raw[off:off + CHUNK_SIZE]
            chunk_hdr = struct.pack("<IH", IMAGE_ID, i)
            port.write(build_tab(CMD_IMG_CHUNK, chunk_hdr + chunk))

            pct = int((i + 1) / n_chunks * 100)
            print(f"\r  Chunk {i+1}/{n_chunks} ({pct}%)", end="", flush=True)

            if not wait_ack(port, i):
                print(f"\nChunk {i} NAK — aborting")
                return False

        print()

        # IMG_END
        port.write(build_tab(CMD_IMG_END, b""))
        print("Sent IMG_END...", end=" ", flush=True)
        if not wait_ack(port, 0xFFFE):
            print("FAILED")
            return False
        print("ACK")

    print(f"\nImage sent successfully ({total} bytes).")
    return True


# ── RECEIVE ───────────────────────────────────────────────────────────
def recv_image(port_name: str, output_path: str):
    print(f"\nRequesting image readback from CDH...")
    print(f"  Output : {output_path}")
    print(f"  Port   : {port_name}\n")

    chunks = {}
    total_bytes = 0
    fmt = 0

    with serial.Serial(port_name, 115200, timeout=2) as port:
        time.sleep(0.5)

        # Send IMG_REQ
        port.write(build_tab(CMD_IMG_REQ, b""))
        print("Sent IMG_REQ, waiting for data...\n")

        while True:
            msg = parse_tab(port, timeout=10.0)
            if msg is None:
                print("Timeout waiting for data.")
                break

            if msg["cmd"] == CMD_IMG_CHUNK:
                if len(msg["data"]) < 6:
                    continue
                image_id  = struct.unpack_from("<I", msg["data"], 0)[0]
                chunk_idx = struct.unpack_from("<H", msg["data"], 4)[0]
                chunk_data = msg["data"][6:]
                chunks[chunk_idx] = chunk_data
                total_bytes += len(chunk_data)
                print(f"\r  Received chunk {chunk_idx} ({len(chunk_data)} bytes, "
                      f"total {total_bytes})", end="", flush=True)

            elif msg["cmd"] == CMD_IMG_END:
                if len(msg["data"]) >= 1:
                    fmt = msg["data"][0]
                print(f"\n\nIMG_END received — {len(chunks)} chunks, "
                      f"{total_bytes} bytes total")
                break

    if not chunks:
        print("No data received.")
        return False

    # Reconstruct file from ordered chunks
    ordered = sorted(chunks.keys())
    raw = b"".join(chunks[i] for i in ordered)

    # If no extension given, add based on format
    if "." not in os.path.basename(output_path):
        ext = ".jpg" if fmt == 1 else ".png" if fmt == 2 else ".bin"
        output_path += ext

    with open(output_path, "wb") as f:
        f.write(raw)

    print(f"Image saved to: {output_path} ({len(raw)} bytes)")
    print(f"Open {output_path} to verify the image looks correct.")
    return True


# ── DEMO (full round trip) ────────────────────────────────────────────
def demo(port_name: str, image_path: str):
    # Step 1: send
    ok = send_image(port_name, image_path)
    if not ok:
        print("Send failed — aborting demo.")
        return

    # Step 2: prompt for power cycle
    print("\n" + "="*50)
    print("  Image stored in flash.")
    print("  NOW: power cycle the CDH board.")
    print("  Then press ENTER to request readback.")
    print("="*50)
    input()

    # Step 3: receive and reconstruct
    base = os.path.splitext(image_path)[0]
    ext  = os.path.splitext(image_path)[1]
    out_path = base + "_recovered" + ext
    recv_image(port_name, out_path)


# ── Entry point ───────────────────────────────────────────────────────
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        list_ports()
        sys.exit(0)

    cmd = sys.argv[1]

    if cmd == "send":
        if len(sys.argv) < 4:
            print("Usage: cdh_img_demo.py send <port> <image_file>")
            sys.exit(1)
        send_image(sys.argv[2], sys.argv[3])

    elif cmd == "recv":
        if len(sys.argv) < 4:
            print("Usage: cdh_img_demo.py recv <port> <output_file>")
            sys.exit(1)
        recv_image(sys.argv[2], sys.argv[3])

    elif cmd == "demo":
        if len(sys.argv) < 4:
            print("Usage: cdh_img_demo.py demo <port> <image_file>")
            sys.exit(1)
        demo(sys.argv[2], sys.argv[3])

    elif cmd == "ports":
        list_ports()

    else:
        print(f"Unknown command: {cmd}")
        print(__doc__)
        sys.exit(1)
