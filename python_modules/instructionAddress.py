class InstructionAddress(object):
    '''
    Attributes:
    ip (int): Instruction Pointer of the last instruction executed from the main executable (may be equal to the actualIp).
    actualIp (int): actual Instruction Pointer of the instruction. Equal to ip only if it is an instruction from the main executable.
    libName (str): name of the library the instruction belongs to
    baseAddr (int): base address where the library the instruction belongs to is loaded
    '''
    def __init__(self, ip, actualIp, libName, baseAddr):
        self.ip = int(ip, 16)
        self.actualIp = int(actualIp, 16)
        self.libName = libName
        self.baseAddr = int(baseAddr, 16)


    def __eq__(self, other):
        if not isinstance(other, InstructionAddress):
            return False

        # Instructions belong to different libraries or they are called from different locations
        # by the main executable
        if self.libName != other.libName or self.ip != other.ip:
            return False

        return self.actualIp - self.baseAddr == other.actualIp - other.baseAddr

    def __hash__(self):
        return (self.ip, self.libName, self.actualIp - self.baseAddr).__hash__()


    def __str__(self):
        l = [hex(self.ip), "(", hex(self.actualIp), ")", ": ", self.libName, " loaded at @ ", hex(self.baseAddr)]
        return "".join(l)