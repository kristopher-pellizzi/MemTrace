import subprocess as subp
import os
import argparse
import sys

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("executable_path",
        help = "Path of the executable to be run"
    )
    parser.add_argument("testcase_path",
        help = "Path of the directory containing both the input file and the arguments file"
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
 
    launcher = os.path.realpath(os.path.join(sys.path[0], "..", "bin", "launchDebug"))
    if not os.path.exists(launcher):
        print("launchDebug not exists, please execute 'make debug' from the root folder of the project")
        return


    argv = [launcher, '--', executable_path]
    with open(args_path, "rb") as f:
        arg = f.readline()
        while len(arg) > 0:
            if isInputPath(arg, testcase_path):
                argv.append(os.path.join(testcase_path, b"input"))
            else:
                argv.append(arg[:-1])
            arg = f.readline()

    with open("output", "w") as f:
        p = subp.Popen(argv)
        p.wait()


if __name__ == "__main__":
    main()
