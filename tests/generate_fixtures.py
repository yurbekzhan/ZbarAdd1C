"""Recreate the committed functional test images."""
from pathlib import Path

import qrcode
from PIL import Image

output = Path(__file__).parent / "fixtures"
output.mkdir(exist_ok=True)
qr = qrcode.make("https://example.org/1c?qr=12345").convert("RGB")
qr.save(output / "qr.png")
qr.save(output / "тест_QR_1С.png")
qr.convert("P").save(output / "palette.png")
qr.rotate(90, expand=True).save(output / "rotated.png")
transparent = qr.convert("RGBA")
transparent.putdata([
    (r, g, b, 0 if r == 255 else 255)
    for r, g, b in qr.get_flattened_data()
])
transparent.save(output / "transparent.png")
first = qrcode.make("FIRST").convert("RGB")
second = qrcode.make("SECOND").convert("RGB")
multiple = Image.new("RGB", (first.width + second.width + 80,
                             max(first.height, second.height)), "white")
multiple.paste(first, (0, 0))
multiple.paste(second, (first.width + 80, 0))
multiple.save(output / "multiple.png")
Image.new("RGB", (300, 300), "white").save(output / "blank.png")
qrcode.make("Привет, 1С!").save(output / "unicode.png")
qrcode.make('quote" slash\\ line\n').save(output / "escaped.png")
(output / "broken.png").write_bytes(b"not a PNG")
(output / "truncated.png").write_bytes((output / "qr.png").read_bytes()[:80])
