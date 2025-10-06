#include "../include/utils.h"
#include "../include/tty.h"

void print_logo()
{
    printk("\t                                                         \n");

}

void about(char *version)
{
    printk("\n\tPegasus - v%s - A simple 32-bit Ring 0 operating system", version);

}