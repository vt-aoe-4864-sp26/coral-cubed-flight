# demo.py

# UART Control of COM board for PDR Demos.
# Requires USB-to-TTL Cable to plug into COM board to plug into TX/RX of spare COM board to act
# in place of a ground station, sending commands to the flatsat COM board.
#
# Written by Jack Rathert
# Other contributors: Nico Demarinis

# See the top-level LICENSE file for the license.

import serial 
import sys, os, time, pathlib

from utils.path_utils import get_repo_root
from utils.tab import *

class PCB:
    def __init__(self, port=None, BAUD=None, HWID=None, msgid=None):
        self.dev = port if port is not None else '/dev/ttyUSB0'
        self.BAUD = BAUD if BAUD is not None else 115200
        self.HWID = HWID if HWID is not None else 0xccc3
        self.msgid = msgid if msgid is not None else 0x0000
        
        self.rx_cmd_buff = RxCmdBuff()
        self.serial_port = None

    def _wait_for_serial(self, timeout=30):
        if not self.dev:
            raise ValueError("Serial port is not provided or is None.")
        
        print(f"Waiting for serial device at {self.dev}...")
        start = time.time()
        while time.time() - start < timeout:
            if pathlib.Path(self.dev).exists():
                self.serial_port = serial.Serial(port=self.dev, baudrate=self.BAUD)
                print("Serial Connection Established to PCB\n")
                return True
            time.sleep(0.5)
        raise TimeoutError(f"Serial port {self.dev} not found within {timeout}s")
    
    def _send_and_wait(self, cmd):
        """Helper to handle the repetitive TX/RX loop for all commands."""
        byte_i = 0
        while self.rx_cmd_buff.state != RxCmdBuffState.COMPLETE:
            # Transmit
            if byte_i < cmd.get_byte_count():
                self.serial_port.write(cmd.data[byte_i].to_bytes(1, byteorder='big'))
                byte_i += 1
            
            # Receive
            if self.serial_port.in_waiting > 0:
                bytes_read = self.serial_port.read(1)
                for b in bytes_read:
                    self.rx_cmd_buff.append_byte(b)
                    
        print('txcmd: ' + str(cmd))
        print('reply: ' + str(self.rx_cmd_buff) + '\n')
        
        # Cleanup and increment for next message
        cmd.clear()
        self.rx_cmd_buff.clear()
        self.msgid += 1
        time.sleep(1.0)

    # Opcodes

    def common_ack(self):
        cmd = TxCmd(COMMON_ACK_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def common_nack(self):
        cmd = TxCmd(COMMON_NACK_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def common_debug(self, message: str):
        cmd = TxCmd(COMMON_DEBUG_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_debug(message)
        self._send_and_wait(cmd)

    def common_data(self, data: list):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data(data)
        self._send_and_wait(cmd)

    def bootloader_ack(self):
        cmd = TxCmd(BOOTLOADER_ACK_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def bootloader_nack(self):
        cmd = TxCmd(BOOTLOADER_NACK_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def bootloader_ping(self):
        cmd = TxCmd(BOOTLOADER_PING_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def bootloader_erase(self):
        cmd = TxCmd(BOOTLOADER_ERASE_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)

    def bootloader_write_page(self, page_number: int, page_data: list):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.bootloader_write_page(page_number=page_number, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_write_page_addr32(self, addr: int, page_data: list):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_ADDR32_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.bootloader_write_page_addr32(addr=addr, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_jump(self):
        cmd = TxCmd(BOOTLOADER_JUMP_OPCODE, self.HWID, self.msgid, GND, COM)
        self._send_and_wait(cmd)
        
    def blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data([0x06, 0x01, 0x01])
        self._send_and_wait(cmd)


if __name__ == '__main__':
    # Parse script arguments
    port = '/dev/ttyUSB0' # Default
    if len(sys.argv) == 2:
        port = sys.argv[1]
    elif len(sys.argv) > 2:
        print('Usage: python3 demo.py [/path/to/dev]')
        sys.exit(1)

    print(f"Initializing PCB communication on {port}...")

    board = PCB(port=port, BAUD=115200, HWID=0xccc3, msgid=0x0000)

    try:
        # Wait for device and connect
        board._wait_for_serial(timeout=10)

        print("--- Commencing Tests ---")
        
        # Test LEDs - COM
        board.blink_demo()
        
        # Test Common Ack
        board.common_ack()
        
        # Test Common Nack
        board.common_nack()

        # Test Common Debug
        board.common_debug('Hello, world!')

        # Enable Payload
        board.common_data([0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b])

        # Enable COM RF Frontend
        board.common_data([0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x0a,0x09,0x0b])

        # Enable COM RX

        # Enable COM TX



        print("--- Testing Complete ---")

    except Exception as e:
        print(f"Error during execution: {e}")
        sys.exit(1)