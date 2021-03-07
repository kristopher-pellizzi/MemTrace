#include <vector>

using std::vector;

/*
    This struct contains the index of the considered syscall argument and a boolean indicating 
    whether we need to compute its size or not.
*/
struct SyscallArg{
    unsigned short index;
    bool requiresSize;

    SyscallArg(unsigned short index, bool requiresSize) :
        index(index),
        requiresSize(requiresSize)
    {}
};

struct SyscallArgBase{
    virtual size_t size() const;
};

// Class used in case the size of the argument type is needed
// E.g. if the syscall takes a pointer to a class that is going to be filled,
// we need to know the size of the struct
template<typename T> 
class SyscallArgType : public SyscallArgBase{
    private:
        T arg;

    public:
        // There's no need to copy or assign such a class, it is only used
        // to retrieve the size of syscalls arguments
        SyscallArgType(const SyscallArg& other) = delete;
        SyscallArgType& operator=(const SyscallArgType& other) = delete;

        SyscallArgType() {}

        size_t size() const override{
            return sizeof(arg);
        }
};