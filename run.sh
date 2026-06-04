#!/bin/bash
echo "========================================="
echo "        WhaleOS Compilador Seguro        "
echo "========================================="

# 1. COMPILAÇÃO PRIMEIRO (Evita Timeout do noVNC)
echo "⚙️  1. Transpilando main.eos para C..."
./eos_compiler main.eos stub.asm

echo "🔨 2. Compilando objetos Bare-Metal..."
nasm -f elf32 boot.asm -o boot.o
gcc -m32 -c kernel.c -o kernel.o -ffreestanding -O2 -Wall -Wextra

echo "🔗 3. Efetuando linkagem física estrita..."
ld -m elf_i386 -T linker.ld boot.o kernel.o -o kernel.bin

if [ ! -f kernel.bin ]; then
    echo "❌ [ERRO] kernel.bin não foi gerado!"
    exit 1
fi

echo "💿 4. Empacotando árvore do GRUB e criando ISO..."
mkdir -p iso_root/boot/grub
cp kernel.bin iso_root/boot/kernel.bin
echo -e 'set default=0\nset timeout=0\nmenuentry "EasyOS" {\n    multiboot /boot/kernel.bin\n    boot\n}' > iso_root/boot/grub/grub.cfg
grub-mkrescue -o os.iso iso_root &> /dev/null

echo "✅ [SUCESSO] os.iso gerada perfeitamente!"

# 2. LIMPEZA E INICIALIZAÇÃO DOS SERVIDORES GRÁFICOS (Logo antes do QEMU)
echo "🧹 5. Reiniciando barramentos de vídeo virtuais..."
pkill -f qemu-system-i386 || true
pkill -f websockify || true
pkill -f Xvfb || true
sleep 1

echo "📺 6. Inicializando Servidor Gráfico Virtual Xvfb e noVNC..."
Xvfb :0 -screen 0 1024x768x16 &
sleep 1
websockify --web=/usr/share/novnc 6080 localhost:5900 &
sleep 1

echo "🐳 7. Disparando a VM EasyOS no QEMU..."
qemu-system-i386 -cdrom os.iso -boot d -rtc base=localtime -vga std -vnc :0 &

echo "----------------------------------------------------"
echo "🚀 TUDO PRONTO! Atualize a aba do noVNC na porta 6080"
echo "----------------------------------------------------"