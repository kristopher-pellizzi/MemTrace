#!/usr/bin/env python3

import functools
import os
import argparse

from functools import reduce
from collections import deque
from typing import Deque, Tuple, Dict, List
from binOverlapParser import remove_useless_writes, parse, ParseError, print_table_header, print_table_footer, PartialOverlapsWriter
from parsedData import MemoryAccess, ParseResult, AccessType, MemType
from instructionAddress import InstructionAddress
from maSet import MASet

def merge_reports(tracer_out_path: str, ignored_addresses = set()):

    def merge_ma_sets(accumulator: Deque[MASet], element: MASet):

        # Returns True if 2 ma_sets are to be considered equal
        def compare_ma_sets(accumulator_el: MASet):
            acc_el_ma_set = accumulator_el.set
            el_ma_set = element.set
            if len(acc_el_ma_set) != len(el_ma_set):
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


    def remove_ignored_addresses(ma_set: Deque[MemoryAccess]) -> Deque[MemoryAccess]:
        ret = deque()

        while len(ma_set) > 0:
            ma = ma_set.popleft()
            if ma.isUninitializedRead and int(ma.actualIp, 16) in ignored_addresses:
                print(ma.actualIp, " ignored")
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


    def findLib(ip_str: str, load_bases: List[Tuple[str, str]]):
        ip = int(ip_str, 16)
        name, addr = max(filter(lambda x: x[1] <= ip, map(lambda x: (x[0], int(x[1], 16)), load_bases)), key = lambda x: x[1])
        return (name, hex(addr))

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

                        if instrAddr in tmp_partial_overlaps:
                            maSet = tmp_partial_overlaps[instrAddr]
                            maSet.append(MASet.create(input_ref, writes))
                        else:
                            newSet = deque()
                            newSet.append(MASet.create(input_ref, writes))
                            tmp_partial_overlaps[instrAddr] = newSet

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
        for ia in partial_overlaps:
            access_set = partial_overlaps[ia]
            new_access_set: Deque[MASet] = deque()

            for maSet in access_set:
                newSet = remove_ignored_addresses(maSet.set)
                if len(newSet) > 0:
                    maSet.replaceMASet(newSet)
                    new_access_set.append(maSet)

            if len(new_access_set) > 0:
                partial_overlaps[ia] = new_access_set
            else:
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

    for ia, access_set in partial_overlaps.items():
        if len(access_set) < 2:
            continue
        header = str(ia)
        print_table_header(po, header)

        for maSet in access_set:
            lines = deque()
            lines.append("Generated by {0} inputs:".format(len(maSet.origins)))
            # lines.extend(map(lambda x: " ".join(["[*]", x]), maSet.origins))
            lines.append(maSet.origins[0])
            lines.append("***********************************************")

            for entry in maSet.set:
                # Print overlap entries 
                lines.append(str(entry))

            lines.append("***********************************************\n")
            po.writelines(lines)
        print_table_footer(po)


def parse_args():

    def parse_hex_addr(hex_str):
        return int(hex_str, 16)

    parser = argparse.ArgumentParser()

    parser.add_argument("tracer_out_path",
        help = "Path of the directory containing the output of the tracer"
    )

    parser.add_argument("-i", "--ignore",
        default = [],
        action = 'append',
        type = parse_hex_addr,
        dest = "ignored_addresses",
        help =  "This option allows to specify addresses of instructions to be ignored during parsing."
                "This option is mainly to be used when the user already verified some of the reported overlaps, and "
                "has selected some not relevant ones. So, it is possible to use this option to re-generate the "
                "report ignoring those instructions."
                "The addresses to be provided, are the actual Instruction Pointer of the uninitialized reads you want to ignore."
                "All the write accesses only related to those reads will be automatically removed."
                "Each option '-i' can be used to specify only 1 address, but the option can be used multiple times "
                "(e.g. './binOverlapParser.py -i 0xdeadbeef -i 0xcafebabe'. "
                "NOTE: addresses must be provided in hexadecimal form."
    )

    return parser.parse_args()


def main():
    args = parse_args()
    ignored_addresses = set(args.ignored_addresses)
    merge_reports(os.path.realpath(args.tracer_out_path), ignored_addresses)
    print("Finished parsing binary file. Textual reports created")


if __name__ == "__main__":
    main()
