/* --- WhaleOS Extensible Engine Target --- */
#include <stdint.h>

int k_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

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
char current_tag[32] = "[WhaleOS]# ";

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

void sys_vfs_list() {
    k_print("\nArquivos no VFS:\n");
    if(fs_count == 0) { k_print("  (vazio)\n"); }
    for(int idx = 0; idx < fs_count; idx++) { k_print("  - "); k_print(storage_ram[idx].name); k_print("\n"); }
}

void run_native_tcc_compiler(const char* code) {
    k_print("\n[WhaleOS TCC v1.0] Compilando no Heap...\n");
    int loop_limit = 1; char print_buffer[64] = {0};
    int has_for = 0; int has_printf = 0;

    char* token_table = (char*)whale_malloc(256);
    if(!token_table) return;

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
        for(int i = 0; i < loop_limit; i++) {
            if(has_for) { k_print("Iteracao "); k_print_char('0' + i); k_print(": "); }
            k_print(print_buffer);
        }
    }
    whale_free(token_table);
}

char input_buffer[64]; int input_len = 0; char saved_editor_name[16] = {0};
char kbd_map_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };
char kbd_map_shift[128]  = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };

void process_shell_command(char* cmd) {
    if (k_strcmp(cmd, "malloc") == 0) {
        k_print("\n[Malloc] Alocando 16 bytes...\n");
        void* ptr = whale_malloc(16);
        if(ptr) { k_print("[OK] Sucesso!\n"); whale_free(ptr); }
    }
    else if (k_strcmp(cmd, "dir") == 0) {
        sys_vfs_list();
    }
}


void whale_kernel_main() {
    int i = 0;
    while("WhaleOS C:/"[i]) { current_tag[i] = "WhaleOS C:/"[i]; i++; } current_tag[i] = 0;
    init_whale_malloc();
    current_color_attribute = 0x0A;
    for(int i=0; i<80*25*2; i+=2) { vga_mem[i]=' '; vga_mem[i+1]=current_color_attribute; }
    k_print("[WhaleOS Framework Shell Ativo]\n");
    k_print(current_tag);
    while(1) {
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; }
            else if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; }
            else if (!(scancode & 0x80)) {
                char key = shift_pressed ? kbd_map_shift[scancode] : kbd_map_normal[scancode];
                if (key == '\n') {
                    input_buffer[input_len] = 0;
                    /* --- Execucao do Interpretador Customizado --- */
                    if (k_strcmp(input_buffer, "dir") == 0) {
                    sys_vfs_list();
                    } else {
                    k_print("\nComando nao reconhecido no motor da baleia");
                    }
                    if(!editor_mode_active) { k_print("\n"); k_print(current_tag); }
                    input_len = 0;
                } else if (key == '\b') {
                    if (input_len > 0) { input_len--; cursor_idx -= 2; vga_mem[cursor_idx] = ' '; }
                } else if (key > 0 && input_len < 63) {
                    input_buffer[input_len++] = key; k_print_char(key);
                }
            }
        }
    }
}
