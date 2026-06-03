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

int use_default_shell = 1; 
char custom_terminal_tag[32] = "[WhaleOS]# ";

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
    static int kernel_infrastructure_written = 0;
    static FILE *c_out = NULL;

    if (!kernel_infrastructure_written) {
        c_out = fopen("kernel.c", "w");
        
        fprintf(c_out, "/* --- WhaleOS Extensible Engine Target --- */\n");
        fprintf(c_out, "#include <stdint.h>\n\n");

        fprintf(c_out, "int k_strcmp(const char* s1, const char* s2) {\n");
        fprintf(c_out, "    while (*s1 && (*s1 == *s2)) { s1++; s2++; }\n");
        fprintf(c_out, "    return *(const unsigned char*)s1 - *(const unsigned char*)s2;\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "static inline uint8_t inb(uint16_t port) {\n");
        fprintf(c_out, "    uint8_t ret;\n");
        fprintf(c_out, "    __asm__ volatile(\"inb %%w1, %%b0\" : \"=a\"(ret) : \"Nd\"(port));\n");
        fprintf(c_out, "    return ret;\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "volatile char* vga_mem = (volatile char*)0xB8000;\n");
        fprintf(c_out, "int cursor_idx = 0;\n");
        fprintf(c_out, "int editor_mode_active = 0;\n");
        fprintf(c_out, "uint8_t current_color_attribute = 0x0A;\n");
        fprintf(c_out, "int shift_pressed = 0;\n");
        fprintf(c_out, "char current_tag[32] = \"[WhaleOS]# \";\n\n");

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

        fprintf(c_out, "void sys_vfs_list() {\n");
        fprintf(c_out, "    k_print(\"\\nArquivos no VFS:\\n\");\n");
        fprintf(c_out, "    if(fs_count == 0) { k_print(\"  (vazio)\\n\"); }\n");
        fprintf(c_out, "    for(int idx = 0; idx < fs_count; idx++) { k_print(\"  - \"); k_print(storage_ram[idx].name); k_print(\"\\n\"); }\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "void run_native_tcc_compiler(const char* code) {\n");
        fprintf(c_out, "    k_print(\"\\n[WhaleOS TCC v1.0] Compilando no Heap...\\n\");\n");
        fprintf(c_out, "    int loop_limit = 1; char print_buffer[64] = {0};\n");
        fprintf(c_out, "    int has_for = 0; int has_printf = 0;\n\n");
        fprintf(c_out, "    char* token_table = (char*)whale_malloc(256);\n");
        fprintf(c_out, "    if(!token_table) return;\n\n");
        fprintf(c_out, "    char* cursor = (char*)code;\n");
        fprintf(c_out, "    while(*cursor) {\n");
        fprintf(c_out, "        if(cursor[0]=='f' && cursor[1]=='o' && cursor[2]=='r') {\n");
        fprintf(c_out, "            int idx = 3; while(cursor[idx] && cursor[idx] != '<') idx++;\n");
        fprintf(c_out, "            if(cursor[idx] == '<') {\n");
        fprintf(c_out, "                idx++; while(cursor[idx] == ' ') idx++;\n");
        fprintf(c_out, "                if(cursor[idx] >= '0' && cursor[idx] <= '9') { loop_limit = cursor[idx] - '0'; has_for = 1; }\n");
        fprintf(c_out, "            }\n");
        fprintf(c_out, "        }\n        cursor++;\n");
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
        fprintf(c_out, "        for(int i = 0; i < loop_limit; i++) {\n");
        fprintf(c_out, "            if(has_for) { k_print(\"Iteracao \"); k_print_char('0' + i); k_print(\": \"); }\n");
        fprintf(c_out, "            k_print(print_buffer);\n");
        fprintf(c_out, "        }\n");
        fprintf(c_out, "    }\n    whale_free(token_table);\n");
        fprintf(c_out, "}\n\n");

        fprintf(c_out, "char input_buffer[64]; int input_len = 0; char saved_editor_name[16] = {0};\n");
        fprintf(c_out, "char kbd_map_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\b', '\\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\\'', '`', 0, '\\\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };\n");
        fprintf(c_out, "char kbd_map_shift[128]  = { 0, 27, '!', '@', '#', '$', '%%', '^', '&', '*', '(', ')', '_', '+', '\\b', '\\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };\n\n");

        fprintf(c_out, "void process_shell_command(char* cmd) {\n");
        fprintf(c_out, "    if (k_strcmp(cmd, \"malloc\") == 0) {\n");
        fprintf(c_out, "        k_print(\"\\n[Malloc] Alocando 16 bytes...\\n\");\n");
        fprintf(c_out, "        void* ptr = whale_malloc(16);\n");
        fprintf(c_out, "        if(ptr) { k_print(\"[OK] Sucesso!\\n\"); whale_free(ptr); }\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "    else if (k_strcmp(cmd, \"dir\") == 0) {\n");
        fprintf(c_out, "        sys_vfs_list();\n");
        fprintf(c_out, "    }\n");
        fprintf(c_out, "}\n\n");

        fprintf(asm_fp, "bits 32\nsection .multiboot\nalign 4\n    dd 0x1BADB002\n    dd 0x00\n    dd - (0x1BADB002 + 0x00)\n\n");
        fprintf(asm_fp, "section .text\nglobal _start\nextern whale_kernel_main\n_start:\n    cli\n    mov esp, kernel_stack_top\n    call whale_kernel_main\n.hang:\n    hlt\n    jmp .hang\n\nsection .bss\nalign 16\nkernel_stack_bottom:\n    resb 16384\nkernel_stack_top:\n");

        kernel_infrastructure_written = 1;
    }

    // Pass 1: Global Settings
    fseek(file_stream, 0, SEEK_SET);
    while (fgets(line, sizeof(line), file_stream)) {
        char *trimmed = line; while (isspace((unsigned char)*trimmed)) trimmed++;
        if (strstr(trimmed, "bootWith.Shell = False") != NULL || strstr(trimmed, "bootWith.Shell = false") != NULL) {
            use_default_shell = 0;
        }
        if (strstr(trimmed, "TagScreen") != NULL) {
            extract_string(trimmed, custom_terminal_tag);
        }
    }

    // GERA A MAIN DO KERNEL
    fprintf(c_out, "\nvoid whale_kernel_main() {\n");
    fprintf(c_out, "    int i = 0;\n");
    fprintf(c_out, "    while(\"%s\"[i]) { current_tag[i] = \"%s\"[i]; i++; } current_tag[i] = 0;\n", custom_terminal_tag, custom_terminal_tag);
    fprintf(c_out, "    init_whale_malloc();\n");

    // Pass 2: Boot Sequence (Ignora tudo que estiver dentro de chaves)
    fseek(file_stream, 0, SEEK_SET);
    int if_block_open = 0;
    int else_block_open = 0;
    while (fgets(line, sizeof(line), file_stream)) {
        char *trimmed = line; while (isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0' || strncmp(trimmed, "//", 2) == 0 || strstr(trimmed, "boot.start") != NULL) continue;
        if (strstr(trimmed, "bootWith.Shell") != NULL || strstr(trimmed, "TagScreen") != NULL) continue;

        if (strncmp(trimmed, "if", 2) == 0) { if_block_open = 1; continue; }
        if (strncmp(trimmed, "} else", 6) == 0 || strncmp(trimmed, "else", 4) == 0) { else_block_open = 1; continue; }
        if (trimmed[0] == '}' || strncmp(trimmed, "end", 3) == 0) { if_block_open = 0; else_block_open = 0; continue; }

        if (if_block_open || else_block_open) continue;

        if (strstr(trimmed, "Screen.Color") != NULL) {
            char color_hex[16]; extract_string(trimmed, color_hex);
            fprintf(c_out, "    current_color_attribute = 0x%02X;\n", parse_hex_color(color_hex));
        }
        else if (strstr(trimmed, "printScreen") != NULL) {
            char txt[MAX_TOKEN_LEN]; extract_string(trimmed, txt);
            fprintf(c_out, "    k_print(\"%s\\n\");\n", txt);
        }
        else if (strstr(trimmed, "always") != NULL) break;
    }

    fprintf(c_out, "    for(int i=0; i<80*25*2; i+=2) { vga_mem[i]=' '; vga_mem[i+1]=current_color_attribute; }\n");
    fprintf(c_out, "    k_print(\"[WhaleOS Framework Shell Ativo]\\n\");\n");
    fprintf(c_out, "    k_print(current_tag);\n");
    
    fprintf(c_out, "    while(1) {\n");
    fprintf(c_out, "        if (inb(0x64) & 1) {\n");
    fprintf(c_out, "            uint8_t scancode = inb(0x60);\n");
    fprintf(c_out, "            if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; }\n");
    fprintf(c_out, "            else if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; }\n");
    fprintf(c_out, "            else if (!(scancode & 0x80)) {\n");
    fprintf(c_out, "                char key = shift_pressed ? kbd_map_shift[scancode] : kbd_map_normal[scancode];\n");
    fprintf(c_out, "                if (key == '\\n') {\n");
    fprintf(c_out, "                    input_buffer[input_len] = 0;\n");
    
    if (use_default_shell) {
        fprintf(c_out, "                    process_shell_command(input_buffer);\n");
    } else {
        fprintf(c_out, "                    /* --- Execucao do Interpretador Customizado --- */\n");
        
        // Pass 3: Shell Customizado (Somente comandos dentro das chaves)
        fseek(file_stream, 0, SEEK_SET);
        int shell_if_open = 0;
        while (fgets(line, sizeof(line), file_stream)) {
            char *trimmed = line; while (isspace((unsigned char)*trimmed)) trimmed++;
            
            if (strncmp(trimmed, "if", 2) == 0) {
                char match_str[64]; extract_string(trimmed, match_str);
                fprintf(c_out, "                    if (k_strcmp(input_buffer, \"%s\") == 0) {\n", match_str);
                shell_if_open = 1;
            } else if (strncmp(trimmed, "} else", 6) == 0 || strncmp(trimmed, "else", 4) == 0) {
                fprintf(c_out, "                    } else {\n");
                shell_if_open = 1;
            } else if (shell_if_open && (trimmed[0] == '}' || strncmp(trimmed, "end", 3) == 0)) {
                fprintf(c_out, "                    }\n");
                shell_if_open = 0;
            } else if (shell_if_open) {
                if (strstr(trimmed, "FileMode.List") != NULL) {
                    fprintf(c_out, "                    sys_vfs_list();\n");
                } else if (strstr(trimmed, "FileMode.start()") != NULL) {
                    fprintf(c_out, "                    editor_mode_active = 1;\n");
                    fprintf(c_out, "                    k_print(\"\\n[FileMode] Digite o texto e ENTER:\\n> \");\n");
                } else if (strstr(trimmed, "printScreen") != NULL) {
                    char txt[MAX_TOKEN_LEN]; extract_string(trimmed, txt);
                    fprintf(c_out, "                    k_print(\"\\n%s\");\n", txt);
                }
            }
        }
    }
    
    fprintf(c_out, "                    if(!editor_mode_active) { k_print(\"\\n\"); k_print(current_tag); }\n");
    fprintf(c_out, "                    input_len = 0;\n");
    fprintf(c_out, "                } else if (key == '\\b') {\n");
    fprintf(c_out, "                    if (input_len > 0) { input_len--; cursor_idx -= 2; vga_mem[cursor_idx] = ' '; }\n");
    fprintf(c_out, "                } else if (key > 0 && input_len < 63) {\n");
    fprintf(c_out, "                    input_buffer[input_len++] = key; k_print_char(key);\n");
    fprintf(c_out, "                }\n            }\n        }\n    }\n}\n");
    
    fclose(c_out);
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
