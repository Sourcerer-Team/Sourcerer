; Syntax highlighting test for assembly (GNU assembler syntax): sections,
; directives, labels, instructions, registers, memory operands and comments.

	.section .rodata
greeting:
	.asciz "Hello, world!\n"
	.align 8
counts:
	.int 10, 0x1F, -3
	.float 3.14
	.byte 0b1010

	.section .bss
	.zero 64

	.text
	.globl main

/* A block comment
   spanning several lines. */
main:
	pushq %rbp
	movq %rsp, %rbp
	subq $32, %rsp
	movl $1, -4(%rbp)        # an inline comment
	movzbl -1(%rbp), %eax
	leaq 8(%rip), %rdi
	call printf
	addl $5, %eax; imull %eax, %eax
	cmpl $0, %eax
	jne .Lloop
	xorl %eax, %eax
.Lloop:
	movq (%rsp), %rdx
	leave
	ret

	.ident "GCC 13.2.0"
	.section .comment
