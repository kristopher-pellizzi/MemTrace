from collections import deque
from typing import *

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
        self.executionOrder = executionOrder
        self.ip = ip
        self.actualIp = actualIp
        self.spOffset = spOffset
        self.bpOffset = bpOffset
        self.accessType = accessType
        self.accessSize = accessSize
        self.disasm = disasm
        self.isUninitializedRead = isUninitializedRead
        self.uninitializedIntervals = uninitializedIntervals
        self.memType = memType
        self.isPartialOverlap = isPartialOverlap

    def __eq__(self, other):
        if not isinstance(other, MemoryAccess):
            return False
        return  (
                self.ip == other.ip and
                self.actualIp == other.actualIp and
                self.accessType == other.accessType and
                self.accessSize == other.accessSize and
                self.isUninitializedRead == other.isUninitializedRead and
                self.uninitializedIntervals == other.uninitializedIntervals and
                self.memType == other.memType and
                self.isPartialOverlap == other.isPartialOverlap
        )


class AccessIndex(object):
    def __init__(self, address, size):
        self.address = address
        self.size = size

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