	.arch armv6k
	.section .text.spinwait, "ax", %progbits
	.arm
	.align  2
	.global spinwait
	.syntax unified
	.type   spinwait, %function
spinwait:
	subs   r0, r0, #2
	nop
	bgt     spinwait
	bx      lr
	.size   spinwait, .-spinwait
