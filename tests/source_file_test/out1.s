.text

.globl main
main:
    movl $3, %eax
    negl %eax
    pushq %rax
    movl $1, %eax
    pushq %rax
    movl $5, %eax
    pushq %rax
    movl $3, %eax
    movl %eax, %ecx
    popq %rax
    cltd
    idivl %ecx
    movl %edx, %eax
    movl %eax, %ecx
    popq %rax
    subl %ecx, %eax
    movl %eax, %ecx
    popq %rax
    imull %ecx, %eax
    pushq %rax
    movl $14, %eax
    movl %eax, %ecx
    popq %rax
    imull %ecx, %eax
    ret
.section .note.GNU-stack,"",@progbits
