#!/usr/bin/env python3

from collections import deque
from binascii import b2a_hex

class ParseError(Exception):
    def __init__(self, file, message="Error while parsing"):
        super().__init__(message)
        print("Error @ byte " + str(file.tell()))
        file.seek(-6, 1)
        print(b"... " + file.read(50))
        print("            ^")
        print("            |")


class ReportWriter(object):
    file = None

    def __del__(self):
        self.file.close()
    
    def writelines(self, lines):
        for line in lines:
            self.file.writelines([line, "\n"])

    def write(self, str):
        self.file.write(str)


class FullOverlapsWriter(ReportWriter):   
    def __init__(self):
        self.file = open("overlaps.log", "w")


class PartialOverlapsWriter(ReportWriter):
    def __init__(self):
        self.file = open("partialOverlaps.log", "w")


def accept(f, acceptable):
    l = len(acceptable)
    if(f.read(l) == acceptable):
        return True
    f.seek(-l, 1)
    return False


def expect(f, expected_list):
    read_byte = f.read(1)
    for expected_val in expected_list:
        if read_byte == expected_val:
            return read_byte
    raise ParseError(f, "Expected byte not found.")


def parse_integer(f):
    try:
        int_val = b""
        is_negative = accept(f, b"-")

        read_byte = f.read(1)
        while(read_byte != b";"):
            if read_byte == b"":
                raise ParseError(f, "EOF reached while parsing integer")
            int_val += read_byte
            read_byte = f.read(1)
        
        if int_val == b"":
            return 0
        
        ret = int(int_val, 10)
        return - ret if is_negative else ret
    except:
        raise ParseError(f)


def parse_address(f, reg_size):
    try:
        addr = f.read(reg_size)
        if len(addr) != reg_size:
            raise ParseError(f, "EOF reached while parsing an address")
        addr = int(b2a_hex(addr[::-1]), 16)
        return hex(addr)
    except:
        raise ParseError(f)


def parse_string(f):
    try:
        string = b""
        read_byte = f.read(1)
        while read_byte != b";":
            if(read_byte == b""):
                raise ParseError(f, "EOF reached while parsing a string")
            string += read_byte
            read_byte = f.read(1)
        return string.decode("utf-8")
    except:
        raise ParseError(f)


def get_fo_legend():
    legend = \
        "ENTRY FORMAT: \n[*]<binaryIP> (<actualIP>): [<assembly_instr>] {R/W} <access_size> B @ "\
            "(sp {+/-} <sp_relative_offset>); (bp {+/-} <bp_relative_offset>) "\
            "['['<uninitialized_lower_bound> ~ <uninitialized_upper_bound>']']"
    return legend


def print_table_header(rw, header):
    l1 = "==============================================="
    l2 = header
    rw.writelines([l1, l2, l1])


def print_table_entry(rw, entry):
    rw.writelines([entry])


def print_table_footer(rw):
    l1 = "==============================================="
    rw.writelines([l1, l1, "", "", "", ""])


def parse_full_overlap_entry(f, reg_size):
    global fo
    
    is_uninitialized_read = True if f.read(1) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_inst = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_size = parse_integer(f)
    mem_type = expect(f, [b'\x1c', b'\x1d'])
    sp_offset = parse_integer(f)
    bp_offset = parse_integer(f)
    uninitialized_intervals = deque()

    # If it is an uninitialized read access, the report contains also
    # the interval that is considered uninitialized
    if is_uninitialized_read:
        n = parse_integer(f)
        for _ in range(n):
            interval_lower_bound = parse_integer(f)
            interval_upper_bound = parse_integer(f)
            uninitialized_intervals.append((interval_lower_bound, interval_upper_bound))

    # Emit full overlap table entry
    str_list = []
    str_list.append("*" if is_uninitialized_read else "")
    str_list.append(ip + " (" + actual_ip + "):")
    str_list.append("\t" if len(disassembled_inst) > 0 else " ")
    str_list.append(disassembled_inst)
    str_list.append(" W " if access_type == b"\x1a" else " R ")
    str_list.append(str(access_size) + " B ")
    # if it is a stack access, write also sp and bp offsets
    if mem_type == b'\x1c':
        str_list.append("@ (sp ")
        str_list.append("+ " if sp_offset >= 0 else "- ")
        str_list.append(str(abs(sp_offset)) + "); ")
        str_list.append("(bp ")
        str_list.append("+ " if bp_offset >= 0 else "- ")
        str_list.append(str(abs(bp_offset)) + ")")
    while len(uninitialized_intervals) > 0:
        lower_bound, upper_bound = uninitialized_intervals.popleft()
        str_list.append(" [" + str(lower_bound) + " ~ " + str(upper_bound) + "]")

    print_table_entry(fo, "".join(str_list))



def parse_full_overlap(f, reg_size):
    global fo

    addr = parse_address(f, reg_size)
    access_size = parse_integer(f)
    # Emit table header
    header = addr + " - " + str(access_size)
    print_table_header(fo, header)
    while(not accept(f, b"\x00\x00\x00\x01")):
        parse_full_overlap_entry(f, reg_size)
    print_table_footer(fo)


def parse_partial_overlap_accessing_instr(f, reg_size):
    global po

    is_uninitialized_read = True if f.read(1) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_inst = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_size = parse_integer(f)
    sp_offset = parse_integer(f)
    bp_offset = parse_integer(f)

    # Emit full overlap table entry
    str_list = []
    str_list.append("*" if is_uninitialized_read else "")
    str_list.append(ip + " (" + actual_ip + "):")
    str_list.append("\t" if len(disassembled_inst) > 0 else " ")
    str_list.append(disassembled_inst)
    str_list.append(" W " if access_type == b"\x1a" else " R ")
    str_list.append(str(access_size) + " B @ ")
    str_list.append("(sp ")
    str_list.append("+ " if sp_offset >= 0 else "- ")
    str_list.append(str(abs(sp_offset)) + "); ")
    str_list.append("(bp ")
    str_list.append("+ " if bp_offset >= 0 else "- ")
    str_list.append(str(abs(bp_offset)) + ")")

    print_table_entry(po, "".join(str_list))


def parse_partial_overlap_entry(f, reg_size):
    global po

    is_uninitialized_read = True if expect(f, [b"\x0a", b"\x0b"]) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_instr = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_size = parse_integer(f)
    mem_type = expect(f, [b'\x1c', b'\x1d'])
    sp_offset = parse_integer(f)
    bp_offset = parse_integer(f)
    is_partial_overlap = accept(f, b"\xab\xcd\xef\xff")
    n = parse_integer(f)
    uninitialized_intervals = deque()
    for _ in range(n):
        lower_bound = parse_integer(f)
        upper_bound = parse_integer(f)
        uninitialized_intervals.append((lower_bound, upper_bound))

    str_list = []
    if is_partial_overlap:
        str_list.append("=> ")
    else:
        str_list.append("   ")

    if is_uninitialized_read:
        str_list.append("*")

    str_list.append(ip + " (" + actual_ip + "):")
    str_list.append("\t" if len(disassembled_instr) > 0 else " ")
    str_list.append(disassembled_instr)
    str_list.append(" W " if access_type == b"\x1a" else " R ")
    str_list.append(str(access_size) + " B ")
    if mem_type == b'\x1c':
        str_list.append("@ (sp ")
        str_list.append("+ " if sp_offset >= 0 else "- ")
        str_list.append(str(abs(sp_offset)))
        str_list.append("); ")
        str_list.append("(bp ")
        str_list.append("+ " if bp_offset >= 0 else "- ")
        str_list.append(str(abs(bp_offset)))
        str_list.append("); ")
    
    while len(uninitialized_intervals) > 0:
        lower_bound, upper_bound = uninitialized_intervals.popleft()
        str_list.append("[" + str(lower_bound) + " ~ " + str(upper_bound) + "]")

    print_table_entry(po, "".join(str_list))
    # print("[LOG]: parsed ", "".join(str_list))


def parse_partial_overlap(f, reg_size):
    global po

    addr = parse_address(f, reg_size)
    access_size = parse_integer(f)
    # Emit table header
    header = addr + " - " + str(access_size)
    print_table_header(po, header)
    while not accept(f, b"\x00\x00\x00\03"):
        parse_partial_overlap_entry(f, reg_size)
    print_table_footer(po)


fo = FullOverlapsWriter()
po = PartialOverlapsWriter()

with open("overlaps.bin", "rb") as f:
    read_bytes = b"\xff"
    while(read_bytes != b"\x00\x00\x00\x00"):
        while(read_bytes != b"\x00"):
            if(read_bytes == b""):
                raise ParseError(f, "Report beginning (byte 0x00) not found")
            read_bytes = f.read(1)
        f.seek(-1, 1)
        read_bytes = f.read(4)
    
    reg_size = parse_integer(f)

    libc_base = parse_address(f, reg_size)
    stack_base = parse_address(f, reg_size)

    fo.writelines([get_fo_legend(), "\n"])

    fo.writelines(["Libc base address: " + libc_base])
    fo.writelines(["Stack base address: " + stack_base, "\n"])

    # While there are full overlaps...
    while not accept(f, b"\x00\x00\x00\x02"):
        parse_full_overlap(f, reg_size)

    # While there are partial overlaps...
    while not accept(f, b"\x00\x00\x00\x04"):
        parse_partial_overlap(f, reg_size)

print("Finished parsing binary file. Textual reports created")
