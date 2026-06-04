bits 32

section .text
align 4
multiboot_header:
    dd 0x1BADB002          ; Magic
    dd 0x00000004          ; Flags: Modo de Vídeo
    dd -(0x1BADB002 + 0x00000004) ; Checksum
    dd 0, 0, 0, 0, 0       ; Ignorado
    dd 0                   ; 0 = Linear Graphics
    dd 1024                ; Largura
    dd 768                 ; Altura
    dd 32                  ; 32 Bits True Color

global _start
extern whale_kernel_main

_start:
    cli 
    mov esp, stack_top
    push ebx               ; Passa o ponteiro Multiboot Info para o Kernel!
    push eax               ; Passa o Magic Number
    call whale_kernel_main 
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384             
stack_top: