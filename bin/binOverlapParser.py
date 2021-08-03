#!/usr/bin/env python3

# Add parent folder in sys.path to allow using modules
import sys
import argparse as ap
import os
sys.path.append(os.path.join(os.path.dirname(sys.path[0]), "python_modules"))

import stringFilter as sf

from collections import deque
from binascii import b2a_hex
from functools import reduce

from parsedData import * 
from maSet import remove_useless_writes, check_for_overlapping_writes

class ParseError(Exception):
    def __init__(self, file, message="Error while parsing"):
        super().__init__(message)
        print("Error @ byte " + str(file.tell()))
        try:
            file.seek(-6, 1)
            print(b"... " + file.read(50))
        except:
            file.seek(0,0)
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


def parse_full_overlap_entry(f, reg_size, exec_order):    
    is_uninitialized_read = True if f.read(1) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_inst = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_type = AccessType.WRITE if access_type == b"\x1a" else AccessType.READ
    access_size = parse_integer(f)
    mem_type = expect(f, [b'\x1c', b'\x1d'])
    mem_type = MemType.STACK if mem_type == b"\x1c" else MemType.HEAP
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

    ma = MemoryAccess(exec_order, ip, actual_ip, sp_offset, bp_offset, access_type, access_size, disassembled_inst, is_uninitialized_read, uninitialized_intervals, mem_type, False)
    return ma


def parse_full_overlap(f, reg_size, ignore_if_no_overlapping_write):
    overlaps = deque()
    exec_order = 0

    addr = parse_address(f, reg_size)
    access_size = parse_integer(f)

    ai = AccessIndex(addr, access_size)
    
    while(not accept(f, b"\x00\x00\x00\x01")):
        entry = parse_full_overlap_entry(f, reg_size, exec_order)
        overlaps.append(entry)
        exec_order += 1

    if len(overlaps) > 0:
        return (ai, overlaps)
    
    return None


def parse_partial_overlap_entry(f, reg_size, exec_order):
    is_uninitialized_read = True if expect(f, [b"\x0a", b"\x0b"]) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_instr = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_type = AccessType.WRITE if access_type == b"\x1a" else AccessType.READ
    access_size = parse_integer(f)
    mem_type = expect(f, [b'\x1c', b'\x1d'])
    mem_type = MemType.STACK if mem_type == b"\x1c" else MemType.HEAP
    sp_offset = parse_integer(f)
    bp_offset = parse_integer(f)
    is_partial_overlap = accept(f, b"\xab\xcd\xef\xff")
    n = parse_integer(f)
    uninitialized_intervals = deque()
    for _ in range(n):
        lower_bound = parse_integer(f)
        upper_bound = parse_integer(f)
        uninitialized_intervals.append((lower_bound, upper_bound))

    ma = MemoryAccess(exec_order, ip, actual_ip, sp_offset, bp_offset, access_type, access_size, disassembled_instr, is_uninitialized_read, uninitialized_intervals, mem_type, is_partial_overlap)
    return ma


def parse_partial_overlap(f, reg_size, ignore_if_no_overlapping_write, ignored_addresses):
    exec_order = 0
    ignored = 0
    overlaps = deque()

    addr = parse_address(f, reg_size)
    access_size = parse_integer(f)

    ai = AccessIndex(addr, access_size)

    while not accept(f, b"\x00\x00\x00\03"):
        entry = parse_partial_overlap_entry(f, reg_size, exec_order)
        if entry.isUninitializedRead:
            if ignore_if_no_overlapping_write:
                has_overlapping_writes = check_for_overlapping_writes(overlaps, entry)
                if not has_overlapping_writes:
                    continue

            if int(entry.actualIp, 16) in ignored_addresses:
                print(entry.actualIp, " ignored")
                ignored += 1
                continue

        overlaps.append(entry)
        exec_order += 1

    if ignore_if_no_overlapping_write or ignored > 0:
        overlaps = remove_useless_writes(overlaps)
    
    if len(overlaps) > 0:
        return (ai, overlaps)
    
    return None


def parse(ignore_if_no_overlapping_write: bool = True, bin_report_dir = ".", ignored_addresses = set())->ParseResult:
    ret = ParseResult()

    bin_report_path = os.path.join(bin_report_dir, "overlaps.bin")
    with open(bin_report_path, "rb") as f:
        read_bytes = b"\xff"
        while(read_bytes != b"\x00\x00\x00\x00"):
            while(read_bytes != b"\x00"):
                if(read_bytes == b""):
                    raise ParseError(f, "Report beginning (byte 0x00) not found")
                read_bytes = f.read(1)
            f.seek(-1, 1)
            read_bytes = f.read(4)

            if len(read_bytes) < 4:
                raise ParseError(f, "Report beginning (sequence \\x00\\x00\\x00\\x00) not found")
            # If this is True, the first byte is indeed a \x00, but the sequence is not correct.
            # Starting from the second byte of the sequence, try again to find \x00\x00\x00\x00
            if read_bytes != b"\x00\x00\x00\x00":
                f.seek(-3, 1)
        
        reg_size = parse_integer(f)

        load_bases = set()
        while not accept(f, b"\x00\x00\x00\x05"):
            load_bases.add((parse_string(f), parse_address(f, reg_size)))
        stack_base = parse_address(f, reg_size)

        # Order images base addresses in increasing order of their load address
        load_bases = list(load_bases)
        load_bases.sort(key = lambda x: x[1])

        ret.load_bases = load_bases
        ret.stack_base = stack_base

        # While there are full overlaps...
        while not accept(f, b"\x00\x00\x00\x02"):
            overlaps = parse_full_overlap(f, reg_size, ignore_if_no_overlapping_write)
            if overlaps is not None:
                ret.full_overlaps.append(overlaps)

        # While there are partial overlaps...
        while not accept(f, b"\x00\x00\x00\x04"):
            overlaps = parse_partial_overlap(f, reg_size, ignore_if_no_overlapping_write, ignored_addresses)
            if overlaps is not None:
                ret.partial_overlaps.append(overlaps)

        return ret


def parse_args():

    def parse_hex_addr(hex_str):
        return int(hex_str, 16)

    parser = ap.ArgumentParser()

    parser.add_argument('-a', '--all',
        action = "store_false",
        help =  "If this flag is enabled, then the parser will return all the memory accesses, avoiding discarding " 
                "those uninitialized read accesses that don't have any write overlapping the uninitialized interval. "
                "By default, this is disabled, so that the tool only reports actual memory overlaps. "
                "NOTE: with this flag enabled, most of the uninitialized reads performed "
                "by the program are reported. The tool, however, is not a memory sanitizer. The uninitialized reads "
                "reported by the tool, are all the instructions loading from memory at least 1 byte that has not been "
                "initialized. During program execution, this might happen many times (e.g. string operations almost "
                "always read something beyond the initialized string), so reintroducing all the uninitialized reads "
                "may generate huge reports.",
        dest = "ignore_if_no_overlap"
    )

    parser.add_argument("-d", "--directory",
        default = ".",
        help = "Path of the directory containing the binary report file",
        dest = "bin_report_dir"
    )

    parser.add_argument("-i", "--ignore",
        default = [],
        action = 'append',
        type = parse_hex_addr,
        help =  "This option allows to specify addresses of instructions to be ignored during parser."
                "The main usage of this option is that the user already verified some of the reported overlaps, and "
                "has selected some not relevant ones. So, it is possible to use this option to re-generate the "
                "report ignoring those instructions."
                "The addresses to be provided, are the actual Instruction Pointer of the uninitialized reads you want to ignore."
                "All the write accesses only related to those reads will be automatically removed."
                "Each option '-i' can be used to specify only 1 address, but the option can be used multiple times "
                "(e.g. './binOverlapParser.py -i 0xdeadbeef -i 0xcafebabe'. "
                "NOTE: addresses must be provided in hexadecimal form.",
        dest = 'ignored_addresses'
    )

    return parser.parse_args()


def main():
    args = parse_args()

    bin_report_dir = args.bin_report_dir
    ignore_if_no_overlapping_write = args.ignore_if_no_overlap
    ignored_addresses = set(args.ignored_addresses)
    fo = FullOverlapsWriter()
    po = PartialOverlapsWriter()


    parse_res = parse(ignore_if_no_overlapping_write, bin_report_dir, ignored_addresses)
    print("Applying filter...")
    parse_res = sf.apply_filter(parse_res)
    print("Filter applied")

    is_full_overlaps_empty = len(parse_res.full_overlaps) == 0
    is_partial_overlaps_empty = len(parse_res.partial_overlaps) == 0

    if not is_full_overlaps_empty:
        fo.writelines([get_fo_legend(), "\n"])
        fo.writelines(["LOAD ADDRESSES:"])

        for (name, addr) in parse_res.load_bases:
            fo.writelines([name + " base address: " + addr])
        fo.writelines(["Stack base address: " + parse_res.stack_base, "\n"])
    else:
        fo.writelines(["** NO OVERLAPS DETECTED **"])
    
    if not is_partial_overlaps_empty:
        po.writelines(["LOAD ADDRESSES:"])

        for (name, addr) in parse_res.load_bases:
            po.writelines([name + " base address: " + addr])

        po.writelines(["Stack base address: " + parse_res.stack_base, "\n"])
    else:
        po.writelines(["** NO OVERLAPS DETECTED **"])

    # Print textual report

    # Print Full Overlaps
    for ai, ma_set in parse_res.full_overlaps:
        header = ai.address + " - " + str(ai.size)
        print_table_header(fo, header)

        for entry in ma_set:
            # Emit full overlap table entry
            print_table_entry(fo, str(entry))

        print_table_footer(fo)


    # Print Partial Overlaps
    # Emit PO table header
    for ai, ma_set in parse_res.partial_overlaps:
        header = ai.address + " - " + str(ai.size)
        print_table_header(po, header)

        for entry in ma_set:
            # Print overlap entries
            str_list = []
            if entry.isPartialOverlap:
                str_list.append("=> ")
            else:
                str_list.append("   ")

            if entry.isUninitializedRead:
                str_list.append("*")

            str_list.append(entry.ip + " (" + entry.actualIp + "):")
            str_list.append("\t" if len(entry.disasm) > 0 else " ")
            str_list.append(entry.disasm)
            str_list.append(" W " if entry.accessType == AccessType.WRITE else " R ")
            str_list.append(str(entry.accessSize) + " B ")
            if entry.memType == MemType.STACK:
                str_list.append("@ (sp ")
                str_list.append("+ " if entry.spOffset >= 0 else "- ")
                str_list.append(str(abs(entry.spOffset)))
                str_list.append("); ")
                str_list.append("(bp ")
                str_list.append("+ " if entry.bpOffset >= 0 else "- ")
                str_list.append(str(abs(entry.bpOffset)))
                str_list.append("); ")
            
            while len(entry.uninitializedIntervals) > 0:
                lower_bound, upper_bound = entry.uninitializedIntervals.popleft()
                str_list.append("[" + str(lower_bound) + " ~ " + str(upper_bound) + "]")

            print_table_entry(po, "".join(str_list))
            # print("[LOG]: parsed ", "".join(str_list))

        print_table_footer(po)

    print("Finished parsing binary file. Textual reports created")
    

if __name__ == "__main__":
    main()
