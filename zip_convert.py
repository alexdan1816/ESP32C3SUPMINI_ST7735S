"""
Chuyển tất cả frame ảnh trong file .zip sang RGB565 cho màn ST7735S.

Yêu cầu: để file này cùng thư mục với image_convert.py
Cài:     py -m pip install pillow numpy
Dùng:    py zip_convert.py frames.zip
         py zip_convert.py frames.zip -n anim -W 160 -H 80 -f cover
         py zip_convert.py frames.zip --no-dither      (nhanh hơn, hợp ảnh màu phẳng)
         py zip_convert.py frames.zip --header         (xuất thêm .h, chỉ nên dùng ít frame)
Kết quả: <tên>.raw  = tất cả frame nối tiếp nhau, mỗi frame W*H*2 byte (big-endian)
"""
import argparse
import io
import re
import zipfile

from image_convert import load_and_resize, to_rgb565

EXTS = (".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif")


def natural_key(s):
    # frame2 phải đứng trước frame10
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", s)]


def main():
    p = argparse.ArgumentParser(description="Zip frame -> RGB565")
    p.add_argument("zip", help="đường dẫn file .zip chứa các frame")
    p.add_argument("-W", "--width", type=int, default=160)
    p.add_argument("-H", "--height", type=int, default=80)
    p.add_argument("-n", "--name", default="anim", help="tên file / tên biến xuất ra")
    p.add_argument("-f", "--fit", choices=["cover", "contain", "stretch"], default="cover")
    p.add_argument("--no-dither", action="store_true", help="tắt dithering")
    p.add_argument("--header", action="store_true", help="xuất thêm file .h")
    args = p.parse_args()

    with zipfile.ZipFile(args.zip) as zf:
        names = [
            n for n in zf.namelist()
            if n.lower().endswith(EXTS)
            and not n.endswith("/")
            and "__MACOSX" not in n
            and not n.split("/")[-1].startswith(".")
        ]
        names.sort(key=natural_key)

        if not names:
            print("Không tìm thấy ảnh nào trong zip.")
            return

        total = len(names)
        frame_bytes = args.width * args.height * 2
        print(f"Tìm thấy {total} frame, mỗi frame {frame_bytes} bytes "
              f"(tổng {total * frame_bytes / 1024:.1f} KB)")

        all_frames = []
        with open(f"{args.name}.raw", "wb") as raw:
            for i, n in enumerate(names, 1):
                # đọc hết vào RAM vì zip entry không seek được
                data = io.BytesIO(zf.read(n))
                img = load_and_resize(data, args.width, args.height, args.fit)
                px = to_rgb565(img, dither=not args.no_dither).flatten().tolist()

                raw.write(b"".join(bytes([v >> 8, v & 0xFF]) for v in px))
                if args.header:
                    all_frames.append(px)

                print(f"\r[{i}/{total}] {n}".ljust(70), end="", flush=True)
        print()

    if args.header:
        up = args.name.upper()
        with open(f"{args.name}.h", "w") as f:
            f.write(f"#define {up}_W {args.width}\n")
            f.write(f"#define {up}_H {args.height}\n")
            f.write(f"#define {up}_FRAMES {total}\n")
            f.write(f"const uint16_t {args.name}[{total}][{args.width * args.height}] PROGMEM = {{\n")
            for px in all_frames:
                f.write("  {\n")
                for j in range(0, len(px), 16):
                    f.write("    " + ", ".join(f"0x{v:04X}" for v in px[j:j + 16]) + ",\n")
                f.write("  },\n")
            f.write("};\n")
        print(f"Đã tạo {args.name}.h")

    print(f"Xong: {args.name}.raw ({total} frame, {args.width}x{args.height})")


if __name__ == "__main__":
    main()