import customtkinter as ctk
import requests
import threading

API_URL = "http://127.0.0.1:8000"

ctk.set_appearance_mode("Dark")  # Modes: "System" (standard), "Dark", "Light"
ctk.set_default_color_theme("blue")  # Themes: "blue" (standard), "green", "dark-blue"

class App(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("Coral Cubed Flatsat Control Panel")
        self.geometry("800x600")
        
        # Configure grid layout
        self.grid_columnconfigure(1, weight=1)
        self.grid_columnconfigure(2, weight=1)
        self.grid_rowconfigure(0, weight=1)
        
        # --- Sidebar ---
        self.sidebar_frame = ctk.CTkFrame(self, width=200, corner_radius=0)
        self.sidebar_frame.grid(row=0, column=0, rowspan=4, sticky="nsew")
        self.sidebar_frame.grid_rowconfigure(6, weight=1)
        
        self.logo_label = ctk.CTkLabel(self.sidebar_frame, text="Flatsat Control", font=ctk.CTkFont(size=20, weight="bold"))
        self.logo_label.grid(row=0, column=0, padx=20, pady=(20, 10))
        
        self.port_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="Port (e.g. /dev/ttyACM0)")
        self.port_entry.grid(row=1, column=0, padx=20, pady=10)
        self.port_entry.insert(0, "/dev/ttyACM0")
        
        self.baud_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="Baud Rate")
        self.baud_entry.grid(row=2, column=0, padx=20, pady=10)
        self.baud_entry.insert(0, "115200")
        
        self.connect_button = ctk.CTkButton(self.sidebar_frame, text="Connect", command=self.connect_to_api)
        self.connect_button.grid(row=3, column=0, padx=20, pady=10)
        
        self.handshake_button = ctk.CTkButton(self.sidebar_frame, text="Handshake", command=self.handshake)
        self.handshake_button.grid(row=4, column=0, padx=20, pady=10)
        
        # Status Log
        self.status_box = ctk.CTkTextbox(self.sidebar_frame, height=150)
        self.status_box.grid(row=6, column=0, padx=20, pady=20, sticky="nsew")
        self.log_msg("Ready.")

        # --- Main View: Routing & Basic Commands ---
        self.main_frame = ctk.CTkFrame(self)
        self.main_frame.grid(row=0, column=1, padx=20, pady=20, sticky="nsew")
        
        self.main_label = ctk.CTkLabel(self.main_frame, text="System Commands", font=ctk.CTkFont(size=16, weight="bold"))
        self.main_label.grid(row=0, column=0, columnspan=2, padx=20, pady=(20, 10))
        
        # Blinking
        self.blink_board_var = ctk.StringVar(value="com")
        self.blink_combobox = ctk.CTkComboBox(self.main_frame, values=["com", "comg", "cdh"], variable=self.blink_board_var)
        self.blink_combobox.grid(row=1, column=0, padx=20, pady=10)
        self.blink_button = ctk.CTkButton(self.main_frame, text="Blink Board", command=self.blink_board)
        self.blink_button.grid(row=1, column=1, padx=20, pady=10)
        
        # NVS Safety
        self.reset_id_button = ctk.CTkButton(self.main_frame, text="Reset NVS IDs", fg_color="red", hover_color="darkred", command=self.reset_ids)
        self.reset_id_button.grid(row=2, column=0, padx=20, pady=10)
        self.clear_queue_button = ctk.CTkButton(self.main_frame, text="Clear NVS Queue", fg_color="red", hover_color="darkred", command=self.clear_queue)
        self.clear_queue_button.grid(row=2, column=1, padx=20, pady=10)

        # --- Main View: Payload Commands ---
        self.payload_frame = ctk.CTkFrame(self)
        self.payload_frame.grid(row=0, column=2, padx=20, pady=20, sticky="nsew")
        
        self.payload_label = ctk.CTkLabel(self.payload_frame, text="Payload Commands", font=ctk.CTkFont(size=16, weight="bold"))
        self.payload_label.grid(row=0, column=0, columnspan=2, padx=20, pady=(20, 10))
        
        self.pwr_var = ctk.BooleanVar(value=False)
        self.pwr_switch = ctk.CTkSwitch(self.payload_frame, text="Payload Power", variable=self.pwr_var, command=self.toggle_power)
        self.pwr_switch.grid(row=1, column=0, padx=20, pady=10, sticky="w")
        
        self.run_demo_button = ctk.CTkButton(self.payload_frame, text="Run Inference Demo", command=self.run_inference)
        self.run_demo_button.grid(row=2, column=0, padx=20, pady=10, sticky="w")
        
        # --- Main View: Radio Commands ---
        self.radio_label = ctk.CTkLabel(self.payload_frame, text="Radio Controls", font=ctk.CTkFont(size=16, weight="bold"))
        self.radio_label.grid(row=3, column=0, columnspan=2, padx=20, pady=(20, 10))
        
        self.rf_var = ctk.BooleanVar(value=False)
        self.rf_switch = ctk.CTkSwitch(self.payload_frame, text="Enable RF", variable=self.rf_var, command=lambda: self.toggle_radio("rf", self.rf_var.get()))
        self.rf_switch.grid(row=4, column=0, padx=20, pady=10, sticky="w")
        
        self.tx_var = ctk.BooleanVar(value=False)
        self.tx_switch = ctk.CTkSwitch(self.payload_frame, text="Enable TX", variable=self.tx_var, command=lambda: self.toggle_radio("tx", self.tx_var.get()))
        self.tx_switch.grid(row=5, column=0, padx=20, pady=10, sticky="w")
        
        self.rx_var = ctk.BooleanVar(value=False)
        self.rx_switch = ctk.CTkSwitch(self.payload_frame, text="Enable RX", variable=self.rx_var, command=lambda: self.toggle_radio("rx", self.rx_var.get()))
        self.rx_switch.grid(row=6, column=0, padx=20, pady=10, sticky="w")

    def log_msg(self, msg):
        self.status_box.insert("end", msg + "\n")
        self.status_box.see("end")

    def api_post(self, endpoint, json=None):
        def task():
            try:
                res = requests.post(f"{API_URL}{endpoint}", json=json)
                if res.status_code == 200:
                    self.log_msg(f"Success: {endpoint}")
                else:
                    self.log_msg(f"Error {res.status_code}: {res.text}")
            except Exception as e:
                self.log_msg(f"Request failed: {str(e)}")
        threading.Thread(target=task).start()

    def connect_to_api(self):
        port = self.port_entry.get()
        try:
            baud = int(self.baud_entry.get())
        except:
            self.log_msg("Invalid Baud")
            return
        self.log_msg(f"Connecting to {port} @ {baud}...")
        self.api_post("/connect", {"port": port, "baud": baud})

    def handshake(self):
        self.log_msg("Sending handshake...")
        self.api_post("/handshake")

    def blink_board(self):
        board = self.blink_board_var.get()
        self.log_msg(f"Blinking {board}...")
        self.api_post(f"/blink/{board}")

    def reset_ids(self):
        self.log_msg("Resetting message IDs...")
        self.api_post("/payload/reset_ids")

    def clear_queue(self):
        self.log_msg("Clearing NVS Queue...")
        self.api_post("/payload/clear_queue")

    def toggle_power(self):
        enable = self.pwr_var.get()
        self.log_msg(f"Setting Payload Power: {enable}")
        self.api_post("/payload/power", {"enable": enable})

    def toggle_radio(self, action, enable):
        self.log_msg(f"Setting Radio {action.upper()}: {enable}")
        self.api_post(f"/radio/{action}", {"enable": enable})

    def run_inference(self):
        self.log_msg("Running Inference (60s timeout)...")
        self.api_post("/payload/run_demo")

if __name__ == "__main__":
    app = App()
    app.mainloop()
