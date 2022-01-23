#! /usr/bin/env python3

import os
import sys
import subprocess as subp
from functools import reduce
import shutil as su

tests_path = os.path.realpath(sys.path[0])
memTracer_path = os.path.realpath(os.path.join(tests_path, '..', 'bin', 'memTracer.py'))
src_path = os.path.realpath(os.path.join(tests_path, '..', 'src'))
triggering_test_out_path = os.path.join(tests_path, 'triggering_test_out')
pkg_names = ['md4c', 'oocborrt', 'coreutils-7.6', 'coreutils-6.9.90', 'CTF']
pkg_paths = list(map(lambda x: os.path.join(tests_path, x), pkg_names))
ctf_path = os.path.join(tests_path, 'CTF')
cgc_repo = 'https://github.com/GrammaTech/cgc-cbs'
bin_names = ['md2html', 'cbor2json', 'cp', 'tail', 'contacts', 'full_protection', 'watchstop', 'accel', 'textsearch', 'hackman', 'tfttp', 'sso']
input_dir_paths = list(map(lambda x: os.path.join(tests_path, 'trigger_inputs', x), bin_names))
out_paths = list(map(lambda x: os.path.join(triggering_test_out_path, x), bin_names))
patch_dir_path = os.path.join(tests_path, 'patches')

required_pkgs = ['patchelf', 'cmake', 'zip', 'unzip', 'tar', 'sudo', 'valgrind', 'bc', 'autoconf', 'automake', 'autopoint', 'bison', 'gperf', 'makeinfo', 'rsync']

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


def confirm_removal():
    while True:
        print("Triggering test results detected.")
        print("Do you want to delete them and repeat the test? [Y/n]")
        resp = input("> ")

        if resp.lower().startswith('n'):
            exit(0)

        if resp.startswith('Y'):
            if os.path.exists(triggering_test_out_path):
                su.rmtree(triggering_test_out_path)

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
    if not os.path.exists(triggering_test_out_path):
        os.mkdir(triggering_test_out_path)
        for path in out_paths:
            os.mkdir(path)
            os.mkdir(os.path.join(path, 'tracer_out'))
    else:
        out_path_exists = reduce(lambda acc, el: acc or os.path.exists(el), out_paths, False)
        if out_path_exists:
            confirm_removal()

        os.mkdir(triggering_test_out_path)
        for path in out_paths:
            os.mkdir(path)
            os.mkdir(os.path.join(path, 'tracer_out'))

    # Extract archives and download cgc repo
    for pkg in pkg_names:
        pkg_path = os.path.join(tests_path, pkg)
        if not os.path.exists(pkg_path):
            extract_archive(pkg)

    trigger_inputs_path = os.path.join(tests_path, 'trigger_inputs')
    if not os.path.exists(trigger_inputs_path):
        extract_archive('trigger_inputs')
    
    cgc_path = os.path.join(tests_path, 'cgc-cbs')
    if not os.path.exists(cgc_path):
        print("Downloading cgc binaries...")
        p = subp.Popen(['git', 'clone', cgc_repo], cwd = tests_path)
        p.wait()
        su.copy(os.path.join(patch_dir_path, 'cgc_compile.sh'), os.path.join(tests_path, 'cgc-cbs', 'compile.sh'))

    # Compile binaries.
    for bin in bin2path:
        if not os.path.exists(os.path.join(tests_path, bin2path[bin])):
            compile_script_dirname = os.path.join(tests_path, compile_script_path[bin])
            path = os.path.join(compile_script_dirname, 'compile.sh')
            print("Compiling {0}...".format(bin))
            p = subp.Popen([path], cwd = compile_script_dirname)
            p.wait()
            if p.returncode != 0:
                print("Compilation of {0} failed".format(bin))
                print("Try to compile manually in folder {0}".format(os.path.join(tests_path, compile_script_dirname)))

            
    # Perform tests
    
    # Test cbor2json
    bin_name = 'cbor2json'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path, '-i', input_path]
        with open(cmd_output_path, "w") as f:
            subp.Popen(cmd, cwd = out_path, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test md2html. This will not find any overlap
    bin_name = 'md2html'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path, input_path]
        with open(cmd_output_path, "w") as f:
            subp.Popen(cmd, cwd = out_path, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test cp
    bin_name = 'cp'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'path', 'to', 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        os.mkdir(os.path.join(out_path, 'path'))
        cmd = [memTracer_path, '-x', '--', exec_path, '--parent', '--preserve', input_path, out_path]
        with open(cmd_output_path, "w") as f:
            subp.Popen(cmd, cwd = out_path, stdout = f, stderr = subp.STDOUT)
            p.wait()
        # Remove test artifacts
        try:
            os.remove(os.path.join(out_path, 'path'))
        except:
            print("Impossible to remove artifacts. Please manually remove {0}".format(os.path.join(out_path, 'path')))

    # Test tail
    bin_name = 'tail'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path, '-']
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test contacts
    bin_name = 'contacts'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test full_protection
    bin_name = 'full_protection'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test watchstop
    bin_name = 'watchstop'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

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
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test textsearch
    bin_name = 'textsearch'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test hackman
    bin_name = 'hackman'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

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
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait()

    # Test sso (COPY SCRIPT FOR SSO HERE)
    bin_name = 'full_protection'
    exec_path = os.path.join(tests_path, bin2path[bin_name])
    if os.path.exists(exec_path):
        print("Testing {0}...".format(bin_name))
        input_path = os.path.join(tests_path, 'trigger_inputs', bin_name, 'input')
        out_path = os.path.join(triggering_test_out_path, bin_name)
        cmd_output_path = os.path.join(out_path, 'cmd_output')
        cmd = [memTracer_path, '-x', '--', exec_path]
        with open(cmd_output_path, "w") as f, open(input_path, "rb") as input:
            subp.Popen(cmd, cwd = out_path, stdin = input, stdout = f, stderr = subp.STDOUT)
            p.wait() 

    # Patch and recompile MemTrace
    os.remove(os.path.join(src_path, 'MallocHandler.h'))
    su.copy(os.path.join(patch_dir_path, 'MallocHandler_orig.h'), os.path.join(src_path, 'MallocHandler.h'))
    p = subp.Popen(['make'], cwd = os.path.realpath(os.path.join(tests_path, '..')), stdout = subp.DEVNULL, stderr = subp.STDOUT)
    p.wait()

    print("Tests finished.")
    print("Check {0} to see the results".format(triggering_test_out_path))

if __name__ == '__main__':
    main()