#! /usr/bin/env python3

import os
import sys
import subprocess as subp

tests_path = os.path.realpath(sys.path[0])
memtrace_test_path = os.path.join(tests_path, 'test.sh')
valgrind_test_path = os.path.join(tests_path, 'valgrindTest.sh')
exec_times_path = os.path.join(tests_path, 'execution_times.csv')
memtrace_exec_times_path = os.path.join(tests_path, 'memtrace_execution_times.csv')
valgrind_exec_times_path = os.path.join(tests_path, 'valgrind_execution_times.csv')
coreutils_path = os.path.join(tests_path, 'coreutils_8.32')
required_pkgs = ['sudo', 'valgrind', 'bc', 'autoconf', 'automake', 'autopoint', 'bison', 'gperf', 'makeinfo', 'rsync']
nulltool_path = os.path.realpath(os.path.join(tests_path, '..', 'tool', 'NullTool.so'))

def extract_coreutils():
    print("Extracting coreutils_8.32...")
    p = subp.Popen(['tar', '-xvJf', 'coreutils_8.32.tar.xz'], cwd = tests_path)
    p.wait()


def confirm_removal():
    while True:
        print("Timing test results detected.")
        print("Do you want to delete them and repeat the test? [Y/n]")
        resp = input("> ")

        if resp.lower().startswith('n'):
            exit(0)

        if resp.startswith('Y'):
            if os.path.exists(memtrace_exec_times_path):
                os.remove(memtrace_exec_times_path)
            
            if os.path.exists(valgrind_exec_times_path):
                os.remove(valgrind_exec_times_path)

            return

        print()


def main():
    if not os.path.exists(nulltool_path):
        p = subp.Popen(['make', 'nulltool'], cwd=os.path.realpath(os.path.join(tests_path, '..')))
        p.wait()

    if not os.path.exists(coreutils_path):
        extract_coreutils()

    for pkg in required_pkgs:
        try:
            p = subp.Popen([pkg, '--version'], stdout = subp.DEVNULL, stderr = subp.STDOUT)
            p.wait()
        except FileNotFoundError:
            print("{0} not found. Be sure to install the following packages:".format(pkg))
            print(" ".join(list(map(lambda x: 'texinfo' if x == 'makeinfo' else x, required_pkgs))))
            exit(-1)

    if not os.path.exists(os.path.join(coreutils_path, 'install', 'bin', 'tail')):
        compile_script_path = os.path.join(coreutils_path, 'compile.sh')
        p = subp.Popen([compile_script_path], cwd=coreutils_path)
        p.wait()

    if os.path.exists(memtrace_exec_times_path) or os.path.exists(valgrind_exec_times_path):
        confirm_removal()

    print("Executing {0}. It will take a while...".format(memtrace_test_path))
    p = subp.Popen([memtrace_test_path], stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()
    
    print("{0} terminated. Executing {1}. It may take a while...".format(memtrace_test_path, valgrind_test_path))
    os.rename(exec_times_path, memtrace_exec_times_path)
    p = subp.Popen([valgrind_test_path], stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()
    
    print("{0} terminated.".format(valgrind_test_path))
    os.rename(exec_times_path, valgrind_exec_times_path)


if __name__ == '__main__':
    main()