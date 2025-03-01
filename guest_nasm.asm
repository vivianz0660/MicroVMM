[BITS 16]
global _start

section .data
    addr db " "

section .text
_start:
    mov ebx, addr
    mov ecx, 0
check_key:
    in al, 0x45         ; check keyboard status reg
    cmp al, 0
    je check_key          
    mov al, 0
    out 0x45, al        ; clear keyboard status
    in al, 0x44         ; read contents of port 0x44 into al - read key
    mov [ebx+ecx], al
    inc ecx
    cmp al, `\n`
    jne check_key
    call out_str_console
    mov al, 0x1
    out 0x47, al
    mov al, 200         ; set timer interval in milliseconds here
    out 0x46, al
check_timer:
    in al, 0x45
    cmp al, 0
    jne check_exit
    in al, 0x47
    cmp al, 0x3
    jne check_timer
    mov al, 0x1
    out 0x47, al
    call out_str_console
    ;add al, '0'
    ;out 0x42, al
    ;mov al, `\n`
    ;out 0x42, al
    jmp check_timer
check_exit:    
    mov al, 0
    out 0x45, al        ; clear keyboard status
    in al, 0x44
    cmp al, `\n`
    jne check_timer
    hlt

out_str_console:
    mov ecx, 0
loop:
    mov al, [ebx+ecx]
    out 0x42, al        ; output contents of al to port
    inc ecx
    cmp al, `\n`
    jne loop
    ret


