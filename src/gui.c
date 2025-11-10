#include "../include/gui.h"
#include "../include/tty.h"
#include "../include/kbd.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/memory.h"

/* Simple text-mode window manager implementation */
struct gui_window {
    int id;
    char title[GUI_TITLE_MAX];
    int width;
    int height;
    char **lines; /* array of pointers to lines (height) */
    int visible;
    int focused;
};

static struct gui_window windows[GUI_MAX_WINDOWS];
static int window_count = 0;
static int focused_win = -1;

int gui_init(void)
{
    /* nothing to initialize for text-mode GUI */
    for (int i = 0; i < GUI_MAX_WINDOWS; ++i) windows[i].id = -1;
    window_count = 0;
    focused_win = -1;
    return 0;
}

void gui_draw_box(const char *title, const char *text)
{
    printk("+------------------------------+\n");
    if (title) printk("| %s\n", title);
    printk("+------------------------------+\n");
    if (text) printk("%s\n", text);
    printk("+------------------------------+\n");
}

/* prompt for input using keyboard scan; hide=1 to not echo characters (password) */
int gui_prompt(const char *prompt, char *out, size_t out_sz, int hide)
{
    if (!out || out_sz == 0) return -1;
    out[0] = '\0';
    size_t pos = 0;
    int caps = 0, shift = 0;
    if (prompt) printk("%s", prompt);

    while (1) {
        uint8_t b;
        while ((b = scan()) == 0) ;
        /* handle toggles */
        if (togglecode[b] == CAPSLOCK) { caps = !caps; continue; }
        if (b == 0x2A || b == 0x36) { shift = 1; continue; }
        char ch;
        if (caps) ch = capslockmap[b];
        else if (shift) { ch = shiftmap[b]; shift = 0; }
        else ch = normalmap[b];
        if ((unsigned char)ch >= 0xE0) continue; /* ignore special keys */
        if (ch == '\n' || ch == '\r') {
            out[pos] = '\0';
            printk("\n");
            return 0;
        }
        if (ch == '\b') {
            if (pos > 0) {
                pos--;
                if (!hide) printk("\b \b");
            }
            continue;
        }
        if (pos + 1 < out_sz) {
            out[pos++] = ch;
            if (!hide) {
                char s[2] = {ch, '\0'};
                printk("%s", s);
            } else {
                printk("*");
            }
        }
    }
    return -1;
}

/* Create a window: returns id or -1 on error */
gui_id_t gui_create_window(const char *title, int width, int height)
{
    if (window_count >= GUI_MAX_WINDOWS) return -1;
    int idx = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; ++i) if (windows[i].id == -1) { idx = i; break; }
    if (idx == -1) return -1;
    struct gui_window *w = &windows[idx];
    w->id = idx;
    strncpy(w->title, title ? title : "", GUI_TITLE_MAX-1);
    w->title[GUI_TITLE_MAX-1] = '\0';
    w->width = width;
    w->height = height;
    w->lines = (char **)kmalloc(sizeof(char*) * height);
    if (!w->lines) { w->id = -1; return -1; }
    for (int i = 0; i < height; ++i) {
        w->lines[i] = (char *)kmalloc(width + 1);
        if (!w->lines[i]) {
            for (int j = 0; j < i; ++j) kfree(w->lines[j]);
            kfree(w->lines);
            w->id = -1; return -1;
        }
        w->lines[i][0] = '\0';
    }
    w->visible = 1;
    w->focused = 0;
    window_count++;
    return w->id;
}

int gui_destroy_window(gui_id_t id)
{
    if (id < 0 || id >= GUI_MAX_WINDOWS) return -1;
    struct gui_window *w = &windows[id];
    if (w->id == -1) return -1;
    for (int i = 0; i < w->height; ++i) kfree(w->lines[i]);
    kfree(w->lines);
    w->id = -1;
    window_count--;
    if (focused_win == id) focused_win = -1;
    return 0;
}

int gui_set_focus(gui_id_t id)
{
    if (id < 0 || id >= GUI_MAX_WINDOWS) return -1;
    if (windows[id].id == -1) return -1;
    if (focused_win >= 0 && focused_win < GUI_MAX_WINDOWS) windows[focused_win].focused = 0;
    focused_win = id;
    windows[id].focused = 1;
    return 0;
}

gui_id_t gui_get_focused(void)
{
    return focused_win;
}

int gui_window_write(gui_id_t id, int line, const char *text)
{
    if (id < 0 || id >= GUI_MAX_WINDOWS) return -1;
    struct gui_window *w = &windows[id];
    if (w->id == -1) return -1;
    if (line < 0 || line >= w->height) return -1;
    strncpy(w->lines[line], text ? text : "", w->width);
    w->lines[line][w->width] = '\0';
    return 0;
}

int gui_draw_window(gui_id_t id)
{
    if (id < 0 || id >= GUI_MAX_WINDOWS) return -1;
    struct gui_window *w = &windows[id];
    if (w->id == -1 || !w->visible) return -1;
    /* draw a simple framed box */
    printk("+"); for (int i=0;i<w->width;i++) printk("-"); printk("+\n");
    printk("| %s\n", w->title);
    printk("+"); for (int i=0;i<w->width;i++) printk("-"); printk("+\n");
    for (int i = 0; i < w->height; ++i) {
        printk("%s\n", w->lines[i]);
    }
    printk("+"); for (int i=0;i<w->width;i++) printk("-"); printk("+\n");
    return 0;
}
