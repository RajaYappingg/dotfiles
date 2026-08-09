#!/usr/bin/env python3
"""
convert_yixuan.py - Convert Windows .ani and .cur cursors to Linux Xcursor theme format
"""
import os
import sys
import struct
import io
import pathlib
from PIL import Image

def parse_bmp_cur_data(img_data, w, h):
    if len(img_data) < 16:
        return None
    dib_size, biW, biH, biPlanes, biBitCount = struct.unpack('<iIIHH', img_data[:16])
    real_h = biH // 2 if biH > h else biH
    
    colors_count = (1 << biBitCount) if biBitCount <= 8 else 0
    palette_size = colors_count * 4
    
    # XOR mask size in bytes (padded to 32-bit boundary)
    row_bytes = ((biW * biBitCount + 31) // 32) * 4
    xor_size = row_bytes * real_h
    
    off_bits = 14 + dib_size + palette_size
    fixed_dib = struct.pack('<iIIHH', dib_size, biW, real_h, biPlanes, biBitCount) + img_data[16:dib_size + palette_size + xor_size]
    bmp_bytes = b'BM' + struct.pack('<IHHI', 14 + len(fixed_dib), 0, 0, off_bits) + fixed_dib
    
    try:
        img = Image.open(io.BytesIO(bmp_bytes)).convert('RGBA')
    except Exception:
        return None

    # Parse 1-bit AND mask for transparency if available
    and_offset = dib_size + palette_size + xor_size
    and_row_bytes = ((biW + 31) // 32) * 4
    and_mask_size = and_row_bytes * real_h
    
    if and_offset + and_mask_size <= len(img_data):
        and_bytes = img_data[and_offset:and_offset + and_mask_size]
        pix = img.load()
        for y in range(real_h):
            # BMP rows are stored bottom-to-top
            bmp_y = real_h - 1 - y
            for x in range(biW):
                byte_idx = bmp_y * and_row_bytes + (x // 8)
                bit_idx = 7 - (x % 8)
                if (and_bytes[byte_idx] >> bit_idx) & 1:
                    r, g, b, _ = pix[x, y]
                    pix[x, y] = (r, g, b, 0)
                    
    return img

def parse_cur_or_ico(data):
    if len(data) < 6:
        return []
    reserved, typ, count = struct.unpack('<HHH', data[:6])
    if typ not in (1, 2):
        return []
    
    results = []
    for i in range(count):
        off = 6 + i * 16
        if off + 16 > len(data):
            break
        w, h, colors, res, xhot, yhot, size, img_off = struct.unpack('<BBBBHHII', data[off:off+16])
        if w == 0: w = 256
        if h == 0: h = 256
        
        img_data = data[img_off:img_off+size]
        if not img_data:
            continue
            
        img = None
        if img_data.startswith(b'\x89PNG'):
            try:
                img = Image.open(io.BytesIO(img_data)).convert('RGBA')
            except Exception:
                pass
        else:
            img = parse_bmp_cur_data(img_data, w, h)
            
        if img:
            results.append((w, h, xhot, yhot, img))
            
    return results

def parse_ani(data):
    if not data.startswith(b'RIFF') or b'ACON' not in data[:12]:
        return []
    
    frames = []
    rates = []
    seq = []
    
    pos = 12
    while pos + 8 <= len(data):
        chunk_id = data[pos:pos+4]
        chunk_size = struct.unpack('<I', data[pos+4:pos+8])[0]
        chunk_data = data[pos+8:pos+8+chunk_size]
        pos += 8 + chunk_size + (chunk_size % 2)
        
        if chunk_id == b'anih':
            pass
        elif chunk_id == b'rate':
            rates = list(struct.unpack(f'<{len(chunk_data)//4}I', chunk_data))
        elif chunk_id == b'seq ':
            seq = list(struct.unpack(f'<{len(chunk_data)//4}I', chunk_data))
        elif chunk_id == b'LIST' and chunk_data.startswith(b'fram'):
            fpos = 4
            while fpos + 8 <= len(chunk_data):
                fc_id = chunk_data[fpos:fpos+4]
                fc_sz = struct.unpack('<I', chunk_data[fpos+4:fpos+8])[0]
                fc_data = chunk_data[fpos+8:fpos+8+fc_sz]
                fpos += 8 + fc_sz + (fc_sz % 2)
                if fc_id == b'icon':
                    parsed = parse_cur_or_ico(fc_data)
                    if parsed:
                        frames.append(parsed[0])
                        
    if not frames:
        return []

    ordered_frames = [frames[i] for i in seq if i < len(frames)] if seq else frames
        
    final_frames = []
    for idx, frame in enumerate(ordered_frames):
        jif = rates[idx] if idx < len(rates) else 6
        delay_ms = max(20, int(jif * 16.666))
        w, h, xh, yh, img = frame
        final_frames.append((w, h, xh, yh, delay_ms, img))
        
    return final_frames

def build_xcursor(frames):
    if not frames:
        return b''
        
    TOC_TYPE_IMAGE = 0xfffd0002
    VERSION = 1
    
    header = struct.pack('<4sIII', b'Xcur', 16, 0x10000, len(frames))
    toc_entries = []
    image_chunks = []
    
    current_offset = 16 + len(frames) * 12
    
    for w, h, xh, yh, delay, img in frames:
        toc_entries.append(struct.pack('<III', TOC_TYPE_IMAGE, w, current_offset))
        
        img_rgba = img.convert('RGBA')
        pixels = bytearray()
        for y in range(h):
            for x in range(w):
                r, g, b, a = img_rgba.getpixel((x, y))
                # Premultiply alpha for Xcursor
                r = (r * a) // 255
                g = (g * a) // 255
                b = (b * a) // 255
                pixels.extend(struct.pack('<BBBB', b, g, r, a))
                
        img_hdr = struct.pack('<IIIIIIIII', 36, TOC_TYPE_IMAGE, w, VERSION, w, h, xh, yh, delay)
        chunk = img_hdr + bytes(pixels)
        image_chunks.append(chunk)
        current_offset += len(chunk)
        
    return header + b''.join(toc_entries) + b''.join(image_chunks)

def convert_dir_to_theme(source_dir, theme_name):
    target_theme_dir = pathlib.Path.home() / "Pictures" / "cursor" / theme_name
    target_cursors_dir = target_theme_dir / "cursors"
    target_cursors_dir.mkdir(parents=True, exist_ok=True)
    
    with open(target_theme_dir / "index.theme", "w") as f:
        f.write(f"[Icon Theme]\nName={theme_name}\nComment={theme_name} Cursor Pack\n")

    mappings = {
        "Normal": ["left_ptr", "default", "arrow", "top_left_arrow"],
        "Link": ["pointer", "hand", "hand2", "pointing_hand"],
        "Text": ["xterm", "text", "ibeam"],
        "Busy": ["wait", "watch"],
        "Working": ["progress", "half-busy", "left_ptr_watch"],
        "Help": ["help", "question_arrow", "whats_this"],
        "Unavailable": ["not-allowed", "forbidden", "crossed_circle", "circle"],
        "Precision": ["crosshair", "cross", "tcross"],
        "Handwriting": ["pencil"],
        "Move": ["move", "fleur"],
        "Vertical": ["v_double_arrow", "ns-resize", "size_ver", "sb_v_double_arrow"],
        "Horizontal": ["h_double_arrow", "ew-resize", "size_hor", "sb_h_double_arrow"],
        "Diagonal1": ["fd_double_arrow", "nesw-resize", "size_bdiag"],
        "Diagonal2": ["bd_double_arrow", "nwse-resize", "size_fdiag"],
        "Alternate": ["up-arrow", "right_ptr"],
        "Pin": ["pin"],
        "Person": ["person"]
    }
    
    converted_count = 0
    
    for base_name, x11_names in mappings.items():
        file_path = None
        for ext in ['.ani', '.cur', '.ANI', '.CUR']:
            p = source_dir / (base_name + ext)
            if p.exists():
                file_path = p
                break
                
        if not file_path:
            continue
            
        with open(file_path, 'rb') as f:
            data = f.read()
            
        frames = parse_ani(data) if file_path.suffix.lower() == '.ani' else []
        if not frames:
            parsed = parse_cur_or_ico(data)
            if parsed:
                w, h, xh, yh, img = parsed[0]
                frames = [(w, h, xh, yh, 50, img)]
                
        if frames:
            xcursor_bytes = build_xcursor(frames)
            if xcursor_bytes:
                first_name = x11_names[0]
                first_path = target_cursors_dir / first_name
                with open(first_path, 'wb') as f:
                    f.write(xcursor_bytes)
                converted_count += 1
                
                for alias in x11_names[1:]:
                    alias_path = target_cursors_dir / alias
                    if alias_path.exists() or alias_path.is_symlink():
                        alias_path.unlink()
                    alias_path.symlink_to(first_name)
                    
    print(f"✨ Converted {converted_count} cursors for '{theme_name}' into {target_cursors_dir}")

def main():
    pic_cursor = pathlib.Path.home() / "Pictures" / "cursor"
    if (pic_cursor / "Cursors").is_dir():
        convert_dir_to_theme(pic_cursor / "Cursors", "Yixuan-Animated")
    if (pic_cursor / "Static").is_dir():
        convert_dir_to_theme(pic_cursor / "Static", "Yixuan-Static")

if __name__ == "__main__":
    main()
