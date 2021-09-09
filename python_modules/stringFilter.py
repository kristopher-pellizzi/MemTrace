from parsedData import ParseResult, MemoryAccess, AccessIndex, AccessType
from libFinder import findLib
from collections import deque
from typing import Deque, Set
import subprocess as subp
from maSet import remove_useless_writes
import sys
import os
from elftools.elf.elffile import ELFFile

# This set will contain the offsets that have already been checked to belong to a string function
# and as such should be removed from the report. It is used to avoid executing a subprocess if the 
# offset is the same as a previously checked one.
already_removed: Set[int] = set()

# This set will contain the offsets that have already been checked NOT TO belong to a string function
already_checked: Set[int] = set()

debug_folders = {"/usr/lib/debug", "/lib/debug", "/usr/bin/.debug"}

detected_debug_file = None

def find_debug_path(name, feellucky = False):
    '''
    Searches for the debug symbols file of libc library inside the commonly used folders.
    Returns the detected debug symbols file, if any, or the path passed as argument.
    Since there may be many locations for the same debug symbols file, or many files containing
    "libc" as part of their name, this is thought to be an interactive function (requires user's interaction).
    However, it is possible to set the flag |feellucky| to disable interactive mode, and accept the first library
    detected. Note that in this way, it is possible that the wrong library is selected.
    '''
    global detected_debug_file

    if detected_debug_file is None:
        dirname = os.path.dirname(name)[1:]
        for path in debug_folders:
            if not os.path.exists(path):
                continue

            full_path = os.path.join(path, dirname)
            if not os.path.exists(full_path):
                continue

            libs = []
            for lib_name in os.listdir(full_path):
                if 'libc' in lib_name:
                    libs.append(lib_name)

            libs_num = len(libs)

            if libs_num == 0:
                continue
            elif libs_num == 1:
                debug_path = os.path.join(full_path, libs[0])
                print("Debug file detected: ", debug_path)
                valid_choice = False
                selected = False
                while not valid_choice:
                    res = input("Use this file? [y/n]: ")
                    if res.lower() == "y":
                        detected_debug_file = debug_path
                        print(debug_path, " selected")
                        selected = True
                        valid_choice = True
                    elif res.lower() == "n":
                        valid_choice = True
                        
                if selected:
                    break
            else:
                print("Many libraries found with 'libc' as part of its name:")
                for i in range(libs_num):
                    debug_path = os.path.join(full_path, libs[i])
                    print("[", i, "] ", debug_path)
                print("[", libs_num, "] Cancel")
                
                print()
                valid_choice = False
                selected = False
                while not valid_choice:
                    res = input("Please select a library, or 'cancel' to search in the other folders: ")
                    try:
                        int_res = int(res, 10)
                    except ValueError as e:
                        print("Invalid input: ", e)
                        continue
                    if int_res < libs_num:
                        detected_debug_file = os.path.join(full_path, libs[int_res])
                        print(detected_debug_file, " selected")
                        selected = True
                        valid_choice = True
                    elif int_res == libs_num:
                        valid_choice = True

                if selected:
                    break

        if detected_debug_file is None:
            detected_debug_file = name
            print("No debug symbols file found. Using library's path: ", name)

    return detected_debug_file


def find_function_name(elf_path: str, address: int):
    binary = open(elf_path, "rb")
    elf = ELFFile(binary)

    symbol_table = elf.get_section_by_name(".symtab")
    if symbol_table is None:
        return None

    symbols = symbol_table.iter_symbols()

    for symbol in symbols:
        symbol_info = symbol['st_info']
        if symbol_info['type'] == 'STT_FUNC':
            start_addr = symbol['st_value']
            end_addr = start_addr + symbol['st_size'] - 1
            if start_addr <= address <= end_addr:
                return symbol.name
               
    return None



def apply_filter(parsedAccesses: ParseResult) -> ParseResult:
    load_bases = parsedAccesses.load_bases
    stack_base = parsedAccesses.stack_base
    partial_overlaps = parsedAccesses.partial_overlaps

    for _ in range(len(partial_overlaps)):
        overlap = partial_overlaps.popleft()
        new_ma_set: Deque[MemoryAccess] = deque()
        ai, ma_set = overlap

        for ma in ma_set:
            if ma.accessType == AccessType.WRITE:
                new_ma_set.append(ma)
            else:
                name, addr = findLib(ma.actualIp, load_bases)
                if 'libc' in name:
                    offset = int(ma.actualIp, 16) - int(addr, 16)
                    if offset in already_removed:
                        continue
                    elif offset in already_checked:
                        new_ma_set.append(ma)
                        continue

                    debug_path = find_debug_path(name)
                    
                    func_name = find_function_name(debug_path, offset)
                    
                    if func_name is None:
                        print("No function found for address ", hex(offset))
                        new_ma_set.append(ma)
                        already_checked.add(offset)
                        continue

                    func_name = func_name.lower()
                    # Cannot find stpcpy or similar functions in the list of functions in string.h header
                    # This assumes all string related functions contain 'str' or is stpcpy
                    if "str" in func_name or 'stpcpy' in func_name:
                        print("String operation function found: ", func_name)
                        already_removed.add(offset)
                    else:
                        new_ma_set.append(ma)
                        already_checked.add(offset)
                else:
                    new_ma_set.append(ma)

        new_ma_set = remove_useless_writes(new_ma_set)
        if(len(new_ma_set) > 0):
            overlap = (ai, new_ma_set)
            partial_overlaps.append(overlap)

    return parsedAccesses


def parse_args():
    def parse_address(arg: str) -> int:
        return int(arg, 16)

    parser = ap.ArgumentParser()

    parser.add_argument("elf_path",
        help = "Path of the elf to be analyzed",
    )

    parser.add_argument("address", 
        help = "Address whose function name is required",
        type = parse_address
    )

    return parser.parse_args()


if __name__ == "__main__":
    import argparse as ap
    args = parse_args()
    elf_path = args.elf_path
    address = args.address
    func_name = find_function_name(elf_path, address)

    if func_name is None:
        print("No function can be found for address {0} in {1}".format(hex(address), elf_path))
    else:
        print("Instruction @ offset {0} in {1} is part of function {2}".format(hex(address), elf_path, func_name))

