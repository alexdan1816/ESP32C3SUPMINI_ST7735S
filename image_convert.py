"""
Chuyển ảnh (webp, png, jpg, ...) sang RGB565 cho màn ST7735S 80x160.

Cài thư viện:   py -m pip install pillow numpy
Cách dùng:      py image_convert.py anh.webp
                py image_convert.py anh.webp -W 160 -H 80 -n logo -f cover
                py image_convert.py anh.webp --no-dither      (logo/icon màu phẳng)
                py image_convert.py anh.webp --swap           (nếu màu ra sai)
Kết quả:        <tên>.h  và  <tên>.raw  (big-endian, gửi thẳng ra SPI được)
"""
import argparse
import numpy as np
from PIL import Image


def load_and_resize(path, W, H, fit):
    img = Image.open(path).convert("RGBA")

    # Dán lên nền đen để kênh alpha của WebP/PNG không tạo viền lạ
    bg = Image.new("RGBA", img.size, (0, 0, 0, 255))
    img = Image.alpha_composite(bg, img).convert("RGB")

    if fit == "cover":        # lấp đầy màn, cắt phần thừa, không méo
        s = max(W / img.width, H / img.height)
        nw, nh = round(img.width * s), round(img.height * s)
        img = img.resize((nw, nh), Image.LANCZOS)
        l, t = (nw - W) // 2, (nh - H) // 2
        img = img.crop((l, t, l + W, t + H))
    elif fit == "contain":    # giữ nguyên cả ảnh, thừa thì viền đen
        s = min(W / img.width, H / img.height)
        nw, nh = round(img.width * s), round(img.height * s)
        img = img.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new("RGB", (W, H), (0, 0, 0))
        canvas.paste(img, ((W - nw) // 2, (H - nh) // 2))
        img = canvas
    else:                     # stretch: kéo giãn cho vừa
        img = img.resize((W, H), Image.LANCZOS)
    return img


LEVELS = np.array([31, 63, 31], dtype=np.float32)   # số mức tối đa của R, G, B


def to_rgb565(img, dither=True):
    """Trả về mảng uint16 (H x W). dither=True dùng Floyd-Steinberg."""
    a = np.asarray(img, dtype=np.float32).copy()    # H x W x 3
    H, W, _ = a.shape
    out = np.zeros((H, W), dtype=np.uint16)

    if not dither:
        q = np.clip(np.round(a / 255.0 * LEVELS), 0, LEVELS).astype(np.uint16)
        return (q[..., 0] << 11) | (q[..., 1] << 5) | q[..., 2]

    for y in range(H):
        for x in range(W):
            old = a[y, x]
            q = np.clip(np.round(old / 255.0 * LEVELS), 0, LEVELS)
            err = old - q / LEVELS * 255.0

            if x + 1 < W:
                a[y, x + 1] += err * 7 / 16
            if y + 1 < H:
                if x > 0:
                    a[y + 1, x - 1] += err * 3 / 16
                a[y + 1, x] += err * 5 / 16
                if x + 1 < W:
                    a[y + 1, x + 1] += err * 1 / 16

            r, g, b = q.astype(int)
            out[y, x] = (r << 11) | (g << 5) | b
    return out


def main():
    p = argparse.ArgumentParser(description="Ảnh -> RGB565 cho màn TFT")
    p.add_argument("path", help="đường dẫn ảnh (webp, png, jpg...)")
    p.add_argument("-W", "--width", type=int, default=160)
    p.add_argument("-H", "--height", type=int, default=80)
    p.add_argument("-n", "--name", default="image", help="tên biến / tên file xuất")
    p.add_argument("-f", "--fit", choices=["cover", "contain", "stretch"], default="cover")
    p.add_argument("--no-dither", action="store_true", help="tắt dithering")
    p.add_argument("--swap", action="store_true", help="đảo byte cao/thấp trong file .h")
    args = p.parse_args()

    img = load_and_resize(args.path, args.width, args.height, args.fit)
    px = to_rgb565(img, dither=not args.no_dither).flatten().tolist()

    # .h dùng cho #include (đảo byte nếu --swap)
    h_vals = [((v & 0xFF) << 8) | (v >> 8) for v in px] if args.swap else px
    up = args.name.upper()
    with open(f"{args.name}.h", "w") as f:
        f.write(f"#define {up}_W {args.width}\n")
        f.write(f"#define {up}_H {args.height}\n")
        f.write(f"const uint16_t {args.name}[{len(h_vals)}] PROGMEM = {{\n")
        for i in range(0, len(h_vals), 16):
            f.write("  " + ", ".join(f"0x{v:04X}" for v in h_vals[i:i + 16]) + ",\n")
        f.write("};\n")

    # .raw: byte cao trước, dùng cho LittleFS/SD
    with open(f"{args.name}.raw", "wb") as f:
        for v in px:
            f.write(bytes([v >> 8, v & 0xFF]))

    print(f"Xong: {args.name}.h và {args.name}.raw "
          f"({args.width}x{args.height}, {len(px) * 2} bytes, "
          f"dither={'tắt' if args.no_dither else 'bật'})")


if __name__ == "__main__":
    main()