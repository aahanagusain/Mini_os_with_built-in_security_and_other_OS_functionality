/* Minimal text-mode GUI / window manager API (Phase 2)
 * - Provides simple window creation, drawing, focus management
 * - Windows are rendered as framed text blocks on the console
 */
#ifndef GUI_H
#define GUI_H

#include "stddef.h"

#define GUI_TITLE_MAX 32
#define GUI_MAX_WINDOWS 8

typedef int gui_id_t;

/* Initialize GUI subsystem. */
int gui_init(void);

/* Basic legacy helpers (existing) */
void gui_draw_box(const char *title, const char *text);
int gui_prompt(const char *prompt, char *out, size_t out_sz, int hide);

/* Window manager API */
gui_id_t gui_create_window(const char *title, int width, int height);
int gui_destroy_window(gui_id_t id);
int gui_set_focus(gui_id_t id);
gui_id_t gui_get_focused(void);
int gui_window_write(gui_id_t id, int line, const char *text);
int gui_draw_window(gui_id_t id);

#endif /* GUI_H */
