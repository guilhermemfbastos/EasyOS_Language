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
    char buffer[256]; 
    int i = 0;
    
    while (*start && *start != ')') {
        if (*start != '\'' && *start != '"') {
            buffer[i++] = *start;
        }
        start++;
    }
    buffer[i] = '\0';
    
    char *token = strtok(buffer, ",");
    while (token && *arg_count < 10) {
        while (*token == ' ') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') { 
            *end = '\0'; 
            end--; 
        }
        strcpy(args[*arg_count], token);
        (*arg_count)++;
        token = strtok(NULL, ",");
    }
}

void process_file() {
    FILE *c_out = fopen("kernel.c", "w");
    fprintf(c_out, "/* --- WhaleOS 32-Bit True Color Engine (Debounced Click) --- */\n");
    fprintf(c_out, "#include <stdint.h>\n\n");
    
    // Processamento de Imagens
    int img_cnt = 0;
    fseek(source_fp, 0, SEEK_SET);
    char line[512];
    while (fgets(line, sizeof(line), source_fp)) {
        if (strstr(line, "Draw.Image") != NULL) {
            char args[10][64]; 
            int arg_cnt; 
            extract_args(line, args, &arg_cnt);
            char cmd[256];
            sprintf(cmd, "python3 img2c.py %s %s %s %d", args[0], args[2], args[1], img_cnt);
            system(cmd);
            fprintf(c_out, "#include \"img_%d.h\"\n", img_cnt);
            img_cnt++;
        }
    }
    fprintf(c_out, "\n");

    // Funções Core
    fprintf(c_out, "int k_strcmp(const char* s1, const char* s2) {\n");
    fprintf(c_out, "    while (*s1 && (*s1 == *s2)) {\n");
    fprintf(c_out, "        s1++; s2++;\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "    return *(const unsigned char*)s1 - *(const unsigned char*)s2;\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void k_memcpy(void* dest, const void* src, uint32_t count) {\n");
    fprintf(c_out, "    uint8_t* d = (uint8_t*)dest;\n");
    fprintf(c_out, "    const uint8_t* s = (const uint8_t*)src;\n");
    fprintf(c_out, "    while (count--) *d++ = *s++;\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void k_strcpy(char* dest, const char* src) {\n");
    fprintf(c_out, "    while ((*dest++ = *src++));\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "static inline uint8_t inb(uint16_t port) {\n");
    fprintf(c_out, "    uint8_t ret;\n");
    fprintf(c_out, "    __asm__ volatile(\"inb %%w1, %%b0\" : \"=a\"(ret) : \"Nd\"(port));\n");
    fprintf(c_out, "    return ret;\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "static inline void outb(uint16_t port, uint8_t val) {\n");
    fprintf(c_out, "    __asm__ volatile(\"outb %%b0, %%w1\" : : \"a\"(val), \"Nd\"(port));\n");
    fprintf(c_out, "}\n\n");
    
    // Globais Gráficas
    fprintf(c_out, "volatile uint32_t* vga;\n");
    fprintf(c_out, "uint32_t double_buffer[1024 * 768];\n\n");
    
    fprintf(c_out, "const uint32_t legacy_palette[16] = {\n");
    fprintf(c_out, "    0x00000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,\n");
    fprintf(c_out, "    0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,\n");
    fprintf(c_out, "    0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,\n");
    fprintf(c_out, "    0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF\n");
    fprintf(c_out, "};\n\n");
    
    fprintf(c_out, "uint32_t get_color(uint32_t c) {\n");
    fprintf(c_out, "    if (c <= 15) return legacy_palette[c];\n");
    fprintf(c_out, "    return c;\n");
    fprintf(c_out, "}\n\n");
    
    // Globais de Controle de Estado e Mouse
    fprintf(c_out, "int mouse_x = 512;\nint mouse_y = 384;\nuint8_t mouse_cycle = 0;\nuint8_t mouse_byte[3];\n");
    fprintf(c_out, "int clicked_object_id = -1;\n");
    fprintf(c_out, "int mouse_was_clicked = 0;\n"); // NOVO: Gatilho de borda (Debounce)
    fprintf(c_out, "int terminal_active = 0;\nchar input_buffer[64];\nint input_len = 0;\n");
    fprintf(c_out, "struct gui_object { int id; int x; int y; int w; int h; };\n");
    fprintf(c_out, "struct gui_object objects[50];\nint obj_count = 0;\n");
    fprintf(c_out, "int menu_open = 0;\nint app_open[10] = {0};\n\n");

    // Fonte e Cursor
    fprintf(c_out, "unsigned char font8x8[95][8] = {\n");
    fprintf(c_out, "    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00}, {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},\n");
    fprintf(c_out, "    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, {0x38,0x6C,0x6C,0x38,0x6D,0x66,0x3B,0x00}, {0x0C,0x18,0x30,0x00,0x00,0x00,0x00,0x00},\n");
    fprintf(c_out, "    {0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00}, {0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00}, {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},\n");
    fprintf(c_out, "    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, {0x06,0x0C,0x18,0x30,0x60,0xC0,0x00,0x00},\n");
    fprintf(c_out, "    {0x3C,0x66,0x66,0x6E,0x76,0x66,0x3C,0x00}, {0x18,0x38,0x58,0x18,0x18,0x18,0x7E,0x00}, {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, {0x7E,0x06,0x1C,0x06,0x06,0x66,0x3C,0x00},\n");
    fprintf(c_out, "    {0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00}, {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}, {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},\n");
    fprintf(c_out, "    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}, {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},\n");
    fprintf(c_out, "    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00},\n");
    fprintf(c_out, "    {0x3C,0x66,0x6E,0x6E,0x60,0x3E,0x00,0x00}, {0x3C,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},\n");
    fprintf(c_out, "    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},\n");
    fprintf(c_out, "    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, {0x3E,0x18,0x18,0x18,0x18,0x18,0x3E,0x00}, {0x06,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},\n");
    fprintf(c_out, "    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},\n");
    fprintf(c_out, "    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},\n");
    fprintf(c_out, "    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},\n");
    fprintf(c_out, "    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},\n");
    fprintf(c_out, "    {0x00,0x60,0x30,0x18,0x0C,0x06,0x00,0x00}, {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},\n");
    fprintf(c_out, "    {0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00},\n");
    fprintf(c_out, "    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, {0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00}, {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C},\n");
    fprintf(c_out, "    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0x38}, {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00},\n");
    fprintf(c_out, "    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, {0x00,0x00,0x76,0x7F,0x6B,0x6B,0x6B,0x00}, {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00},\n");
    fprintf(c_out, "    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00},\n");
    fprintf(c_out, "    {0x30,0x78,0x30,0x30,0x30,0x34,0x18,0x00}, {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, {0x00,0x00,0x63,0x6B,0x6B,0x7F,0x36,0x00},\n");
    fprintf(c_out, "    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, {0x00,0x00,0x66,0x66,0x66,0x3E,0x0C,0x38}, {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00},\n");
    fprintf(c_out, "    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}\n");
    fprintf(c_out, "};\n\n");
    
    fprintf(c_out, "const uint8_t win_cursor[8][8] = {\n");
    fprintf(c_out, "    {15, 15,  0,  0,  0,  0,  0,  0},\n");
    fprintf(c_out, "    {15, 15, 15,  0,  0,  0,  0,  0},\n");
    fprintf(c_out, "    {15, 15, 15, 15,  0,  0,  0,  0},\n");
    fprintf(c_out, "    {15, 15, 15, 15, 15,  0,  0,  0},\n");
    fprintf(c_out, "    {15, 15, 15, 15, 15, 15,  0,  0},\n");
    fprintf(c_out, "    {15, 15, 15, 15, 15, 15, 15,  0},\n");
    fprintf(c_out, "    {15, 15, 15,  0,  0,  0,  0,  0},\n");
    fprintf(c_out, "    {15,  0,  0, 15,  0,  0,  0,  0}\n");
    fprintf(c_out, "};\n\n");

    // Funções de Desenho
    fprintf(c_out, "void draw_real_char(char c, int x, int y, uint32_t color) {\n");
    fprintf(c_out, "    if (c < 32 || c > 126) return;\n");
    fprintf(c_out, "    int idx = c - 32;\n");
    fprintf(c_out, "    uint32_t c_hex = get_color(color);\n");
    fprintf(c_out, "    for (int i = 0; i < 8; i++) {\n");
    fprintf(c_out, "        for (int j = 0; j < 8; j++) {\n");
    fprintf(c_out, "            if (font8x8[idx][i] & (1 << (7 - j))) {\n");
    fprintf(c_out, "                if ((y + i) < 768 && (x + j) < 1024) {\n");
    fprintf(c_out, "                    double_buffer[(y + i) * 1024 + (x + j)] = c_hex;\n");
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void draw_real_string(const char* s, int x, int y, uint32_t color) {\n");
    fprintf(c_out, "    while (*s) {\n");
    fprintf(c_out, "        draw_real_char(*s, x, y, color);\n");
    fprintf(c_out, "        x += 8;\n");
    fprintf(c_out, "        s++;\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void draw_real_rectangle(int w, int h, uint32_t color, int x, int y, int id) {\n");
    fprintf(c_out, "    if (obj_count < 50 && id > 0) {\n");
    fprintf(c_out, "        objects[obj_count].id = id;\n");
    fprintf(c_out, "        objects[obj_count].x = x;\n");
    fprintf(c_out, "        objects[obj_count].y = y;\n");
    fprintf(c_out, "        objects[obj_count].w = w;\n");
    fprintf(c_out, "        objects[obj_count].h = h;\n");
    fprintf(c_out, "        obj_count++;\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "    uint32_t c_hex = get_color(color);\n");
    fprintf(c_out, "    for (int i = 0; i < h; i++) {\n");
    fprintf(c_out, "        for (int j = 0; j < w; j++) {\n");
    fprintf(c_out, "            if ((y + i) < 768 && (x + j) < 1024) {\n");
    fprintf(c_out, "                double_buffer[(y + i) * 1024 + (x + j)] = c_hex;\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void draw_image_array(const uint32_t* img_data, int w, int h, int start_x, int start_y) {\n");
    fprintf(c_out, "    for (int y = 0; y < h; y++) {\n");
    fprintf(c_out, "        for (int x = 0; x < w; x++) {\n");
    fprintf(c_out, "            uint32_t pixel = img_data[y * w + x];\n");
    fprintf(c_out, "            if ((pixel >> 24) > 10) {\n");
    fprintf(c_out, "                if ((start_y + y) < 768 && (start_x + x) < 1024) {\n");
    fprintf(c_out, "                    double_buffer[(start_y + y) * 1024 + (start_x + x)] = pixel;\n");
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");

    // Drivers IO
    fprintf(c_out, "void mouse_wait(uint8_t a_type) {\n");
    fprintf(c_out, "    uint32_t timeout = 100000;\n");
    fprintf(c_out, "    if (a_type == 0) {\n");
    fprintf(c_out, "        while (timeout--) {\n");
    fprintf(c_out, "            if ((inb(0x64) & 1) == 1) return;\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    } else {\n");
    fprintf(c_out, "        while (timeout--) {\n");
    fprintf(c_out, "            if ((inb(0x64) & 2) == 0) return;\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void mouse_write(uint8_t a_write) {\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x64, 0xD4);\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x60, a_write);\n");
    fprintf(c_out, "}\n\n");
    
    fprintf(c_out, "void init_hardware_mouse() {\n");
    fprintf(c_out, "    uint8_t s;\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x64, 0xA8);\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x64, 0x20);\n");
    fprintf(c_out, "    mouse_wait(0);\n");
    fprintf(c_out, "    s = (inb(0x60) | 2);\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x64, 0x60);\n");
    fprintf(c_out, "    mouse_wait(1);\n");
    fprintf(c_out, "    outb(0x60, s);\n");
    fprintf(c_out, "    mouse_write(0xF4);\n");
    fprintf(c_out, "    inb(0x60);\n");
    fprintf(c_out, "}\n\n");

    fprintf(c_out, "char kbd_map[128] = { 0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\\b','\\t','q','w','e','r','t','y','u','i','o','p','[',']','\\n',0,'a','s','d','f','g','h','j','k','l',';','\\'','`',0,'\\\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ' };\n");
    fprintf(c_out, "void handle_keyboard() {\n");
    fprintf(c_out, "    uint8_t s = inb(0x64);\n");
    fprintf(c_out, "    if (s & 1 && !(s & 0x20)) {\n");
    fprintf(c_out, "        uint8_t scan = inb(0x60);\n");
    fprintf(c_out, "        if (!(scan & 0x80)) {\n");
    fprintf(c_out, "            char key = kbd_map[scan];\n");
    fprintf(c_out, "            if (key == '\\n') {\n");
    fprintf(c_out, "                input_buffer[input_len] = 0;\n");
    fprintf(c_out, "                if (k_strcmp(input_buffer, \"cli\") == 0) terminal_active = 1;\n");
    fprintf(c_out, "                else if (k_strcmp(input_buffer, \"gui\") == 0) terminal_active = 0;\n");
    fprintf(c_out, "                input_len = 0;\n");
    fprintf(c_out, "            } else if (key > 0 && input_len < 30) {\n");
    fprintf(c_out, "                input_buffer[input_len++] = key;\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "        }\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");
    
    // Atualização do Mouse com DEBOUNCE Absoluto
    fprintf(c_out, "void update_hardware_mouse() {\n");
    fprintf(c_out, "    uint8_t s = inb(0x64);\n");
    fprintf(c_out, "    if (!(s & 1) || !(s & 0x20)) return;\n");
    fprintf(c_out, "    uint8_t val = inb(0x60);\n");
    fprintf(c_out, "    switch (mouse_cycle) {\n");
    fprintf(c_out, "        case 0:\n");
    fprintf(c_out, "            if (val & 0x08) {\n");
    fprintf(c_out, "                mouse_byte[0] = val;\n");
    fprintf(c_out, "                mouse_cycle++;\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "            break;\n");
    fprintf(c_out, "        case 1:\n");
    fprintf(c_out, "            mouse_byte[1] = val;\n");
    fprintf(c_out, "            mouse_cycle++;\n");
    fprintf(c_out, "            break;\n");
    fprintf(c_out, "        case 2:\n");
    fprintf(c_out, "            mouse_byte[2] = val;\n");
    fprintf(c_out, "            mouse_cycle = 0;\n");
    fprintf(c_out, "            mouse_x += (int8_t)mouse_byte[1];\n");
    fprintf(c_out, "            mouse_y -= (int8_t)mouse_byte[2];\n");
    fprintf(c_out, "            if (mouse_x < 0) mouse_x = 0;\n");
    fprintf(c_out, "            if (mouse_x > 1016) mouse_x = 1016;\n");
    fprintf(c_out, "            if (mouse_y < 0) mouse_y = 0;\n");
    fprintf(c_out, "            if (mouse_y > 760) mouse_y = 760;\n");
    
    // Lógica do Debounce: Somente dispara se o botão mudou de "solto" para "pressionado"
    fprintf(c_out, "            if (mouse_byte[0] & 1) {\n");
    fprintf(c_out, "                if (mouse_was_clicked == 0) {\n");
    fprintf(c_out, "                    for (int i = 0; i < obj_count; i++) {\n");
    fprintf(c_out, "                        if (mouse_x >= objects[i].x && mouse_x <= (objects[i].x + objects[i].w) && mouse_y >= objects[i].y && mouse_y <= (objects[i].y + objects[i].h)) {\n");
    fprintf(c_out, "                            clicked_object_id = objects[i].id;\n");
    fprintf(c_out, "                        }\n");
    fprintf(c_out, "                    }\n");
    fprintf(c_out, "                    mouse_was_clicked = 1;\n"); // Trava o gatilho
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            } else {\n");
    fprintf(c_out, "                mouse_was_clicked = 0;\n"); // Libera o gatilho
    fprintf(c_out, "            }\n");
    fprintf(c_out, "            break;\n");
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n\n");

    // PASS 1: Captura VFS
    fseek(source_fp, 0, SEEK_SET);
    while (fgets(line, sizeof(line), source_fp)) {
        if (strstr(line, "bootSet.ExecutableApp") != NULL) { 
            char* p = strchr(line, '='); 
            if (p) sscanf(p+1, " %s", app_ext); 
        }
        else if (strstr(line, "import.MyAppExecutable") != NULL) {
            char full_name[64]; 
            sscanf(line, "import.MyAppExecutable %s", full_name);
            char* dot = strchr(full_name, '.'); 
            if (dot) *dot = '\0';
            strcpy(imported_apps[imported_count++], full_name);
        }
    }

    // Ponto de Entrada Mestre (MBD)
    fprintf(c_out, "void whale_kernel_main(uint32_t magic, uint32_t mbd) {\n");
    fprintf(c_out, "    if (magic != 0x2BADB002) return;\n");
    fprintf(c_out, "    uint32_t fb_addr = *(((uint32_t*)(mbd + 88)));\n");
    fprintf(c_out, "    vga = (volatile uint32_t*)fb_addr;\n\n");

    fprintf(c_out, "    char app_registry[%d][32];\n", imported_count == 0 ? 1 : imported_count);
    for(int i = 0; i < imported_count; i++) {
        fprintf(c_out, "    k_strcpy(app_registry[%d], \"%s\");\n", i, imported_apps[i]);
    }
    
    // ==========================================
    // LOOP PRINCIPAL COM ORDEM CORRETA DE HITBOX
    // ==========================================
    fprintf(c_out, "    while (1) {\n");
    
    fprintf(c_out, "        handle_keyboard();\n");
    fprintf(c_out, "        update_hardware_mouse();\n");
    
    // LIMPA HITBOXES DEPOIS DO MOUSE LER O FRAME ANTERIOR! (Fim da cidade fantasma)
    fprintf(c_out, "        obj_count = 0;\n\n"); 
    
    fprintf(c_out, "        if (!terminal_active) {\n");
    fprintf(c_out, "            for (int i = 0; i < 786432; i++) double_buffer[i] = 0xFF222222;\n");
    fprintf(c_out, "        } else {\n");
    fprintf(c_out, "            for (int i = 0; i < 786432; i++) double_buffer[i] = 0xFF000000;\n");
    fprintf(c_out, "        }\n\n");
    
    // PASS 2: Parser das Instruções
    fseek(source_fp, 0, SEEK_SET);
    int current_img = 0;
    while (fgets(line, sizeof(line), source_fp)) {
        char *trimmed = line; 
        while (isspace((unsigned char)*trimmed)) trimmed++;
        if (*trimmed == '\0' || strncmp(trimmed, "//", 2) == 0) continue;
        
        char args[10][64]; 
        int arg_cnt = 0; 
        extract_args(trimmed, args, &arg_cnt);
        
        if (strstr(trimmed, "EasyOS.System.Graphics.Mouse.start") != NULL) {
            fprintf(c_out, "        if (mouse_x == 512 && mouse_y == 384) {\n");
            fprintf(c_out, "            init_hardware_mouse();\n");
            fprintf(c_out, "        }\n");
        } 
        else if (strstr(trimmed, "Draw.Image") != NULL && arg_cnt >= 5) {
            fprintf(c_out, "        if (!terminal_active) {\n");
            fprintf(c_out, "            draw_image_array(img_%d, %s, %s, %s, %s);\n", current_img, args[2], args[1], args[3], args[4]);
            fprintf(c_out, "        }\n");
            current_img++;
        }
        else if (strstr(trimmed, "Draw.Rectangle") != NULL && arg_cnt >= 6) {
            fprintf(c_out, "        if (!terminal_active) {\n");
            fprintf(c_out, "            draw_real_rectangle(%s, %s, %s, %s, %s, %s);\n", args[0], args[1], args[2], args[3], args[4], args[5]);
            fprintf(c_out, "        }\n");
        } 
        else if (strstr(trimmed, "Draw.Label") != NULL && arg_cnt >= 5) {
            fprintf(c_out, "        if (!terminal_active) {\n");
            fprintf(c_out, "            draw_real_string(\"%s\", %s, %s, %s);\n", args[0], args[2], args[3], args[1]);
            fprintf(c_out, "        }\n");
        }
        else if (strncmp(trimmed, "if", 2) == 0) {
            char match_id[16]; 
            const char *start = strchr(trimmed, '(');
            if(start) {
                start++; 
                int k=0; 
                while(start[k] && start[k] != ')') { 
                    if(isdigit(start[k])) { 
                        match_id[k] = start[k]; k++; 
                    } else { 
                        start++; 
                    } 
                } 
                match_id[k] = '\0';
                
                fprintf(c_out, "        if (clicked_object_id == %s) {\n", match_id);
                fprintf(c_out, "            menu_open = !menu_open;\n");
                fprintf(c_out, "            clicked_object_id = -1;\n");
                fprintf(c_out, "        }\n");
            }
        }
        else if (strstr(trimmed, "Draw.Start") != NULL && arg_cnt >= 6) {
            fprintf(c_out, "        if (menu_open && !terminal_active) {\n");
            fprintf(c_out, "            draw_real_rectangle(%s, %s, %s, %s, %s, -1);\n", args[2], args[1], args[3], args[4], args[5]);
            fprintf(c_out, "            draw_real_rectangle(%s, 20, 1, %s, %s, -1);\n", args[2], args[4], args[5]);
            fprintf(c_out, "            draw_real_string(\"%s\", %s + 10, %s + 6, 15);\n", args[0], args[4], args[5]);
            fprintf(c_out, "            for (int i = 0; i < %d; i++) {\n", imported_count);
            fprintf(c_out, "                draw_real_string(app_registry[i], %s + 20, %s + 35 + (i * 25), 15);\n", args[4], args[5]);
            fprintf(c_out, "                draw_real_rectangle(%s - 40, 20, 8, %s + 10, %s + 30 + (i * 25), 300 + i);\n", args[2], args[4], args[5]);
            fprintf(c_out, "            }\n");
            fprintf(c_out, "        }\n");
        }
    }
    
    // Gerenciador de Janelas (WM)
    fprintf(c_out, "        if (!terminal_active) {\n");
    fprintf(c_out, "            for (int i = 0; i < %d; i++) {\n", imported_count);
    fprintf(c_out, "                if (app_open[i]) {\n");
    fprintf(c_out, "                    int wx = 300 + (i * 40);\n");
    fprintf(c_out, "                    int wy = 200 + (i * 40);\n");
    fprintf(c_out, "                    draw_real_rectangle(400, 300, 7, wx, wy, -1);\n");
    fprintf(c_out, "                    draw_real_rectangle(400, 25, 1, wx, wy, -1);\n");
    fprintf(c_out, "                    draw_real_string(app_registry[i], wx + 10, wy + 8, 15);\n");
    fprintf(c_out, "                    draw_real_rectangle(20, 15, 4, wx + 375, wy + 5, 400 + i);\n");
    fprintf(c_out, "                    draw_real_string(\"X\", wx + 381, wy + 8, 15);\n");
    fprintf(c_out, "                    draw_real_rectangle(380, 260, 15, wx + 10, wy + 30, -1);\n");
    fprintf(c_out, "                    draw_real_string(\"APP ABERTO\", wx + 150, wy + 150, 0);\n");
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            }\n\n");
    
    // Interceptação de CLIQUES nas Janelas
    fprintf(c_out, "            if (clicked_object_id >= 300 && clicked_object_id < 400) {\n");
    fprintf(c_out, "                app_open[clicked_object_id - 300] = 1;\n");
    fprintf(c_out, "                menu_open = 0;\n");
    fprintf(c_out, "                clicked_object_id = -1;\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "            if (clicked_object_id >= 400 && clicked_object_id < 500) {\n");
    fprintf(c_out, "                app_open[clicked_object_id - 400] = 0;\n");
    fprintf(c_out, "                clicked_object_id = -1;\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "            clicked_object_id = -1;\n\n"); // Consome cliques nulos
    
    // Renderiza Mouse
    fprintf(c_out, "            for (int i = 0; i < 8; i++) {\n");
    fprintf(c_out, "                for (int j = 0; j < 8; j++) {\n");
    fprintf(c_out, "                    if (win_cursor[i][j]) {\n");
    fprintf(c_out, "                        if (mouse_y + i < 768 && mouse_x + j < 1024) {\n");
    fprintf(c_out, "                            double_buffer[(mouse_y + i) * 1024 + (mouse_x + j)] = 0xFFFFFFFF;\n");
    fprintf(c_out, "                        }\n");
    fprintf(c_out, "                    }\n");
    fprintf(c_out, "                }\n");
    fprintf(c_out, "            }\n\n");
    
    fprintf(c_out, "            k_memcpy((void*)vga, double_buffer, 1024 * 768 * 4);\n");
    
    fprintf(c_out, "        } else {\n");
    fprintf(c_out, "            draw_real_string(\"WHALEOS NATIVE CLI\", 10, 10, 10);\n");
    fprintf(c_out, "            draw_real_string(\"# \", 10, 30, 15);\n");
    fprintf(c_out, "            for (int n = 0; n < input_len; n++) {\n");
    fprintf(c_out, "                draw_real_char(input_buffer[n], 26 + (n * 8), 30, 15);\n");
    fprintf(c_out, "            }\n");
    fprintf(c_out, "            k_memcpy((void*)vga, double_buffer, 1024 * 768 * 4);\n");
    fprintf(c_out, "        }\n");
    
    fprintf(c_out, "    }\n");
    fprintf(c_out, "}\n");
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