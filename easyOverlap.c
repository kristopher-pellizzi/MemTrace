int main(){
    __asm__(
        "sub $0x8, %rsp;"
        "mov $0x4141414141414141, %rax;"
        "mov %rax, (%rsp);"
        "mov (%rsp), %rbx;"
        "mov 0x04(%rsp), %eax;"
        "add $0x8, %rsp;"
    );
    return 0;
}