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
        self.dev = port if port is not None else '/dev/ttyACM0'
        self.BAUD = BAUD if BAUD is not None else 115200
        self.HWID = HWID if HWID is not None else 0xccc3
        self.msgid = msgid if msgid is not None else 0x0000
        
        self.rx_cmd_buff = RxCmdBuff()
        self.serial_port = None

    def _wait_for_serial(self, timeout=30):
        if not self.dev:
            raise ValueError("serial port is not provided or is none.")
        
        print(f"waiting for serial device at {self.dev}...")
        start = time.time()
        while time.time() - start < timeout:
            if pathlib.Path(self.dev).exists():
                self.serial_port = serial.Serial(port=self.dev, baudrate=self.BAUD)
                print("serial connection established to pcb\n")
                return True
            time.sleep(0.5)
        raise TimeoutError(f"serial port {self.dev} not found within {timeout}s")
    
    def _send_and_wait(self, cmd, timeout=5.0, retries=3):
        """helper to handle the repetitive tx/rx loop for all commands."""
        
        # Log the command BEFORE sending so you know what is happening if it times out
        print('txcmd: ' + str(cmd))
        
        for attempt in range(retries):
            start_time = time.time()
            self.rx_cmd_buff.clear()
            
            # 1. Purge the OS buffer of any stale ACKs/echoes before transmitting
            self.serial_port.reset_input_buffer()
            
            # 2. Blast the entire payload at once for max USB throughput
            packet_len = cmd.get_byte_count()
            self.serial_port.write(bytes(cmd.data[:packet_len]))
            
            # 3. Wait for the complete reply
            reply_found = False
            while not reply_found:
                
                # Check how many bytes the OS has received
                waiting = self.serial_port.in_waiting
                if waiting > 0:
                    # Read them all in one chunk, then feed them to the state machine
                    bytes_read = self.serial_port.read(waiting)
                    for b in bytes_read:
                        self.rx_cmd_buff.append_byte(b)
                        
                        # 4. Check if the state machine successfully completed a packet
                        if self.rx_cmd_buff.state == RxCmdBuffState.COMPLETE:
                            if self.rx_cmd_buff.bus_msg_id == self.msgid:
                                print('reply: ' + str(self.rx_cmd_buff) + '\n')
                                
                                # cleanup and increment for next message
                                cmd.clear()
                                self.rx_cmd_buff.clear()
                                self.msgid += 1
                                time.sleep(1.0)
                                return True
                            else:
                                print(f"ignoring stale reply with msg_id 0x{self.rx_cmd_buff.bus_msg_id:04x}")
                                self.rx_cmd_buff.clear()
                else:
                    # Yield the thread briefly so we don't cook the CPU
                    time.sleep(0.001)

                if time.time() - start_time > timeout:
                    print(f"timeout waiting for reply on attempt {attempt + 1}/{retries}")
                    break

        # If we exit the for-loop, all retries failed
        print("failed to receive valid reply after all retries.\n")
        cmd.clear()
        self.rx_cmd_buff.clear()
        self.msgid += 1
        return False

    # opcodes

    def common_ack(self, dst=COM):
        cmd = TxCmd(COMMON_ACK_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def common_nack(self, dst=COM):
        cmd = TxCmd(COMMON_NACK_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def common_debug(self, message: str, dst=COM):
        cmd = TxCmd(COMMON_DEBUG_OPCODE, self.HWID, self.msgid, GND, dst)
        cmd.common_debug(message)
        self._send_and_wait(cmd)

    def common_data(self, data: list, dst=COM):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, dst)
        cmd.common_data(data)
        self._send_and_wait(cmd)

    def bootloader_ack(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_ACK_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def bootloader_nack(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_NACK_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def bootloader_ping(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_PING_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def bootloader_erase(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_ERASE_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)

    def bootloader_write_page(self, page_number: int, page_data: list, dst=COM):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_OPCODE, self.HWID, self.msgid, GND, dst)
        cmd.bootloader_write_page(page_number=page_number, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_write_page_addr32(self, addr: int, page_data: list, dst=COM):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_ADDR32_OPCODE, self.HWID, self.msgid, GND, dst)
        cmd.bootloader_write_page_addr32(addr=addr, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_jump(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_JUMP_OPCODE, self.HWID, self.msgid, GND, dst)
        self._send_and_wait(cmd)
    
    def send_alive(self, destid):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, destid)
        cmd.common_data([0x00,0x01,0x01])
        self._send_and_wait(cmd)
        
    def handshake(self):
        print("initiating uart handshake...")
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COMG)
        cmd.common_data([0x00,0x01,0x01]) # send alive
        success = self._send_and_wait(cmd, timeout=2.0, retries=5)
        if success:
            print("uart handshake successful.\n")
        else:
            print("uart handshake failed. please check the connection to the comG board.\n")
            sys.exit(1)
        

    # ==== Updated to route directly to CDH ==== #
    def cdh_enable_pay(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, CDH)
        cmd.common_data([0x02, 0x01, val])
        self._send_and_wait(cmd)

    def cdh_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, CDH)
        cmd.common_data([0x06, 0x01, 0x01])
        self._send_and_wait(cmd)
        
    # ==== Coral Micro Payload Commands ==== #
    def cdh_coral_wake(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, CDH)
        cmd.common_data([0x08, 0x01, val])
        self._send_and_wait(cmd)
        
    def cdh_coral_cam_on(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, CDH)
        cmd.common_data([0x09, 0x01, val])
        self._send_and_wait(cmd)
        
    def cdh_coral_infer_denby(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, PLD)
        cmd.common_data([0x0A, 0x01, val])
        self._send_and_wait(cmd)

    def cdh_coral_infer_blk(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, PLD)
        cmd.common_data([0x0D, 0x01, val])
        self._send_and_wait(cmd)

    def coral_run_demo(self, enable=True):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, PLD)
        cmd.common_data([0x0b, 0x01, 0x01])
        self._send_and_wait(cmd)

        
    def wait_for_inference(self, timeout=60.0):
        print("waiting for autonomous inference results...")
        start_time = time.time()
        self.rx_cmd_buff.clear()
        
        while time.time() - start_time < timeout:
            if self.serial_port.in_waiting > 0:
                bytes_read = self.serial_port.read(1)
                for b in bytes_read:
                    self.rx_cmd_buff.append_byte(b)
                    
                    if self.rx_cmd_buff.state == RxCmdBuffState.COMPLETE:
                        if self.rx_cmd_buff.data[8] == COMMON_DATA_OPCODE:
                            if self.rx_cmd_buff.data[9] == 0x0c: # VAR_CODE_INFERENCE_RESULT
                                result_len = self.rx_cmd_buff.data[10]
                                result_bytes = self.rx_cmd_buff.data[11:11+result_len]
                                try:
                                    result_str = bytes(result_bytes).decode('utf-8')
                                    print(f"--- Inference Result: {result_str} ---")
                                except UnicodeDecodeError:
                                    print(f"--- Inference Result (raw bytes): {result_bytes} ---")
                                self.rx_cmd_buff.clear()
                                return True
                        self.rx_cmd_buff.clear()
            else:
                time.sleep(0.001)
                
        print("timeout waiting for inference result.\n")
        return False
    # ========================================== #

    def enable_rf(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data([0x03, 0x01, val])
        self._send_and_wait(cmd)

    def enable_tx(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data([0x04, 0x01, val]) 
        self._send_and_wait(cmd)

    def enable_rx(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data([0x05, 0x01, val]) 
        self._send_and_wait(cmd)
    
    def com_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COM)
        cmd.common_data([0x07, 0x01, 0x01])
        self._send_and_wait(cmd)

    def comg_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, GND, COMG)
        cmd.common_data([0x07, 0x01, 0x01])
        self._send_and_wait(cmd)



if __name__ == '__main__':
    # parse script arguments
    port = '/dev/ttyACM0' # default
    if len(sys.argv) == 2:
        port = sys.argv[1]
    elif len(sys.argv) > 2:
        print('usage: python3 demo.py [/path/to/dev]')
        sys.exit(1)

    print(f"initializing pcb communication on {port}...")
    flatsat = PCB(port=port, BAUD=115200, HWID=0xccc3, msgid=0x0000)
    try:
        # wait for device and connect
        flatsat._wait_for_serial(timeout=60)

        input("\nthe device is connected. press ENTER to begin running tests...\n")
        time.sleep(1.0)

        print("--- Commencing Tests ---")
        print("--------------------------------")
        print("Verify Connection to Ground Station")
        flatsat.handshake()

        # print("--- Blinking Ground Station---") 
        # flatsat.comg_blink_demo()
        # print("blinked comg")

        # time.sleep(10.0) # time for the iee stack to boot
        # print("--- Blinking COM ---")
        # test leds - com
        # flatsat.com_blink_demo()
        # print("blinked com")

        # time.sleep(10.0) 
        
        # print("--- Blinking CDH ---")
        # flatsat.cdh_blink_demo()
        # print("blinked cdh")

        # time.sleep(10.0)


        # print("--- Testing Payload Routing & Inference ---")

        # # enable payload eps
        # print("Powering up Payload PCB from CDH...")
        # flatsat.cdh_enable_pay(enable=True)
        
        # time.sleep(15.0)

        # print("Waiting for FreeRTOS and Edge TPU to boot...")
        # time.sleep(5.0)

        flatsat.cdh_coral_infer_denby()
        time.sleep(5.0)
        flatsat.wait_for_inference()

        flatsat.cdh_coral_infer_blk()
        flatsat.wait_for_inference()

        print("--- Testing Complete ---")
    except Exception as e:
        print(f"error during execution: {e}")
        sys.exit(1)