#include "MemoryMapParser.h"

ADDRINT LinuxMemoryMapParser::getStackBaseAddress(){
    // If |stackline| is empty, parse /proc/<pid>/maps file stream
    // to find the line containing stack's address range
    if(stackLine.empty()){
        unsigned bufSize = BUFSIZE;
        char* buf = (char*) malloc(sizeof(char) * bufSize);
        do{
            maps.getline(buf, bufSize);
            if(maps.gcount() == bufSize){
                // Buffer may be too small. Reallocate it, doubling its size, and seek
                // stream's cursor as if the last getline operation never happened
                // and re-execute it
                free(buf);
                unsigned oldSize = bufSize;
                bufSize *= 2;
                buf = (char*) malloc(sizeof(char) * bufSize);
                maps.seekg(- oldSize, std::ios_base::cur);
                continue;
            }
            stackLine = buf;
        } while(stackLine.find("[stack]") == stackLine.npos);
        free(buf);
    }

    // |stackLine| contains the line of /proc/<pid>/maps giving the address range of process' stack.
    // Address range in this file is reported as:
    // cafebabe-deadbeef, where the sequence before "-" is the lowest address of the range, while the 
    // sequence following "-" is the highest address of the range.
    // The address range is followed by a space character.
    unsigned addrRangeSeparatorPos = stackLine.find("-");
    unsigned finalSpaceCharacterPos = stackLine.find(" ", addrRangeSeparatorPos);
    unsigned addrLen = finalSpaceCharacterPos - addrRangeSeparatorPos - 1;
    std::string highestAddr = stackLine.substr(addrRangeSeparatorPos + 1, addrLen);
    std::istringstream ss(highestAddr);
    ADDRINT ret;
    ss >> std::hex >> ret;
    return ret;
}