#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_TOKEN_LEN 256
char string_database[50][MAX_TOKEN_LEN];
int string_count = 0;
int line_counter = 0;
FILE *source_fp;
FILE *asm_fp;

void extract_string(const char *line, char *output) {
    const char *start = strchr(line, '\'');
    if (!start) start = strchr(line, '"');
    if (start) {
        start++;
        const char *end = strchr(start, '\'');
        if (!end) end = strchr(start, '"');
        if (end) {
            size_t len = end - start;
            if (len < MAX_TOKEN_LEN) {
                strncpy(output, start, len);
                output[len] = '\0';
                return;
            }
        }
    }
    strcpy(output, "");
}

uint8_t parse_hex_color(const char* hex_str) {
    if (!hex_str || strlen(hex_str) == 0) return 0x0A;
    unsigned int val = 0;
    sscanf(hex_str, "%x", &val);
    return (uint8_t)val;
}

void process_file(FILE *file_stream, int is_imported) {
    char line[512];
    int if_block_open = 0;
    static int kernel_infrastructure_written = 0;
    static FILE *c_out = NULL;

    if (!kernel_infrastructure_written) {
        c_out = fopen("kernel.c", "w");
        
        fprintf(c_out, "/* --- WhaleOS Self-Hosted Monolithic OS --- */\n");
        fprintf(c_out, "#include <stdint.h>\n\n");

        fprintf(c_out, "static inline uint8_t inb(uint16_t port) {\n");
        fprintf(c_out, "    uint8_t ret;\n");
        fprintf(c_out, "    __asm__ volatile(\"inb %%w1, %%b0\" : \"=a\"(ret) : \"Nd\"(port));\n");
        fprintf(c_out, "    return ret;\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "volatile char* vga_mem = (volatile char*)0xB8000;\n");
        fprintf(c_out, "int cursor_idx = 0;\n");
        fprintf(c_out, "int editor_mode_active = 0;\n");
        fprintf(c_out, "uint8_t current_color_attribute = 0x0A;\n");
        fprintf(c_out, "int shift_pressed = 0;\n\n");

        fprintf(c_out, "void k_print(const char* str) {\n");
        fprintf(c_out, "    while (*str) {\n");
        fprintf(c_out, "        if (*str == '\\n') { cursor_idx = ((cursor_idx / 160) + 1) * 160; }\n");
        fprintf(c_out, "        else { vga_mem[cursor_idx++] = *str; vga_mem[cursor_idx++] = current_color_attribute; }\n");
        fprintf(c_out, "        str++;\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void k_print_char(char c) {\n");
        fprintf(c_out, "    if (c == '\\n') { cursor_idx = ((cursor_idx / 160) + 1) * 160; }\n");
        fprintf(c_out, "    else { vga_mem[cursor_idx++] = c; vga_mem[cursor_idx++] = current_color_attribute; }\n");
        fprintf(c_out, "}\n\n");

        // HEAP & MALLOC
        fprintf(c_out, "#define HEAP_SIZE 65536\n");
        fprintf(c_out, "static uint8_t whale_heap[HEAP_SIZE];\n\n");
        fprintf(c_out, "typedef struct k_mem_header {\n");
        fprintf(c_out, "    uint32_t size;\n    uint8_t  is_free;\n    struct k_mem_header* next;\n");
        fprintf(c_out, "} k_mem_header_t;\n\n");
        fprintf(c_out, "k_mem_header_t* free_list = (k_mem_header_t*)whale_heap;\n\n");
        
        fprintf(c_out, "void init_whale_malloc() {\n");
        fprintf(c_out, "    free_list->size = HEAP_SIZE - sizeof(k_mem_header_t);\n");
        fprintf(c_out, "    free_list->is_free = 1; free_list->next = 0;\n");
        fprintf(c_out, "    k_print(\"[Kernel RAM] Heap de 64KB Inicializado.\\n\");\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void* whale_malloc(uint32_t size) {\n");
        fprintf(c_out, "    k_mem_header_t* curr = free_list;\n");
        fprintf(c_out, "    while (curr) {\n");
        fprintf(c_out, "        if (curr->is_free && curr->size >= size) {\n");
        fprintf(c_out, "            if (curr->size > size + sizeof(k_mem_header_t) + 4) {\n");
        fprintf(c_out, "                k_mem_header_t* next_block = (k_mem_header_t*)((uint8_t*)curr + sizeof(k_mem_header_t) + size);\n");
        fprintf(c_out, "                next_block->size = curr->size - size - sizeof(k_mem_header_t);\n");
        fprintf(c_out, "                next_block->is_free = 1; next_block->next = curr->next;\n");
        fprintf(c_out, "                curr->size = size; curr->next = next_block;\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "            curr->is_free = 0;\n");
        fprintf(c_out, "            return (void*)((uint8_t*)curr + sizeof(k_mem_header_t));\n");
        fprintf(c_out, "        }\n");
        fprintf(c_out, "        curr = curr->next;\n");
        fprintf(c_out, "    }\n    return 0;\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void whale_free(void* ptr) {\n");
        fprintf(c_out, "    if (!ptr) return;\n");
        fprintf(c_out, "    k_mem_header_t* header = (k_mem_header_t*)((uint8_t*)ptr - sizeof(k_mem_header_t));\n");
        fprintf(c_out, "    header->is_free = 1;\n");
        fprintf(c_out, "}\n\n");

        // VFS
        fprintf(c_out, "struct vfs_entry { char name[16]; char content[512]; };\n");
        fprintf(c_out, "struct vfs_entry storage_ram[16];\nint fs_count = 0;\n\n");

        fprintf(c_out, "void sys_vfs_write(const char* name, const char* data) {\n");
        fprintf(c_out, "    if (fs_count >= 16) return;\n");
        fprintf(c_out, "    int i = 0; while (name[i] && i < 15) { storage_ram[fs_count].name[i] = name[i]; i++; }\n");
        fprintf(c_out, "    storage_ram[fs_count].name[i] = 0;\n");
        fprintf(c_out, "    i = 0; while (data[i] && i < 511) { storage_ram[fs_count].content[i] = data[i]; i++; }\n");
        fprintf(c_out, "    storage_ram[fs_count].content[i] = 0;\n");
        fprintf(c_out, "    fs_count++;\n");
        fprintf(c_out, "}\n\n");

        // TCC NATIVE COMPILER
        fprintf(c_out, "void run_native_tcc_compiler(const char* code) {\n");
        fprintf(c_out, "    k_print(\"\\n[WhaleOS TCC v1.0] Iniciando compilacao nativa no Heap...\\n\");\n");
        fprintf(c_out, "    int loop_limit = 1; char print_buffer[64] = {0};\n");
        fprintf(c_out, "    int has_for = 0; int has_printf = 0;\n\n");
        
        fprintf(c_out, "    char* token_table = (char*)whale_malloc(256);\n");
        fprintf(c_out, "    if(!token_table) { k_print(\"[FATAL] Sem memoria RAM.\\n\"); return; }\n\n");

        fprintf(c_out, "    char* cursor = (char*)code;\n");
        fprintf(c_out, "    while(*cursor) {\n");
        fprintf(c_out, "        if(cursor[0]=='f' && cursor[1]=='o' && cursor[2]=='r') {\n");
        fprintf(c_out, "            int idx = 3; while(cursor[idx] && cursor[idx] != '<') idx++;\n");
        fprintf(c_out, "            if(cursor[idx] == '<') {\n");
        fprintf(c_out, "                idx++; while(cursor[idx] == ' ') idx++;\n");
        fprintf(c_out, "                if(cursor[idx] >= '0' && cursor[idx] <= '9') { loop_limit = cursor[idx] - '0'; has_for = 1; }\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n");
        fprintf(c_out, "        cursor++;\n");
        fprintf(c_out, "    }\n\n");

        fprintf(c_out, "    cursor = (char*)code;\n");
        fprintf(c_out, "    while(*cursor) {\n");
        fprintf(c_out, "        if(cursor[0]=='p' && cursor[1]=='r' && cursor[2]=='i' && cursor[3]=='n' && cursor[4]=='t' && cursor[5]=='f') {\n");
        fprintf(c_out, "            char* s = 0; char* e = 0; int idx = 6;\n");
        fprintf(c_out, "            while(cursor[idx] && cursor[idx] != '\"') idx++;\n");
        fprintf(c_out, "            if(cursor[idx] == '\"') { s = &cursor[idx+1]; idx++; }\n");
        fprintf(c_out, "            while(cursor[idx] && cursor[idx] != '\"') idx++;\n");
        fprintf(c_out, "            if(cursor[idx] == '\"') e = &cursor[idx];\n");
        fprintf(c_out, "            if(s && e) {\n");
        fprintf(c_out, "                int k = 0; while(s < e && k < 63) {\n");
        fprintf(c_out, "                    if(*s == '\\\\' && *(s+1) == 'n') { print_buffer[k++] = '\\n'; s+=2; }\n");
        fprintf(c_out, "                    else { print_buffer[k++] = *s; s++; }\n");
        fprintf(c_out, "                } print_buffer[k] = 0; has_printf = 1;\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n        cursor++;\n");
        fprintf(c_out, "    }\n\n");

        fprintf(c_out, "    if(has_printf) {\n");
        fprintf(c_out, "        k_print(\"[TCC] Binario gerado com sucesso! Executando:\\n\");\n");
        fprintf(c_out, "        for(int i = 0; i < loop_limit; i++) {\n");
        fprintf(c_out, "            if(has_for) { k_print(\"Iteracao \"); k_print_char('0' + i); k_print(\": \"); }\n");
        fprintf(c_out, "            k_print(print_buffer);\n");
        fprintf(c_out, "        }\n");
        fprintf(c_out, "    } else { k_print(\"[TCC Warning] Codigo compilado sem rotinas VGA.\\n\"); }\n");
        fprintf(c_out, "    whale_free(token_table);\n");
        fprintf(c_out, "}\n\n");

        // SHELL & KEYBOARD
        fprintf(c_out, "char input_buffer[64]; int input_len = 0; char saved_editor_name[16] = {0};\n");
        fprintf(c_out, "char kbd_map_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\b', '\\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\\'', '`', 0, '\\\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };\n");
        fprintf(c_out, "char kbd_map_shift[128]  = { 0, 27, '!', '@', '#', '$', '%%', '^', '&', '*', '(', ')', '_', '+', '\\b', '\\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };\n\n");

        fprintf(c_out, "void process_shell_command(char* cmd) {\n");
        fprintf(c_out, "    if (cmd[0] == 'm' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 'l' && cmd[4] == 'o' && cmd[5] == 'c') {\n");
        fprintf(c_out, "        k_print(\"\\n[Malloc] Alocando 16 bytes no Heap...\\n\");\n");
        fprintf(c_out, "        void* ptr = whale_malloc(16);\n");
        fprintf(c_out, "        if(ptr) { k_print(\"[OK] Bloco reservado. Liberando...\\n\"); whale_free(ptr); }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (cmd[0] == 's' && cmd[1] == 'a' && cmd[2] == 'v' && cmd[3] == 'e') {\n");
        fprintf(c_out, "        int i = 5, j = 0; char f_target[16]; while(cmd[i]) { f_target[j++] = cmd[i++]; } f_target[j] = 0;\n");
        fprintf(c_out, "        sys_vfs_write(f_target, \"Salvo com sucesso.\"); k_print(\"\\n[VFS] Arquivo gravado!\\n\");\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (cmd[0] == 'd' && cmd[1] == 'i' && cmd[2] == 'r') {\n");
        fprintf(c_out, "        k_print(\"\\nArquivos no VFS:\\n\");\n");
        fprintf(c_out, "        for(int idx = 0; idx < fs_count; idx++) { k_print(\"  - \"); k_print(storage_ram[idx].name); k_print(\"\\n\"); }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't') {\n");
        fprintf(c_out, "        int i = 5, j = 0; while(cmd[i]) { saved_editor_name[j++] = cmd[i++]; } saved_editor_name[j] = 0;\n");
        fprintf(c_out, "        k_print(\"\\n[EDITOR] Digite o texto e ENTER:\\n> \"); editor_mode_active = 1;\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't') {\n");
        fprintf(c_out, "        int i = 4, j = 0; char f_search[16]; int found = 0;\n");
        fprintf(c_out, "        while(cmd[i]) { f_search[j++] = cmd[i++]; } f_search[j] = 0;\n");
        fprintf(c_out, "        for(int idx = 0; idx < fs_count; idx++) {\n");
        fprintf(c_out, "            int m = 0; while(f_search[m] && storage_ram[idx].name[m] == f_search[m]) m++;\n");
        fprintf(c_out, "            if(f_search[m] == 0 && storage_ram[idx].name[m] == 0) {\n");
        fprintf(c_out, "                k_print(\"\\n\"); k_print(storage_ram[idx].content); k_print(\"\\n\"); found = 1; break;\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n        if(!found) { k_print(\"\\nNao encontrado.\\n\"); }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (cmd[0] == 'g' && cmd[1] == 'c' && cmd[2] == 'c') {\n");
        fprintf(c_out, "        int i = 4, j = 0; char f_search[16]; int found = 0;\n");
        fprintf(c_out, "        while(cmd[i]) { f_search[j++] = cmd[i++]; } f_search[j] = 0;\n");
        fprintf(c_out, "        for(int idx = 0; idx < fs_count; idx++) {\n");
        fprintf(c_out, "            int m = 0; while(f_search[m] && storage_ram[idx].name[m] == f_search[m]) m++;\n");
        fprintf(c_out, "            if(f_search[m] == 0 && storage_ram[idx].name[m] == 0) {\n");
        fprintf(c_out, "                run_native_tcc_compiler(storage_ram[idx].content); found = 1; break;\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n        if(!found) { k_print(\"\\nNao encontrado.\\n\"); }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void handle_keyboard() {\n");
        fprintf(c_out, "    if (inb(0x64) & 1) {\n");
        fprintf(c_out, "        uint8_t scancode = inb(0x60);\n");
        fprintf(c_out, "        if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; }\n");
        fprintf(c_out, "        else if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; }\n");
        fprintf(c_out, "        else if (!(scancode & 0x80)) {\n");
        fprintf(c_out, "            char key = shift_pressed ? kbd_map_shift[scancode] : kbd_map_normal[scancode];\n");
        fprintf(c_out, "            if (key == '\\n') {\n");
        fprintf(c_out, "                input_buffer[input_len] = 0;\n");
        fprintf(c_out, "                if (editor_mode_active) {\n");
        fprintf(c_out, "                    sys_vfs_write(saved_editor_name, input_buffer);\n");
        fprintf(c_out, "                    k_print(\"\\n[VFS] Arquivo salvo!\\n[WhaleOS]# \");\n");
        fprintf(c_out, "                    editor_mode_active = 0; saved_editor_name[0] = 0;\n");
        fprintf(c_out, "                } else {\n");
        fprintf(c_out, "                    process_shell_command(input_buffer);\n");
        fprintf(c_out, "                    if (!editor_mode_active) k_print(\"\\n[WhaleOS]# \");\n");
        fprintf(c_out, "                }\n");
        fprintf(c_out, "                input_len = 0;\n");
        fprintf(c_out, "            } else if (key == '\\b') {\n");
        fprintf(c_out, "                if (input_len > 0) { input_len--; cursor_idx -= 2; vga_mem[cursor_idx] = ' '; }\n");
        fprintf(c_out, "            } else if (key > 0 && input_len < 63) {\n");
        fprintf(c_out, "                input_buffer[input_len++] = key; k_print_char(key);\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void whale_kernel_main() {\n");
        fprintf(c_out, "    init_whale_malloc();\n");
        fprintf(c_out, "    for(int i=0; i<80*25*2; i+=2) { vga_mem[i]=' '; vga_mem[i+1]=current_color_attribute; }\n");

        fprintf(asm_fp, "bits 32\nsection .multiboot\nalign 4\n    dd 0x1BADB002\n    dd 0x00\n    dd - (0x1BADB002 + 0x00)\n\n");
        fprintf(asm_fp, "section .text\nglobal _start\nextern whale_kernel_main\n_start:\n    cli\n    mov esp, kernel_stack_top\n    call whale_kernel_main\n.hang:\n    hlt\n    jmp .hang\n\nsection .bss\nalign 16\nkernel_stack_bottom:\n    resb 16384\nkernel_stack_top:\n");

        kernel_infrastructure_written = 1;
    }

    while (fgets(line, sizeof(line), file_stream)) {
        char *trimmed = line; while (isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0' || strncmp(trimmed, "//", 2) == 0 || strstr(trimmed, "boot.start") != NULL) continue;
        if (strstr(trimmed, "Screen.Color") != NULL) {
            char color_hex[16]; extract_string(trimmed, color_hex);
            fprintf(c_out, "    current_color_attribute = 0x%02X;\n", parse_hex_color(color_hex));
        }
        else if (strstr(trimmed, "printScreen") != NULL) {
            char txt[MAX_TOKEN_LEN], escaped[MAX_TOKEN_LEN * 2]; extract_string(trimmed, txt);
            int len = strlen(txt); for(int k=0; k<len; k++) { if(txt[k]=='\\n') txt[k]=' '; }
            fprintf(c_out, "    k_print(\"%s\\n\");\n", txt);
        }
        else if (strstr(trimmed, "always") != NULL) break;
    }

    if (!is_imported) { 
        fprintf(c_out, "    k_print(\"\\n[WhaleOS Shell Ativo]\\n[WhaleOS]# \");\n");
        fprintf(c_out, "    while(1) { handle_keyboard(); }\n}\n");
        fclose(c_out);
    }
}

void parse(void) {
    fseek(source_fp, 0, SEEK_SET);
    process_file(source_fp, 0);
}

int main(int argc, char** argv) {
    if(argc < 3) { printf("Erro: Uso %s input.eos output.asm\n", argv[0]); return 1; }
    source_fp = fopen(argv[1], "r");
    asm_fp = fopen(argv[2], "w");
    if(!source_fp || !asm_fp) { printf("Erro de arquivo.\n"); return 1; }
    parse();
    fclose(source_fp); fclose(asm_fp);
    return 0;
}
