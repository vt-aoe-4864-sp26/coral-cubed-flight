from fastapi import FastAPI, HTTPException
from fastapi.responses import RedirectResponse
from pydantic import BaseModel
from typing import Optional
import threading
import json
import os

from demo import PCB, GND

app = FastAPI(title="Flatsat Operations API", description="API for controlling the flatsat via the ground station.", version="1.0.0")

# Global PCB instance and lock to ensure thread safety when communicating over serial
flatsat: Optional[PCB] = None # TODO: Update for optional source ID calls
serial_lock = threading.Lock()
MSG_ID_FILE = "msg_id.json"

def save_msg_id(msg_id):
    with open(MSG_ID_FILE, "w") as f:
        json.dump({"msg_id": msg_id}, f)

def load_msg_id():
    if os.path.exists(MSG_ID_FILE):
        try:
            with open(MSG_ID_FILE, "r") as f:
                return json.load(f).get("msg_id", 0)
        except:
            return 0
    return 0

class ConnectRequest(BaseModel):
    port: str = "/dev/ttyACM0"
    baud: int = 115200

class BooleanRequest(BaseModel):
    enable: bool

@app.get("/")
def read_root():
    return RedirectResponse(url="/docs")

@app.get("/status")
def get_status():
    global flatsat
    if flatsat:
        return {"connected": True, "msg_id": flatsat.msgid}
    return {"connected": False, "msg_id": 0}

@app.post("/connect")
def connect(req: ConnectRequest):
    global flatsat
    with serial_lock:
        try:
            initial_id = load_msg_id()
            flatsat = PCB(port=req.port, BAUD=req.baud, ID=GND, msgid=initial_id)
            flatsat._wait_for_serial(timeout=10)
            flatsat.start()
            return {"status": "connected", "port": req.port, "msg_id": initial_id}
        except Exception as e:
            flatsat = None
            error_msg = str(e)
            if "Device or resource busy" in error_msg:
                error_msg += " (Make sure the port is not open in another program like VS Code Serial Monitor)"
            raise HTTPException(status_code=500, detail=f"Failed to connect: {error_msg}")

@app.post("/handshake")
def handshake():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            # We don't want to sys.exit(1) on failure like demo.py does, 
            # so we'll wrap this or we can call it. Wait, demo.py's handshake calls sys.exit(1).
            # Let's write our own logic here or modify demo.py?
            # actually, demo.py: 
            # success = self._send_and_wait(cmd, timeout=2.0, retries=5)
            # if success: ... else: sys.exit(1)
            # We can't let it exit the server. So we bypass `flatsat.handshake()` 
            # and just call `send_alive` and wait for a response, or just call it directly.
            # But let's just do it manually here to avoid exiting.
            from demo import TxCmd, COMMON_DATA_OPCODE, COMG
            cmd = TxCmd(COMMON_DATA_OPCODE, flatsat.HWID, flatsat.msgid, flatsat.ID, COMG)
            cmd.common_data([0x00,0x01,0x01]) # send alive
            success = flatsat._send_and_wait(cmd, timeout=2.0, retries=5)
            if success:
                return {"status": "handshake_successful"}
            else:
                raise HTTPException(status_code=500, detail="Handshake failed")
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/blink/{board}")
def blink(board: str):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            if board.lower() == "com":
                flatsat.com_blink_demo()
            elif board.lower() == "comg":
                flatsat.comg_blink_demo()
            elif board.lower() == "cdh":
                flatsat.cdh_blink_demo()
            else:
                raise HTTPException(status_code=400, detail="Invalid board. Use com, comg, or cdh.")
            return {"status": f"Blink command sent to {board}"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/power")
def payload_power(req: BooleanRequest):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.cdh_enable_pay(enable=req.enable)
            return {"status": "Payload power toggled", "enabled": req.enable}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/infer/denby")
def payload_infer_denby(name: str = "RESULT01"):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.cdh_coral_infer_denby(name=name)
            return {"status": "Denby Inference triggered", "name": name}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/infer/blk")
def payload_infer_blk(name: str = "RESULT01"):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.cdh_coral_infer_blk(name=name)
            return {"status": "Black Image Inference triggered", "name": name}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/fetch_result/{name}")
def payload_fetch_result(name: str):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            success = flatsat.cdh_coral_fetch_result(name)
            if success:
                return {"status": f"Result for {name} retrieved"}
            else:
                raise HTTPException(status_code=500, detail=f"Failed to retrieve result for {name}")
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/clear_results")
def payload_clear_results():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.cdh_coral_clear_results()
            save_msg_id(flatsat.msgid)
            return {"status": "Payload inference results cleared"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.get("/payload/list_results")
def payload_list_results():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            # wait_for_inference returns a string for the list results command
            results = flatsat.cdh_coral_list_results()
            if results:
                # results is likely "ID name1: content" or similar if we use wait_for_inference
                # but we updated coral_main.cc to send just the list
                # actually, wait_for_inference prints and returns data
                return {"results": results.split('\n') if results else []}
            return {"results": []}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/reset_ids")
def payload_reset_ids():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.reset_message_ids()
            return {"status": "Message IDs reset to 0"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/clear_queue")
def payload_clear_queue():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.clear_cdh_queue()
            return {"status": "CDH Command Queue cleared"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/radio/{action}")
def radio_enable(action: str, req: BooleanRequest):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            if action.lower() == "rf":
                flatsat.enable_rf(enable=req.enable)
            elif action.lower() == "tx":
                flatsat.enable_tx(enable=req.enable)
            elif action.lower() == "rx":
                flatsat.enable_rx(enable=req.enable)
            else:
                raise HTTPException(status_code=400, detail="Invalid action. Use rf, tx, or rx.")
            
            return {"status": f"Radio {action} toggled", "enabled": req.enable}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/debug/{board}")
def debug_board(board: str):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            from demo import CDH, COM, COMG
            dst_map = {"cdh": CDH, "com": COM, "comg": COMG}
            dst = dst_map.get(board.lower())
            if dst is None:
                raise HTTPException(status_code=400, detail=f"Invalid board: {board}")
            
            flatsat.common_debug(message="check", dst=dst)
            save_msg_id(flatsat.msgid)
            return {"status": f"Debug message sent to {board}"}
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.post("/payload/sync_id/{board}")
def payload_sync_id(board: str):
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            from demo import CDH, COM, COMG
            dst_map = {"cdh": CDH, "com": COM, "comg": COMG}
            dst = dst_map.get(board.lower())
            if dst is None:
                raise HTTPException(status_code=400, detail=f"Invalid board: {board}")
            
            success = flatsat.sync_message_id(dst=dst)
            if success:
                save_msg_id(flatsat.msgid)
                return {"status": f"Synced with {board}", "msg_id": flatsat.msgid}
            else:
                raise HTTPException(status_code=500, detail=f"Sync failed with {board}")
        except Exception as e:
            raise HTTPException(status_code=500, detail=str(e))

@app.get("/payload/unsolicited")
def get_unsolicited_messages():
    if not flatsat:
        return {"messages": []}
    
    messages = []
    try:
        while not flatsat.unsolicited_queue.empty():
            messages.append(flatsat.unsolicited_queue.get_nowait())
        return {"messages": messages}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
