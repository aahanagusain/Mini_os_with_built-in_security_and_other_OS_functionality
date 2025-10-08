#include "../include/utils.h"
#include "../include/tty.h"
#include "../include/time.h"

void print_logo()
{
    printk("\t                                                         \n");

}

void about(char *version)
{
    printk("\n\tPegasus -- A simple 32-bit operating system");

    /* print version and build time */
    if (version) {
        printk("\n\tVersion: %s", version);
    }
    const char *bt = get_build_time();
    if (bt) {
        printk("\n\tBuild time: %s", bt);
    }

}