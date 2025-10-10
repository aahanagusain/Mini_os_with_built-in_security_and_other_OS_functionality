global time

section .text
time:
    push ebp
    mov ebp, esp
    ; Embed a placeholder time string. Replace this at build time if you want the actual build time.
    datetime db "__TIME__", 0x0
    mov eax, datetime
    mov esp, ebp
    pop ebp
    ret