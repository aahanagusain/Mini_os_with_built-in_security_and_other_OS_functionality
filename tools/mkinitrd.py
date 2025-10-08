#!/usr/bin/env python3
"""
Simple initrd packer: converts files in a directory into a C source file
containing arrays and a table of `struct fs_file` entries.

Usage: tools/mkinitrd.py <directory> > src/initrd_data.c
"""
import sys
import os

def emit_c(path, relpath):
    name = os.path.basename(relpath).replace('.', '_')
    var = f"file_{abs(hash(relpath)) & 0xffffffff:08x}"
    with open(path, 'rb') as f:
        data = f.read()
    print(f"static const uint8_t {var}[] = ")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        line = ', '.join(str(b) for b in chunk)
        print(f"    {line},")
    print("};")
    print(f"    ")
    return var, len(data)

def main():
    if len(sys.argv) != 2:
        print("Usage: mkinitrd.py <dir>", file=sys.stderr)
        sys.exit(2)
    root = sys.argv[1]
    entries = []
    print('#include "../include/fs.h"')
    print()
    for dirpath, dirs, files in os.walk(root):
        for fn in files:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root)
            rel = '/' + rel.replace('\\', '/')
            var, size = emit_c(full, rel)
            entries.append((rel, var, size))
    print('const struct fs_file initrd_files[] = {')
    for path, var, size in entries:
        print(f'    {{ "{path}", {var}, {size} }},')
    print('};')
    print()
    print(f'const unsigned int initrd_files_count = sizeof(initrd_files)/sizeof(initrd_files[0]);')

if __name__ == '__main__':
    main()
