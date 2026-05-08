import argparse
import numpy as np
from PIL import Image, ImageOps

def convert_image(input_path, output_path, target_size=(300, 300), is_int8=True):
    # Open source image
    img = Image.open(input_path)
    
    # Convert to RGB (strips out alpha channels)
    img = img.convert("RGB")
    
    # Resize and crop to the target size (centered crop)
    img = ImageOps.fit(img, target_size, Image.Resampling.LANCZOS)
    
    # Convert to numpy array
    data = np.array(img)
    
    # Shift to int8 if required
    if is_int8:
        # Shift [0, 255] to [-128, 127]
        data = data.astype(np.int16) - 128
        data = data.astype(np.int8)
    
    # Save as pure raw binary
    with open(output_path, "wb") as f:
        f.write(data.tobytes())
    
    print(f"Generated {output_path} ({len(data.tobytes())} bytes) from {input_path} (Cropped to {target_size[0]}x{target_size[1]})")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert images to raw RGB binary for TPU inference.")
    parser.add_argument("input", nargs="?", default=".", help="Input image path or directory (default: current directory)")
    parser.add_argument("--output", help="Output .rgb path (only for single file input)")
    parser.add_argument("--size", type=int, default=300, help="Target square dimension (default: 300)")
    parser.add_argument("--int8", action="store_true", help="Quantize to int8 (-128 to 127)")
    
    args = parser.parse_args()
    
    import os
    if os.path.isdir(args.input):
        # Batch process all PNGs in the directory
        for f in os.listdir(args.input):
            if f.lower().endswith(".png"):
                input_path = os.path.join(args.input, f)
                output_path = input_path.rsplit('.', 1)[0] + ".rgb"
                convert_image(input_path, output_path, (args.size, args.size), args.int8)
    else:
        # Single file process
        out = args.output if args.output else args.input.rsplit('.', 1)[0] + ".rgb"
        convert_image(args.input, out, (args.size, args.size), args.int8)