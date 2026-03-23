# DEMO 3/19

## Order Of Operations

### 1. Power On

### 2. Initialize Peripherals

Clocks, LEDs, GPIO

### 3. Power on COM via GPIO

Sends a signal to EPS to open COM_3V3

### 4. UART Handshake

CDH will blink util it gets a UART connection with COM

CDH prompts COM until it recieves a return, then blinking ends

### 5. Blink LEDs at Different Rates on Both Boards

First at one speed, then faster for both bords

### 6. Enable the Payload via GPIO

Coral Micro runs on 5V, so as stand-in it waits for the signal from PAY_EN to begin inference
