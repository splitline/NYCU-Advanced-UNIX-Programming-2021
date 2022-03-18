extern main
extern exit

	section .text
	global _start
_start:
	mov	rdi, [rsp]	; argc
	lea	rsi, [rsp+8]	; argv
	call	main
	mov	rdi, rax	; exit code
	call	exit
	ret
