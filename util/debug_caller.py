#! /usr/bin/env python3

import subprocess as subp
import os
import argparse
import sys

from arg_getter import *

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("executable_path",
        help = "Path of the executable to be run"
    )
    parser.add_argument("testcase_path",
        help = "Path of the directory containing both the input file and the arguments file"
    )

    parser.add_argument("-p, --pause-tool",
        help = "Flag used to specify that the tool should be paused to allow the user attach a debugger",
        action = "store_true",
        dest = "pause_tool"
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
    environ_path = os.path.join(testcase_path, b"environ")
 
    launcher = os.path.realpath(os.path.join(sys.path[0], "..", "third_party", "PIN", "pin", "pin"))
    if not os.path.exists(launcher):
        print("launchDebug not exists, please execute 'make debug' from the root folder of the project")
        return

    if args.pause_tool:
        argv = [launcher, "-pause_tool", "10", "-t", os.path.join(sys.path[0], "..", "debug", "MemTrace.so"), "--", executable_path]
    else:
        argv = [launcher, "-t", os.path.join(sys.path[0], "..", "debug", "MemTrace.so"), "--", executable_path]
    
    if os.path.exists(args_path):
        parsed_args = get_argv_from_file(args_path)
        argv.extend(parsed_args)

    environ = dict()
    with open(environ_path, "rb") as f:
        line = f.readline()[:-1]
        while len(line) > 0:
            line = line.decode('utf-8')
            splitted = line.split("=")
            # By doing this way, we can keep untouched any value string even if that contains '='
            # This may happen, for instance, with environment variable 'LS_COLOR'
            key_len = len(splitted[0])
            environ[splitted[0]] = line[key_len + 1 : ]
            line = f.readline()[:-1]

    p = subp.Popen(argv, env = environ)
    p.wait()


if __name__ == "__main__":
    main()
