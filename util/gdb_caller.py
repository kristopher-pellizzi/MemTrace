#! /usr/bin/env python3

import subprocess as subp
import os
import argparse
from time import sleep
import sys

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("executable_path",
        help = "Path of the executable to be run"
    )
    parser.add_argument("testcase_path",
        help = "Path of the directory containing both the input file and the arguments file"
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
    environ_path = os.path.join(testcase_path, b"environ")
    script_dir = os.fsencode(os.path.realpath(sys.path[0]))

    argv = [b'gdb']
    if args.stdin:
        input_file_path = os.path.join(testcase_path, b"input")
        #argv.extend([b'-ex', b'echo Execute "define launch". Define the new command as:\n'])
        argv.extend([b'-ex', b'echo Please manually insert these lines:\n'])
        argv.extend([b'-ex', b"echo r < '" + input_file_path + b"'\n"])
        argv.extend([b'-ex', b'echo end\n'])
        #argv.extend([b'-ex', b"echo Then launch with: 'launch'\n"])
        argv.extend([b'-ex', b'define hook-run'])
    argv.extend([b'-ex', b'set $testcase_path = "' + testcase_path + b'"'])
    argv.extend([b'-ex', b'set $cwd = "' + os.path.realpath(os.fsencode(sys.path[0])) + b'"'])
    argv.extend([b'-ex', b'source ' + os.path.join(script_dir, b'verification_gdb_session.py')])
    argv_ext = [b'--args', executable_path]
    argv.extend(argv_ext)
    
    if os.path.exists(args_path):
        with open(args_path, "rb") as f:
            arg = f.readline()
            while len(arg) > 0:
                if isInputPath(arg, testcase_path):
                    argv.append(os.path.join(testcase_path, b"input"))
                else:
                    argv.append(arg[:-1])
                arg = f.readline()

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

    with open("output", "w") as f:
        p = subp.Popen(argv, env = environ)
        p.wait()


if __name__ == "__main__":
    main()
