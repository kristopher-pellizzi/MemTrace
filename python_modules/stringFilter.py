from parsedData import ParseResult, MemoryAccess, AccessIndex, AccessType
from libFinder import findLib
from collections import deque
from typing import Deque, Set
import subprocess as subp
from maSet import remove_useless_writes
import sys
import os

# This set will contain the offsets that have already been checked to belong to a string function
# and as such should be removed from the report. It is used to avoid executing a subprocess if the 
# offset is the same as a previously checked one.
already_removed: Set[int] = set()

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


def apply_filter(parsedAccesses: ParseResult) -> ParseResult:
    load_bases = parsedAccesses.load_bases
    stack_base = parsedAccesses.stack_base
    partial_overlaps = parsedAccesses.partial_overlaps
    function_finder_path = os.path.realpath(os.path.join(sys.path[0], "..", "python_modules", "libcFunctionFinder.py"))

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
                        print("String operation function was already detected")
                        continue

                    offset = hex(offset)
                    debug_path = find_debug_path(name)
                    p = subp.Popen(["gdb", "-q", "-x", function_finder_path], stdin = subp.PIPE, stdout = subp.PIPE, stderr = subp.STDOUT)
                    out, err = p.communicate(b"".join([debug_path.encode("utf-8"), b"\n", offset.encode("utf-8"), b"\n"]))
                    p.wait()
                    func_name = out.decode("utf-8").split("\n")[-2]
                    if "str" in func_name.lower():
                        print("String operation function found: ", func_name)
                        already_removed.add(int(offset, 16))
                    else:
                        print("Function name: ", func_name)
                        new_ma_set.append(ma)
                else:
                    new_ma_set.append(ma)

        new_ma_set = remove_useless_writes(new_ma_set)
        if(len(new_ma_set) > 0):
            overlap = (ai, new_ma_set)
            partial_overlaps.append(overlap)

    return parsedAccesses


if __name__ == "__main__":
    function_finder_path = os.path.join(sys.path[0], "libcFunctionFinder.py")
    p = subp.Popen(["gdb", "-q", "-x", function_finder_path], stdin = subp.PIPE, stdout = subp.PIPE, stderr = subp.STDOUT)
    out, err = p.communicate(b"".join([b"/lib/debug/lib/x86_64-linux-gnu/libc-2.31.so", b"\n", b"0x186f05", b"\n"]))
    p.wait()
    func_name = out.decode("utf-8").split("\n")[-2]
    print("Instruction @ offset 0x186f05 is part of function ", func_name)

