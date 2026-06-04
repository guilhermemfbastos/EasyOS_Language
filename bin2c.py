import sys

def convert():
    if len(sys.argv) < 3:
        print("Uso: python3 bin2c.py <arquivo.bin> <nome_do_app>")
        sys.exit(1)
        
    filename = sys.argv[1]
    app_name = sys.argv[2].lower()
    
    try:
        with open(filename, "rb") as f:
            data = f.read()
    except Exception as e:
        print(f"Erro ao ler binário: {e}")
        sys.exit(1)
        
    with open(f"bin_{app_name}.h", "w") as f:
        f.write(f"/* Arquivo gerado automaticamente pelo bin2c.py */\n")
        f.write(f"const uint8_t app_bin_{app_name}[{len(data)}] = {{\n    ")
        for i, byte in enumerate(data):
            f.write(f"0x{byte:02X}, ")
            if (i + 1) % 16 == 0:
                f.write("\n    ")
        f.write("\n};\n")
        f.write(f"const uint32_t app_bin_{app_name}_len = {len(data)};\n")

if __name__ == "__main__":
    convert()