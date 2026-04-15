# DEMO 4/9

## Order Of Operations

### 1. Verify Local Connection

Send a Message Back and Forth

Blink Locally

### 2. Send Blink Message to COM

COM sees the message ID it for it and it will process it locally.

### 3. Send Blink Message to CDH

This message passes through COM, and COM routes the message ID to its UART port for CDH.

### 4. Command PLD to  Perform Inference

COMG sends over RF, Recieved by COM, passed to CDH, and then finally routed to the Coral.

This inference is performed on a statically defined image from nonvolatile storage.

![denby](TPU/images/denby.png)

A successul inference will return the message below:

```bash
INFER_OK
```

That message is routed from PLD, through CDH to COM, blasted down through RF, recieved by COMG, and sent up over UART to the Host PC.
