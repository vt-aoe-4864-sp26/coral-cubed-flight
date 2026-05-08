import customtkinter as ctk
import requests
import threading
from PIL import Image
import os

API_URL = "http://127.0.0.1:8000"

ctk.set_appearance_mode("Dark")  # Modes: "System" (standard), "Dark", "Light"
ctk.set_default_color_theme("blue")  # Themes: "blue" (standard), "green", "dark-blue"

class App(ctk.CTk):
    def __init__(self):
        super().__init__()

        self.title("Coral Cubed Flatsat Control Panel")
        self.geometry("900x700")
        
        # Global Font Sizes
        self.base_font = ctk.CTkFont(size=18)
        self.title_font = ctk.CTkFont(size=22, weight="bold")
        self.logo_font = ctk.CTkFont(size=26, weight="bold")
        
        # Configure grid layout
        self.grid_columnconfigure(1, weight=1)
        self.grid_columnconfigure(2, weight=1)
        self.grid_rowconfigure(0, weight=1)
        
        # --- Sidebar ---
        self.sidebar_frame = ctk.CTkFrame(self, width=250, corner_radius=0)
        self.sidebar_frame.grid(row=0, column=0, rowspan=4, sticky="nsew")
        self.sidebar_frame.grid_rowconfigure(6, weight=1)
        
        self.logo_label = ctk.CTkLabel(self.sidebar_frame, text="Flatsat Control", font=self.logo_font)
        self.logo_label.grid(row=0, column=0, padx=20, pady=(20, 10))
        
        self.port_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="Port (e.g. /dev/ttyACM0)", font=self.base_font)
        self.port_entry.grid(row=1, column=0, padx=20, pady=10)
        self.port_entry.insert(0, "/dev/ttyACM0")
        
        self.baud_entry = ctk.CTkEntry(self.sidebar_frame, placeholder_text="Baud Rate", font=self.base_font)
        self.baud_entry.grid(row=2, column=0, padx=20, pady=10)
        self.baud_entry.insert(0, "115200")
        
        self.connect_button = ctk.CTkButton(self.sidebar_frame, text="Connect", font=self.base_font, command=self.connect_to_api)
        self.connect_button.grid(row=3, column=0, padx=20, pady=10)
        
        self.handshake_button = ctk.CTkButton(self.sidebar_frame, text="Handshake", font=self.base_font, command=self.handshake)
        self.handshake_button.grid(row=4, column=0, padx=20, pady=10)
        
        self.msgid_label = ctk.CTkLabel(self.sidebar_frame, text="Last Msg ID: --", font=self.base_font)
        self.msgid_label.grid(row=5, column=0, padx=20, pady=10)
        
        # Status Log
        self.status_box = ctk.CTkTextbox(self.sidebar_frame, height=200, font=self.base_font)
        self.status_box.grid(row=6, column=0, padx=20, pady=20, sticky="nsew")
        self.log_msg("Ready.")

        # --- Middle View: Inference Commands ---
        self.main_frame = ctk.CTkFrame(self)
        self.main_frame.grid(row=0, column=1, padx=20, pady=20, sticky="nsew")
        self.main_frame.grid_columnconfigure(0, weight=1)
        
        self.main_label = ctk.CTkLabel(self.main_frame, text="Inference Commands", font=self.title_font)
        self.main_label.grid(row=0, column=0, padx=20, pady=(20, 10))
        
        self.image_choice = ctk.StringVar(value="denby")
        self.choice_denby = ctk.CTkRadioButton(self.main_frame, text="Denby Image", variable=self.image_choice, value="denby", font=self.base_font, command=self.update_image_preview)
        self.choice_denby.grid(row=1, column=0, padx=20, pady=10)
        
        self.choice_blk = ctk.CTkRadioButton(self.main_frame, text="Black Image", variable=self.image_choice, value="blk", font=self.base_font, command=self.update_image_preview)
        self.choice_blk.grid(row=2, column=0, padx=20, pady=10)
        
        # Image Preview
        self.image_label = ctk.CTkLabel(self.main_frame, text="")
        self.image_label.grid(row=3, column=0, padx=20, pady=20)
        self.load_images()
        self.update_image_preview()
        
        self.infer_button = ctk.CTkButton(self.main_frame, text="Confirm & Send Inference", font=self.base_font, height=40, command=self.run_inference)
        self.infer_button.grid(row=4, column=0, padx=20, pady=20)

        # NVS Safety
        self.reset_id_button = ctk.CTkButton(self.main_frame, text="Reset NVS IDs", fg_color="red", hover_color="darkred", font=self.base_font, command=self.reset_ids)
        self.reset_id_button.grid(row=5, column=0, padx=20, pady=10)
        self.clear_queue_button = ctk.CTkButton(self.main_frame, text="Clear NVS Queue", fg_color="red", hover_color="darkred", font=self.base_font, command=self.clear_queue)
        self.clear_queue_button.grid(row=6, column=0, padx=20, pady=10)

        # --- Right View: Blink & Debug Commands ---
        self.payload_frame = ctk.CTkFrame(self)
        self.payload_frame.grid(row=0, column=2, padx=20, pady=20, sticky="nsew")
        self.payload_frame.grid_columnconfigure(0, weight=1)
        self.payload_frame.grid_columnconfigure(1, weight=1)
        
        self.payload_label = ctk.CTkLabel(self.payload_frame, text="Board Commands", font=self.title_font)
        self.payload_label.grid(row=0, column=0, columnspan=2, padx=20, pady=(20, 10))
        
        self.blink_com_btn = ctk.CTkButton(self.payload_frame, text="Blink COM", font=self.base_font, height=50, command=lambda: self.blink_board("com"))
        self.blink_com_btn.grid(row=1, column=0, padx=10, pady=10, sticky="ew")
        self.debug_com_btn = ctk.CTkButton(self.payload_frame, text="Debug COM", font=self.base_font, height=50, fg_color="gray", hover_color="darkgray", command=lambda: self.debug_board("com"))
        self.debug_com_btn.grid(row=1, column=1, padx=10, pady=10, sticky="ew")
        
        self.blink_comg_btn = ctk.CTkButton(self.payload_frame, text="Blink COMG", font=self.base_font, height=50, command=lambda: self.blink_board("comg"))
        self.blink_comg_btn.grid(row=2, column=0, padx=10, pady=10, sticky="ew")
        self.debug_comg_btn = ctk.CTkButton(self.payload_frame, text="Debug COMG", font=self.base_font, height=50, fg_color="gray", hover_color="darkgray", command=lambda: self.debug_board("comg"))
        self.debug_comg_btn.grid(row=2, column=1, padx=10, pady=10, sticky="ew")
        
        self.blink_cdh_btn = ctk.CTkButton(self.payload_frame, text="Blink CDH", font=self.base_font, height=50, command=lambda: self.blink_board("cdh"))
        self.blink_cdh_btn.grid(row=3, column=0, padx=10, pady=10, sticky="ew")
        self.debug_cdh_btn = ctk.CTkButton(self.payload_frame, text="Debug CDH", font=self.base_font, height=50, fg_color="gray", hover_color="darkgray", command=lambda: self.debug_board("cdh"))
        self.debug_cdh_btn.grid(row=3, column=1, padx=10, pady=10, sticky="ew")

        # Start background polling
        self.check_status()

    def load_images(self):
        denby_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "TPU", "images", "denby.png"))
        
        if os.path.exists(denby_path):
            try:
                pil_denby = Image.open(denby_path)
                self.ctk_denby = ctk.CTkImage(light_image=pil_denby, dark_image=pil_denby, size=(200, 200))
            except Exception as e:
                self.ctk_denby = None
                self.log_msg(f"Failed to load {denby_path}: {e}")
        else:
            self.ctk_denby = None
            self.log_msg(f"Warning: {denby_path} not found.")

        # Create a solid black image dynamically using Pillow
        pil_blk = Image.new('RGB', (200, 200), color='black')
        self.ctk_blk = ctk.CTkImage(light_image=pil_blk, dark_image=pil_blk, size=(200, 200))

    def update_image_preview(self):
        choice = self.image_choice.get()
        if choice == "denby" and hasattr(self, 'ctk_denby') and self.ctk_denby:
            self.image_label.configure(image=self.ctk_denby, text="")
        elif choice == "blk":
            self.image_label.configure(image=self.ctk_blk, text="")
        else:
            self.image_label.configure(image="", text="[Image Not Found]")

    def check_status(self):
        def task():
            try:
                res = requests.get(f"{API_URL}/status", timeout=1)
                if res.status_code == 200:
                    data = res.json()
                    connected = data.get("connected", False)
                    msgid = data.get("msg_id", 0)
                    
                    if connected:
                        self.connect_button.configure(fg_color="green", hover_color="darkgreen", text="Connected")
                        self.msgid_label.configure(text=f"Last Msg ID: {msgid}")
                    else:
                        self.connect_button.configure(fg_color="red", hover_color="darkred", text="Disconnected")
                else:
                    self.connect_button.configure(fg_color="red", hover_color="darkred", text="Disconnected")
            except Exception:
                self.connect_button.configure(fg_color="red", hover_color="darkred", text="Disconnected")
                
        threading.Thread(target=task).start()
        # Poll again every 1 second
        self.after(1000, self.check_status)

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

    def blink_board(self, board):
        self.log_msg(f"Blinking {board.upper()}...")
        self.api_post(f"/blink/{board}")

    def debug_board(self, board):
        self.log_msg(f"Sending Debug to {board.upper()}...")
        self.api_post(f"/debug/{board}")

    def reset_ids(self):
        self.log_msg("Resetting message IDs...")
        self.api_post("/payload/reset_ids")

    def clear_queue(self):
        self.log_msg("Clearing NVS Queue...")
        self.api_post("/payload/clear_queue")

    def run_inference(self):
        choice = self.image_choice.get()
        if choice == "denby":
            self.log_msg("Running Denby Inference (60s timeout)...")
            self.api_post("/payload/infer/denby")
        elif choice == "blk":
            self.log_msg("Running Black Image Inference (60s timeout)...")
            self.api_post("/payload/infer/blk")

if __name__ == "__main__":
    app = App()
    app.mainloop()
