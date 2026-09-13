"""Read barcode-scanner input in HID keyboard mode on Windows.

Configure the scanner to append Enter after every scan, then run:
    python barcode_reader.py
"""

import msvcrt


def main() -> None:
    print("等待扫码；扫码枪请设置为 HID 键盘模式，结束符为 Enter。按 Ctrl+C 退出。")
    buffer: list[str] = []

    try:
        while True:
            char = msvcrt.getwch()

            if char in ("\r", "\n"):
                barcode = "".join(buffer).strip()
                buffer.clear()
                if barcode:
                    print(f"条码: {barcode}", flush=True)
                continue

            if char == "\x03":  # Ctrl+C
                raise KeyboardInterrupt

            if char == "\b":
                if buffer:
                    buffer.pop()
                continue

            if char.isprintable():
                buffer.append(char)
    except KeyboardInterrupt:
        print("\n已退出。")


if __name__ == "__main__":
    main()
