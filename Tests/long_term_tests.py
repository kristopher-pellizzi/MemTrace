#! /usr/bin/env python3

from os import system, listdir
import os
import shutil

window0_is_busy = False

# Send a command to tmux session named "test-session"
def tmux_ctrl(command):
    system('tmux {0}'.format(command))

# Run a shell command inside a new tmux window
def tmux_run(command, name = None):
    global window0_is_busy

    window_name = name if name is not None else command.split()[0]

    if window0_is_busy:
        tmux_ctrl('new-window -n {0}'.format(window_name))
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
    else:
        tmux_ctrl('rename-window -t test-session:0 {0}'.format(window_name))
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
        window0_is_busy = True

# Run a new tmux session named "test-session"
def init():
    tmux_ctrl('new-session -d -s test-session')
    os.mkdir('test_out')

def main():
    bin_basepath = '/usr/bin'
    test_command = 'memTracer.py -t 1h -s 1 -p 2 --store-tracer-out -d {1} -- {0} @@'
    print("Setting up test environment in a new tmux session...")
    init()
    bin_list = listdir(bin_basepath)
    for i in range(7):
        bin_name = bin_list[0]
        bin_list = bin_list[1:]
        bin_path = os.path.join(bin_basepath, bin_name)
        bin_test_folder = os.path.realpath(os.path.join('test_out', bin_name))
        os.mkdir(bin_test_folder)
        init_testcase_folder = os.path.join(bin_test_folder, 'in')
        os.mkdir(init_testcase_folder)
        shutil.copy('rand', init_testcase_folder)
        tmux_run(test_command.format(bin_path, bin_test_folder), name =  bin_name)

    print("Tests started in session 'test-session'")
    print('Type "tmux a -t test-session" to attach to it') 

if __name__ == '__main__':
    main()
