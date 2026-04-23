from fastapi import FastAPI, HTTPException
from fastapi.responses import RedirectResponse
from pydantic import BaseModel
from typing import Optional
import threading

from demo import PCB

app = FastAPI(title="Flatsat Operations API", description="API for controlling the flatsat via the ground station.", version="1.0.0")

# Global PCB instance and lock to ensure thread safety when communicating over serial
flatsat: Optional[PCB] = None
serial_lock = threading.Lock()

class ConnectRequest(BaseModel):
    port: str = "/dev/ttyACM0"
    baud: int = 115200

class BooleanRequest(BaseModel):
    enable: bool

@app.get("/")
def read_root():
    return RedirectResponse(url="/docs")

@app.post("/connect")
def connect(req: ConnectRequest):
    global flatsat
    with serial_lock:
        try:
            flatsat = PCB(port=req.port, BAUD=req.baud)
            flatsat._wait_for_serial(timeout=10)
            return {"status": "connected", "port": req.port}
        except Exception as e:
            flatsat = None
            raise HTTPException(status_code=500, detail=f"Failed to connect: {str(e)}")

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
            from demo import TxCmd, COMMON_DATA_OPCODE, GND, COMG
            cmd = TxCmd(COMMON_DATA_OPCODE, flatsat.HWID, flatsat.msgid, GND, COMG)
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

@app.post("/payload/run_demo")
def payload_run_demo():
    if not flatsat:
        raise HTTPException(status_code=400, detail="Not connected. Call /connect first.")
    
    with serial_lock:
        try:
            flatsat.coral_run_demo()
            success = flatsat.wait_for_inference(timeout=60.0)
            if success:
                # wait_for_inference clears the buffer, but prints to stdout.
                # In demo.py, it doesn't return the result string.
                # We can just say it completed for now. 
                # (Later we could modify demo.py to return the string.)
                return {"status": "Inference completed"}
            else:
                raise HTTPException(status_code=500, detail="Timeout waiting for inference")
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
