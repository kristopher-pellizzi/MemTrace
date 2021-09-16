#!/usr/bin/env python3

import functools
import os
import argparse

from functools import partial, reduce
from collections import deque, defaultdict
from typing import Deque, Tuple, Dict, List, Set
from binOverlapParser import parse, ParseError, print_table_header, print_table_footer, PartialOverlapsWriter
import stringFilter as sf
from parsedData import MemoryAccess, ParseResult, AccessType, MemType
from instructionAddress import InstructionAddress
from maSet import MASet, remove_useless_writes
from libFinder import findLib, findLibByName

class ArgumentError(Exception):
    pass


def merge_reports(tracer_out_path: str, ignored_addresses: Dict[str, Set[int]] = dict(), apply_string_filter: bool = True, report_unique_access_sets = False, ignore_if_no_overlap = True):

    def merge_ma_sets(accumulator: Deque[MASet], element: MASet):

        # Returns True if 2 ma_sets are to be considered equal
        def compare_ma_sets(accumulator_el: MASet):
            acc_el_ma_set = accumulator_el.set
            el_ma_set = element.set
            if len(acc_el_ma_set) != len(el_ma_set) or not accumulator_el.compareMemLocation(element):
                return False
            
            # Get the list of tuples (name, address) of loaded libraries for the accumulator element
            # Note that the accumulator element may have more than 1 origin at this time. However,
            # the reported set comes from the first origin. Sets from other origins may be identical,
            # unless they may have different addresses due to ASLR or different library load address.
            # This means we need to use the load_bases of the first origins to compare offsets.
            acc_el_load_bases = load_bases[accumulator_el.origins[0]]

            res = True
            # We already know el_ma_set and acc_el_ma_set have the same length.
            # Since we are using a deque, it is less expensive to pop and re-append 
            # elements circularly than performing random access.
            for _  in range(len(el_ma_set)):
                el_ma = el_ma_set.popleft()
                acc_el_ma = acc_el_ma_set.popleft()
                el_ma_set.append(el_ma)
                acc_el_ma_set.append(acc_el_ma)

                # If we have already found that the sets are not identical, it is useless to
                # try and keep comparing them. However, we need to complete the whole loop,
                # as we must leave the deques |el_ma_set| and |acc_el_ma_set| untouched.
                if res:
                    el_load_base = int(findLib(el_ma.actualIp, element_load_bases)[1], 16)
                    acc_el_load_base = int(findLib(acc_el_ma.actualIp, acc_el_load_bases)[1], 16)

                    # Compare MemoryAccess object to check ma_sets equality
                    if not el_ma.compare(acc_el_ma, el_load_base, acc_el_load_base):
                        res = False
                
            return res
            

        # |merge_ma_sets| beginning

        # Note that the element comes from |tmp_partial_overlaps|, which has 1 origin for each entry
        element_origin = element.origins[0] 
        element_load_bases = load_bases[element_origin]
        filtered_acc = list(filter(compare_ma_sets, accumulator))

        # If there are no elements with the same ma_set as |element|, insert |element| itself
        if len(filtered_acc) == 0:
            accumulator.append(element)
        # If there's at least 1, we are sure there's exactly one (as a consequence of the applied reduce
        # that uses this function)
        else:
            maSet = filtered_acc[0]
            maSet.addOrigins(element.origins)

        return accumulator


    def remove_ignored_addresses(maSet: MASet) -> Deque[MemoryAccess]:
        ret = deque()
        ma_set = maSet.set
        ignored = 0

        while len(ma_set) > 0:
            ma = ma_set.popleft()

            if ma.isUninitializedRead and len(ignored_addresses) > 0:
                lib_name, base_addr = findLib(ma.actualIp, load_bases)
                if lib_name in ignored_addresses:
                    offset = int(ma.actualIp, 16) - int(base_addr, 16)
                    offset_set = ignored_addresses[lib_name]
                    if offset in offset_set:
                        print("Offset {0} of library {1} ignored".format(hex(offset), lib_name))
                        ignored += 1
                        continue

            ret.append(ma)

        ret = remove_useless_writes(ret)

        return ret


    def remove_masked_writes(writes: Deque[MemoryAccess], readSize: int):
        ret = deque()
        bitMap = list([0] * readSize)
        for _ in range(len(writes)):
            ma = writes.pop()
            lower, upper = ma.uninitializedIntervals[0]
            upper += 1
            portion = bitMap[lower:upper]
            size = upper - lower
            if reduce(lambda acc, el: acc + el, portion, 0) < size:
                bitMap[lower:upper] = [1] * size
                ret.appendleft(ma)
            
        return ret


    load_bases: Dict[str, List[Tuple[str, str]]] = dict()
    stack_bases: Dict[str, str] = dict()
    partial_overlaps: Dict[InstructionAddress, Deque[MASet]] = dict()

    # Although |tmp_partial_overlaps| has the same type of |partial_overlaps|, this is thought to contain MASets
    # containing, in turns, only 1 input reference (origin). Afterwards, this will be processed to merge all MASets having the same set of 
    # MemoryAccess objects into 1 single MASet containing all the identical origins.
    tmp_partial_overlaps: Dict[InstructionAddress, Deque[MASet]] = dict()
    for instance in os.listdir(tracer_out_path):
        instance_path = os.path.join(tracer_out_path, instance)
        for input_dir in os.listdir(instance_path):
            input_dir_path = os.path.join(instance_path, input_dir)
            
            # If the binary report is not found, the tracer failed to be run (random argv generation may cause execve to fail)
            try:
                print("Parsing binary report from {0}".format(input_dir_path))
                parse_res: ParseResult = parse(bin_report_dir = input_dir_path)
                if apply_string_filter:
                    print("Applying filter")
                    parse_res = sf.apply_filter(parse_res)
            except FileNotFoundError:
                print("Binary report not found in {0}".format(input_dir_path))
                continue
            except ParseError as e:
                print("ParseError raised for {0}".format(input_dir_path))
                print(str(e))
                continue
            cur_load_bases = parse_res.load_bases
            cur_stack_base = parse_res.stack_base
            overlaps = parse_res.partial_overlaps
            input_ref = os.path.join(instance, input_dir)

            # Fill partial_overlaps, load_bases and stack_bases
            for ia, ma_set in overlaps:
                writes: Deque[MemoryAccess] = deque()
                last_read = None
                for _ in range(len(ma_set)):
                    ma = ma_set.popleft()
                    if ma.accessType == AccessType.WRITE:
                        writes.append(ma)
                    else:
                        if last_read is not None:
                            writes = remove_masked_writes(writes, ma.accessSize)
                        last_read = ma
                        libName, baseAddr = findLib(ma.actualIp, cur_load_bases)
                        instrAddr = InstructionAddress(ma.ip, ma.actualIp, libName, baseAddr)
                        # Temporarily append the read access to the deque (cost O(1))
                        writes.append(ma)

                        newSet = MASet.create(input_ref, writes)
                        mem_base = None
                        if ma.memType == MemType.STACK:
                            mem_base = int(cur_stack_base, 16)
                        else:
                            mem_base = int(findLibByName("Heap", cur_load_bases), 16)
                        newSet.setMemLocation(ma.accessedAddress, ma.memType, mem_base)
                        
                        if instrAddr in tmp_partial_overlaps:
                            maSet = tmp_partial_overlaps[instrAddr]
                            maSet.append(newSet)
                        else:
                            newMASets = deque()
                            newMASets.append(newSet)
                            tmp_partial_overlaps[instrAddr] = newMASets

                        # Remove the read access from |writes| (cost O(1))
                        writes.pop()

            load_bases.update([(input_ref, cur_load_bases)])
            stack_bases.update([(input_ref, cur_stack_base)])

    # Sort partial_overlaps by AccessIndex (increasing address, decreasing access size)
    # sorted_partial_overlaps = sorted(tmp_partial_overlaps.items(), key=lambda x: x[0])

    print("Starting merging sets...")

    for ia, access_set in tmp_partial_overlaps.items():
        initial = partial_overlaps[ia] if ia in partial_overlaps else deque()
        ma_sets = reduce(merge_ma_sets, access_set, initial)
        partial_overlaps[ia] = ma_sets

    if len(ignored_addresses) > 0:
        print("Removing ignored addresses...")

        # Remove ignored addresses and related writes
        ia_to_ignore: Set[InstructionAddress] = set()
        for ia in partial_overlaps:
            if ia.libName in ignored_addresses:
                ignored_set = ignored_addresses[ia.libName]
                offset = ia.actualIp - ia.baseAddr
                if offset in ignored_set:
                    print("Offset {0} of library {1} ignored".format(hex(offset), ia.libName))
                    ia_to_ignore.add(ia)

        for ia in ia_to_ignore:
            del partial_overlaps[ia]

    # Generate textual report files: 1 with only the accesses, 1 with address bases
    print("Generating textual report...")
    # Generate base addresses report
    with open("base_addresses.log", "w") as f:
        for input_ref in load_bases:
            imgs = load_bases[input_ref]
            stack = stack_bases[input_ref]

            f.write("===============================================\n")
            f.write(input_ref)
            f.write("\n===============================================\n")
            for img_name, img_addr in imgs:
                f.write("{0} base address: {1}\n".format(img_name, img_addr))
            
            f.write("Stack base address: {0}\n".format(stack))
            f.write("===============================================\n")
            f.write("===============================================\n\n")

    # Generate partial overlaps textual report

    po = PartialOverlapsWriter()

    if len(partial_overlaps) == 0:
        po.writelines(["** NO OVERLAPS DETECTED **"])
        return
    
    # It's possible that the partial overlaps set is not empty, but the tool discards all overlaps with a unique
    # write access sets. In that case, we use the following boolean flag to check if we write at least 1 table in the report
    isEmpty = True
    for ia, access_set in partial_overlaps.items():
        if not report_unique_access_sets and len(access_set) < 2:
            continue
        isEmpty = False
        header = str(ia)
        print_table_header(po, header)

        for maSet in access_set:
            lines = deque()
            lines.append("Generated by {0} inputs:".format(len(maSet.origins)))
            # lines.extend(map(lambda x: " ".join(["[*]", x]), maSet.origins))
            lines.append(maSet.origins[0])
            memType = "stack" if maSet.memType == MemType.STACK else "heap"
            offset_sign = "+" if maSet.memOffset >= 0 else "-"
            lines.append("Reads {0} bytes from <{1}_base> {2} {3}".format(maSet.size, memType, offset_sign, abs(maSet.memOffset)))
            lines.append("***********************************************")

            for entry in maSet.set:
                # Print overlap entries 
                lines.append(str(entry))

            lines.append("***********************************************\n")
            po.writelines(lines)
        print_table_footer(po)

    if isEmpty:
        po.writelines(["** NO OVERLAPS DETECTED **"])


def parse_args():

    lib = None
    ignored_addresses: Dict[str, Set[int]] = defaultdict(set)

    def parse_lib(lib_name):
        nonlocal lib

        lib = lib_name

    def parse_addr(addr_str):

        if lib is None:
            raise ArgumentError("No library specified before option --ignore")

        toAdd = None

        if '-' in addr_str:
            operands = list(map(lambda x: x.strip(), addr_str.split('-')))
            # If there are less or more than 2 operands, or any operand has a length of 0
            if len(operands) != 2 or len(operands[0]) == 0 or len(operands[1]) == 0:
                raise ArgumentError("2 operands expected, 1 given")
            
            # Note that we are sure len(operands) is 2 at this point
            for i in range(2):
                try:
                    operands[i] = int(operands[i], 0)
                except ValueError:
                    raise ArgumentError("Not valid operand {0}".format(operands[i]))

            toAdd = operands[0] - operands[1]
        else:
            try:
                toAdd = int(addr_str, 0)
            except ValueError:
                raise ArgumentError("Not valid address: {0}".format(addr_str))

        ignored_addresses[lib].add(toAdd)

    parser = argparse.ArgumentParser()

    parser.add_argument("tracer_out_path",
        help = "Path of the directory containing the output of the tracer"
    )

    parser.add_argument("--disable-string-filter",
        action = "store_true",
        help =  "This flag allows to disable the filter which removes all the uninitialized read accesses "
                "which come from a string function (e.g. strcpy, strcmp...). This filter has been designed because "
                "string functions are optimized, and because of that, they very often read uninitialized bytes, but "
                "those uninitialized reads are not relevant. An heuristic is already used to try and reduce this kind of "
                "false positives. However, when we use the fuzzer and merge all the results, the final report may still "
                "contain a lot of them.",
        dest = "disable_string_filter"
    )

    parser.add_argument("--into",
        type = parse_lib,
        help =  "Path of the library from which the next addresses passed to the option --ignore are taken from. "
                "This paths can be found inside 'partialOverlaps.log'. "
                "Note that the order of options --into and --ignore is important: every address passed to --ignore is considered "
                "to be taken from the last library passed to argument --into.",
        dest = "lib"
    )

    parser.add_argument("-i", "--ignore",
        type = parse_addr,
        help =  "This option allows to specify addresses of instructions to be ignored during parser."
                "The main usage of this option is that the user already verified some of the reported overlaps, and "
                "has selected some not relevant ones. So, it is possible to use this option to re-generate the "
                "report ignoring those instructions."
                "The addresses to be provided, are the offsets from the corresponding library's beginning of the uninitialized reads " 
                "you want to ignore. "
                "All the write accesses only related to those reads will be automatically removed."
                "Each option '-i' can be used to specify only 1 address, but the option can be used multiple times "
                "(e.g. './binOverlapParser.py -i 0xdeadbeef -i 0xcafebabe'. "
                "Note that the order of options --into and --ignore is important: every address passed to --ignore is considered "
                "to be taken from the last library passed to argument --into. "
                "If no argument --into is passed before --ignore is found, an error is raised. "
                "It is possible to specify addresses to be ignored from multiple libraries. For instance: "
                "'.../binOverlapParser.py --into path/to/libc --ignore 0xdeadbeef --ignore 0xbadc0c0a --into path/to/another/library --ignore 0xcafebabe'. "
                "This command will ignore addresses 0xdeadbeef and 0xbadc0c0a from libc and address 0xcafebabe from the other library. "
                "NOTE: you can pass the addresses in 2 ways: either directly pass the offset from library's beginning, or pass an address subtraction "
                "where the first operand is the actualIp of the instruction, and the second is the library base address (e.g. '0xdeadbeef - 0xcafebabe')",
        dest = 'ignored_addresses'
    )

    parser.add_argument("--unique-access-sets", "-q",
        action = "store_true",
        help =  "This flag is used to report also those uninitialized read accesses which have a unique access set. "
                "Indeed, by default, the tool only reports read accesses that have more access sets, which are those that can read different bytes "
                "according to the input."
                "This behaviour is thought to report only those uninitialized reads that are more likely to be interesting. "
                "By enabling this flag, even the uninitialized read accesses that always read from the same set of write accesses are reported in the merged report.",
        dest = "unique_access_sets"
    )

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

    ret = parser.parse_args()
    ret.ignored_addresses = ignored_addresses

    return ret


def main():
    args = parse_args()
    ignored_addresses: Dict[str, Set[int]] = args.ignored_addresses
    apply_string_filter = not args.disable_string_filter
    report_unique_access_sets = args.unique_access_sets
    ignore_if_no_overlap = args.ignore_if_no_overlap
    merge_reports(os.path.realpath(args.tracer_out_path), ignored_addresses, apply_string_filter, report_unique_access_sets, ignore_if_no_overlap)
    print("Finished parsing binary file. Textual reports created")


if __name__ == "__main__":
    main()
