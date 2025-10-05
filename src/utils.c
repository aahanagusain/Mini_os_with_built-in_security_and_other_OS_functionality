#include "../include/utils.h"
#include "../include/tty.h"

void print_logo()
{
    printk("\t                                                         \n");
    printk("\t  _____  _____     _                           ____    _____ \n");
    printk("\t |  __ \\     (_)                         / __ \\  / ____|\n");
    printk("\t | |__) | ____     __ ___   _   _  ___  | |  | || (___  \n");
    printk("\t |  ___/| |   || '_ ` _ \\ | | | |/ __| | |  | | \\___ \\ \n");
    printk("\t | |    | |   | || | | | | || |_| |\\__ \\ | |__| | ____) |\n");
    printk("\t |_|    |_|   |_||_| |_| |_| \\__,_||___/  \\____/ |_____/ \n");
    printk("\t                                                         \n");
    printk("\t                                                         \n");
}

void about(char *version)
{
    printk("\n\tPegasus - v%s - A simple 32-bit Ring 0 operating system", version);

}