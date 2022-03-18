
%macro gensys 2
    global sys_%2:function
sys_%2:
    push    r10
    mov    r10, rcx
    mov    rax, %1
    syscall
    pop    r10
    ret
%endmacro

; RDI, RSI, RDX, RCX, R8, R9

extern    errno

    section .data

    section .text

    gensys 0, read
    gensys 1, write
    gensys 2, open
    gensys 3, close
    gensys 9, mmap
    gensys  10, mprotect
    gensys  11, munmap
    gensys  22, pipe
    gensys  32, dup
    gensys  33, dup2
    gensys  34, pause
    gensys  35, nanosleep
    gensys  57, fork
    gensys  60, exit
    gensys  79, getcwd
    gensys  80, chdir
    gensys  82, rename
    gensys  83, mkdir
    gensys  84, rmdir
    gensys  85, creat
    gensys  86, link
    gensys  88, unlink
    gensys  89, readlink
    gensys  90, chmod
    gensys  92, chown
    gensys  95, umask
    gensys  96, gettimeofday
    gensys 102, getuid
    gensys 104, getgid
    gensys 105, setuid
    gensys 106, setgid
    gensys 107, geteuid
    gensys 108, getegid

    gensys  13, rt_sigaction
    gensys  14, rt_sigprocmask
    gensys  37, alarm
    gensys 127, rt_sigpending

    global open:function
open:
    call    sys_open
    cmp    rax, 0
    jge    open_success    ; no error :)
open_error:
    neg    rax
%ifdef NASM
    mov    rdi, [rel errno wrt ..gotpc]
%else
    mov    rdi, [rel errno wrt ..gotpcrel]
%endif
    mov    [rdi], rax    ; errno = -rax
    mov    rax, -1
    jmp    open_quit
open_success:
%ifdef NASM
    mov    rdi, [rel errno wrt ..gotpc]
%else
    mov    rdi, [rel errno wrt ..gotpcrel]
%endif
    mov    qword [rdi], 0    ; errno = 0
open_quit:
    ret

    global sleep:function
sleep:
    sub    rsp, 32        ; allocate timespec * 2
    mov    [rsp], rdi        ; req.tv_sec
    mov    qword [rsp+8], 0    ; req.tv_nsec
    mov    rdi, rsp    ; rdi = req @ rsp
    lea    rsi, [rsp+16]    ; rsi = rem @ rsp+16
    call    sys_nanosleep
    cmp    rax, 0
    jge    sleep_quit    ; no error :)
sleep_error:
    neg    rax
    cmp    rax, 4        ; rax == EINTR?
    jne    sleep_failed
sleep_interrupted:
    lea    rsi, [rsp+16]
    mov    rax, [rsi]    ; return rem.tv_sec
    jmp    sleep_quit
sleep_failed:
    mov    rax, 0        ; return 0 on error
sleep_quit:
    add    rsp, 32
    ret

global setjmp:function
setjmp:
    mov     qword [rdi],        rbx
    mov     qword [rdi+8],      rsp
    mov     qword [rdi+8*2],    rbp
    mov     qword [rdi+8*3],    r12
    mov     qword [rdi+8*4],    r13
    mov     qword [rdi+8*5],    r14
    mov     qword [rdi+8*6],    r15

    mov     rax, qword [rsp]
    mov     qword [rdi+8*7],    rax

    call    sys_rt_sigprocmask

    mov     rsi, 0
    lea     rdx, [rdi+8*8]
    mov     rax, 0
    ret


global longjmp:function
longjmp:
    mov     rbx, qword [rdi]
    mov     rsp, qword [rdi+8]
    mov     rbp, qword [rdi+8*2]
    mov     r12, qword [rdi+8*3]
    mov     r13, qword [rdi+8*4]
    mov     r14, qword [rdi+8*5]
    mov     r15, qword [rdi+8*6]

    pop     rax
    mov     rax, qword [rdi+8*7]
    push    rax

    call    sys_rt_sigprocmask

    push    rsi
    mov     rdi, 2
    mov     rcx, 8
    lea     rsi, [rdi+8*8]
    mov     rdx, 0
    pop     rax
    ret

    global __myrt:function
__myrt:
    mov     rax, 15
    syscall
    ret
