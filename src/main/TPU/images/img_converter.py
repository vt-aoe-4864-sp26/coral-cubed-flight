import argparse
import numpy as np
from PIL import Image, ImageOps

def convert_image(input_path, output_path, target_size=(320, 320), is_int8=False):
    # Open source image
    img = Image.open(input_path)
    
    # Convert to RGB (strips out alpha channels)
    img = img.convert("RGB")
    
    # Resize with letterboxing (preserve aspect ratio)
    # Using ImageOps.contain or custom padding
    img.thumbnail(target_size, Image.Resampling.LANCZOS)
    
    # Create new black background image
    new_img = Image.new("RGB", target_size, (0, 0, 0))
    # Paste resized image into center
    new_img.paste(img, ((target_size[0] - img.size[0]) // 2,
                         (target_size[1] - img.size[1]) // 2))
    
    # Convert to numpy array
    data = np.array(new_img)
    
    # Shift to int8 if required
    if is_int8:
        # Shift [0, 255] to [-128, 127]
        data = data.astype(np.int16) - 128
        data = data.astype(np.int8)
    
    # Save as pure raw binary
    with open(output_path, "wb") as f:
        f.write(data.tobytes())
    
    print(f"Generated {output_path} ({len(data.tobytes())} bytes) from {input_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert images to raw RGB binary for TPU inference.")
    parser.add_argument("input", help="Input image path (e.g. denby.png)")
    parser.add_argument("--output", help="Output .rgb path (default: input with .rgb extension)")
    parser.add_argument("--size", type=int, default=320, help="Target square dimension (default: 320)")
    parser.add_argument("--int8", action="store_true", help="Quantize to int8 (-128 to 127)")
    
    args = parser.parse_args()
    
    out = args.output if args.output else args.input.rsplit('.', 1)[0] + ".rgb"
    convert_image(args.input, out, (args.size, args.size), args.int8)