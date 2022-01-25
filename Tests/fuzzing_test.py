#! /usr/bin/env python3

import os
import sys
import subprocess as subp
from functools import reduce
from time import sleep
import shutil as su
import threading as th

window0_is_busy = False

tests_path = os.path.realpath(sys.path[0])
memTracer_path = os.path.realpath(os.path.join(tests_path, '..', 'bin', 'memTracer.py'))
dict_path = os.path.realpath(os.path.join(tests_path, 'dictionaries'))
src_path = os.path.realpath(os.path.join(tests_path, '..', 'src'))
fuzzing_test_out_path = os.path.join(tests_path, 'fuzzing_test_out')
pkg_names = ['md4c', 'oocborrt', 'coreutils-7.6', 'coreutils-6.9.90', 'CTF']
pkg_paths = list(map(lambda x: os.path.join(tests_path, x), pkg_names))
ctf_path = os.path.join(tests_path, 'CTF')
cgc_repo = 'https://github.com/GrammaTech/cgc-cbs'
bin_names = ['md2html', 'cbor2json', 'cp', 'tail', 'contacts', 'full_protection', 'watchstop', 'accel', 'textsearch', 'hackman', 'tfttp', 'sso']
input_dir_paths = list(map(lambda x: os.path.join(tests_path, 'fuzzing_inputs', x), bin_names))
out_paths = list(map(lambda x: os.path.join(fuzzing_test_out_path, x), bin_names))
patch_dir_path = os.path.join(tests_path, 'patches')

required_pkgs = ['tmux', 'patchelf', 'cmake', 'zip', 'unzip', 'tar', 'sudo', 'valgrind', 'bc', 'autoconf', 'automake', 'autopoint', 'bison', 'gperf', 'makeinfo', 'rsync']

compile_script_path = {
    'md2html':'md4c', 
    'cbor2json':'oocborrt', 
    'cp':'coreutils-6.9.90', 
    'tail':'coreutils-7.6', 
    'contacts':os.path.join('CTF', 'picoCTF_2018_contacts'),
    'full_protection':os.path.join('CTF', 'ASIS_CTF_2020_full_protection'),
    'watchstop':os.path.join('CTF', 'zer0ptsCTF_2021_watchstop'),
    'accel':'cgc-cbs',
    'textsearch':'cgc-cbs',
    'hackman':'cgc-cbs',
    'tfttp':'cgc-cbs',
    'sso':'cgc-cbs'
}

bin2path = {
    'md2html':os.path.join('md4c', 'md2html', 'md2html'), 
    'cbor2json':os.path.join('oocborrt', 'util', 'cbor2json'), 
    'cp':os.path.join('coreutils-6.9.90','build', 'src', 'cp'), 
    'tail':os.path.join('coreutils-7.6', 'install', 'bin', 'tail'), 
    'contacts':os.path.join('CTF', 'picoCTF_2018_contacts', 'contacts_cpy'),
    'full_protection':os.path.join('CTF', 'ASIS_CTF_2020_full_protection', 'full_protection'),
    'watchstop':os.path.join('CTF', 'zer0ptsCTF_2021_watchstop', 'watchstop'),
    'accel':os.path.join('cgc-cbs','cqe-challenges', 'KPRCA_00013', 'KPRCA_00013'),
    'textsearch':os.path.join('cgc-cbs','cqe-challenges', 'KPRCA_00036', 'KPRCA_00036'),
    'hackman':os.path.join('cgc-cbs','cqe-challenges', 'KPRCA_00017', 'KPRCA_00017'),
    'tfttp':os.path.join('cgc-cbs','cqe-challenges', 'NRFIN_00012', 'NRFIN_00012'),
    'sso':os.path.join('cgc-cbs','cqe-challenges', 'NRFIN_00033', 'NRFIN_00033')
}


def check_tmux_session_end():
    while True:
        p = subp.Popen(['tmux', 'has-session', '-t', 'test-session'])
        retcode = p.wait()
        if retcode != 1:
            return

        sleep(1)


def wait_tmux_session_end():
    t = th.Thread(target = check_tmux_session_end)
    t.start()
    print("MemTrace needs to be recompiled to continue, so testing will resume as soon as the already started tests will end (in about 8 hours).")
    print("Note that at the end of the fuzzing process, MemTrace may require user input to conclude.")
    print("Digit 'tmux a -t test-session' to check the status of the tests.")
    print("When all the started tests ended, you can close the tmux session, and the test will resume.")
    t.join()


def confirm_removal():
    while True:
        print("Fuzzing test results detected.")
        print("Do you want to delete them and repeat the test? [Y/n]")
        resp = input("> ")

        if resp.lower().startswith('n'):
            exit(0)

        if resp.startswith('Y'):
            if os.path.exists(fuzzing_test_out_path):
                su.rmtree(fuzzing_test_out_path)

            return

        print()


def extract_archive(archive_name: str):
    path = os.path.join(tests_path, archive_name)
    cmd = None
    if archive_name == 'coreutils-6.9.90':
        ext = 'zip'
        archive = "{0}.{1}".format(path, ext)
        cmd = ['unzip', archive]
    else:
        ext = 'tar.xz'
        archive = "{0}.{1}".format(path, ext)
        cmd = ['tar', '-xvJf', archive]

    p = subp.Popen(cmd, stdout = subp.DEVNULL, stderr = subp.STDOUT, cwd = tests_path)
    p.wait()


# Send a command to tmux session named "test-session"
def tmux_ctrl(command):
    os.system('tmux {0}'.format(command))

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
    for pkg in required_pkgs:
        try:
            p = subp.Popen([pkg, '--version'], stdout = subp.DEVNULL, stderr = subp.STDOUT)
            p.wait()
        except FileNotFoundError:
            print("{0} not found. Be sure to install the following packages:".format(pkg))
            print(" ".join(list(map(lambda x: 'texinfo' if x == 'makeinfo' else x, required_pkgs))))
            exit(-1)

    # Create output folders
    if not os.path.exists(fuzzing_test_out_path):
        os.mkdir(fuzzing_test_out_path)
        for path in out_paths:
            os.mkdir(path)
            os.mkdir(os.path.join(path, 'tracer_out'))
    else:
        out_path_exists = reduce(lambda acc, el: acc or os.path.exists(el), out_paths, False)
        if out_path_exists:
            confirm_removal()

        os.mkdir(fuzzing_test_out_path)
        for path in out_paths:
            os.mkdir(path)
            os.mkdir(os.path.join(path, 'tracer_out'))

    # Extract archives and download cgc repo
    for pkg in pkg_names:
        pkg_path = os.path.join(tests_path, pkg)
        if not os.path.exists(pkg_path):
            extract_archive(pkg)

    fuzzing_inputs_path = os.path.join(tests_path, 'fuzzing_inputs')
    if not os.path.exists(fuzzing_inputs_path):
        extract_archive('fuzzing_inputs')
    
    cgc_path = os.path.join(tests_path, 'cgc-cbs')
    if not os.path.exists(cgc_path):
        print("Downloading cgc binaries...")
        p = subp.Popen(['git', 'clone', cgc_repo], cwd = tests_path)
        p.wait()
        su.copy(os.path.join(patch_dir_path, 'cgc_compile.sh'), os.path.join(tests_path, 'cgc-cbs', 'compile.sh'))
        # NOTE: this patch actually slightly modifies the source code of the binary. However, this is done because the original binary was thought for 32 bit x86
        # and some reads are managed assuming it is executed on such an architecture (and therefore assumes pointers have a size of 4 bytes).
        # The patch is only meant to fix the retrieval of packages, without affecting the actual behavior of the original program.
        su.copy(os.path.join(patch_dir_path, 'sso', 'src', 'service.c'), os.path.realpath(os.path.join(tests_path, bin2path['sso'], '..', 'src')))

    # Compile binaries.
    for bin in bin2path:
        if not os.path.exists(os.path.join(tests_path, bin2path[bin])):
            compile_script_dirname = os.path.join(tests_path, compile_script_path[bin])
            path = os.path.join(compile_script_dirname, 'compile.sh')
            print("Compiling {0}...".format(bin))
            p = subp.Popen([path], cwd = compile_script_dirname)
            p.wait()
            if p.returncode != 0 and not os.path.exists(os.path.join(tests_path, bin2path[bin])):
                print("Compilation of {0} failed".format(bin))
                print("Try to compile manually in folder {0}".format(os.path.join(tests_path, compile_script_dirname)))

            
    # Perform tests
    print("The script will launch tests in different tmux windows of a session called 'test-session'.")
    print("Since for some of the binaries a custom malloc handler is required, tests will be done in batches and recompiled when needed.")
    sleep(3)
    
    if not os.path.exists('rand'):
        subp.run(['dd', 'if=/dev/urandom', 'of=rand', 'count=1K', 'iflag=count_bytes'])

    orig_environ = dict(os.environ)
    os.environ['AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES'] = 1
    os.environ['AFL_SKIP_CPUFREQ'] = 1
    os.environ['FORCE_UNSAFE_CONFIGURE'] = 1

    # Test cbor2json
    bin_name = 'cbor2json'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1} -i @@'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test md2html. This will not find any overlap
    bin_name = 'md2html'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} --dict {2} -- {1} @@'.format(out_path, exec_path, os.path.join(dict_path, 'md2html.dict'))
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)


    # Test tail
    bin_name = 'tail'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1} @@'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test contacts
    bin_name = 'contacts'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        custom_dict_path = os.path.join(dict_path, 'contacts.dict')
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} --dict {2} -- {1}'.format(out_path, exec_path, custom_dict_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test full_protection
    bin_name = 'full_protection'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1}'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test watchstop
    bin_name = 'watchstop'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        custom_dict_path = os.path.join(dict_path, 'watchstop.dict')
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} --dict {2} -- {1}'.format(out_path, exec_path, custom_dict_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)


    wait_tmux_session_end() 
    print("Resuming testing script...")

    # Patch and recompile MemTrace
    os.remove(os.path.join(src_path, 'MallocHandler.h'))
    su.copy(os.path.join(patch_dir_path, 'MallocHandler_KPRCA.h'), os.path.join(src_path, 'MallocHandler.h'))
    if not os.path.exists(os.path.join(src_path, 'KPRCA_00021_malloc_handlers.h')):
        su.copy(os.path.join(patch_dir_path, 'KPRCA_00021_malloc_handlers.h'), os.path.join(src_path, 'KPRCA_00021_malloc_handlers.h'))
    p = subp.Popen(['make', 'tool_custom_malloc'], cwd = os.path.realpath(os.path.join(tests_path, '..')), stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()

    # Test accel
    bin_name = 'accel'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        custom_dict_path = os.path.join(dict_path, 'accel.dict')
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} --dict {2} -- {1}'.format(out_path, exec_path, custom_dict_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test textsearch
    bin_name = 'textsearch'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        custom_dict_path = os.path.join(dict_path, 'textsearch.dict')
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} --dict {2} -- {1}'.format(out_path, exec_path, custom_dict_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test hackman
    bin_name = 'hackman'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1}'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    wait_tmux_session_end()

    # Patch and recompile MemTrace
    os.remove(os.path.join(src_path, 'MallocHandler.h'))
    su.copy(os.path.join(patch_dir_path, 'MallocHandler_NRFIN.h'), os.path.join(src_path, 'MallocHandler.h'))
    if not os.path.exists(os.path.join(src_path, 'NRFIN_00033_malloc_handlers.h')):
        su.copy(os.path.join(patch_dir_path, 'NRFIN_00033_malloc_handlers.h'), os.path.join(src_path, 'NRFIN_00033_malloc_handlers.h'))
    p = subp.Popen(['make', 'tool_custom_malloc'], cwd = os.path.realpath(os.path.join(tests_path, '..')), stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()

    # Test tfttp
    bin_name = 'tfttp'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1}'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    # Test sso 
    bin_name = 'sso'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        out_path = os.path.join(fuzzing_test_out_path, bin_name)
        test_command = 'memTracer.py -t 8h -s 3 -p 4 --store-tracer-out -d {0} -- {1}'.format(out_path, exec_path)
        init_testcase_dir = os.path.join(out_path, 'in')
        os.mkdir(init_testcase_dir)
        input_basename = os.path.join(tests_path, 'fuzzing_inputs', bin_name)
        for input_file in os.listdir(input_basename):
            input_path = os.path.join(input_basename, input_file)
            su.copy(input_path, init_testcase_dir)
        su.copy('rand', init_testcase_dir)
        tmux_run(test_command, name =  bin_name, working_dir = out_path)

    wait_tmux_session_end()

    # Patch and recompile MemTrace
    os.remove(os.path.join(src_path, 'MallocHandler.h'))
    su.copy(os.path.join(patch_dir_path, 'MallocHandler_orig.h'), os.path.join(src_path, 'MallocHandler.h'))
    p = subp.Popen(['make'], cwd = os.path.realpath(os.path.join(tests_path, '..')), stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()

    print("Tests finished.")
    os.environ = orig_environ
    print("Check {0} to see the results".format(fuzzing_test_out_path))

if __name__ == '__main__':
    main()