from collections import deque
from typing import Deque, Iterable
from parsedData import MemoryAccess

class MASet(object):
    def __init__(self):
        self.origins: Deque[str] = deque()
        self.set: Deque[MemoryAccess] = deque()


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