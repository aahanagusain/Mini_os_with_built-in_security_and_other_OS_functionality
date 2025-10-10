/* This file is a minimal hand-written initrd for Phase 1 testing.
 *  use tools/mkinitrd.py to generate a larger table.
 */
#include "../include/fs.h"

static const uint8_t readme_txt[] =
    "Welcome to Pegasus (embedded initrd)!\n"
    "This file is provided by initrd_data.c\n";

static const uint8_t version_txt[] =
    "version=0.1\n";

const struct fs_file initrd_files[] = {
    { "/README.txt", readme_txt, sizeof(readme_txt)-1 },
    { "/version.txt", version_txt, sizeof(version_txt)-1 },
};

const unsigned int initrd_files_count = sizeof(initrd_files)/sizeof(initrd_files[0]);
