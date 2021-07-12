import subprocess as subp
import os
import argparse

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("executable_rel_path",
        help = "Relative path of the executable to be run"
    )
    parser.add_argument("testcase_rel_path",
        help = "Relative path of the directory containing both the input file and the arguments file"
    )

    return parser.parse_args()


def main():
    args = parse_args()
    
    # base_path is the full path of the current working directory as a bytes string
    base_path = os.fsencode(os.path.realpath(os.getcwd()))
    testcase_path = os.path.join(base_path, os.fsencode(args.testcase_rel_path))
    executable_path = os.path.join(base_path, os.fsencode(args.executable_rel_path))
    args_path = os.path.join(testcase_path, b"argv")

    argv = [b'gdb', b'--args', executable_path]
    with open(args_path, "rb") as f:
        arg = f.readline()
        while len(arg) > 0:
            argv.append(arg[:-1])
            arg = f.readline()

    with open("output", "w") as f:
        p = subp.Popen(argv)
        p.wait()


if __name__ == "__main__":
    main()
