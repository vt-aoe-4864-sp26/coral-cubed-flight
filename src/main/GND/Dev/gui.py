import customtkinter as ctk
import requests
import threading
import time
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
        self.handshake_button.grid(row=4, column=0, padx=20, pady=5)
        
        self.sync_id_button = ctk.CTkButton(self.sidebar_frame, text="Sync ID (COM)", font=self.base_font, command=lambda: self.sync_id("com"))
        self.sync_id_button.grid(row=5, column=0, padx=20, pady=5)
        
        self.msgid_label = ctk.CTkLabel(self.sidebar_frame, text="Last Msg ID: --", font=self.base_font)
        self.msgid_label.grid(row=6, column=0, padx=20, pady=10)
        
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
        
        self.result_name_entry = ctk.CTkEntry(self.main_frame, placeholder_text="Result Name (8 chars)", font=self.base_font)
        self.result_name_entry.grid(row=4, column=0, padx=20, pady=10)
        self.result_name_entry.insert(0, "RESULT01")
        
        self.infer_button = ctk.CTkButton(self.main_frame, text="Confirm & Send Inference", font=self.base_font, height=40, command=self.run_inference)
        self.infer_button.grid(row=5, column=0, padx=20, pady=10)

        # NVS Safety
        self.reset_id_button = ctk.CTkButton(self.main_frame, text="Reset NVS IDs", fg_color="red", hover_color="darkred", font=self.base_font, command=self.reset_ids)
        self.reset_id_button.grid(row=5, column=0, padx=20, pady=10)
        self.clear_queue_button = ctk.CTkButton(self.main_frame, text="Clear NVS Queue", fg_color="red", hover_color="darkred", font=self.base_font, command=self.clear_queue)
        self.clear_queue_button.grid(row=6, column=0, padx=20, pady=10)

        # Stored Results Section
        self.fetch_label = ctk.CTkLabel(self.main_frame, text="Stored Results", font=self.title_font)
        self.fetch_label.grid(row=7, column=0, padx=20, pady=(20, 10))
        
        self.fetch_id_entry = ctk.CTkEntry(self.main_frame, placeholder_text="Result Name to Fetch", font=self.base_font)
        self.fetch_id_entry.grid(row=8, column=0, padx=20, pady=10)
        
        self.fetch_button = ctk.CTkButton(self.main_frame, text="Fetch Result", font=self.base_font, fg_color="green", hover_color="darkgreen", command=self.fetch_result)
        self.fetch_button.grid(row=9, column=0, padx=20, pady=5)
        
        self.list_res_button = ctk.CTkButton(self.main_frame, text="List All Results", font=self.base_font, fg_color="blue", hover_color="darkblue", command=self.list_results)
        self.list_res_button.grid(row=10, column=0, padx=20, pady=5)
        
        self.clear_res_button = ctk.CTkButton(self.main_frame, text="Clear All Results", font=self.base_font, fg_color="red", hover_color="darkred", command=self.clear_results)
        self.clear_res_button.grid(row=11, column=0, padx=20, pady=5)

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

        # --- Bottom Right: Terminal Console ---
        self.console_frame = ctk.CTkFrame(self)
        self.console_frame.grid(row=1, column=1, columnspan=2, padx=20, pady=(0, 20), sticky="nsew")
        self.console_frame.grid_columnconfigure(0, weight=1)
        self.console_frame.grid_rowconfigure(1, weight=1)

        self.console_label = ctk.CTkLabel(self.console_frame, text="Satellite Traffic Console", font=self.title_font)
        self.console_label.grid(row=0, column=0, padx=20, pady=(10, 5), sticky="w")

        self.terminal_box = ctk.CTkTextbox(self.console_frame, font=("Courier New", 12), fg_color="black", text_color="#00FF00")
        self.terminal_box.grid(row=1, column=0, padx=10, pady=10, sticky="nsew")
        self.terminal_box.insert("0.0", "--- Waiting for connection ---\n")
        self.terminal_box.configure(state="disabled")
        
        # Start background polling
        self.after(1000, self.poll_unsolicited)
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

    def sync_id(self, board):
        self.log_msg(f"Syncing ID with {board.upper()}...")
        def task():
            try:
                res = requests.post(f"{API_URL}/payload/sync_id/{board}")
                if res.status_code == 200:
                    data = res.json()
                    msg_id = data.get("msg_id")
                    self.log_msg(f"SUCCESS: Synced with {board.upper()}. New ID: {msg_id}")
                    self.msgid_label.configure(text=f"Last Msg ID: {msg_id}")
                else:
                    self.log_msg(f"Error {res.status_code}: {res.text}")
            except Exception as e:
                self.log_msg(f"Request failed: {str(e)}")
        threading.Thread(target=task).start()

    def reset_ids(self):
        self.log_msg("Resetting message IDs...")
        self.api_post("/payload/reset_ids")

    def clear_queue(self):
        self.log_msg("Clearing NVS Queue...")
        self.api_post("/payload/clear_queue")

    def run_inference(self):
        choice = self.image_choice.get()
        name = self.result_name_entry.get().strip()
        if not name:
            self.log_msg("Error: Please enter a result name.")
            return
        
        endpoint = f"/payload/infer/{choice}?name={name}"
        self.log_msg(f"Triggering {choice.upper()} inference as '{name}'...")
        
        def task():
            try:
                res = requests.post(f"{API_URL}{endpoint}")
                if res.status_code == 200:
                    data = res.json()
                    res_name = data.get("name")
                    self.log_msg(f"SUCCESS: Inference triggered as '{res_name}'")
                    # Auto-fill the fetch entry
                    self.fetch_id_entry.delete(0, "end")
                    self.fetch_id_entry.insert(0, str(res_name))
                else:
                    self.log_msg(f"Error {res.status_code}: {res.text}")
            except Exception as e:
                self.log_msg(f"Request failed: {str(e)}")
        threading.Thread(target=task).start()

    def fetch_result(self):
        name = self.fetch_id_entry.get().strip()
        if not name:
            self.log_msg("Error: Please enter a result name to fetch.")
            return
        
        self.log_msg(f"Fetching result for '{name}'...")
        self.api_post(f"/payload/fetch_result/{name}")

    def list_results(self):
        self.log_msg("Listing all results on payload...")
        def task():
            try:
                res = requests.get(f"{API_URL}/payload/list_results")
                if res.status_code == 200:
                    results = res.json().get("results", [])
                    if results:
                        self.log_msg("Results found:\n" + "\n".join(results))
                    else:
                        self.log_msg("No results found.")
                else:
                    self.log_msg(f"Error {res.status_code}: {res.text}")
            except Exception as e:
                self.log_msg(f"Request failed: {str(e)}")
        threading.Thread(target=task).start()

    def clear_results(self):
        self.log_msg("Clearing all payload results...")
        self.api_post("/payload/clear_results")

    def poll_unsolicited(self):
        def task():
            try:
                res = requests.get(f"{API_URL}/payload/unsolicited", timeout=2)
                if res.status_code == 200:
                    messages = res.json().get("messages", [])
                    if messages:
                        print(f"DEBUG: GUI received {len(messages)} messages")
                        # Schedule GUI update on main thread
                        self.after(0, lambda: self.update_terminal(messages))
            except Exception as e:
                print(f"DEBUG: GUI poll error: {e}")
                pass
        
        # Start task thread
        threading.Thread(target=task, daemon=True).start()
        # Schedule next poll on main thread
        self.after(500, self.poll_unsolicited)

    def update_terminal(self, messages):
        print(f"DEBUG: UI updating terminal with {len(messages)} messages")
        self.terminal_box.configure(state="normal")
        for msg in messages:
            timestamp = time.strftime("[%H:%M:%S] ")
            self.terminal_box.insert("end", f"{timestamp}{msg}\n")
        self.terminal_box.see("end")
        self.terminal_box.configure(state="disabled")

if __name__ == "__main__":
    app = App()
    app.mainloop()
