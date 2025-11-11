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
#include "../include/user.h"
#include "../include/gui.h"
#include "../include/netsec.h"

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
	uint8_t byte = 0;
	node_t *head = NULL;
	memset(buffer, 0, BUFFER_SIZE);

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
	printk("\nMounting embedded initrd...");
	int mres = fs_mount_initrd_embedded();
	if (mres != FS_OK) {
		printk("failed: %d\n", mres);
	} else {
		printk("ok\n");
		/* Try to list /etc to see what's there */
		printk("\nListing /etc:\n");
		const struct fs_file *f;
		unsigned int idx = 0;
		while (fs_listdir("/etc", idx, &f) == FS_OK) {
			printk("\t%s (%u bytes)\n", f->name, (unsigned)f->size);
			idx++;
		}
		if (idx == 0) printk("\t(empty)\n");
	}

	/* Initialize user database from initrd and show GUI login if available */
	printk("\nInitializing user database...");
	int ur = user_init_from_file("/etc/passwd");
	if (ur != 0) printk("failed: %d\n", ur);
	else printk("ok\n");

	/* Initialize network security */
	printk("Initializing network security...");
	int nr = netsec_init();
	if (nr != 0) printk("failed: %d\n", nr);
	else printk("ok\n");
	gui_init();

	/* Attempt a GUI-based login (max 3 tries). If no users are present, skip. */
	{
		char uname[USER_NAME_MAX];
		char passwd[USER_PASS_MAX];
		int tries = 0;
		int logged = 0;
		while (tries < 3) {
			uname[0] = '\0'; passwd[0] = '\0';
			if (gui_prompt("login: ", uname, sizeof(uname), 0) != 0) break;
			if (gui_prompt("password: ", passwd, sizeof(passwd), 1) != 0) break;
			if (user_login(uname, passwd) == 0) {
				const user_t *cur = user_current();
				if (cur) printk("\nWelcome, %s!\n", cur->name);
				logged = 1; break;
			} else {
				printk("\nLogin failed\n");
			}
			tries++;
		}
		if (!logged) printk("\nProceeding as guest.\n");
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
		while ((byte = scan()) != 0)
		{
			if (byte == ENTER)
			{
				char cmd_copy[BUFFER_SIZE];
				strncpy(cmd_copy, buffer, BUFFER_SIZE-1);
				cmd_copy[BUFFER_SIZE-1] = '\0';
				for (int i = 0; cmd_copy[i]; i++) {
					if (cmd_copy[i] >= 'A' && cmd_copy[i] <= 'Z') {
						cmd_copy[i] = cmd_copy[i] - 'A' + 'a';
					}
				}
				insert_at_head(&head, create_new_node(buffer));
				 if (strlen(buffer) > 0 && strncmp(cmd_copy, "ls", 2) == 0)
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
						/* print permissions, owner, size */
						unsigned int mode = f->mode;
						char perms[11];
						perms[10] = '\0';
						perms[0] = (f->data == NULL) ? 'd' : '-';
						for (int b = 0; b < 9; ++b) {
							int shift = 8 - b;
							unsigned int bit = (mode >> shift) & 1;
							int pos = 1 + b;
							if (b % 3 == 0) perms[pos] = bit ? 'r' : '-';
							else if (b % 3 == 1) perms[pos] = bit ? 'w' : '-';
							else perms[pos] = bit ? 'x' : '-';
						}
						if (fs_is_overlay(f->name))
							printk("\n\t%s %s %u:%u %u bytes (overlay)", perms, base, f->uid, f->gid, (unsigned)f->size);
						else
							printk("\n\t%s %s %u:%u %u bytes", perms, base, f->uid, f->gid, (unsigned)f->size);
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
					printk("\n\n\tUser Management:\n");
					printk("\n\t whoami             - \tshow current user");
					printk("\n\t users              - \tlist all users");
					printk("\n\t adduser <name>     - \tcreate new user");
					printk("\n\t deluser <name>     - \tdelete user (root only)");
					printk("\n\t su <name>          - \tswitch to another user");
					printk("\n\t sudo <command>     - \texecute command as root (root only)");
					printk("\n\t logout             - \tlogout current user");
					printk("\n\n\tFirewall:\n");
					printk("\n\t fw list            - \tlist firewall rules");
					printk("\n\t fw allow port N    - \tallow traffic on port N");
					printk("\n\t fw allow ip A.B.C.D- \tallow traffic from IP");
					printk("\n\t fw deny port N     - \tdeny traffic on port N"); 
					printk("\n\t fw deny ip A.B.C.D - \tdeny traffic from IP");
					printk("\n\n\tFile Management:\n");
					printk("\n\t pwd                - \tprint working directory");
					printk("\n\t cd <path>          - \tchange directory (limited)");
					printk("\n\t stat <path>        - \tfile statistics");
					printk("\n\t touch <path>       - \tcreate empty file");
					printk("\n\t mkdir <path>       - \tcreate directory");
					printk("\n\t echo <text> > <f>  - \twrite text to file");
					printk("\n\t edit <path>        - \tview file contents");
					printk("\n");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "about") == 0)
				{
					about(current_version);
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "pwd") == 0)
				{
					printk("\n/\n");
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "cd ", 3) == 0)
				{
					printk("\n(cd not implemented in single-level filesystem)\n");
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "stat ", 5) == 0)
				{
					char *p = buffer + 5;
					while (*p == ' ') p++;
					if (*p == '\0') {
						printk("\nUsage: stat <path>\n");
					} else {
						struct fs_stat st;
						if (fs_stat(p, &st) != FS_OK) {
							printk("\nFile not found: %s\n", p);
						} else {
							printk("\nFile: %s\n", p);
							printk("  Size: %u bytes\n", (unsigned)st.size);
							printk("  Owner: uid:%u gid:%u\n", st.uid, st.gid);
							printk("  Mode: %o\n", st.mode);
							printk("  Type: %s\n", (st.mode & 0x4000) ? "directory" : "file");
						}
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "touch ", 6) == 0)
				{
					const char *path = buffer + 6;
					while (*path == ' ') path++;
					if (*path == '\0') {
						printk("\nUsage: touch <path>\n");
					} else {
						int r = fs_create(path, NULL, 0);
						if (r == FS_OK) {
							const user_t *cur = user_current();
							unsigned int uid = cur ? cur->uid : 0;
							unsigned int gid = cur ? cur->gid : 0;
							fs_chown(path, uid, gid);
							printk("\nCreated: %s\n", path);
						} else {
							printk("\nFailed to create: %s (error: %d)\n", path, r);
						}
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
						if (r == FS_OK) {
							const user_t *cur = user_current();
							unsigned int uid = cur ? cur->uid : 0;
							unsigned int gid = cur ? cur->gid : 0;
							fs_chown(path, uid, gid);
							printk("\nDirectory created: %s\n", path);
						} else {
							printk("\nFailed to create directory: %s (error: %d)\n", path, r);
						}
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "echo ", 5) == 0)
				{
					const char *text = buffer + 5;
					while (*text == ' ') text++;
					
					/* Check if redirecting to file (> filename) */
					char *redir = strchr(text, '>');
					if (redir) {
						*redir = '\0';
						char *filename = redir + 1;
						while (*filename == ' ') filename++;
						
						if (*filename == '\0') {
							printk("\nUsage: echo <text> > <file>\n");
						} else {
							int c = fs_create(filename, (const uint8_t *)text, strlen(text));
							if (c == FS_OK) {
								printk("\nWritten to: %s\n", filename);
							} else {
								printk("\nFailed to write: %d\n", c);
							}
						}
					} else {
						/* Just print */
						printk("\n%s\n", text);
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
						if (r == FS_OK) printk("\nRemoved: %s\n", path);
						else printk("\nFailed to remove: %s (error: %d)\n", path, r);
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "edit ", 5) == 0)
				{
					char *p = buffer + 5;
					while (*p == ' ') p++;
					if (*p == '\0') {
						printk("\nUsage: edit <path>\n");
					} else {
						struct fs_stat st;
						if (fs_stat(p, &st) != FS_OK) {
							printk("\nFile not found: %s\n", p);
						} else {
							/* Read current content */
							char *buf = kmalloc(st.size + 1);
							if (!buf) {
								printk("\nOut of memory\n");
							} else {
								fs_fd_t fd = fs_open(p, FS_O_RDONLY);
								if (fd < 0) {
									printk("\nCannot open file: %s\n", p);
									kfree(buf);
								} else {
									int got = fs_read(fd, buf, st.size);
									fs_close(fd);
									printk("\n--- File: %s (size: %u) ---\n", p, (unsigned)st.size);
									buf[got] = '\0';
									printk("%s\n", buf);
									printk("--- (View only mode) ---\n");
									kfree(buf);
								}
							}
						}
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
							fs_fd_t fd = -1;

							/* Try to open the file */
							fd = fs_open(p, FS_O_RDONLY);
							if (fd < 0) {
								/* not present: create empty overlay file first */
								int c = fs_create(p, (const uint8_t *)"", 0);
								if (c != FS_OK) { 
									printk("\n(write) create failed: %d\n", c);
									continue; 
								}
								fd = fs_open(p, FS_O_RDONLY);
								if (fd < 0) { 
									printk("\n(write) open failed after create: %d\n", fd);
									continue; 
								}
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
				else if (strlen(buffer) > 0 && strncmp(buffer, "cp ", 3) == 0)
				{
					char *p = buffer + 3;
					while (*p == ' ') p++;
					char *q = strchr(p, ' ');
					if (!q) { printk("\nUsage: cp <src> <dst>\n"); }
					else {
						*q = '\0';
						char *src = p; char *dst = q + 1;
						while (*dst == ' ') dst++;
						struct fs_stat st;
						if (fs_stat(src, &st) != FS_OK) { printk("\n(cp) source not found\n"); }
						else {
							char *buf = kmalloc(st.size);
							if (!buf) { printk("\n(cp) oom\n"); }
							else {
								fs_fd_t r = fs_open(src, FS_O_RDONLY);
								if (r < 0) { printk("\n(cp) open read failed\n"); kfree(buf); }
								else {
									int got = fs_read(r, buf, st.size);
									fs_close(r);
									if (got < 0) { printk("\n(cp) read failed\n"); kfree(buf); }
									else {
										int c = fs_create(dst, (const uint8_t *)buf, (size_t)got);
										if (c == FS_OK) printk("\n(cp) %s -> %s\n", src, dst);
										else printk("\n(cp) create failed: %d\n", c);
										kfree(buf);
									}
								}
							}
						}
					}
				}

				else if (strlen(buffer) > 0 && strncmp(buffer, "chmod ", 6) == 0)
				{
					char *p = buffer + 6; while (*p == ' ') p++;
					char *q = strchr(p, ' ');
					if (!q) { printk("\nUsage: chmod <mode> <path>\n"); }
					else {
						*q = '\0'; char *mstr = p; char *path = q + 1; while (*path == ' ') path++;
						/* parse mode: octal if starts with 0 */
						int mode = 0; int base = 10; if (mstr[0] == '0') base = 8;
						for (char *c = mstr; *c; ++c) { if (*c >= '0' && *c <= '9') mode = mode * base + (*c - '0'); }
						int r = fs_chmod(path, (unsigned int)mode);
						if (r == FS_OK) printk("\n(chmod) %s -> %o\n", path, mode); else printk("\n(chmod) failed: %d\n", r);
					}
				}

				else if (strlen(buffer) > 0 && strncmp(buffer, "chown ", 6) == 0)
				{
					char *p = buffer + 6; while (*p == ' ') p++;
					/* support two forms: chown uid:gid path  OR chown uid gid path */
					char *sp = strchr(p, ' ');
					if (!sp) { printk("\nUsage: chown <uid>:<gid> <path>  OR chown <uid> <gid> <path>\n"); }
					else {
						*sp = '\0'; char *arg = p; char *rest = sp + 1; while (*rest == ' ') rest++;
						unsigned int uid = 0, gid = 0;
						char *colon = strchr(arg, ':');
						if (colon) { *colon = '\0'; uid = (unsigned int)atoi(arg); gid = (unsigned int)atoi(colon + 1); }
						else {
							/* next token is gid */
							char *sp2 = strchr(rest, ' ');
							if (!sp2) { printk("\nUsage: chown <uid> <gid> <path>\n"); continue; }
							*sp2 = '\0'; uid = (unsigned int)atoi(arg); gid = (unsigned int)atoi(rest); rest = sp2 + 1; while (*rest == ' ') rest++; }
						int r = fs_chown(rest, uid, gid);
						if (r == FS_OK) printk("\n(chown) %s -> %u:%u\n", rest, uid, gid); else printk("\n(chown) failed: %d\n", r);
					}
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "fontcolor") == 0)
				{
					default_font_color = change_font_color();
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "whoami") == 0)
				{
					const user_t *cur = user_current();
					if (cur) {
						printk("\nCurrent user: %s (uid:%u)", cur->name, cur->uid);
					} else {
						printk("\nNo user logged in (guest)");
					}
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "users") == 0)
				{
					printk("\nRegistered users:");
					user_list_all();
					printk("\n");
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "logout") == 0)
				{
					user_logout();
					printk("\nLogged out successfully\n");
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "adduser ", 8) == 0)
				{
					char *p = buffer + 8;
					while (*p == ' ') p++;
					if (*p == '\0') {
						printk("\nUsage: adduser <username>\n");
					} else {
						char *username = p;
						char passwd[USER_PASS_MAX];
						/* Get password interactively */
						printk("\nEnter password: ");
						int i = 0;
						char c;
						while ((c = getch_blocking()) != '\n' && c != '\r' && i < USER_PASS_MAX-1) {
							passwd[i++] = c;
							printk("*");
						}
						passwd[i] = '\0';
						printk("\n");
						/* Try to create user */
						int ur = user_add(username, passwd);
						if (ur == 0) printk("User created successfully\n");
						else if (ur == -2) printk("Error: User database full\n");
						else if (ur == -3) printk("Error: User already exists\n");
						else printk("Error: Failed to create user: %d\n", ur);
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "deluser ", 8) == 0)
				{
					/* Only root can delete users */
					if (!user_is_root()) {
						printk("\nError: Only root can delete users\n");
					} else {
						char *p = buffer + 8;
						while (*p == ' ') p++;
						if (*p == '\0') {
							printk("\nUsage: deluser <username>\n");
						} else {
							int ur = user_delete(p);
							if (ur == 0) printk("\nUser deleted successfully\n");
							else if (ur == -2) printk("\nError: Cannot delete current user\n");
							else printk("\nError: User not found\n");
						}
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "su ", 3) == 0)
				{
					char *p = buffer + 3;
					while (*p == ' ') p++;
					if (*p == '\0') {
						printk("\nUsage: su <username>\n");
					} else {
						char *username = p;
						char passwd[USER_PASS_MAX];
						printk("\nPassword: ");
						int i = 0;
						char c;
						while ((c = getch_blocking()) != '\n' && c != '\r' && i < USER_PASS_MAX-1) {
							passwd[i++] = c;
							printk("*");
						}
						passwd[i] = '\0';
						printk("\n");
						int ur = user_switch(username, passwd);
						if (ur == 0) {
							const user_t *cur = user_current();
							printk("Switched to user: %s\n", cur->name);
						} else {
							printk("Authentication failed\n");
						}
					}
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "sudo ", 5) == 0)
				{
					if (!user_is_root()) {
						printk("\nError: Only root can use sudo\n");
					} else {
						/* Just execute as current user (root) */
						char *cmd = buffer + 5;
						while (*cmd == ' ') cmd++;
						printk("\nExecuting as root: %s\n", cmd);
						/* The command would be processed in the next iteration */
						/* For now, just acknowledge */
					}
				}
				else if (strlen(buffer) > 0 && strcmp(buffer, "clear") == 0)
				{
					terminal_initialize(default_font_color, COLOR_BLACK);
					strcpy(&buffer[strlen(buffer)], "");
				}
				else if (strlen(buffer) > 0 && strncmp(buffer, "fw ", 3) == 0)
				{
					char *p = buffer + 3;
					while (*p == ' ') p++;

					if (strncmp(p, "list", 4) == 0) {
						struct fw_rule rules[MAX_RULES];
						int num = netsec_list_rules(rules, MAX_RULES);
						if (num < 0) {
							printk("\nError listing rules\n");
						} else {
							printk("\nFirewall Rules:\n");
							for (int i = 0; i < num; i++) {
								struct fw_rule *r = &rules[i];
								printk("\n%d: %s ", i, 
									r->type == RULE_ALLOW ? "ALLOW" : "DENY");
								switch(r->target_type) {
									case TARGET_ANY:
										printk("ANY");
										break;
									case TARGET_PORT:
										printk("PORT %u", r->port);
										break;
									case TARGET_ADDRESS:
										printk("IP %u.%u.%u.%u/%u.%u.%u.%u",
											(r->address >> 24) & 0xFF,
											(r->address >> 16) & 0xFF,
											(r->address >> 8) & 0xFF,
											r->address & 0xFF,
											(r->mask >> 24) & 0xFF,
											(r->mask >> 16) & 0xFF,
											(r->mask >> 8) & 0xFF,
											r->mask & 0xFF);
										break;
								}
							}
							printk("\n");
						}
					}
					else if (strncmp(p, "allow ", 6) == 0 || strncmp(p, "deny ", 5) == 0) {
						int is_allow = (p[0] == 'a');
						p += is_allow ? 6 : 5;
						while (*p == ' ') p++;

						struct fw_rule rule;
						rule.type = is_allow ? RULE_ALLOW : RULE_DENY;

						if (strncmp(p, "port ", 5) == 0) {
							p += 5;
							rule.target_type = TARGET_PORT;
							rule.port = atoi(p);
							int r = netsec_add_rule(&rule);
							if (r == 0) {
								printk("\nAdded rule to %s port %u\n",
									is_allow ? "allow" : "deny", rule.port);
							} else {
								printk("\nFailed to add rule: %d\n", r);
							}
						}
						else if (strncmp(p, "ip ", 3) == 0) {
							p += 3;
							rule.target_type = TARGET_ADDRESS;
							/* Parse IP A.B.C.D */
							uint32_t addr = 0;
							for (int i = 0; i < 4; i++) {
								int val = 0;
								while (*p >= '0' && *p <= '9') {
									val = val * 10 + (*p - '0');
									p++;
								}
								addr = (addr << 8) | (val & 0xFF);
								if (i < 3) {
									if (*p != '.') goto bad_ip;
									p++;
								}
							}
							rule.address = addr;
							rule.mask = 0xFFFFFFFF;  /* Full mask */
							int r = netsec_add_rule(&rule);
							if (r == 0) {
								printk("\nAdded rule to %s IP %u.%u.%u.%u\n",
									is_allow ? "allow" : "deny",
									(addr >> 24) & 0xFF,
									(addr >> 16) & 0xFF,
									(addr >> 8) & 0xFF,
									addr & 0xFF);
							} else {
								printk("\nFailed to add rule: %d\n", r);
							}
							continue;
						bad_ip:
							printk("\nInvalid IP address format. Use: A.B.C.D\n");
						}
						else {
							printk("\nUnknown target type. Use: port N or ip A.B.C.D\n");
						}
					}
					else {
						printk("\nUnknown firewall command\n");
						printk("Usage:\n");
						printk("  fw list\n");
						printk("  fw allow|deny port N\n");
						printk("  fw allow|deny ip A.B.C.D\n");
					}
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
				if (strlen(buffer) > 0) {
					char c = normalmap[byte];
					char backspace_str[2] = {c, '\0'};
					printk("%s", backspace_str);
					buffer[strlen(buffer) - 1] = '\0';
				}
			}
			else
			{
				char c1 = togglecode[byte];
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
				size_t curr_len = strlen(buffer);
				if (curr_len + 2 < BUFFER_SIZE) { // +2 for char and null terminator
					strncpy(&buffer[curr_len], s, BUFFER_SIZE - curr_len - 1);
					buffer[BUFFER_SIZE-1] = '\0';
				}
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
