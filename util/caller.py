#! /usr/bin/env python3

import subprocess as subp
import os
import argparse
import sys

def parse_args():

    def parse_heuristic_status(s: str):
        return s.upper()

    parser = argparse.ArgumentParser()

    parser.add_argument("executable_path",
        help = "Path of the executable to be run"
    )
    parser.add_argument("testcase_path",
        help = "Path of the directory containing both the input file and the arguments file"
    )

    parser.add_argument("--str-opt-heuristic", "-u",
        default = "LIBS",
        help =  "Option used to specify whether to enable the heuristic thought to remove the high number of not "
                "relevant uninitialized reads due to the optimized versions of string operations (e.g. strcpy).\n"
                "The aforementioned heuristic can be either disabled, enabled or partially enabled.\n"
                "If the heuristic is disabled, it will never be applied.\n"
                "If it is enabled, it will be always applied.\n"
                "If it is partially enabled, it is applied only for accesses performed by code from libraries (e.g. libc).\n"
                "By default, the heuristic is partially enabled.\n"
                "Possible choices are:\n"
                "OFF => Disabled,\n"
                "\tON => Enabled,\n"
                "\tLIBS => Partially enabled.\n"
                "WARNING: when the heuristic is enabled, many false negatives may arise. If you want to avoid false negatives "
                "due to the application of the heuristic, you can simply disable it, but at a cost. "
                "Indeed, in a single program there may be many string operations, and almost all of them will generate "
                "uninitialized reads due to the optimizations (e.g. strcpy usually loads 32 bytes at a time, but then checks "
                "are performed on the loaded bytes to avoid copying junk).\n"
                "So, the execution of the program with the heuristic disabled may generate a huge number of uninitialized reads.\n"
                "Those are not false positives (those reads actually load uninitialized bytes from memory to registers), but they "
                "may be not relevant.\n"
                "Example: strcpy(dest, src), where |src| is a 4 bytes string, may load 32 bytes from memory to SIMD registers. Then, "
                "after some checks performed on the loaded bytes, only the 4 bytes belonging to |src| are copied in the pointer |dest|.",
        dest = "heuristic_status",
        type = parse_heuristic_status,
        choices = ("OFF", "ON", "LIBS")
    )

    parser.add_argument("--stdin",
        action = "store_true",
        help =  "Flag used to specify the program should read from stdin instead of from an input file",
        dest = "stdin"
    )

    return parser.parse_args()


def isInputPath(arg, testcase_path):
    return b"input" in arg and os.path.basename(testcase_path) in arg


def main():
    args = parse_args()
    
    # base_path is the full path of the current working directory as a bytes string
    testcase_path = os.fsencode(args.testcase_path)
    executable_path = os.fsencode(args.executable_path)
    args_path = os.path.join(testcase_path, b"argv")
 
    launcher = os.path.realpath(os.path.join(sys.path[0], "..", "bin", "launcher"))
    if not os.path.exists(launcher):
        print("launcher not exists, please execute 'make debug' from the root folder of the project")
        return


    argv = [launcher, '-u', args.heuristic_status, '--', executable_path]
    if os.path.exists(args_path):
        with open(args_path, "rb") as f:
            arg = f.readline()
            while len(arg) > 0:
                if isInputPath(arg, testcase_path):
                    argv.append(os.path.join(testcase_path, b"input"))
                else:
                    argv.append(arg[:-1])
                arg = f.readline()

    if args.stdin:
        input_file_path = os.path.join(testcase_path, b"input")
        with open(input_file_path, "rb") as f:
            p = subp.Popen(argv, stdin = f)
            p.wait()
    else:
        with open("output", "w") as f:
            p = subp.Popen(argv)
            p.wait()


if __name__ == "__main__":
    main()
