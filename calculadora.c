/* --- calculadora.c (Aplicativo Externo para o EasyOS) --- */
#include <stdint.h> // Define os tipos padronizados de inteiros da CPU

// Endereço cravado da tabela de funções compartilhada pelo seu Kernel
#define SYSCALL_TABLE_ADDR 0x5000

// Definição dos tipos de ponteiros de função para bater com o Kernel
typedef void (*draw_rect_t)(int, int, uint32_t, int, int, int);
typedef void (*draw_str_t)(const char*, int, int, uint32_t);

// Ponto de entrada exato do aplicativo externo na RAM
void _start() {
    // 1. Aponta para a tabela lendo a RAM do Kernel
    uint32_t* syscalls = (uint32_t*)SYSCALL_TABLE_ADDR;
    
    // 2. Extrai os endereços físicos das funções gráficas do Kernel
    draw_rect_t draw_rectangle = (draw_rect_t)syscalls[0];
    draw_str_t draw_string     = (draw_str_t)syscalls[1];

    // 3. Desenha a interface interna do seu aplicativo de forma autônoma
    // Fundo da calculadora (Branco)
    draw_rectangle(200, 200, 15, 400, 300, -1); 
    
    // Tela do visor (Preto)
    draw_rectangle(160, 30, 0, 420, 320, -1);
    
    // Texto do visor simulado (Verde brilhante)
    draw_string("12345", 430, 330, 10);
    
    // Botão numérico interativo '7' (ID de clique 600)
    draw_rectangle(40, 40, 8, 420, 370, 600);
    draw_string("7", 435, 385, 15);

    // Loop de persistência do processo para manter o app vivo na memória
    while(1) {
        // A lógica de execução interna do app roda isolada aqui dentro
    }
}