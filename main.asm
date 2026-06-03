bits 32
section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00
    dd - (0x1BADB002 + 0x00)

section .text
global _start
extern whale_kernel_main
_start:
    cli
    mov esp, kernel_stack_top
    call whale_kernel_main
.hang:
    hlt
    jmp .hang

section .bss
align 16
kernel_stack_bottom:
    resb 16384
kernel_stack_top:
