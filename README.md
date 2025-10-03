# Mini_os_with_built-in_security_and_other_OS_functionality


Requirements:-
-GNU/Kali Linux.
-GNU Assembler(gas) to instruct the bootloader for loading the starting point of our kernel.
-GCC - GNU Compiler Collection a cross compiler. A newer version of GCC (7.2.0 version of GCC)
-Xorriso - A package that creates, loads, manipulates ISO 9660 filesystem images.(man xorriso)
-grub-mkrescue - Make a GRUB rescue image, this package internally calls the xorriso functionality to build an iso image.
-VMware
-qemu


Make Iso file:
bash ./run.sh


Run ISO file:
qemu-system-x86_64 -m 1024 -boot d -enable-kvm -smp 3 -net nic -net user -hda testing-image.img -cdrom RunDemo.iso   