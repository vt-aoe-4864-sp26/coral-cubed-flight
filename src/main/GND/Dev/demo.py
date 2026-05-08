# demo.py

# UART Control of COM board for PDR Demos.
# Requires USB-to-TTL Cable to plug into COM board to plug into TX/RX of spare COM board to act
# in place of a ground station, sending commands to the flatsat COM board.
#
# Written by Jack Rathert
# Other contributors: Nico Demarinis

# See the top-level LICENSE file for the license.

import serial 
import sys, os, time, pathlib, threading, queue

from utils.path_utils import get_repo_root
from utils.tab import *

class PCB:
    def __init__(self, port=None, BAUD=None, HWID=None, ID=None, msgid=None):
        self.ID = ID if ID is not None else DBG
        self.dev = port if port is not None else '/dev/ttyACM0'
        self.BAUD = BAUD if BAUD is not None else 115200
        self.HWID = HWID if HWID is not None else 0xccc3
        self.msgid = msgid if msgid is not None else 0x0000
        
        self.rx_cmd_buff = RxCmdBuff()
        self.serial_port = None
        self.serial_lock = threading.Lock()
        
        self.unsolicited_queue = queue.Queue()
        self.ack_events = {} # msgid -> threading.Event()
        self.ack_packets = {} # msgid -> RxCmdBuff
        self.stop_event = threading.Event()
        self.worker_thread = None

    def start(self):
        if not self.serial_port:
            self._wait_for_serial()
        
        self.stop_event.clear()
        self.worker_thread = threading.Thread(target=self._serial_worker, daemon=True)
        self.worker_thread.start()
        print("Serial worker thread started.")

    def stop(self):
        self.stop_event.set()
        if self.worker_thread:
            self.worker_thread.join(timeout=1.0)
            
    def _serial_worker(self):
        worker_rx_buff = RxCmdBuff()
        while not self.stop_event.is_set():
            try:
                # Use the lock to check in_waiting and read
                with self.serial_lock:
                    if self.serial_port and self.serial_port.in_waiting > 0:
                        waiting = self.serial_port.in_waiting
                        bytes_read = self.serial_port.read(waiting)
                        print(f"DEBUG: Worker read {len(bytes_read)} bytes: {bytes_read.hex()}")
                    else:
                        bytes_read = b''
                
                if bytes_read:
                    for b in bytes_read:
                        worker_rx_buff.append_byte(b)
                        print(f"DEBUG: Byte 0x{b:02x} -> State {worker_rx_buff.state}")
                        if worker_rx_buff.state == RxCmdBuffState.COMPLETE:
                            msg_id = worker_rx_buff.bus_msg_id
                            
                            # Log all incoming packets for visibility
                            print(f"\n<<< Received Packet: ID=0x{msg_id:04x}, Opcode=0x{worker_rx_buff.data[OPCODE_INDEX]:02x}")
                            
                            # Sync local ID if it doesn't match
                            if msg_id != self.msgid:
                                print(f"Syncing local ID (0x{self.msgid:04x}) to satellite ID (0x{msg_id:04x})")
                                self.msgid = msg_id
                            
                            # Always put in traffic queue for GUI console
                            packet_str = str(worker_rx_buff)
                            self.unsolicited_queue.put(packet_str)
                            
                            # Dispatch to ACK waiters
                            if msg_id in self.ack_events:
                                # Copy the buffer state
                                self.ack_packets[msg_id] = RxCmdBuff()
                                self.ack_packets[msg_id].data = list(worker_rx_buff.data)
                                self.ack_packets[msg_id].state = RxCmdBuffState.COMPLETE
                                self.ack_packets[msg_id].bus_msg_id = msg_id
                                self.ack_events[msg_id].set()
                            
                            worker_rx_buff.clear()
                else:
                    time.sleep(0.01)
            except Exception as e:
                print(f"Serial worker error: {e}")
                time.sleep(1.0)

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
        
        # Log the command BEFORE sending
        print('txcmd: ' + str(cmd))
        
        current_id = self.msgid
        
        for attempt in range(retries):
            # Create an event for this message ID
            ack_event = threading.Event()
            self.ack_events[current_id] = ack_event
            
            # Blast the payload
            packet_len = cmd.get_byte_count()
            with self.serial_lock:
                self.serial_port.write(bytes(cmd.data[:packet_len]))
            
            # Wait for the worker thread to signal completion
            if ack_event.wait(timeout=timeout):
                reply = self.ack_packets.get(current_id)
                print('reply: ' + str(reply) + '\n')
                
                # Cleanup
                del self.ack_events[current_id]
                if current_id in self.ack_packets:
                    del self.ack_packets[current_id]
                
                cmd.clear()
                self.msgid += 1
                time.sleep(0.5) # Reduced delay as we are async now
                return True
            else:
                print(f"timeout waiting for reply on attempt {attempt + 1}/{retries}")
                if current_id in self.ack_events:
                    del self.ack_events[current_id]
        
        # If we exit the for-loop, all retries failed
        print("failed to receive valid reply after all retries.\n")
        cmd.clear()
        self.msgid += 1
        return False

    # opcodes

    def common_ack(self, dst=COM):
        cmd = TxCmd(COMMON_ACK_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def common_nack(self, dst=COM):
        cmd = TxCmd(COMMON_NACK_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def common_debug(self, message: str, dst=COM):
        cmd = TxCmd(COMMON_DEBUG_OPCODE, self.HWID, self.msgid, self.ID, dst)
        cmd.common_debug(message)
        self._send_and_wait(cmd)

    def common_data(self, data: list, dst=COM):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, dst)
        cmd.common_data(data)
        self._send_and_wait(cmd)

    def bootloader_ack(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_ACK_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def bootloader_nack(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_NACK_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def bootloader_ping(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_PING_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def bootloader_erase(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_ERASE_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)

    def bootloader_write_page(self, page_number: int, page_data: list, dst=COM):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_OPCODE, self.HWID, self.msgid, self.ID, dst)
        cmd.bootloader_write_page(page_number=page_number, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_write_page_addr32(self, addr: int, page_data: list, dst=COM):
        cmd = TxCmd(BOOTLOADER_WRITE_PAGE_ADDR32_OPCODE, self.HWID, self.msgid, self.ID, dst)
        cmd.bootloader_write_page_addr32(addr=addr, page_data=page_data)
        self._send_and_wait(cmd)

    def bootloader_jump(self, dst=COM):
        cmd = TxCmd(BOOTLOADER_JUMP_OPCODE, self.HWID, self.msgid, self.ID, dst)
        self._send_and_wait(cmd)
    
    def send_alive(self, destid):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, destid)
        cmd.common_data([0x00,0x01,0x01])
        self._send_and_wait(cmd)
        
    def handshake(self):
        print("initiating uart handshake...")
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        cmd.common_data([0x00,0x01,0x01]) # send alive
        success = self._send_and_wait(cmd, timeout=2.0, retries=5)
        if success:
            print("uart handshake successful.\n")
        else:
            print("uart handshake failed. please check the connection to the comG board.\n")
            sys.exit(1)
            
    def reset_message_ids(self, dst=COM):
        # We send a COMMON_ACK with msg_id 0xFFFF to trigger the reset cascade
        cmd = TxCmd(COMMON_ACK_OPCODE, self.HWID, 0xFFFF, self.ID, dst)
        # We don't wait for ACK because resetting will break the flow, just blast it.
        packet_len = cmd.get_byte_count()
        self.serial_port.write(bytes(cmd.data[:packet_len]))
        self.msgid = 0

    def sync_message_id(self, dst=COM):
        # We send a SYNC with msg_id 0 to request the current board ID
        cmd = TxCmd(COMMON_SYNC_MSG_ID_OPCODE, self.HWID, 0x0000, self.ID, dst)
        
        # We need a custom wait logic that extracts the ID from the ACK
        print(f"Syncing message ID with {dst}...")
        start_time = time.time()
        self.rx_cmd_buff.clear()
        self.serial_port.reset_input_buffer()
        packet_len = cmd.get_byte_count()
        self.serial_port.write(bytes(cmd.data[:packet_len]))
        
        while time.time() - start_time < 2.0:
            if self.serial_port.in_waiting > 0:
                bytes_read = self.serial_port.read(self.serial_port.in_waiting)
                for b in bytes_read:
                    self.rx_cmd_buff.append_byte(b)
                    if self.rx_cmd_buff.state == RxCmdBuffState.COMPLETE:
                        # Extract the ID from the ACK packet's MsgID field
                        new_id = self.rx_cmd_buff.bus_msg_id
                        print(f"Synced! New Message ID: {new_id}")
                        self.msgid = new_id + 1 # Increment for next message
                        self.rx_cmd_buff.clear()
                        return True
            time.sleep(0.01)
        print("Sync failed.")
        return False
        
    def clear_cdh_queue(self):
        cmd = TxCmd(COMMON_CLEAR_QUEUE_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        self._send_and_wait(cmd)
        

    # ==== Updated to route directly to CDH ==== #
    def cdh_enable_pay(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        cmd.common_data([0x02, 0x01, val])
        self._send_and_wait(cmd)

    def cdh_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        cmd.common_data([0x06, 0x01, 0x01])
        self._send_and_wait(cmd)
        
    # ==== Coral Micro Payload Commands ==== #
    def cdh_coral_wake(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        cmd.common_data([0x08, 0x01, val])
        self._send_and_wait(cmd)
        
    def cdh_coral_cam_on(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, CDH)
        cmd.common_data([0x09, 0x01, val])
        self._send_and_wait(cmd)
        
    def cdh_coral_infer_denby(self, name="RESULT01"):
        # Ensure name is exactly 8 chars
        name_bytes = list(name[:8].ljust(8, '_').encode('ascii'))
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
        cmd.common_data([0x0A, 0x08] + name_bytes)
        self._send_and_wait(cmd)

    def cdh_coral_infer_blk(self, name="RESULT01"):
        # Ensure name is exactly 8 chars
        name_bytes = list(name[:8].ljust(8, '_').encode('ascii'))
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
        cmd.common_data([0x0D, 0x08] + name_bytes)
        self._send_and_wait(cmd)

    def cdh_coral_fetch_result(self, name):
        # name is the 8-char string of the inference result
        name_bytes = list(name[:8].ljust(8, '_').encode('ascii'))
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
        # Payload: [VAR_CODE_FETCH_RESULT (0x0E), LEN (0x08), name[0..7]]
        cmd.common_data([0x0E, 0x08] + name_bytes)
        # We wait for a reply, which will be the inference result message from the TPU
        success = self._send_and_wait(cmd)
        if success:
             # Wait for the autonomous result packet
             return self.wait_for_inference(timeout=5.0)
        return False

    def cdh_coral_clear_results(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
        # Payload: [VAR_CODE_CLEAR_RESULTS (0x0F), LEN (0x01), 0x01]
        cmd.common_data([0x0F, 0x01, 0x01])
        self._send_and_wait(cmd)

    def cdh_coral_list_results(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
        # Payload: [VAR_CODE_LIST_RESULTS (0x10), LEN (0x00)]
        cmd.common_data([0x10, 0x00])
        success = self._send_and_wait(cmd)
        if success:
             return self.wait_for_inference(timeout=5.0)
        return False

    def coral_run_demo(self, enable=True):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, PLD)
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
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, COM)
        cmd.common_data([0x03, 0x01, val])
        self._send_and_wait(cmd)

    def enable_tx(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, COM)
        cmd.common_data([0x04, 0x01, val]) 
        self._send_and_wait(cmd)

    def enable_rx(self, enable=True):
        val = 0x01 if enable else 0x02
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, COM)
        cmd.common_data([0x05, 0x01, val]) 
        self._send_and_wait(cmd)
    
    def com_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, COM)
        cmd.common_data([0x07, 0x01, 0x01])
        self._send_and_wait(cmd)

    def comg_blink_demo(self):
        cmd = TxCmd(COMMON_DATA_OPCODE, self.HWID, self.msgid, self.ID, COMG)
        cmd.common_data([0x07, 0x01, 0x01])
        self._send_and_wait(cmd)



if __name__ == '__main__':
    # parse script arguments
    port = '/dev/ttyUSB0'  # default UART port for CDH board
    if len(sys.argv) == 2:
        port = sys.argv[1]
    elif len(sys.argv) > 2:
        print('usage: python3 demo.py [/path/to/dev]')
        sys.exit(1)

    print(f"initializing pcb communication on {port}...")
    flatsat = PCB(port=port, BAUD=115200, HWID=0xccc3, ID = DBG, msgid=0x0000)
    try:
        # wait for device and connect
        flatsat._wait_for_serial(timeout=60)

        input("\nthe device is connected. press ENTER to begin running tests...\n")
        time.sleep(1.0)

        print("--- Commencing Tests ---")
        print("--------------------------------")
        print("Verify Connection to CDH\n")
        #flatsat.handshake()

        # com blink demo
        # flatsat.com_blink_demo()
        # print("blinked com")

        # flatsat.cdh_coral_infer_denby()
        # time.sleep(5.0)
        # flatsat.wait_for_inference()

        # flatsat.cdh_coral_infer_blk()
        # flatsat.wait_for_inference()

        # Send acknowledgments to CDH, COM, and PLD
        flatsat.common_debug(message= "check", dst=CDH)
        flatsat.common_debug(message= "check", dst=COM)


        print("--- Testing Complete ---")
    except Exception as e:
        print(f"error during execution: {e}")
        sys.exit(1)