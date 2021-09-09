from collections import deque
from typing import Deque, Tuple, List

class InvalidConstructorError(Exception):
    pass

class MemType(object):
    STACK = 0
    HEAP = 1

    def __init__(self):
        raise InvalidConstructorError("Class MemType is not instantiatable")


class AccessType(object):
    READ = 0
    WRITE = 1

    def __init__(self):
        raise InvalidConstructorError("Class AccessType is not instantiatable")


class MemoryAccess(object):
    def __init__(
                self,
                executionOrder,
                ip,
                actualIp,
                spOffset,
                bpOffset,
                accessType,
                accessSize,
                disasm,
                isUninitializedRead,
                uninitializedIntervals,
                memType,
                isPartialOverlap
                ):
        self.executionOrder: int = executionOrder
        self.ip: str = ip
        self.actualIp: str = actualIp

        # The following attribute is used only for uninitialized read accesses in order to store
        # the memory location it reads from.
        # This is used in the merging script in order to check if 2 identical read accesses also read 
        # from the same location or not
        self.accessedAddress: AccessIndex = None

        self.spOffset: int = spOffset
        self.bpOffset: int = bpOffset
        self.accessType: AccessType = accessType
        self.accessSize: int = accessSize
        self.disasm: str = disasm
        self.isUninitializedRead: bool = isUninitializedRead
        self.uninitializedIntervals: Deque[Tuple[int, int]] = uninitializedIntervals
        self.memType: MemType = memType
        self.isPartialOverlap: bool = isPartialOverlap

    def __eq__(self, other):
        if not isinstance(other, MemoryAccess):
            return False
        return  (
                self.ip == other.ip and
                #self.actualIp == other.actualIp and
                self.accessType == other.accessType and
                self.accessSize == other.accessSize and
                self.isUninitializedRead == other.isUninitializedRead and
                self.uninitializedIntervals == other.uninitializedIntervals and
                self.memType == other.memType and
                self.isPartialOverlap == other.isPartialOverlap
        )

    def compare(self, ma2, load_base1: int, load_base2: int) -> bool:

        # Returns True if the offset of the actualIP from the library base address is the same
        # for both the MemoryAccess objects
        def compare_actualIP():
            if self.actualIp == ma2.actualIp and load_base1 == load_base2:
                return True
                
            lib_offset1 = int(self.actualIp, 16) - load_base1
            lib_offset2 = int(ma2.actualIp, 16) - load_base2
            return lib_offset1 == lib_offset2

        return self == ma2 and compare_actualIP()


    def compare_mem_location(self, ma2, mem_base1: int, mem_base2: int) -> bool:
        self_addr, self_size = self.accessedAddress
        other_addr, other_size = ma2.accessedAddress
        self_addr = int(self_addr, 16) - mem_base1
        other_addr = int(other_addr, 16) - mem_base2
        return self_addr == other_addr and self_size == other_size


    def __str__(self):
        str_list = deque()
        if self.isPartialOverlap:
            str_list.append("=> ")
        else:
            str_list.append("   ")

        if self.isUninitializedRead:
            str_list.append("*")

        str_list.append(self.ip + " (" + self.actualIp + "):")
        str_list.append("\t" if len(self.disasm) > 0 else " ")
        str_list.append(self.disasm)
        str_list.append(" W " if self.accessType == AccessType.WRITE else " R ")
        str_list.append(str(self.accessSize) + " B ")
        if self.memType == MemType.STACK:
            str_list.append("@ (sp ")
            str_list.append("+ " if self.spOffset >= 0 else "- ")
            str_list.append(str(abs(self.spOffset)))
            str_list.append("); ")
            str_list.append("(bp ")
            str_list.append("+ " if self.bpOffset >= 0 else "- ")
            str_list.append(str(abs(self.bpOffset)))
            str_list.append("); ")

        for _ in range(len(self.uninitializedIntervals)):
            interval = self.uninitializedIntervals.popleft()
            lower_bound, upper_bound = interval
            str_list.append("[" + str(lower_bound) + " ~ " + str(upper_bound) + "]")
            self.uninitializedIntervals.append(interval)

        return "".join(str_list)


class AccessIndex(object):
    def __init__(self, address, size):
        self.address = address
        self.size = size

    def __iter__(self):
        return iter((self.address, self.size))

    def __hash__(self):
        return (self.address, self.size).__hash__()

    def __eq__(self, other):
        if isinstance(other, AccessIndex):
            return self.address == other.address and self.size == other.size
        return False

    def __ne__(self, other):
        if isinstance(other, AccessIndex):
            return not (self == other)
        return False

    def __lt__(self, other):
        if isinstance(other, AccessIndex):
            if(self.address != other.address):
                return self.address < other.address
            
            if(self.size != other.size):
                return self.size > other.size

            return False
        
        return False


class ParseResult(object):
    def __init__(self):
        self.load_bases: List[Tuple[str, str]] = None
        self.stack_base: str = None
        self.full_overlaps: Deque[Tuple[AccessIndex, Deque[MemoryAccess]]] = deque()
        self.partial_overlaps: Deque[Tuple[AccessIndex, Deque[MemoryAccess]]] = deque()