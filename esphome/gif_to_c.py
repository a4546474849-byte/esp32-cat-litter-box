#!/usr/bin/env python3
import sys
import os

def gif_to_c_array(input_path, output_path, array_name="ufo_gif_data"):
    with open(input_path, 'rb') as f:
        data = f.read()
    
    if data[:6] not in [b'GIF87a', b'GIF89a']:
        print("ERROR: Not a GIF file!")
        return
    
    size_kb = len(data) / 1024
    print(f"GIF: {os.path.basename(input_path)}, {len(data)} bytes ({size_kb:.1f} KB)")
    
    with open(output_path, 'w', encoding='ascii') as out:
        out.write(f"// GIF: {os.path.basename(input_path)}\n")
        out.write(f"const unsigned int {array_name}_len = {len(data)};\n")
        out.write(f"const uint8_t {array_name}[] = {{\n")
        
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_bytes = ', '.join(f'0x{b:02X}' for b in chunk)
            if i + 16 < len(data):
                out.write(f"  {hex_bytes},\n")
            else:
                out.write(f"  {hex_bytes}\n")
        
        out.write(f"}};\n")
    
    print(f"Output: {output_path}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python3 gif_to_c.py input.gif output.h")
        sys.exit(1)
    gif_to_c_array(sys.argv[1], sys.argv[2])
