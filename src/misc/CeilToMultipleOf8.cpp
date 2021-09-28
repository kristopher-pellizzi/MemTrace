/*
    This function takes a size in bytes as a parameter and returns the ceiling of the given number of bytes
    to the next multiple of 8 bits.
    E.g. if |byteSize| is 4, it returns 8; it |byteSize| is 10, it returns 16 and so on...
*/
unsigned ceilToMultipleOf8(unsigned byteSize){
    if(byteSize % 8 == 0)
        return byteSize;

    if(byteSize < 8)
        return 8;

    unsigned n = byteSize;
    unsigned ret = 1;
    while(n > 1){
        n >>= 3;
        ++ret;
    }
    return ret * 8;
}