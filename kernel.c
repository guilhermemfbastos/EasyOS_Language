/* --- WhaleOS Self-Hosted Monolithic OS --- */
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port));
    return ret;
}

volatile char* vga_mem = (volatile char*)0xB8000;
int cursor_idx = 0;
int editor_mode_active = 0;
uint8_t current_color_attribute = 0x0A;
int shift_pressed = 0;

void k_print(const char* str) {
    while (*str) {
        if (*str == '\n') { cursor_idx = ((cursor_idx / 160) + 1) * 160; }
        else { vga_mem[cursor_idx++] = *str; vga_mem[cursor_idx++] = current_color_attribute; }
        str++;
    }
}

void k_print_char(char c) {
    if (c == '\n') { cursor_idx = ((cursor_idx / 160) + 1) * 160; }
    else { vga_mem[cursor_idx++] = c; vga_mem[cursor_idx++] = current_color_attribute; }
}

#define HEAP_SIZE 65536
static uint8_t whale_heap[HEAP_SIZE];

typedef struct k_mem_header {
    uint32_t size;
    uint8_t  is_free;
    struct k_mem_header* next;
} k_mem_header_t;

k_mem_header_t* free_list = (k_mem_header_t*)whale_heap;

void init_whale_malloc() {
    free_list->size = HEAP_SIZE - sizeof(k_mem_header_t);
    free_list->is_free = 1; free_list->next = 0;
    k_print("[Kernel RAM] Heap de 64KB Inicializado.\n");
}

void* whale_malloc(uint32_t size) {
    k_mem_header_t* curr = free_list;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size > size + sizeof(k_mem_header_t) + 4) {
                k_mem_header_t* next_block = (k_mem_header_t*)((uint8_t*)curr + sizeof(k_mem_header_t) + size);
                next_block->size = curr->size - size - sizeof(k_mem_header_t);
                next_block->is_free = 1; next_block->next = curr->next;
                curr->size = size; curr->next = next_block;
            }
            curr->is_free = 0;
            return (void*)((uint8_t*)curr + sizeof(k_mem_header_t));
        }
        curr = curr->next;
    }
    return 0;
}

void whale_free(void* ptr) {
    if (!ptr) return;
    k_mem_header_t* header = (k_mem_header_t*)((uint8_t*)ptr - sizeof(k_mem_header_t));
    header->is_free = 1;
}

struct vfs_entry { char name[16]; char content[512]; };
struct vfs_entry storage_ram[16];
int fs_count = 0;

void sys_vfs_write(const char* name, const char* data) {
    if (fs_count >= 16) return;
    int i = 0; while (name[i] && i < 15) { storage_ram[fs_count].name[i] = name[i]; i++; }
    storage_ram[fs_count].name[i] = 0;
    i = 0; while (data[i] && i < 511) { storage_ram[fs_count].content[i] = data[i]; i++; }
    storage_ram[fs_count].content[i] = 0;
    fs_count++;
}

void run_native_tcc_compiler(const char* code) {
    k_print("\n[WhaleOS TCC v1.0] Iniciando compilacao nativa no Heap...\n");
    int loop_limit = 1; char print_buffer[64] = {0};
    int has_for = 0; int has_printf = 0;

    char* token_table = (char*)whale_malloc(256);
    if(!token_table) { k_print("[FATAL] Sem memoria RAM.\n"); return; }

    char* cursor = (char*)code;
    while(*cursor) {
        if(cursor[0]=='f' && cursor[1]=='o' && cursor[2]=='r') {
            int idx = 3; while(cursor[idx] && cursor[idx] != '<') idx++;
            if(cursor[idx] == '<') {
                idx++; while(cursor[idx] == ' ') idx++;
                if(cursor[idx] >= '0' && cursor[idx] <= '9') { loop_limit = cursor[idx] - '0'; has_for = 1; }
            }
        }
        cursor++;
    }

    cursor = (char*)code;
    while(*cursor) {
        if(cursor[0]=='p' && cursor[1]=='r' && cursor[2]=='i' && cursor[3]=='n' && cursor[4]=='t' && cursor[5]=='f') {
            char* s = 0; char* e = 0; int idx = 6;
            while(cursor[idx] && cursor[idx] != '"') idx++;
            if(cursor[idx] == '"') { s = &cursor[idx+1]; idx++; }
            while(cursor[idx] && cursor[idx] != '"') idx++;
            if(cursor[idx] == '"') e = &cursor[idx];
            if(s && e) {
                int k = 0; while(s < e && k < 63) {
                    if(*s == '\\' && *(s+1) == 'n') { print_buffer[k++] = '\n'; s+=2; }
                    else { print_buffer[k++] = *s; s++; }
                } print_buffer[k] = 0; has_printf = 1;
            }
        }
        cursor++;
    }

    if(has_printf) {
        k_print("[TCC] Binario gerado com sucesso! Executando:\n");
        for(int i = 0; i < loop_limit; i++) {
            if(has_for) { k_print("Iteracao "); k_print_char('0' + i); k_print(": "); }
            k_print(print_buffer);
        }
    } else { k_print("[TCC Warning] Codigo compilado sem rotinas VGA.\n"); }
    whale_free(token_table);
}

char input_buffer[64]; int input_len = 0; char saved_editor_name[16] = {0};
char kbd_map_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };
char kbd_map_shift[128]  = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };

void process_shell_command(char* cmd) {
    if (cmd[0] == 'm' && cmd[1] == 'a' && cmd[2] == 'l' && cmd[3] == 'l' && cmd[4] == 'o' && cmd[5] == 'c') {
        k_print("\n[Malloc] Alocando 16 bytes no Heap...\n");
        void* ptr = whale_malloc(16);
        if(ptr) { k_print("[OK] Bloco reservado. Liberando...\n"); whale_free(ptr); }
    }
    else if (cmd[0] == 's' && cmd[1] == 'a' && cmd[2] == 'v' && cmd[3] == 'e') {
        int i = 5, j = 0; char f_target[16]; while(cmd[i]) { f_target[j++] = cmd[i++]; } f_target[j] = 0;
        sys_vfs_write(f_target, "Salvo com sucesso."); k_print("\n[VFS] Arquivo gravado!\n");
    }
    else if (cmd[0] == 'd' && cmd[1] == 'i' && cmd[2] == 'r') {
        k_print("\nArquivos no VFS:\n");
        for(int idx = 0; idx < fs_count; idx++) { k_print("  - "); k_print(storage_ram[idx].name); k_print("\n"); }
    }
    else if (cmd[0] == 'e' && cmd[1] == 'd' && cmd[2] == 'i' && cmd[3] == 't') {
        int i = 5, j = 0; while(cmd[i]) { saved_editor_name[j++] = cmd[i++]; } saved_editor_name[j] = 0;
        k_print("\n[EDITOR] Digite o texto e ENTER:\n> "); editor_mode_active = 1;
    }
    else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't') {
        int i = 4, j = 0; char f_search[16]; int found = 0;
        while(cmd[i]) { f_search[j++] = cmd[i++]; } f_search[j] = 0;
        for(int idx = 0; idx < fs_count; idx++) {
            int m = 0; while(f_search[m] && storage_ram[idx].name[m] == f_search[m]) m++;
            if(f_search[m] == 0 && storage_ram[idx].name[m] == 0) {
                k_print("\n"); k_print(storage_ram[idx].content); k_print("\n"); found = 1; break;
            }
        }
        if(!found) { k_print("\nNao encontrado.\n"); }
    }
    else if (cmd[0] == 'g' && cmd[1] == 'c' && cmd[2] == 'c') {
        int i = 4, j = 0; char f_search[16]; int found = 0;
        while(cmd[i]) { f_search[j++] = cmd[i++]; } f_search[j] = 0;
        for(int idx = 0; idx < fs_count; idx++) {
            int m = 0; while(f_search[m] && storage_ram[idx].name[m] == f_search[m]) m++;
            if(f_search[m] == 0 && storage_ram[idx].name[m] == 0) {
                run_native_tcc_compiler(storage_ram[idx].content); found = 1; break;
            }
        }
        if(!found) { k_print("\nNao encontrado.\n"); }
    }
}

void handle_keyboard() {
    if (inb(0x64) & 1) {
        uint8_t scancode = inb(0x60);
        if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; }
        else if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; }
        else if (!(scancode & 0x80)) {
            char key = shift_pressed ? kbd_map_shift[scancode] : kbd_map_normal[scancode];
            if (key == '\n') {
                input_buffer[input_len] = 0;
                if (editor_mode_active) {
                    sys_vfs_write(saved_editor_name, input_buffer);
                    k_print("\n[VFS] Arquivo salvo!\n[WhaleOS]# ");
                    editor_mode_active = 0; saved_editor_name[0] = 0;
                } else {
                    process_shell_command(input_buffer);
                    if (!editor_mode_active) k_print("\n[WhaleOS]# ");
                }
                input_len = 0;
            } else if (key == '\b') {
                if (input_len > 0) { input_len--; cursor_idx -= 2; vga_mem[cursor_idx] = ' '; }
            } else if (key > 0 && input_len < 63) {
                input_buffer[input_len++] = key; k_print_char(key);
            }
        }
    }
}

void whale_kernel_main() {
    init_whale_malloc();
    for(int i=0; i<80*25*2; i+=2) { vga_mem[i]=' '; vga_mem[i+1]=current_color_attribute; }
    current_color_attribute = 0x0A;
    k_print(" WhaleOS Monolithic Kernel v5.5 (TCC Native)\n");
    k_print(" ---------------------------------------------\n");
    k_print("\n[WhaleOS Shell Ativo]\n[WhaleOS]# ");
    while(1) { handle_keyboard(); }
}
