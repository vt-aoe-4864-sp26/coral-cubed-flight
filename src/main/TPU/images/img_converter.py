from PIL import Image
import numpy as np

# Set these to match what your C++ printf statement told you!
TARGET_WIDTH = 320
TARGET_HEIGHT = 320
IS_INT8_MODEL = False # Set to True if tensor type is 9

# Open your source image
img = Image.open("rect1.png")

# Resize to the EXACT dimensions the tensor expects
img = img.resize((TARGET_WIDTH, TARGET_HEIGHT))

# Convert to RGB (strips out alpha channels if it was a PNG)
img = img.convert("RGB")

# Convert to numpy array
data = np.array(img)

# Shift to int8 if required by the model
if IS_INT8_MODEL:
    # Shift [0, 255] to [-128, 127]
    data = data.astype(np.int16) - 128
    data = data.astype(np.int8)

# Save as pure raw binary
with open("blk.rgb", "wb") as f:
    f.write(data.tobytes())

print(f"Generated blk.rgb ({len(data.tobytes())} bytes)")