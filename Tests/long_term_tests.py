#! /usr/bin/env python3

from os import system, listdir
from random import shuffle
import sys
import os
import shutil
import argparse as ap
import subprocess as subp

window0_is_busy = False

not_to_execute = ['chage', 'chattr', 'chcon', 'chfn', 'chgrp', 'chmod', 'choom', 'chown', 'chrt', 'chsh', 'chvt']

def parse_args():
    parser = ap.ArgumentParser()

    parser.add_argument('-n', '--tests-to-run',
        default = 10,
        type = int,
        help =  'Number of tests to be run',
        dest = 'num'
    )

    parser.add_argument('-i', '--include',
        default = None,
        help = 'Name of a binary to include in the testset',
        dest = 'include'
    )

    parser.add_argument('-o', '--output',
        default = 'test_out',
        help = 'Path of the output folder for all the performed tests',
        dest = 'out_dir'
    )

    return parser.parse_args()

# Send a command to tmux session named "test-session"
def tmux_ctrl(command):
    system('tmux {0}'.format(command))

# Run a shell command inside a new tmux window
def tmux_run(command, name = None, working_dir = None):
    global window0_is_busy

    window_name = name if name is not None else command.split()[0]
    window_working_dir = working_dir if working_dir is not None else os.getcwd()

    if window0_is_busy:
        tmux_ctrl('new-window -n {0} -c {1}'.format(window_name, window_working_dir))
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
    else:
        init(window_name, window_working_dir)
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
        window0_is_busy = True

# Run a new tmux session named "test-session"
def init(name, working_dir):
    tmux_ctrl('new-session -d -s test-session -n {0} -c {1}'.format(name, working_dir))

def main():
    args = parse_args()

    bin_basepath = '/usr/bin'
    out_path = args.out_dir
    test_command = 'memTracer.py -t 1d -s 1 -p 2 --store-tracer-out -d {1} -- {0} @@'
    print("Setting up test environment in a new tmux session...\n")
    if not os.path.exists(out_path):
        os.mkdir(out_path)

    tested_list_path = os.path.join(sys.path[0], 'tested_list.txt')
    non_testable_list_path = os.path.join(sys.path[0], 'non_testable_list.txt')

    if not os.path.exists(tested_list_path):
        f = open(tested_list_path, "w")
        f.close()


    if not os.path.exists(non_testable_list_path):
        f = open(non_testable_list_path, "w")
        # Add as non-testable all the binaries that might change permissions or attributes to files or users
        for bin_name in not_to_execute:
            f.write(bin_name)
        f.close()

    if not os.path.exists('rand'):
        system('dd if=/dev/urandom of=rand count=1K iflag=count_bytes')

    bin_list = listdir(bin_basepath)
    shuffle(bin_list)
    tested_bins = None
    non_testable_bins = None
    selected_bins = set()
    shell_scripts = set()
    with open(tested_list_path, "r") as f:
        tested_bins = set(filter(lambda x: len(x) > 0, f.read().split('\n')))

    with open(non_testable_list_path, "r") as f:
        non_testable_bins = set(filter(lambda x: len(x) > 0, f.read().split('\n')))

    i = 0
    skip = False

    if args.include is not None:
        bin_name = args.include
        bin_path = os.path.join(bin_basepath, bin_name)
        if os.path.islink(bin_path):
            print('{0} is a symbolic link. The symbolic link will be expanded...\n'.format(bin_name))
            p = subp.Popen(['readlink', '-f', bin_path], stdout = subp.PIPE, stderr = subp.DEVNULL)
            p.wait()
            out, err = p.communicate()
            bin_path = out.strip().decode()
            print('{0} is a symbolic name to {1}'.format(bin_name, bin_path))

        p = subp.Popen(['file', bin_path], stdout = subp.PIPE, stderr = subp.DEVNULL)
        p.wait()
        out, err = p.communicate()
        if b'shell script' in out or b'text executable' in out.strip():
            print('{0} is a shell script: it cannot be fuzzed with AFL++'.format(bin_name))
            print("Skipping {0}...\n".format(bin_name))
            shell_scripts.add(bin_name)
            skip = True

        if not skip:
            i += 1
            selected_bins.add(bin_name)
            bin_test_folder = os.path.realpath(os.path.join(out_path, bin_name))
            os.mkdir(bin_test_folder)
            init_testcase_folder = os.path.join(bin_test_folder, 'in')
            os.mkdir(init_testcase_folder)
            shutil.copy('rand', init_testcase_folder)
            tmux_run(test_command.format(bin_path, bin_test_folder), name =  bin_name, working_dir = bin_test_folder)

    while i < args.num:
        bin_name = bin_list[0]
        bin_list = bin_list[1:]
        if bin_name in tested_bins or bin_name in non_testable_bins:
            continue

        bin_path = os.path.join(bin_basepath, bin_name)
        if os.path.islink(bin_path):
            print('{0} is a symbolic link. The symbolic link will be expanded...\n'.format(bin_name))
            p = subp.Popen(['readlink', '-f', bin_path], stdout = subp.PIPE, stderr = subp.DEVNULL)
            p.wait()
            out, err = p.communicate()
            bin_path = out.strip().decode()

        p = subp.Popen(['file', bin_path], stdout = subp.PIPE, stderr = subp.DEVNULL)
        p.wait()
        out, err = p.communicate()
        if b'shell script' in out or b'text executable' in out.strip():
            print('{0} is a shell script: it cannot be fuzzed with AFL++'.format(bin_name))
            print("Skipping {0}...\n".format(bin_name))
            shell_scripts.add(bin_name)
            continue

        i += 1
        selected_bins.add(bin_name)
        bin_test_folder = os.path.realpath(os.path.join(out_path, bin_name))
        os.mkdir(bin_test_folder)
        init_testcase_folder = os.path.join(bin_test_folder, 'in')
        os.mkdir(init_testcase_folder)
        shutil.copy('rand', init_testcase_folder)
        tmux_run(test_command.format(bin_path, bin_test_folder), name =  bin_name, working_dir = bin_test_folder)

    with open(tested_list_path, "a") as f:
        for bin_name in selected_bins:
            f.write(bin_name)
            f.write('\n')
        f.write('\n')

    with open(non_testable_list_path, "a") as f:
        for bin_name in shell_scripts:
            f.write(bin_name)
            f.write('\n')
        f.write('\n')

    print("Tests started in session 'test-session'")
    print('Type "tmux a -t test-session" to attach to it') 

if __name__ == '__main__':
    main()
