#include "../include/version.h"
#include "../include/tty.h"
#include "../include/io.h"
#include "../include/kbd.h"
#include "../include/string.h"
#include "../include/time.h"
#include "../include/math_shell.h"
#include "../include/parsing.h"
#include "../include/bool.h"
#include "../include/sha224.h"
#include "../include/sha256.h"
#include "../include/utils.h"
#include "../include/sleep.h"
#include "../include/thread.h"
#include "../include/memory.h"
#include "../include/shell_history.h"
#include "../include/calculator.h"
#include "../include/fs.h"

#define DEBUG false

#define BUFFER_SIZE 1024

uint8_t numlock = true;
uint8_t capslock = false;
uint8_t scrolllock = false;
uint8_t shift = false;
char current_version[7];

/* Pager: wait for pager key. Return 1 to continue, 0 to quit */
/* forward declaration for blocking getchar used by pager */
static char getch_blocking(void);

static int pager_wait_key(void)
{
	char c = getch_blocking();
	if (c == 'q' || c == 'Q') return 0;
	if (c == ' ' || c == '\n' || c == '\r') return 1;
	return 1; /* any other key continues */
}

/* Blocking getchar: waits for a keypress, handles shift and capslock toggles,
   and returns the mapped character (or 0 for non-printable control keys). */
static char getch_blocking(void)
{
	uint8_t b = 0;
	while (1) {
		while ((b = scan()) == 0) ;

		/* handle toggles */
		if (togglecode[b] == CAPSLOCK) {
			capslock = !capslock;
			continue; /* no character to return */
		}
		/* shift press (make code) - set shift and wait for next key */
		if (b == 0x2A || b == 0x36) {
			shift = true;
			continue;
		}

		char ch;
		if (capslock) ch = capslockmap[b];
		else if (shift) { ch = shiftmap[b]; shift = false; }
		else ch = normalmap[b];

		/* If mapping yields special key constants (like KEY_UP), return 0 */
		if ((unsigned char)ch >= 0xE0) return 0;
		return ch;
	}
}

int main(void)
{
	char buffer[BUFFER_SIZE];
	char *string;
	char *buff;
	uint8_t byte;
	node_t *head = NULL;

	terminal_initialize(default_font_color, COLOR_BLACK);
	terminal_set_colors(COLOR_LIGHT_GREEN, COLOR_BLACK);
	sprintf(current_version, "%u.%u.%u", V1, V2, V3 + 1);
	print_logo();
	about(current_version);
	printk("\n\tType \"help\" for a list of commands.\n\n");
	// printf("\tCurrent datetime: ");
	// datetime();
	printk("\n\tWelcome!\n\n");

	terminal_set_colors(default_font_color, COLOR_BLACK);

	// initialize heap
	heap_init();

	/* Mount embedded initrd (ramfs) and print a test file if present */
	if (fs_mount_initrd_embedded() == FS_OK) {
		fs_fd_t fd = fs_open("/README.txt", FS_O_RDONLY);
		if (fd >= 0) {
			char fbuf[256];
			int r = fs_read(fd, fbuf, sizeof(fbuf) - 1);
			if (r > 0) {
				fbuf[r] = '\0';
				printk("\n--- initrd /README.txt ---\n%s\n---\n", fbuf);
			}
			fs_close(fd);
		} else {
			printk("\n(initrd) /README.txt not found\n");
		}
	}

#if DEBUG
	// memory test
	int *a = (int *)kmalloc(sizeof(int));
	void *b = kmalloc(5000);
	void *c = kmalloc(50000);
	*a = 1;
	printf("\na: %d", *a);
	printf("\na: %p", (void *)a);
	printf("\nb: %p", (void *)b);
	printf("\nc: %p", (void *)c);
	// int *b = (int *)kmalloc(0x1000);
	// int *c = (int *)kmalloc(sizeof(int));
	// printf("\nb: %x", b);
	// printf("\nc: %x", c);
	// kfree(b);
	// int *d = (int *)kmalloc(0x1000); // here should be adress of B
	// printf("\nd: %x", d);
	// kfree(d);
	// kfree(c);
	kfree(a);
	kfree(b);
	kfree(c);
#endif

	strcpy(&buffer[strlen(buffer)], "");
	print_prompt();
	while (true)
	{
		while (byte = scan())
		{
			if (byte == ENTER)
			{
				strcpy(buffer, tolower(buffer));
				insert_at_head(&head, create_new_node(buffer));
				 if (strlen(buffer) > 0 && strncmp(buffer, "ls", 2) == 0)
				{
					/* support: ls [path] -> list immediate children using fs_listdir */
					const struct fs_file *f;
					char *p = buffer + 2;
					while (*p == ' ') p++;
					const char *path = (*p == '\0') ? "/" : p;
					unsigned int idx = 0;
					int found = 0;
					while (fs_listdir(path, idx, &f) == FS_OK) {
						/* print the basename of the returned child */
						const char *name = f->name;
						const char *base = name;
						/* find last '/' */
					int i;
					for (i = strlen(name) - 1; i >= 0; --i) {
						if (name[i] == '/') { base = name + i + 1; break; }
					}
						if (fs_is_overlay(f->name))
							printk("\n\t%s\t%u bytes\t(overlay)", base, (unsigned)f->size);
						else
							printk("\n\t%s\t%u bytes", base, (unsigned)f->size);
						idx++;
						found = 1;
					}
					if (!found) printk("\n\t(empty)\n");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "hello") == 0)
				{
					printk("\nHi!");
				}
				else if (strlen(buffer) > 0 && strstr(buffer, "sha256(") != NULL)
				{
					char *parser;
					char string[64];
					parser = strstr(buffer, "sha256(");
					parser += strlen("sha256(");
					parse_string(string, parser, ')');
					sha256(string);
				}
				else if (strlen(buffer) > 0 && strstr(buffer, "sha224(") != NULL)
				{
					char *parser;
					char string[64];
					parser = strstr(buffer, "sha224(");
					parser += strlen("sha224(");
					parse_string(string, parser, ')');
					sha224(string);
				}
				else if (math_func(buffer))
				{
					math_shell(buffer);
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "math") == 0)
				{
					printk("\n\n\tMathematical functions:\n");
					printk("\n\t rand()             - \tpseudo random number generator");
					printk("\n\t srand()            - \tpseudo random number generator seed");
					printk("\n\t fact(x)            - \treturns factorial of x");
					printk("\n\t abs(x)             - \treturns absolute value of x");
					printk("\n\t sqrt(x)            - \treturns square root of x");
					printk("\n\t pow(x,y)           - \treturns the y power of x");
					printk("\n\t exp(x)             - \treturns the natural exponential of x");
					printk("\n\t ln(x)              - \treturns the natural logarithm of x");
					printk("\n\t log10(x)           - \treturns the logarithm of x base 10");
					printk("\n\t log(x,y)           - \treturns the logarithm of x base y");
					printk("\n\t sin(x)             - \treturns sine of x");
					printk("\n\t cos(x)             - \treturns cosine of x");
					printk("\n\t tan(x)             - \treturns tangent of x");
					printk("\n\t asin(x)            - \treturns arcsine of x");
					printk("\n\t acos(x)            - \treturns arccosine of x");
					printk("\n\t atan(x)            - \treturns arctangent of x");
					printk("\n\t sinh(x)            - \treturns hyperbolic sine of x");
					printk("\n\t cosh(x)            - \treturns hyperbolic cosine of x");
					printk("\n\t tanh(x)            - \treturns hyperbolic tangent of x");
					printk("\n\t asinh(x)           - \treturns inverse hyperbolic sine of x");
					printk("\n\t acosh(x)           - \treturns inverse hyperbolic cosine of x");
					printk("\n\t atanh(x)           - \treturns inverse hyperbolic tangent of x");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "crypto") == 0)
				{
					printk("\n\nCryptography utilities:\n");
					printk("\n\t sha224(string)     - \tSHA-224 hashing");
					printk("\n\t sha256(string)     - \tSHA-256 hashing");
					printk("\n");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "help") == 0)
				{
					printk("\n\n\tBasic kernel commands:\n");
					printk("\n\t about              - \tabout PrimusOS");
					printk("\n\t math               - \tlists all mathematical functions");
					printk("\n\t crypto             - \tlists all cryptography utilities");
					printk("\n\t clear              - \tclears the screen");
					printk("\n\t fontcolor          - \tchange default font color");
					printk("\n\t datetime           - \tdisplays current date and time");
					printk("\n\t date               - \tdisplays current date");
					printk("\n\t clock              - \tdisplays clock");
					printk("\n\t history            - \tdisplays commands history");
					printk("\n\t reboot             - \treboots system");
					printk("\n\t shutdown           - \tsends shutdown signal");
					printk("\n");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "about") == 0)
				{
					about(current_version);
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "touch ", 6) == 0)
				{
					const char *path = buffer + 6;
					while (*path == ' ') path++;
					if (*path == '\0') {
						printk("\nUsage: touch <path>\n");
					} else {
						int r = fs_create(path, NULL, 0);
						if (r == FS_OK) printk("\n(touch) created %s\n", path);
						else printk("\n(touch) failed: %d\n", r);
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "mkdir ", 6) == 0)
				{
					const char *path = buffer + 6;
					while (*path == ' ') path++;
					if (*path == '\0') {
						printk("\nUsage: mkdir <path>\n");
					} else {
						int r = fs_mkdir(path);
						if (r == FS_OK) printk("\n(mkdir) created %s\n", path);
						else printk("\n(mkdir) failed: %d\n", r);
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "rm ", 3) == 0)
				{
					const char *path = buffer + 3;
					while (*path == ' ') path++;
					if (*path == '\0') {
						printk("\nUsage: rm <path>\n");
					} else {
						int r = fs_unlink(path);
						if (r == FS_OK) printk("\n(rm) removed %s\n", path);
						else printk("\n(rm) failed: %d\n", r);
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "write ", 6) == 0)
				{
					char *p = buffer + 6;
					while (*p == ' ') p++;
					if (*p == '\0') {
						printk("\nUsage: write <path> <text>\n");
					} else {
						char *q = strchr(p, ' ');
						if (!q) {
							printk("\nUsage: write <path> <text>\n");
						} else {
							*q = '\0';
							char *text = q + 1;
							while (*text == ' ') text++;
							size_t tlen = strlen(text);
							/* Try to open the file */
							fs_fd_t fd = fs_open(p, FS_O_RDONLY);
							if (fd < 0) {
								/* not present: create empty overlay file first */
								int c = fs_create(p, (const uint8_t *)"", 0);
								if (c != FS_OK) { printk("\n(write) create failed: %d\n", c); continue; }
								fd = fs_open(p, FS_O_RDONLY);
								if (fd < 0) { printk("\n(write) open failed after create: %d\n", fd); continue; }
							}
							/* Attempt to write (fs_write will fail with FS_EIO if the fd refers to a read-only packaged file) */
							int w = fs_write(fd, (const void *)text, tlen);
							if (w == FS_EIO) {
								/* packaged file: copy contents into overlay then retry */
								fs_close(fd);
								struct fs_stat st;
								if (fs_stat(p, &st) == FS_OK && st.size > 0) {
									char *buf = kmalloc(st.size);
									if (buf) {
										fs_fd_t rfd = fs_open(p, FS_O_RDONLY);
										if (rfd >= 0) {
											int got = fs_read(rfd, buf, st.size);
											fs_close(rfd);
											/* create overlay copy */
											fs_create(p, (const uint8_t *)buf, (size_t)got);
											fd = fs_open(p, FS_O_RDONLY);
											if (fd >= 0) {
												w = fs_write(fd, (const void *)text, tlen);
											}
										}
										kfree(buf);
									}
								}
							}
							if (w >= 0) printk("\n(write) wrote %d bytes to %s\n", w, p);
							else printk("\n(write) failed: %d\n", w);
							if (fd >= 0) fs_close(fd);
						}
					}
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "fontcolor") == 0)
				{
					default_font_color = change_font_color();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "clear") == 0)
				{
					terminal_initialize(default_font_color, COLOR_BLACK);
					strcpy(&buffer[strlen(buffer)], "");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "datetime") == 0)
				{
					printk("\nCurrent datetime: ");
					datetime();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "date") == 0)
				{
					printk("\nCurrent date: ");
					date();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "clock") == 0)
				{
					printk("\nCurrent clock: ");
					clock();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "reboot") == 0)
				{
					reboot();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "shutdown") == 0)
				{
					shutdown();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "history") == 0)
				{
					print_history(head);
				}
				else if (strlen(buffer) > 0 && (strstr(buffer, "+") != NULL || strstr(buffer, "-") != NULL || strstr(buffer, "*") != NULL|| strstr(buffer, "/") != NULL ))
				{
					compute(buffer);
				}
				else if (strlen(buffer) == 0)
				{
				}
				else
				{
					printk("\n'%s' is not a recognized command. ", buffer);
				}
				print_prompt();
				memset(buffer, 0, BUFFER_SIZE);
				strcpy(&buffer[strlen(buffer)], "");
				break;
			}
			else if (strlen(buffer) > 0 && strncmp(buffer, "mv ", 3) == 0)
			{
				char *p = buffer + 3;
				while (*p == ' ') p++;
				if (*p == '\0') {
					printk("\nUsage: mv <oldpath> <newpath>\n");
				} else {
					char *q = strchr(p, ' ');
					if (!q) {
						printk("\nUsage: mv <oldpath> <newpath>\n");
					} else {
						*q = '\0';
						char *old = p;
						char *new = q + 1;
						while (*new == ' ') new++;
						if (*new == '\0') {
							printk("\nUsage: mv <oldpath> <newpath>\n");
						} else {
							int r = fs_rename(old, new);
							if (r == FS_OK) printk("\n(mv) renamed %s -> %s\n", old, new);
							else printk("\n(mv) failed: %d\n", r);
						}
					}
				}
			}
			else if (strlen(buffer) > 0 && strncmp(buffer, "truncate ", 9) == 0)
			{
				char *p = buffer + 9;
				while (*p == ' ') p++;
				if (*p == '\0') {
					printk("\nUsage: truncate <path> <size>\n");
				} else {
					char *q = strchr(p, ' ');
					if (!q) {
						printk("\nUsage: truncate <path> <size>\n");
					} else {
						*q = '\0';
						char *path = p;
						char *num = q + 1;
						while (*num == ' ') num++;
						if (*num == '\0') { printk("\nUsage: truncate <path> <size>\n"); }
						else {
							int val = 0; int neg = 0;
							if (*num == '-') { neg = 1; num++; }
							while (*num >= '0' && *num <= '9') { val = val * 10 + (*num - '0'); num++; }
							if (neg) val = -val;
							int r = fs_truncate(path, (size_t)val);
							if (r == FS_OK) printk("\n(truncate) %s => %d\n", path, val);
							else printk("\n(truncate) failed: %d\n", r);
						}
					}
				}
			}
			else if (strlen(buffer) > 0 && strncmp(buffer, "rmdir ", 6) == 0)
			{
				const char *path = buffer + 6;
				while (*path == ' ') path++;
				if (*path == '\0') {
					printk("\nUsage: rmdir <path>\n");
				} else {
					int r = fs_rmdir(path);
					if (r == FS_OK) printk("\n(rmdir) removed %s\n", path);
					else printk("\n(rmdir) failed: %d\n", r);
				}
			}
				else if (strlen(buffer) > 0 && strncmp(buffer, "cat ", 4) == 0)
				{
					/* cat <path> - print file contents from embedded initrd */
					const char *path = buffer + 4;
					/* trim leading spaces */
					while (*path == ' ') path++;
					if (*path == '\0') {
						printk("\nUsage: cat <path>\n");
					} else {
						fs_fd_t fd = fs_open(path, FS_O_RDONLY);
						if (fd < 0) {
							printk("\n(cat) %s: not found\n", path);
						} else {
							char fbuf[256];
							int r;
							int line_count = 0;
							while ((r = fs_read(fd, fbuf, sizeof(fbuf)-1)) > 0) {
								fbuf[r] = '\0';
								/* print and count newlines for pagination */
								for (int i = 0; i < r; ++i) {
									char ch = fbuf[i];
									char s[2] = {ch, '\0'};
									printk("%s", s);
									if (ch == '\n') {
										line_count++;
										if (line_count >= 20) {
											printk("--More-- (space to continue, q to quit)");
											if (!pager_wait_key()) { r = -1; break; }
											line_count = 0;
											printk("\n");
										}
									}
								}
							}
							if (r < 0) {
								printk("\n(cat) read error or cancelled\n");
							}
							fs_close(fd);
							printk("\n");
						}
					}
				}
			else if ((byte == BACKSPACE) && (strlen(buffer) == 0))
			{
			}
			else if (byte == BACKSPACE)
			{
				char c = normalmap[byte];
				char *s;
				s = ctos(s, c);
				printk("%s", s);
				buffer[strlen(buffer) - 1] = '\0';
			}
			else
			{
				char c1 = togglecode[byte];
				char c2 = shiftcode[byte];
				char c;
				if (c1 == CAPSLOCK)
				{
					if (!capslock)
					{
						capslock = true;
					}
					else
					{
						capslock = false;
					}
				}
				if (capslock)
				{
					c = capslockmap[byte];
				}
				else if (shift)
				{
					c = shiftmap[byte];
					shift = false;
				}
				else
				{
					c = normalmap[byte];
				}
				char *s;
				s = ctos(s, c);
				printk("%s", s);
				strcpy(&buffer[strlen(buffer)], s);
				if (byte == 0x2A || byte == 0x36)
				{
					shift = true;
				}
			}
			move_cursor(get_terminal_row(), get_terminal_col());
		}
	}
	return 0;
}
