from collections import deque
from typing import Deque, Iterable
from parsedData import MemoryAccess, AccessType, MemType
from functools import reduce

class MASet(object):
    def __init__(self):
        self.origins: Deque[str] = deque()
        self.set: Deque[MemoryAccess] = deque()
        self.memType: MemType = None
        self.memOffset: int = None
        self.size: int = None


    def addOrigin(self, origin: str):
        if origin not in self.origins:
            self.origins.append(origin)


    def addOrigins(self, origins: Iterable[str]):
        for origin in origins:  
            if origin not in self.origins:
                self.origins.append(origin)


    def addMA(self, ma):
        self.set.append(ma)

    
    def addMASet(self, set):
        self.set.extend(set)


    def replaceMASet(self, set):
        self.set = deque()
        self.addMASet(set)


    def setMemLocation(self, accessIndex: int, type: MemType, mem_base: int):
        addr, size = accessIndex
        addr = int(addr, 16)
        self.memOffset = addr - mem_base
        self.size = size
        self.memType = type


    def compareMemLocation(self, other):
        return self.memType == other.memType and self.memOffset == other.memOffset and self.size == other.size



    # Class (static) method to create a new MASet object given an origin and a
    # MemoryAccess set (implemented as a deque to retain execution order)
    def create(origin: str, set: Deque[MemoryAccess]):
        newSet = MASet()
        newSet.addOrigin(origin)
        newSet.addMASet(set)
        return newSet


    def __str__(self):
        l = deque()
        l.append("From inputs:\n")

        for origin in self.origins:
            l.append("[*] ")
            l.append(origin)
            l.append("\n")

        l.append("***********************************************\n")

        for ma in self.set:
            l.append(str(ma))
            l.append("\n")
        
        l.append("***********************************************\n")

        return "".join(l)


def remove_useless_writes(overlaps: Deque[MemoryAccess]):
    ret = deque()

    # If there are no more read accesses left in the set, just dreturn an empty set
    if len(list(filter(lambda x: x.accessType == AccessType.READ, overlaps))) <= 0:
        return ret

    # Deque objects have a constant time append/remove methods, but a O(n)
    # lookup and has no random access (access is performed by scanning the list)
    # So, it is more convenient to pop entries and insert them again in a new deque
    # instead of removing the bad ones
    while(len(overlaps) > 0):
        entry = overlaps.popleft()

        if entry.accessType == AccessType.READ:
            ret.append(entry) 
        elif is_read_by_uninitialized_read(entry, overlaps):
            ret.append(entry)

    return ret


def check_for_overlapping_writes(overlaps, uninitialized_read):
    read_intervals = uninitialized_read.uninitializedIntervals
    writes = list(filter(lambda x: x.accessType == AccessType.WRITE, overlaps))
    for entry in writes:
        # NOTE: write accesses are supposed to have only 1 uninitialized interval
        write_overlaps_read = lambda x: intervals_overlap(entry.uninitializedIntervals[0], x)
        has_overlapping_writes = reduce(lambda acc, x: acc or x, map(write_overlaps_read, read_intervals), False)
        if has_overlapping_writes:
            return True

    return False


def is_read_by_uninitialized_read(write, overlaps):
    write_interval = write.uninitializedIntervals[0]
    write_lower, write_upper = write_interval
    not_overwritten_bytes = set(range(write_lower, write_upper + 1))

    for entry in overlaps:
        if len(not_overwritten_bytes) == 0:
            return False

        if entry.accessType == AccessType.WRITE:
            entry_interval = entry.uninitializedIntervals[0]
            if not intervals_overlap(write_interval, entry_interval):
                continue

            entry_lower, entry_upper = entry_interval
            not_overwritten_bytes.difference_update(set(range(entry_lower, entry_upper + 1)))
        else:
            entry_interval = (0, entry.accessSize - 1)
            read_overlaps = intervals_overlap(write_interval, entry_interval)
            if read_overlaps:
                return True
    
    return False


def intervals_overlap(first_interval, second_interval):
    first_lower, first_upper = first_interval
    second_lower, second_upper = second_interval

    if first_lower <= second_lower:
        return second_lower <= first_upper
    else:
        return first_lower <= second_upper