#include "pin.H"
#include <fstream>
#include <sstream>
#include <string>

#define BUFSIZE 256

// Base class
class MemoryMapParser{
    public:
        // Method to retrieve the highest address belonging to the stack section in memory
        virtual ADDRINT getStackBaseAddress();
};

class LinuxMemoryMapParser : public MemoryMapParser{
    private:
        unsigned pid;
        std::ifstream maps;
        std::string stackLine;

    public:
        LinuxMemoryMapParser(unsigned pid) : pid(pid) {
            std::stringstream ss;
            ss << "/proc/" << pid << "/maps";
            std::string path;
            ss >> path;
            maps.open(path.c_str());
            stackLine = "";
        }

        ~LinuxMemoryMapParser(){
            maps.close();
        }

        ADDRINT getStackBaseAddress() override;
};