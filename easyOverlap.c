int f(){
    __asm__(
        "sub $0x8, %rsp;"
        "mov (%rsp), %rax;"
        "mov 0x04(%rsp), %eax;"
        "mov $0x4141414141414141, %rax;"
        "mov %rax, (%rsp);"
        "mov (%rsp), %rbx;"
        "mov 0x04(%rsp), %eax;"
        "mov 0x04(%rsp), %rax;"
        "mov 0x01(%rsp), %rax;"
        "add $0x8, %rsp;"
    );
    return 0;
}

int main(){
    int (*func)(void) = f;
    return (*func)();
}