#! /usr/bin/env python3

from os import system

window0_is_busy = False

# Send a command to tmux session named "test-session"
def tmux_ctrl(command):
    system('tmux {0}'.format(command))

# Run a shell command inside a new tmux window
def tmux_run(command):
    global window0_is_busy

    if window0_is_busy:
        tmux_ctrl('new-window -n {0}'.format(command.split()[0]))
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
    else:
        tmux_ctrl('send-keys "{0}" "C-m"'.format(command))
        window0_is_busy = True

# Run a new tmux session named "test-session"
def init():
    tmux_ctrl('new-session -d -s test-session')

def main():
    init()
    tmux_run('nano')
    tmux_run('vim')
    tmux_run('ls')
    tmux_run('vim example.py')

if __name__ == '__main__':
    main()
