#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "SyscallArg.h"

using std::unordered_map;
using std::vector;
using std::pair;

class SyscallSignatureProvider{
    private:
        // For each syscall, contains a vector of a struct containing the index of the argument
        // if it is a pointer from which 
        // the syscall either reads or writes and a boolean telling if we need to
        // compute the size of the pointed structure
        unordered_map<unsigned short, const vector<SyscallArg>> syscallsArgPtr;
        const vector<SyscallArg> unsupportedSyscall = vector<SyscallArg>();

        // Private default constructor to avoid anythin else create another instance
        SyscallSignatureProvider(){}

        // Initialization methods
        void init(){
            // Parse yaml file and fill syscallsArgPtr structure
            // Read syscall
            vector<SyscallArg> v;
            v.push_back(SyscallArg(0, false));
            v.push_back(SyscallArg(1, false));
            v.push_back(SyscallArg(2, false));
            syscallsArgPtr.insert(pair<unsigned short, const vector<SyscallArg>>(0, v));
            // Write syscall
            syscallsArgPtr.insert(pair<unsigned short, const vector<SyscallArg>>(1, v));

        }

    public:
        // Delete both copy constructor and assignment operator
        SyscallSignatureProvider(const SyscallSignatureProvider& other) = delete;
        SyscallSignatureProvider& operator=(const SyscallSignatureProvider& other) = delete;

        static SyscallSignatureProvider& getInstance(){
            static SyscallSignatureProvider instance;
            static bool isInitialized = false;

            if(!isInitialized){
                instance.init();
                isInitialized = true;
            }
            return instance;
        }

        const vector<SyscallArg>& getSyscallSignature(unsigned short sysNum) const{
            auto iter = syscallsArgPtr.find(sysNum);
            return iter == syscallsArgPtr.end() ? unsupportedSyscall : iter->second;
        }
};