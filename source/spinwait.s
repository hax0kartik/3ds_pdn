.balign 4
.arm

.global spinwait
.type spinwait, %function
spinwait:
subs r0, r0, #2
nop
bgt spinwait
bx lr