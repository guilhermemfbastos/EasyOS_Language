#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

FILE *source_fp;
FILE *asm_fp;

char app_ext[32] = ".exe";
char imported_apps[10][64];
int imported_count = 0;

void extract_args(const char *line, char args[][64], int *arg_count) {
    *arg_count = 0;
    const char *start = strchr(line, '(');
    if (!start) return;
    start++;
    char buffer[256]; int i = 0;
    while (*start && *start != ')') {
        if (*start != '\'' && *start != '"') buffer[i++] = *start;
        start++;
    }
    buffer[i] = '\0';
    char *token = strtok(buffer, ",");
    while (token && *arg_count < 10) {
        while (*token == ' ') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') { *end = '\0'; end--; }
        strcpy(args[*arg_count], token);
        (*arg_count)++;
        token = strtok(NULL, ",");
    }
}

// NOVA FUNÇÃO: Processa linhas recursivamente para suportar arquivos inclusos nativos
void parse_eos_line(const char *line, FILE *c_out, int *current_img) {
    char trimmed[512];
    strcpy(trimmed, line);
    char *p = trimmed;
    while (isspace((unsigned char)*p)) p++;
    
    if (*p == '\0' || strncmp(p, "//", 2) == 0) return;
    
    char args[10][64]; 
    int arg_cnt = 0; 
    extract_args(p, args, &arg_cnt);
    
    // Se encontrar a tag de inclusão, abre o sub-arquivo e processa linha por linha
    if (strncmp(p, "include.File", 12) == 0) {
        char include_filename[128];
        if (sscanf(p, "include.File %s", include_filename) == 1) {
            FILE *inc_fp = fopen(include_filename, "r");
            if (inc_fp) {
                char inc_line[512];
                while (fgets(inc_line, sizeof(inc_line), inc_fp)) {
                    parse_eos_line(inc_line, c_out, current_img);
                }
                fclose(inc_fp);
            }
        }
        return;
    }
    
    // Mapeamento das instruções padrão
    if (strstr(p, "EasyOS.System.Graphics.Mouse.start") != NULL) {
        fprintf(c_out, "        if (mouse_x == 512 && mouse_y == 384) init_hardware_mouse();\n");
    } 
    else if (strstr(p, "Draw.Rectangle") != NULL && arg_cnt >= 6) {
        fprintf(c_out, "        if (!terminal_active) draw_real_rectangle(%s, %s, %s, %s, %s, %s);\n", args[0], args[1], args[2], args[3], args[4], args[5]);
    } 
    else if (strstr(p, "Draw.Label") != NULL && arg_cnt >= 5) {
        fprintf(c_out, "        if (!terminal_active) draw_real_string(\"%s\", %s, %s, %s);\n", args[0], args[2], args[3], args[1]);
    }
    else if (strncmp(p, "if", 2) == 0) {
        char match_id[16]; 
        const char *start = strchr(p, '(');
        if (start) {
            start++; int k = 0; 
            while (start[k] && start[k] != ')') { 
                if (isdigit(start[k])) { match_id[k] = start[k]; k++; } else { start++; } 
            } 
            match_id[k] = '\0';
            fprintf(c_out, "        if (clicked_object_id == %s) { menu_open = !menu_open; clicked_object_id = -1; }\n", match_id);
        }
    }
    else if (strstr(p, "Draw.Start") != NULL && arg_cnt >= 6) {
        fprintf(c_out, "        if (menu_open && !terminal_active) {\n");
        fprintf(c_out, "            draw_real_rectangle(%s, %s, %s, %s, %s, -1);\n", args[2], args[1], args[3], args[4], args[5]);
        fprintf(c_out, "            draw_real_rectangle(%s, 20, 1, %s, %s, -1);\n", args[2], args[4], args[5]);
        fprintf(c_out, "            draw_real_string(\"%s\", %s + 10, %s + 6, 15);\n", args[0], args[4], args[5]);
        fprintf(c_out, "            for (int i = 0; i < %d; i++) {\n", imported_count);
        fprintf(c_out, "                draw_real_string(app_registry[i], %s + 20, %s + 35 + (i * 25), 15);\n", args[4], args[5]);
        fprintf(c_out, "                draw_real_rectangle(%s - 40, 20, 8, %s + 10, %s + 30 + (i * 25), 300 + i);\n", args[2], args[4], args[5]);
        fprintf(c_out, "            }\n        }\n");
    }
}

// Varre em busca de imagens (Suporta include de imagens nos sub-arquivos também!)
void scan_for_images(const char *filename, FILE *c_out, int *img_cnt) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (strstr(p, "Draw.Image") != NULL) {
            char cmd[256];
            sprintf(cmd, "python3 img2c.py logo.png 1024 768 %d", *img_cnt);
            system(cmd);
            fprintf(c_out, "#include \"img_%d.h\"\n", *img_cnt);
            (*img_cnt)++;
        }
        else if (strncmp(p, "include.File", 12) == 0) {
            char sub_file[128];
            if (sscanf(p, "include.File %s", sub_file) == 1) {
                scan_for_images(sub_file, c_out, img_cnt);
            }
        }
    }
    fclose(fp);
}

void process_file() {
    FILE *c_out = fopen("kernel.c", "w");
    fprintf(c_out, "/* --- WhaleOS True Color Engine (Modular Loader Build) --- */\n");
    fprintf(c_out, "#include <stdint.h>\n");
    fprintf(c_out, "#include \"bin_calculadora.h\"\n\n");
    
    // Executa a varredura de imagens recursiva nos módulos
    int img_cnt = 0;
    fseek(source_fp, 0, SEEK_SET);
    char line[512];
    // Como process_file lê o argumento mestre, vamos scannar a partir dele
    // Usando uma gambiarra segura: re-scannar sabendo o fluxo mestre
    fclose(source_fp);
    scan_for_images("main.eos", c_out, &img_cnt);
    source_fp = fopen("main.eos", "r");
    
    fprintf(c_out, "\n");

    // Funções Core
    fprintf(c_out, "int k_strcmp(const char* s1, const char* s2) { while (*s1 && (*s1 == *s2)) { s1++; s2++; } return *(const unsigned char*)s1 - *(const unsigned char*)s2; }\n");
    fprintf(c_out, "void k_memcpy(void* dest, const void* src, uint32_t count) { uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src; while (count--) *d++ = *s++; }\n");
    fprintf(c_out, "void k_strcpy(char* dest, const char* src) { while ((*dest++ = *src++)); }\n\n");
    fprintf(c_out, "static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile(\"inb %%w1, %%b0\" : \"=a\"(ret) : \"Nd\"(port)); return ret; }\n");
    fprintf(c_out, "static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile(\"outb %%b0, %%w1\" : : \"a\"(val), \"Nd\"(port)); }\n\n");
    
    // Globais
    fprintf(c_out, "volatile uint32_t* vga;\n");
    fprintf(c_out, "uint32_t double_buffer[1024 * 768];\n\n");
    fprintf(c_out, "const uint32_t legacy_palette[16] = { 0x00000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA, 0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA, 0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF, 0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF };\n");
    fprintf(c_out, "uint32_t get_color(uint32_t c) { if (c <= 15) return legacy_palette[c]; return c; }\n\n");
    fprintf(c_out, "int mouse_x = 512; int mouse_y = 384; uint8_t mouse_cycle = 0; uint8_t mouse_byte[3];\n");
    fprintf(c_out, "int clicked_object_id = -1; int mouse_was_clicked = 0; int terminal_active = 0;\nchar input_buffer[64]; int input_len = 0;\n");
    fprintf(c_out, "struct gui_object { int id; int x; int y; int w; int h; };\n");
    fprintf(c_out, "struct gui_object objects[50]; int obj_count = 0;\n");
    fprintf(c_out, "int menu_open = 0; int app_open[10] = {0};\n\n");

    // Fonte e Cursor
    fprintf(c_out, "unsigned char font8x8[95][8] = { {0},{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},{0x6C,0x6C,0x6C,0},{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},{0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0},{0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0},{0x38,0x6C,0x6C,0x38,0x6D,0x66,0x3B,0},{0x0C,0x18,0x30,0},{0x18,0x30,0x60,0x60,0x60,0x30,0x18,0},{0x60,0x30,0x18,0x18,0x18,0x30,0x60,0},{0x00,0x66,0x3C,0xFF,0x3C,0x66,0},{0x00,0x18,0x18,0x7E,0x18,0x18,0},{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},{0x00,0x00,0x00,0x7E,0},{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0},{0x06,0x0C,0x18,0x30,0x60,0xC0,0},{0x3C,0x66,0x66,0x6E,0x76,0x66,0x3C,0},{0x18,0x38,0x58,0x18,0x18,0x18,0x7E,0},{0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0},{0x7E,0x06,0x1C,0x06,0x06,0x66,0x3C,0},{0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0},{0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0},{0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0},{0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0},{0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0},{0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0},{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0},{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},{0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0},{0x00,0x00,0x7E,0x00,0x7E,0},{0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0},{0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0},{0x3C,0x66,0x6E,0x6E,0x60,0x3E,0},{0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0},{0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0},{0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0},{0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0},{0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0},{0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0},{0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0},{0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0},{0x3E,0x18,0x18,0x18,0x18,0x18,0x3E,0},{0x06,0x06,0x06,0x06,0x06,0x66,0x3C,0},{0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0},{0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0},{0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0},{0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0},{0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0},{0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0},{0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0},{0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0},{0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0},{0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0},{0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0},{0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0},{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},{0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0},{0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0},{0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0},{0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0},{0x00,0x60,0x30,0x18,0x0C,0x06,0},{0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0},{0x18,0x3C,0x66,0},{0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0},{0x30,0x18,0},{0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0},{0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0},{0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0},{0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0},{0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0},{0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0},{0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C},{0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0},{0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0},{0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38},{0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0},{0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0},{0x00,0x00,0x76,0x7F,0x6B,0x6B,0x6B,0},{0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0},{0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0},{0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0}, {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0},{0x30,0x78,0x30,0x30,0x30,0x34,0x18,0},{0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0},{0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0},{0x00,0x00,0x63,0x6B,0x6B,0x7F,0x36,0},{0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0},{0x00,0x00,0x66,0x66,0x66,0x3E,0x0C,0x38},{0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0},{0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0},{0x18,0x18,0x18,0x00,0x18,0x18,0x18,0},{0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0},{0x3B,0x6E,0} };\n");
    fprintf(c_out, "const uint8_t win_cursor[8][8] = { {15,15},{15,15,15},{15,15,15,15},{15,15,15,15,15},{15,15,15,15,15,15},{15,15,15,15,15,15,15},{15,15,15},{15,0,0,15} };\n\n");

    // Funções Gráficas
    fprintf(c_out, "void draw_real_char(char c, int x, int y, uint32_t color) {\n    if (c < 32 || c > 126) return;\n    int idx = c - 32; uint32_t c_hex = get_color(color);\n    for (int i=0; i<8; i++) for (int j=0; j<8; j++) if (font8x8[idx][i] & (1<<(7-j))) if((y+i)<768 && (x+j)<1024) double_buffer[(y+i)*1024 + (x+j)] = c_hex;\n}\n");
    fprintf(c_out, "void draw_real_string(const char* s, int x, int y, uint32_t color) { while(*s) { draw_real_char(*s, x, y, color); x += 8; s++; } }\n");
    fprintf(c_out, "void draw_real_rectangle(int w, int h, uint32_t color, int x, int y, int id) {\n    if(obj_count < 50 && id > 0) { objects[obj_count].id=id; objects[obj_count].x=x; objects[obj_count].y=y; objects[obj_count].w=w; objects[obj_count].h=h; obj_count++; }\n    uint32_t c_hex = get_color(color);\n    for(int i=0; i<h; i++) for(int j=0; j<w; j++) if((y+i)<768 && (x+j)<1024) double_buffer[(y+i)*1024 + (x+j)] = c_hex;\n}\n\n");
    fprintf(c_out, "void draw_image_array(const uint32_t* img_data, int w, int h, int start_x, int start_y) {\n    for(int y=0; y<768; y++) {\n        for(int x=0; x<1024; x++) {\n            double_buffer[y*1024 + x] = img_data[y*1024 + x];\n        }\n    }\n}\n\n");

    // Drivers
    fprintf(c_out, "void mouse_wait(uint8_t a_type) { uint32_t timeout=100000; if(a_type==0) { while(timeout--) if((inb(0x64)&1)==1) return; } else { while(timeout--) if((inb(0x64)&2)==0) return; } }\n");
    fprintf(c_out, "void mouse_write(uint8_t a_write) { mouse_wait(1); outb(0x64, 0xD4); mouse_wait(1); outb(0x60, a_write); }\n");
    fprintf(c_out, "void init_hardware_mouse() { uint8_t s; mouse_wait(1); outb(0x64, 0xA8); mouse_wait(1); outb(0x64, 0x20); mouse_wait(0); s=(inb(0x60)|2); mouse_wait(1); outb(0x64, 0x60); mouse_wait(1); outb(0x60, s); mouse_write(0xF4); inb(0x60); }\n");
    fprintf(c_out, "char kbd_map[128] = { 0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\\b','\\t','q','w','e','r','t','y','u','i','o','p','[',']','\\n',0,'a','s','d','f','g','h','j','k','l',';','\\'','`',0,'\\\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ' };\n");
    fprintf(c_out, "void handle_keyboard() { uint8_t s = inb(0x64); if(s&1 && !(s&0x20)) { uint8_t scan = inb(0x60); if(!(scan&0x80)) { char key = kbd_map[scan]; if(key=='\\n') { input_buffer[input_len]=0; if(k_strcmp(input_buffer, \"cli\")==0) terminal_active=1; else if(k_strcmp(input_buffer, \"gui\")==0) terminal_active=0; input_len=0; } else if(key>0 && input_len<30) input_buffer[input_len++]=key; } } }\n\n");
    fprintf(c_out, "void update_hardware_mouse() {\n    uint8_t s = inb(0x64); if(!(s&1) || !(s&0x20)) return; uint8_t val = inb(0x60);\n    switch(mouse_cycle) { case 0: if(val&0x08) { mouse_byte[0]=val; mouse_cycle++; } break; case 1: mouse_byte[1]=val; mouse_cycle++; break; case 2: mouse_byte[2]=val; mouse_cycle=0; mouse_x += (int8_t)mouse_byte[1]; mouse_y -= (int8_t)mouse_byte[2]; if(mouse_x<0) mouse_x=0; if(mouse_x>1016) mouse_x=1016; if(mouse_y<0) mouse_y=0; if(mouse_y>760) mouse_y=760;\n");
    fprintf(c_out, "    if (mouse_byte[0] & 1) {\n        if (mouse_was_clicked == 0) {\n            for (int i = 0; i < obj_count; i++) {\n                if (mouse_x >= objects[i].x && mouse_x <= (objects[i].x + objects[i].w) && mouse_y >= objects[i].y && mouse_y <= (objects[i].y + objects[i].h)) {\n                    clicked_object_id = objects[i].id;\n                }\n            }\n            mouse_was_clicked = 1;\n        }\n    } else { mouse_was_clicked = 0; }\n    break; }\n}\n\n");

    // PASS 1: Captura VFS do arquivo mestre reaberto
    fseek(source_fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), source_fp)) {
        if (strstr(line, "bootSet.ExecutableApp") != NULL) { char* p = strchr(line, '='); if (p) sscanf(p+1, " %s", app_ext); }
        else if (strstr(line, "import.MyAppExecutable") != NULL) {
            char full_name[64]; sscanf(line, "import.MyAppExecutable %s", full_name);
            char* dot = strchr(full_name, '.'); if (dot) *dot = '\0';
            strcpy(imported_apps[imported_count++], full_name);
        }
    }

    fprintf(c_out, "void whale_kernel_main(uint32_t magic, uint32_t mbd) {\n");
    fprintf(c_out, "    if (magic != 0x2BADB002) return;\n");
    fprintf(c_out, "    uint32_t fb_addr = *(((uint32_t*)(mbd + 88)));\n");
    fprintf(c_out, "    vga = (volatile uint32_t*)fb_addr;\n\n");

    fprintf(c_out, "    uint32_t* syscall_table = (uint32_t*)0x5000;\n");
    fprintf(c_out, "    syscall_table[0] = (uint32_t)&draw_real_rectangle;\n");
    fprintf(c_out, "    syscall_table[1] = (uint32_t)&draw_real_string;\n\n");

    fprintf(c_out, "    char app_registry[%d][32];\n", imported_count == 0 ? 1 : imported_count);
    for(int i = 0; i < imported_count; i++) {
        fprintf(c_out, "    k_strcpy(app_registry[%d], \"%s\");\n", i, imported_apps[i]);
    }
    
    fprintf(c_out, "    while (1) {\n");
    fprintf(c_out, "        handle_keyboard();\n");
    fprintf(c_out, "        update_hardware_mouse();\n");
    fprintf(c_out, "        obj_count = 0;\n\n"); 
    
    fprintf(c_out, "        if (!terminal_active) {\n");
    fprintf(c_out, "            draw_image_array(img_0, 1024, 768, 0, 0);\n");
    fprintf(c_out, "        } else {\n");
    fprintf(c_out, "            for (int i = 0; i < 786432; i++) double_buffer[i] = 0xFF000000;\n");
    fprintf(c_out, "        }\n\n");
    
    // PASS 2: Dispara a transpilação utilizando a nova rotina recursiva modular
    fseek(source_fp, 0, SEEK_SET);
    int current_img = 0;
    while (fgets(line, sizeof(line), source_fp)) {
        parse_eos_line(line, c_out, &current_img);
    }
    
    // Janelas e Render Final do WM
    fprintf(c_out, "        if (!terminal_active) {\n");
    fprintf(c_out, "            for (int i = 0; i < %d; i++) {\n", imported_count);
    fprintf(c_out, "                if (app_open[i]) {\n");
    fprintf(c_out, "                    int wx = 300 + (i * 40); int wy = 200 + (i * 40);\n");
    fprintf(c_out, "                    draw_real_rectangle(400, 300, 7, wx, wy, -1);\n");
    fprintf(c_out, "                    draw_real_rectangle(400, 25, 1, wx, wy, -1);\n");
    fprintf(c_out, "                    draw_real_string(app_registry[i], wx + 10, wy + 8, 15);\n");
    fprintf(c_out, "                    draw_real_rectangle(20, 15, 4, wx + 375, wy + 5, 400 + i);\n");
    fprintf(c_out, "                    draw_real_string(\"X\", wx + 381, wy + 8, 15);\n");
    fprintf(c_out, "                    if (i == 0) {\n");
    fprintf(c_out, "                        k_memcpy((void*)0x800000, app_bin_calculadora, app_bin_calculadora_len);\n");
    fprintf(c_out, "                        void (*run_external_app)() = (void(*)())0x800000;\n");
    fprintf(c_out, "                        run_external_app();\n");
    fprintf(c_out, "                    }\n");
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            }\n\n");
    fprintf(c_out, "            if (clicked_object_id >= 300 && clicked_object_id < 400) { app_open[clicked_object_id - 300] = 1; menu_open = 0; clicked_object_id = -1; }\n");
    fprintf(c_out, "            if (clicked_object_id >= 400 && clicked_object_id < 500) { app_open[clicked_object_id - 400] = 0; clicked_object_id = -1; }\n");
    fprintf(c_out, "            clicked_object_id = -1;\n\n");
    fprintf(c_out, "            for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) if (win_cursor[i][j]) if (mouse_y + i < 768 && mouse_x + j < 1024) double_buffer[(mouse_y + i) * 1024 + (mouse_x + j)] = 0xFFFFFFFF;\n");
    fprintf(c_out, "            k_memcpy((void*)vga, double_buffer, 1024 * 768 * 4);\n");
    fprintf(c_out, "        } else {\n");
    fprintf(c_out, "            draw_real_string(\"WHALEOS NATIVE CLI\", 10, 10, 10);\n");
    fprintf(c_out, "            draw_real_string(\"# \", 10, 30, 15);\n");
    fprintf(c_out, "            for (int n = 0; n < input_len; n++) draw_real_char(input_buffer[n], 26 + (n * 8), 30, 15);\n");
    fprintf(c_out, "            k_memcpy((void*)vga, double_buffer, 1024 * 768 * 4);\n");
    fprintf(c_out, "        }\n    }\n}\n");
    fclose(c_out);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    source_fp = fopen(argv[1], "r");
    if (!source_fp) return 1;
    process_file();
    fclose(source_fp);
    return 0;
}