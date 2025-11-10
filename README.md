# Mini_os_with_built-in_security_and_other_OS_functionality


Requirements:-
-GNU/Kali Linux.
-GNU Assembler(gas) to instruct the bootloader for loading the starting point of our kernel.
-GCC - GNU Compiler Collection a cross compiler. A newer version of GCC (7.2.0 version of GCC)
-Xorriso - A package that creates, loads, manipulates ISO 9660 filesystem images.(man xorriso)
-grub-mkrescue - Make a GRUB rescue image, this package internally calls the xorriso functionality to build an iso image.
-VMware
-qemu


make initrd data file:
    python3 tools/mkinitrd.py fsroot > src/initrd_data.c

Make Iso file:
    make pegasus.iso


Run ISO file:
   - run iso in vmware 
   - qemu-system-x86_64 -m 1024 -cdrom pegasus.iso -serial mon:stdio -no-reboot


references:
 mathematical functions - https://developer.download.nvidia.com/cg/index_stdlib.html
 https://en.wikipedia.org/wiki/X86_calling_conventions
 printf - https://cplusplus.com/reference/cstdio/printf/
 puts - https://cplusplus.com/reference/cstdio/puts/
 stdio library - https://cplusplus.com/reference/cstdio/
https://lass.cs.umass.edu/~shenoy/courses/spring20/lectures/Lec18.pdf

