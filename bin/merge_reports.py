#!/usr/bin/env python3

import os
import argparse
import sys

from functools import reduce
from collections import deque
from typing import Deque, Tuple
from binOverlapParser import *
from parsedData import *

def merge_reports(tracer_out_path: str, ignored_addresses = set()):
    def merge_ma_sets(accumulator: Deque[Tuple[Deque[str], Deque[MemoryAccess]]], element: Tuple[str, Deque[MemoryAccess]]):
        filtered_acc = list(filter(lambda x: x[1] == element[1], accumulator))

        # If there are no elements with the same ma_set as |element|
        if len(filtered_acc) == 0:
            refs = deque()
            refs.append(element[0])
            accumulator.append((refs, element[1]))
        # If there's at least 1, we are sure there's exactly one (as a consequence of the applied reduce
        # that uses this function)
        else:
            tup = filtered_acc[0]
            tup[0].append(element[0])

        return accumulator


    def remove_ignored_addresses(ma_set: Deque[MemoryAccess]) -> Deque[MemoryAccess]:
        ret = deque()

        for ma in ma_set:
            if ma.isUninitializedRead and int(ma.actualIp, 16) in ignored_addresses:
                print(ma.actualIp, " ignored")
                continue

            ret.append(ma)

        ret = remove_useless_writes(ret)

        return ret

    load_bases: Deque[Tuple[str, List[Tuple[str, str]]]] = deque()
    stack_bases: Deque[Tuple[str, str]] = deque()
    partial_overlaps: Dict[AccessIndex, Deque[Tuple[Deque[str], Deque[MemoryAccess]]]] = dict()

    tmp_partial_overlaps: Dict[AccessIndex, Deque[Tuple[str, Deque[MemoryAccess]]]] = dict()
    for instance in os.listdir(tracer_out_path):
        instance_path = os.path.join(tracer_out_path, instance)
        for input_dir in os.listdir(instance_path):
            input_dir_path = os.path.join(instance_path, input_dir)
            
            # If the binary report is not found, the tracer failed to be run (random argv generation may cause execve to fail)
            try:
                parse_res: ParseResult = parse(bin_report_dir = input_dir_path)
            except FileNotFoundError:
                continue
            cur_load_bases = parse_res.load_bases
            cur_stack_base = parse_res.stack_base
            overlaps = parse_res.partial_overlaps
            input_ref = os.path.join(instance, input_dir)

            # Fill partial_overlaps, load_bases and stack_bases
            for ai, ma_set in overlaps:
                if ai not in tmp_partial_overlaps:
                    tmp_partial_overlaps[ai] = deque()

                tmp_partial_overlaps[ai].append((input_ref, ma_set))

            load_bases.append((input_ref, cur_load_bases))
            stack_bases.append((input_ref, cur_stack_base))

    # Sort partial_overlaps by AccessIndex (increasing address, decreasing access size)
    sorted_partial_overlaps = sorted(tmp_partial_overlaps.items(), key=lambda x: x[0])

    # Try to merge those (inputRef, MemoryAccess) tuples whose memory access set is the same
    for ai, access_set in sorted_partial_overlaps:
        initial = partial_overlaps[ai] if ai in partial_overlaps else deque()
        ma_sets = reduce(merge_ma_sets, access_set, initial)
        partial_overlaps[ai] = ma_sets

    # Remove ignored addresses and related writes
    for ai in partial_overlaps:
        access_set = partial_overlaps[ai]
        new_access_set: Deque[Tuple[Deque[str], Deque[MemoryAccess]]] = deque()

        for instances, ma_set in access_set:
            ma_set = remove_ignored_addresses(ma_set)
            if len(ma_set) > 0:
                new_access_set.append((instances, ma_set))

        if len(new_access_set) > 0:
            partial_overlaps[ai] = new_access_set
        else:
            del partial_overlaps[ai]


    # Generate textual report files: 1 with only the accesses, 1 with address bases

    # Generate base addresses report
    with open("base_addresses.log", "w") as f:
        while(len(load_bases) != 0):
            imgs = load_bases.popleft()
            stack = stack_bases.popleft()

            input_ref, img_list = imgs
            f.write("===============================================\n")
            f.write(input_ref)
            f.write("\n===============================================\n")
            for img_name, img_addr in img_list:
                f.write("{0} base address: {1}\n".format(img_name, img_addr))
            
            f.write("Stack base address: {0}\n".format(stack[1]))
            f.write("===============================================\n")
            f.write("===============================================\n\n")

    # Generate partial overlaps textual report

    po = PartialOverlapsWriter()

    if len(partial_overlaps) == 0:
        po.writelines(["** NO OVERLAPS DETECTED **"])
        return

    for ai, access_set in partial_overlaps.items():
        header = " ".join([str(ai.address), "-", str(ai.size)])
        print_table_header(po, header)

        for input_refs, ma_set in access_set:
            lines = deque()
            lines.append("Generated by inputs:")
            lines.extend(map(lambda x: " ".join(["[*]", x]), input_refs))
            lines.append("***********************************************")

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

                lines.append("".join(str_list))

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


if __name__ == "__main__":
    main()