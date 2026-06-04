import sys
import os
try:
    from PIL import Image
except ImportError:
    os.system("python3 -m pip install pillow")
    from PIL import Image

def convert():
    if len(sys.argv) < 5:
        print("Uso: img2c.py <arquivo> <largura> <altura> <id>")
        sys.exit(1)

    filepath = sys.argv[1]
    w = int(sys.argv[2])
    h = int(sys.argv[3])
    img_id = sys.argv[4]

    try:
        # Tenta abrir e forçar o canal Alpha (Transparência)
        img = Image.open(filepath).convert("RGBA").resize((w, h))
        pixels = img.load()
    except Exception as e:
        print(f"Erro ao abrir {filepath}: {e}")
        # Cria uma imagem falsa (magenta) caso a original falhe
        img = Image.new("RGBA", (w, h), (255, 0, 255, 255))
        pixels = img.load()

    with open(f"img_{img_id}.h", "w") as f:
        f.write(f"const uint32_t img_{img_id}[{w * h}] = {{\n")
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixels[x, y]
                # Monta a estrutura 32-bits (ARGB Little Endian)
                val = (a << 24) | (r << 16) | (g << 8) | b
                f.write(f"0x{val:08X}, ")
            f.write("\n")
        f.write("};\n")

if __name__ == "__main__":
    convert()