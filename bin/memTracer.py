#!/usr/bin/env python3

import functools
import sys
import os
import shutil as su
import subprocess as subp
import threading as t
import signal as sig
import time
import argparse as ap
import re
import random

from psutil import Process, STATUS_RUNNING
from collections import deque
from typing import Deque
from merge_reports import merge_reports

class MissingExecutableError(Exception):
    pass

class TooManyProcessesError(Exception):
    pass

class FuzzerError(Exception):
    pass


PROGRESS_UNIT = 0
PROGRESS_LEN = 30
PROGRESS = 0
LAST_PROGRESS_TASK = None
LOCK = t.Lock()

POWER_SCHED = {"explore", "fast", "coe", "quad", "lin", "exploit"}
EXPERIMENTAL_POWER_SCHED = {"mmopt", "rare", "seek"}
ALREADY_LAUNCHED_SCHED = set()
CUSTOM_MUTATOR_DIR_PATH = os.path.join(os.path.dirname(sys.path[0]), "custom_mutators")
CUSTOM_MUTATOR_NAME = "custom"
ENV_VARS_COPY = dict(os.environ)
AFL_PATH = os.path.join(sys.path[0], "..", "third_party", "AFLPlusPlus", "afl-fuzz")

input_path_indices = list()

# The following list will contain the paths of input files passed as command argument when we launch the tool.
# These input files will be copied inside each testcase folder, so that every execution will use its own private copy and won't
# interfere with other execution (e.g. may modify the content of the file).
initial_fixed_input_files = set()


def parse_help_flag(argv_str):
    pat = r"(\s-h)($|\s)"
    match = re.search(pat, argv_str)
    return match is not None

def parse_executable(exec_str):
    splitted_exec = exec_str.split(" ")
    exec_path = splitted_exec[0]
    exec_path = adjust_exec_path(exec_path)
    splitted_exec[0] = exec_path
    executable = [exec_path] + splitted_exec[1:]
    executable = " ".join(executable)

    if not os.path.exists(exec_path):
        raise IOError("Executable {0} does not exist. Check the executable path and try again".format(exec_path))
    return splitted_exec


def parse_args(args):
    
    class HelpFormatter(ap.ArgumentDefaultsHelpFormatter, ap.RawDescriptionHelpFormatter):
        def _format_usage(self, usage, actions, groups, prefix):
            ret_usage = super()._format_usage(usage, actions, groups, prefix).split("\n\n")[0]
            ret_usage += " -- /path/to/executable <executable args>\n\n"
            return ret_usage


    def parse_exec_time(s):
        pat = r"([1-9][0-9]*)([smh])?$"
        m = re.search(pat, s)
        if m is None:
            raise ap.ArgumentTypeError(
                "Malformed execution time string. Accepted format: "
                "<Integer> [s|m|h]. If neither of the unit modifier is used, time will be measured in seconds."
            )

        val = int(m[1])
        modifier = m[2]

        if modifier is not None:
            if modifier == 'm':
                val *= 60
            elif modifier == 'h':
                val *= 3600

        return val


    def parse_heuristic_status(s: str):
        return s.upper()


    parser = ap.ArgumentParser(formatter_class = HelpFormatter)

    parser.add_argument("--disable-argv-rand", "-r",
        action = "store_false",
        help = "Flag used to disable command line argument randomization",
        dest = "argv_rand"
    )

    parser.add_argument("--single-execution", "-x",
        action = "store_true",
        help =  "Flag used to specify that the program should be executed only once with the given parameters."
                "This may be useful in case the program does not read any input (it is useless to run the fuzzer in these "
                "cases) and it requires to run with very specific argv arguments (e.g. utility cp from coreutils requires "
                "correctly formatted, existing paths to work).\n "
                "Options -f, -d, -i, -b, -a, -t, -s, -p, --ignore-cpu-count, -e and --no-fuzzing (i.e. all options related "
                "to the fuzzing task) will be ignored.",
        dest = "single_exec"
    )

    parser.add_argument("--keep-ld",
        action = "store_true",
        help =  "Flag used to specify the tool should not ignore instructions executed from the loader's library "
                "(ld.so in Linux). By default, they are ignored because there may be a degree of randomness which makes "
                "the instructions from that library always change between executions of the same command."
                "This may cause some confusion in the final report, for this reason they are ignored.",
        dest = "keep_ld"
    )

    parser.add_argument("--unique-access-sets", "-q",
        action = "store_true",
        help =  "This flag is used to report also those uninitialized read accesses which have a unique access set. "
                "Indeed, by default, the tool only reports read accesses that have more access sets, which are those that can read different bytes "
                "according to the input."
                "This behaviour is thought to report only those uninitialized reads that are more likely to be interesting. "
                "By enabling this flag, even the uninitialized read accesses that always read from the same set of write accesses are reported in the merged report.",
        dest = "unique_access_sets"
    )

    parser.add_argument("--disable-string-filter",
        action = "store_true",
        help =  "This flag allows to disable the filter which removes all the uninitialized read accesses "
                "which come from a string function (e.g. strcpy, strcmp...). This filter has been designed because "
                "string functions are optimized, and because of that, they very often read uninitialized bytes, but "
                "those uninitialized reads are not relevant. An heuristic is already used to try and reduce this kind of "
                "false positives. However, when we use the fuzzer and merge all the results, the final report may still "
                "contain a lot of them.",
        dest = "disable_string_filter"
    )

    parser.add_argument("--str-opt-heuristic", "-u",
        default = "LIBS",
        help =  "Option used to specify whether to enable the heuristic thought to remove the high number of not "
                "relevant uninitialized reads due to the optimized versions of string operations (e.g. strcpy).\n"
                "The aforementioned heuristic can be either disabled, enabled or partially enabled.\n"
                "If the heuristic is disabled, it will never be applied.\n"
                "If it is enabled, it will be always applied.\n"
                "If it is partially enabled, it is applied only for accesses performed by code from libraries (e.g. libc).\n"
                "By default, the heuristic is partially enabled.\n"
                "Possible choices are:\n"
                "OFF => Disabled,\n"
                "\tON => Enabled,\n"
                "\tLIBS => Partially enabled.\n"
                "WARNING: when the heuristic is enabled, many false negatives may arise. If you want to avoid false negatives "
                "due to the application of the heuristic, you can simply disable it, but at a cost. "
                "Indeed, in a single program there may be many string operations, and almost all of them will generate "
                "uninitialized reads due to the optimizations (e.g. strcpy usually loads 32 bytes at a time, but then checks "
                "are performed on the loaded bytes to avoid copying junk).\n"
                "So, the execution of the program with the heuristic disabled may generate a huge number of uninitialized reads.\n"
                "Those are not false positives (those reads actually load uninitialized bytes from memory to registers), but they "
                "may be not relevant.\n"
                "Example: strcpy(dest, src), where |src| is a 4 bytes string, may load 32 bytes from memory to SIMD registers. Then, "
                "after some checks performed on the loaded bytes, only the 4 bytes belonging to |src| are copied in the pointer |dest|.",
                dest = "heuristic_status",
                type = parse_heuristic_status,
                choices = ("OFF", "ON", "LIBS")
    )

    parser.add_argument("--fuzz-out", "-f", 
        default = "fuzzer_out", 
        help = "Name of the folder containing the results generated by the fuzzer",
        dest = "fuzz_out"
    )

    parser.add_argument("--out", "-o",
        default = "tracer_out",
        help = "Name of the folder containing the results generated by the tracer",
        dest = "tracer_out"
    )

    parser.add_argument("--fuzz-dir", "-d",
        default = "out",
        help =  "Name of the folder containing all the requirements to run the fuzzer. This folder must already exist and "
                "contain all the files/directories required by the fuzzer.",
        dest = "fuzz_dir"
    )

    parser.add_argument("--fuzz-in", "-i",
        default = "in",
        help =  "Name of the folder containing the initial testcases for the fuzzer. This folder must already exist "
                "and contain the testcases",
        dest = "fuzz_in"
    )

    parser.add_argument("--backup", "-b",
        default = "olds",
        help = "Name of the folder used to move old results of past executions before running the tracer again.",
        dest = "olds_dir"
    )

    parser.add_argument("--admin-priv", "-a",
        action = "store_true",
        help =  "Flag used to specify the user has administration privileges (e.g. can use sudo on Linux). This can be used "
                "by the launcher in order to execute a configuration script that, according to the fuzzer's manual, should "
                "speedup the fuzzing task.",
        dest = "admin_priv"
    )

    parser.add_argument("--time", "-t",
        default = "60",
        help =  "Specify fuzzer's execution time. By default, the value is measured in seconds. The following modifiers can "
                "be used: 's', 'm', 'h', to specify time respectively in seconds, minutes or hours",
        dest = "exec_time",
        type = parse_exec_time
    )

    default_slaves_nr = 0
    default_processes_nr = 1

    parser.add_argument("--slaves", "-s",
        default = default_slaves_nr,
        help =  "Specify the number of slave fuzzer instances to run. The fuzzer always launches at least the main instance. " 
                "Launching more instances uses more resources, but allows to find more inputs in a smaller time span. "
                "It is advisable to use this option combined with -p, if possible. Note that the total amount of launched processes won't be "
                "higher than the total number of available cpus, unless --ignore-cpu-count flag is enabled. "
                "However, it is very advisable to launch at least 1 slave instance.",
        dest = "slaves",
        type = int
    )

    parser.add_argument("--processes", "-p",
        default = default_processes_nr,
        help =  "Specify the number of processes executing the tracer. Using more processes allows to launch the tracer with more inputs in the same time span. "
                "It is useless to use many processes for the tracer if the fuzzer finds new inputs very slowly. "
                "If there are few resources available, it is therefore advisable to dedicate them to fuzzer instances rather then to tracer processes. "
                "Note that the total amount of launched processes won't be higher than the total number of available cpus, unless --ignore-cpu-count "
                "flag is enabled.",
        dest = "processes",
        type = int
    )

    parser.add_argument("--ignore-cpu-count",
        action = "store_true",
        help =  "Flag used to ignore the number of available cpus and force the number of processes specified with -s and -p to be launched even if"
                "they are more than that.",
        dest = "ignore_cpu_count"
    )

    parser.add_argument("--experimental", "-e", 
        action = "store_true",
        help = "Flag used to specify whether to use or not experimental power schedules",
        dest = "experimental"
    )

    parser.add_argument("--no-fuzzing", 
        action = "store_true",
        help =  "Flag used to avoid launching the fuzzer.\n"
                "WARNING: this flag assumes a fuzzing task has already been executed and generated the required "
                "folders and files. It is possible to use a fuzzer different from the default one. However, the structure of the output folder must be the same "
                "as the one generated by AFL++.\n"
                "NOTE: this flag discards any fuzzer-related option (i.e. -s, -e, -t, -a). The other options may be still used to specify the paths of "
                "specific folders.",
        dest = "no_fuzzing"    
    )

    parser.add_argument("--stdin",
        action = "store_true",
        help =  "Flag used to specify that the input file should be read as stdin, and not as an input file. Note that this is meaningful only when '--no-fuzzing' is enabled, too."
                "If this flag is used, but '--no-fuzzing' is not, it is simply ignored.",
        dest =  "stdin"
    )

    parser.add_argument("--store-tracer-out",
        action = "store_true",
        help =  "This option allows the tracer thread to redirect both stdout and stdin of every spawned tracer process to a file saved in the same folder where "
                "the input file resides.",
        dest = "store_tracer_out"
    )

    parser.add_argument("--dict",
        default = None,
        help =  "Path of the dictionary to be used by the fuzzer in order to produce new inputs",
        dest = "dictionary"
    )

    parser.epilog = "After the arguments for the script, the user must pass '--' followed by the executable path and the arguments that should be passed to it. "\
                    "If it reads from an input file, write '@@' instead of the input file path. It will be "\
                    "automatically replaced by the fuzzer\n\n"\
                    "Example: ./memTracer.py -- /path/to/the/executable arg1 arg2 --opt1 @@\n\n"\
                    "Remember to NOT PASS the input file as an argument for the executable, but use @@. It will be automatically replaced by the fuzzer starting "\
                    "from the initial testcases and followed by the generated inputs."

    ret = parser.parse_args(args)

    if ret.slaves < default_slaves_nr:
        print(  "[Main Thread] WARNING: The number of slave fuzzer instances must be greater or equal to {0}. "
                "Falling back to default value: {0} slave instances will be launched".format(default_slaves_nr))
        ret.slaves = default_slaves_nr

    if ret.processes < default_processes_nr:
        print(  "[Main Thread] WARNING: The number of slave fuzzer instances must be greater or equal to {0}. "
                "Falling back to default value: {0} tracer process will be launched".format(default_processes_nr))
        ret.processes = default_processes_nr

    return ret


def launchTracer(exec_cmd, args, fuzz_int_event: t.Event, fuzzer_error_event: t.Event = None):

    def remove_terminated_processes(proc_list: Deque[subp.Popen], strikes: Deque[int]):
        list_len = len(proc_list)
        ret = deque()

        for _ in range(list_len):
            proc = proc_list.popleft()
            proc_strikes = strikes.popleft()

            if proc.poll() is None:
                # Process is not terminated yet. Verify its status
                pid = proc.pid
                status = Process(pid).status()
                # If the process is not running, increase its strike, otherwise reset them to 0
                if status != STATUS_RUNNING:
                    proc_strikes += 1
                    print("PID {0}: strike {1}".format(pid, proc_strikes))
                else:
                    proc_strikes = 0

                if proc_strikes >= 3:
                    # After 3 strikes, consider the process as stuck, so terminate it
                    print("Process {0} was stuck. It has been terminated.".format(pid))
                    proc.terminate()
                else:
                    ret.append(proc)
                    strikes.append(proc_strikes)

        return ret


    def wait_process_termination(proc_list: Deque[subp.Popen], strikes:Deque[int]):
        ret = deque(proc_list)
        initial_len = len(proc_list)
        while len(ret) == initial_len:
            print("[Tracer Thread] Waiting for a process to terminate")
            time.sleep(3)
            ret = remove_terminated_processes(ret, strikes)
        print("[Tracer Thread] Process terminated")
        return ret


    def get_argv(input_file_path):
        raw_bytes_chunks = list()
        with open(input_file_path, "rb") as f, open(tmp_file_path, "wb") as temp:
            buf_size = 1024
            args_parsed = False
            while True:
                buf = f.read(buf_size)

                if buf == b"":
                    break

                # If we did not finished parsing arguments (\x00\x00 not found), keep searching the delimiter
                if not args_parsed:
                    m = re.search(b"\x00\x00", buf)
                    # \x00\x00 not found. Everything we have just read is to be added to the arguments raw bytes
                    if m is None:
                        raw_bytes_chunks.append(buf)
                    # \x00\x00 found! Add everything up to \x00\x00 to the raw_bytes, and write the rest to the temporary file
                    else:
                        start_idx = m.start()
                        raw_bytes_chunks.append(buf[:start_idx])
                        temp.write(buf[start_idx + 2 :])
                        args_parsed = True
                # We already found the arguments list delimiter
                else:
                    temp.write(buf)

        os.replace(tmp_file_path, input_file_path)
        raw_bytes = b"".join(raw_bytes_chunks)

        ret = raw_bytes.split(b'\x00')
        for str_index in input_path_indices:
            # -1 is required because the list of arguments we are managing does not have the executable name as
            # the first element
            index = int(str_index) - 1
            if len(ret) > index:
                ret[index] = os.fsencode(os.path.realpath(input_file_path))
        return ret


    def get_argv_from_file(file_path):
        ret = []
        with open(file_path, "rb") as f:
            partial_arg = list()
            while True:
                # Ignore last byte (\n)
                line = f.readline()
                if not line:
                    if len(partial_arg) > 0:
                        arg = b"".join(partial_arg)
                        ret.append(arg)
                    break

                if line[-9 : ] == b"<endarg;\n":
                    partial_arg.append(line[:-9])
                    arg = b"".join(partial_arg)
                    ret.append(arg)
                    partial_arg.clear()
                else:
                    partial_arg.append(line)

        return ret


    def cut_raw_args_from_input_file(input_file_path):
        with open(input_file_path, "rb") as f, open(tmp_file_path, "wb") as temp:
            buf_size = 1024
            args_parsed = False
            while True:
                buf = f.read(buf_size)

                if buf == b"":
                    break

                # If we did not finished parsing arguments (\x00\x00 not found), keep searching the delimiter
                if not args_parsed:
                    m = re.search(b"\x00\x00", buf)
                    # \x00\x00 found. Delete everything up to \x00\x00 from the input file.
                    if m is not None:
                        start_idx = m.start()
                        temp.write(buf[start_idx + 2 :])
                        args_parsed = True
                # We already found the arguments list delimiter
                else:
                    temp.write(buf)

        os.replace(tmp_file_path, input_file_path)


    def get_environ_from_file(file_path):
        ret = dict()
        with open(file_path, "rb") as f:
            while True:
                # Ignore last byte (\n)
                line = f.readline()[:-1].decode('utf-8')
                if not line:
                    break
                splitted = line.split('=')
                ret[splitted[0]] = splitted[1]
        
        return ret


    def replace_files_private_cpy(argv, input_folder):
        bytes_input_path = os.fsencode(input_folder)
        arg_index = 0
        for arg in argv:
            if arg in initial_fixed_input_files:
                dst_path = os.path.join(bytes_input_path, os.path.basename(arg))
                su.copy(arg, dst_path)
                argv[arg_index] = dst_path
            arg_index += 1
        print(" AFTER: ", argv)


    print("[Tracer Thread] Tracer thread started...")
    if fuzzer_error_event is None:
        fuzzer_error_event = t.Event()
    fuzz_dir = args.fuzz_dir
    tmp_file_path = os.path.join(fuzz_dir, "tmp")
    fuzz_out = os.path.join(fuzz_dir, args.fuzz_out)
    tracer_out = os.path.join(fuzz_dir, args.tracer_out)
    inputs_dir = os.path.join(fuzz_out, "Main", "queue")
    launcher_path = os.path.join(sys.path[0], "launcher")
    tracer_cmd = [launcher_path, "-o", "./overlaps.bin", "-u", args.heuristic_status, "--keep-ld", args.keep_ld, "--"] + exec_cmd[:1]
    traced_inputs = set()

    # If this expression evaluates to True, it means the user used --no-fuzzing option, but the tracer output folder
    # already exists, so he probably already launched the script.
    # This is a special case. The default usage should be to launch the script to perform fuzzing and tracing in parallel,
    # so, it is user's responsibility to manually get rid of the tracer output folder.
    if os.path.exists(tracer_out):
        raise IOError("Folder {0} already exists. Either remove or move it and try again.".format(tracer_out))
    os.mkdir(tracer_out)
    args_dir = os.path.join(fuzz_dir, "args")
    environ_dir = os.path.join(fuzz_dir, "environ")
    new_inputs_found = True

    processes = deque()
    strikes = deque()
    inputs_dir_set = set()

    if not args.no_fuzzing:
        if os.path.exists(args_dir):
            su.rmtree(args_dir)
        if os.path.exists(environ_dir):
            su.rmtree(environ_dir)
        os.mkdir(args_dir)
        os.mkdir(environ_dir)
        inputs_dir_set.add(inputs_dir)
        for i in range(args.slaves):
            slave_name = "Slave_" + str(i)
            inputs_dir = os.path.join(fuzz_out, slave_name, "queue")
            inputs_dir_set.add(inputs_dir)

        instance_names = set(map(lambda x: os.path.basename(os.path.dirname(x)), inputs_dir_set)) 

    else:
        instance_names = os.listdir(fuzz_out)
        for name in instance_names:
            path = os.path.join(fuzz_out, name, "queue")
            inputs_dir_set.add(path)   

    for directory in inputs_dir_set:
        while not os.path.exists(directory):
            print("[Tracer Thread] Waiting for inputs directories to be generated by the fuzzer...")
            if fuzz_int_event.is_set():
                print()
                print(  "[Tracer Thread] Fuzzer died before creating inputs directory.\n"
                        "This probably means there has been an error during fuzzer's startup.\n"
                        "Try to launch the script again using -a option. NOTE: you must have admin privileges (i.e. sudo user on Linux)"
                     )
                print()
                print("[Tracer Thread] Tracer thread will be shutdown")
                return
            time.sleep(1)

    fuzz_artifact_rel_paths = {os.path.join("queue", ".state"), os.path.join("crashes", "README.txt")}
    for directory in instance_names:
        for artifact in fuzz_artifact_rel_paths:
            fuzz_artifact_path = os.path.join(fuzz_out, directory, artifact)
            traced_inputs.add((directory, fuzz_artifact_path))
        new_folder = os.path.join(tracer_out, directory)
        os.mkdir(new_folder)
        if not args.no_fuzzing:
            os.mkdir(os.path.join(args_dir, directory))
            os.mkdir(os.path.join(environ_dir, directory))

    pat = r"sync"
    # Loop interrupts if and only if there are no new_inputs and the fuzzer has been interrupted
    while new_inputs_found or not fuzz_int_event.is_set():
        processes = remove_terminated_processes(processes, strikes)
        # This set will contain tuples of type (<Instance_Name>, <Input_File_Name>)
        inputs = set()
        for directory in instance_names:
            inputs_dirs = {os.path.join(fuzz_out, directory, "queue"), os.path.join(fuzz_out, directory, "crashes")}
            for inputs_dir in inputs_dirs:
                for f in os.scandir(inputs_dir):
                    res = re.search(pat, f.name)
                    # If the file contains "sync", it has been imported from other parallel instances, and it is identical
                    # to an input file generated by that instance. Avoid executing it twice.
                    if res is None:
                        path = os.path.join(inputs_dir, f.name)
                        inputs.add((directory, path))

        new_inputs = inputs.difference(traced_inputs)
        if(len(new_inputs) == 0):
            new_inputs_found = False
            if fuzz_int_event.is_set():
                continue
            print("[Tracer Thread] Waiting the fuzzer for new inputs")
            time.sleep(10)
            continue

        new_inputs_found = True

        for el in new_inputs:
            if fuzzer_error_event.is_set():
                print("[Tracer Thread] Tracer thread stopped due to fuzzer error")
                return
            input_folder = os.path.join(tracer_out, el[0], os.path.basename(el[1]))
            os.mkdir(input_folder)
            input_cpy_path = os.path.join(input_folder, "input")
            su.copy(el[1], input_cpy_path)
            tracer_cmd[2] = os.path.join(input_folder, "overlaps.bin")

            full_cmd = list(map(lambda x: x.encode('utf-8'), tracer_cmd)) 
            
            if len(processes) == args.processes:
                processes = wait_process_termination(processes, strikes)

            argv_file_path = os.path.join(input_folder, "argv")
            env_file_path = os.path.join(input_folder, "environ")
            input_folder_basename = os.path.basename(input_folder)

            environ = None

            # Save/restore environment variables
            if args.no_fuzzing:
                environ_file_path = os.path.join(environ_dir, el[0], input_folder_basename)
                if not os.path.exists(environ_file_path):
                    continue
                environ = get_environ_from_file(environ_file_path)
                su.copy(environ_file_path, env_file_path)
            else:
                ENV_VARS_COPY['FUZZ_INSTANCE_NAME'] = el[0]
                environ = ENV_VARS_COPY
                with open(env_file_path, "wb") as f:
                    for key, value in ENV_VARS_COPY.items():
                        f.write("{0}={1}\n".format(key, value).encode('utf-8'))

                su.copy(env_file_path, os.path.join(environ_dir, el[0], input_folder_basename))

            # Save/restore command line arguments
            if args.argv_rand:
                if args.no_fuzzing:
                    args_file_path = os.path.join(args_dir, el[0], input_folder_basename)
                    if not os.path.exists(args_file_path):
                        continue
                    argv = get_argv_from_file(args_file_path)
                    cut_raw_args_from_input_file(input_cpy_path)
                    su.copy(args_file_path, argv_file_path)
                else:
                    # Retrieve arguments from input file and cut it to remove last 128 bytes
                    argv = get_argv(input_cpy_path)
                    # Save used arguments in a binary file called "argv" (possibly not human readable)
                    with open(argv_file_path, "wb") as f:
                        for cmd_arg in argv:
                            f.write(cmd_arg)
                            f.write(b"<endarg;\n")

                    su.copy(argv_file_path, os.path.join(args_dir, el[0], input_folder_basename))
                
                # Save the used arguments in a text file called 'argv.txt' (human readable)
                with open(os.path.join(input_folder, "argv.txt"), "w") as f:
                    for cmd_arg in argv:
                        f.write("{0}\n".format(cmd_arg))

                replace_files_private_cpy(argv, input_folder)
                    
                # Append arguments to full_cmd
                full_cmd.extend(argv)
            # Argv is not fuzzed, but we must keep the arguments passed from the command line
            else:
                replace_files_private_cpy(exec_cmd[1:], input_folder)
                full_cmd.extend(exec_cmd[1:])

            # Set stdin to be either the input file or an empty file, according to the flags used to launch the tool
            if args.no_fuzzing:
                if args.stdin:
                    tracer_stdin = open(input_cpy_path, "rb")
                else:
                    empty_file = open("empty_file", "w")
                    empty_file.close()
                    tracer_stdin = open("empty_file", "rb")
            else:
                # If there isn't any fuzzed input file, use the generated file as stdin
                if len(input_path_indices) == 0:
                    tracer_stdin = open(input_cpy_path, "rb")
                # otherwise, set stdin to an empty file, so that if the program tries to read from that
                # it won't stuck waiting for input
                else:
                    empty_file = open("empty_file", "w")
                    empty_file.close()
                    tracer_stdin = open("empty_file", "rb")

            # Store the exact command used to launch the execution of the analyzed program
            #print("[Tracer Thread] FULL_CMD: ", full_cmd)
            with open(os.path.join(input_folder, "full_cmd"), "wb") as f:
                for cmd_part in full_cmd:
                    f.write(cmd_part)
                    f.write(b' ')

            if args.store_tracer_out:
                output_file_path = os.path.join(input_folder, "output")
                with open(output_file_path, "w") as out:
                    try:
                        processes.append(subp.Popen(full_cmd, env = environ, stdin = tracer_stdin, stdout = out, stderr = subp.STDOUT))
                        strikes.append(0)
                    except Exception as e:
                        print("Exception happened while launching the tracer")
                        out.write("Exception happened while launching the tracer\n")
                        out.write("Exception:\n")
                        out.write(str(e) + "\n\n")
                        out.write("Exception raised with command:\n")
                        out.write(str(full_cmd))
            else:
                try:
                    processes.append(subp.Popen(full_cmd, env = environ, stdin = tracer_stdin, stdout = subp.DEVNULL, stderr = subp.DEVNULL))
                    strikes.append(0)
                except Exception as e:
                    print("Exception happened while launching the tracer")
                    print("Exception:")
                    print(str(e))
                    print()
                    print("Exception raised with command:")
                    print(str(full_cmd))

            if not tracer_stdin.closed:
                tracer_stdin.close()

            print("[Tracer Thread] {0} is running".format(el[1]))
            print()

        traced_inputs.update(new_inputs)
        if fuzzer_error_event.is_set():
            print("[Tracer Thread] Tracer thread stopped due to fuzzer error")
            return
        time.sleep(10)

    while len(processes) > 0:
        processes = wait_process_termination(processes, strikes)
    print("[Tracer Thread] Generating textual report...")
    apply_string_filter = not args.disable_string_filter
    merge_reports(tracer_out, apply_string_filter = apply_string_filter, report_unique_access_sets = args.unique_access_sets)


def move_directory(src, dst_dir, new_name = None):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    if not new_name is None:
        dirname = os.path.dirname(src)
        new_path = os.path.join(dirname, new_name)
        os.rename(src, new_path)
        src = new_path

    basename = os.path.basename(src)
    su.move(src, os.path.join(dst_dir, basename))


def count_dir_content(dir_path):
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)
        return 0

    return len(os.listdir(dir_path))


def adjust_exec_path(path):
    if(os.path.isabs(path)):
        return path

    return os.path.realpath(path)


def main():
    global PROGRESS
    global PROGRESS_UNIT
    global input_path_indices


    ### MAIN HELPER FUNCTIONS ###

    def build_slave_cmd(slave_id, fuzz_in, fuzz_out, experimental_enabled, dictionary, executable):
        global POWER_SCHED
        global EXPERIMENTAL_POWER_SCHED
        global ALREADY_LAUNCHED_SCHED

        if(len(ALREADY_LAUNCHED_SCHED) == 0):
            selected = "fast"
        else:
            if experimental_enabled:
                r = random.random()
                sched_set = POWER_SCHED if r <= 0.90 else EXPERIMENTAL_POWER_SCHED
            else:
                sched_set = POWER_SCHED
            diff = sched_set.difference(ALREADY_LAUNCHED_SCHED)
            if(len(diff) != 0):
                sched_set = diff
            selected = random.choice(list(sched_set))

        ALREADY_LAUNCHED_SCHED.update({selected})

        print("[Main Thread] Launching slave instance nr {0} with power schedule {1}".format(slave_id, selected))
        instance_name = "Slave_{0}".format(slave_id)
        cmd = [AFL_PATH, "-Q", "-S", instance_name, "-p", selected, "-i", fuzz_in, "-o", fuzz_out]
        if dictionary is not None:
            cmd.extend(["-x", dictionary])
        cmd.append("--")
        cmd.extend(executable)
        return cmd, instance_name


    def send_int(process: subp.Popen, slaves):
        for slave in slaves:
            slave.send_signal(sig.SIGINT)
        process.send_signal(sig.SIGINT)


    def print_empty_progress(fuzz_int_ev: t.Event):
        global PROGRESS_LEN
        global LAST_PROGRESS_TASK
        global LOCK

        out = ["|"]
        out.append(" " * PROGRESS_LEN)
        out.append("|")
        print("".join(out))

        LOCK.acquire()
        LAST_PROGRESS_TASK = t.Timer(10, print_progress, [fuzz_int_ev])
        LAST_PROGRESS_TASK.start()
        LOCK.release()


    # |fuzz_int_event| is required because 2 different threads may be running |print_progress|,
    # one with the |finished| flag enabled, and one with the flag disabled.
    # If in that situation the flag-enabled thread acquires the lock first, the flag-disabled
    # thread will acquire it later, setting a new timer, thus never ending setting new timers
    def print_progress(fuzz_int_ev: t.Event, finished = False):
        global PROGRESS
        global PROGRESS_LEN
        global LAST_PROGRESS_TASK
        global LOCK

        if finished:
            progress_bar = PROGRESS_LEN
        else:
            PROGRESS += 10
            progress_bar = PROGRESS // PROGRESS_UNIT
        out = ["|"]
        out.append("=" * progress_bar)
        out.append(" " * (PROGRESS_LEN - progress_bar))
        out.append("|")
        print("".join(out))

        LOCK.acquire()
        if not finished and not fuzz_int_ev.is_set():
            LAST_PROGRESS_TASK = t.Timer(10, print_progress, [fuzz_int_ev])
            LAST_PROGRESS_TASK.start()
        elif not LAST_PROGRESS_TASK is None:
            LAST_PROGRESS_TASK.cancel()
        LOCK.release()


    def clean():
        fuzzer_interrupted_event.set()
        int_timer.cancel()
        print("[Main Thread] Waiting for the tracer thread to shutdown...")
        tracerThread.join()
        print("[Main Thread] Cleaning files...")
        if os.path.exists(FUZZ_OUT):
            su.rmtree(FUZZ_OUT)
        if os.path.exists(tracer_out):
            su.rmtree(tracer_out)
        if os.path.exists(os.path.join(os.getcwd(), "partialOverlaps.log")):
            os.remove("partialOverlaps.log")
        if os.path.exists(os.path.join(os.getcwd(), "base_addresses.log")):
            os.remove("base_addresses.log")

        print("[Main Thread] Restoring originals...")
        restore_originals()
        exit(1)

    def restore_originals():
        orig_testcases_dir = os.path.join(os.getcwd(), "originals")
        if not os.path.exists(orig_testcases_dir):
            return

        for file in os.listdir(orig_testcases_dir):
            su.copy(os.path.join(orig_testcases_dir, file), os.path.join(FUZZ_IN, file))
        su.rmtree(orig_testcases_dir)


    def adjust_executable(executable: list, fuzz_in: str):
        args = executable[1:]
        # If there are no arguments passed, append an empty string, so that
        # a double null byte is added to the beginning of the input file
        if len(args) == 0:
            args.append("")

        # Append to args all the indices where a '@@' is found. Those locations will be replaced
        # by the input file path in the __libc_start_main hook in the library.
        for i in range(len(args)):
            if args[i].strip() == '@@':
                # Append i + 1, because argv[0] is always the executable name
                input_path_indices.append(str(i + 1))
            elif os.path.exists(args[i]):
                initial_fixed_input_files.add(args[i].encode('utf-8'))

        if len(input_path_indices) > 0:
            env_var = ",".join(input_path_indices)
            ENV_VARS_COPY['INPUT_FILE_ARGV_INDICES'] = env_var

        # Append an empty string so that last argument will be terminated by a \x00 byte
        args.append("")

        # If there are no initial testcases provided, raise an error. We need at least 1.
        initial_testcases = os.listdir(fuzz_in)
        if len(initial_testcases) == 0:
            raise FuzzerError("ERROR: no initial testcase found in {0}. Please provide at least a valid testcase.". format(fuzz_in))

        orig_testcases_dir = os.path.join(os.getcwd(), "originals")
        if os.path.exists(orig_testcases_dir):
            su.rmtree(orig_testcases_dir)
        os.mkdir(orig_testcases_dir)

        for file in os.listdir(fuzz_in):
            # Copy original testcases into a new folder
            file_path = os.path.join(fuzz_in, file)
            su.copy(file_path, os.path.join(orig_testcases_dir, file))

            # Convert the arg list into a byte sequence, with each argument terminated by a \x00 byte
            # Terminate the sequence of arguments with a double '\x00' byte.
            args_bytes = b"\x00".join(map(lambda x: x.encode('utf-8'), args))
            args_bytes += b"\x00"

            # Append |args_bytes| to every initial testcase in fuzz_in folder
            # The library requires to have the arguments as a first thing in the file, so
            # we need a temporary file as a copy buffer
            with open(file_path, "r+b") as original, open(TMP_FILE_PATH, "wb") as temp:
                temp.write(args_bytes)
                size = 1024
                while True:
                    cont = original.read(size)
                    if cont == b"":
                        break
                    temp.write(cont)

            # Replace "file_path" file with the one having the arguments at the beginning
            os.replace(TMP_FILE_PATH, file_path)

        # Add custom mutator in the environment variables of process launching fuzzer instances
        #ENV_VARS_COPY['PYTHONPATH'] = CUSTOM_MUTATOR_DIR_PATH
        #ENV_VARS_COPY['AFL_PYTHON_MODULE'] = CUSTOM_MUTATOR_NAME
        ENV_VARS_COPY['AFL_PRELOAD'] = os.path.realpath(os.path.join(sys.path[0], "..", "lib", "argvfuzz64.so"))
        return executable


    
    ### MAIN BEGINNING ###
    # The RNG is used in order to select a random power schedule if more than 1 slave fuzzer instance is launched
    random.seed()

    # sys_args contains the command-line arguments divided in 2 parts:
    # the first element contains a string representing the arguments for this script;
    # the second element contains a string representing the executable and its command-line arguments
    argv_str = " ".join(sys.argv)

    # Check if -h is passed as an argument
    help_requested = parse_help_flag(argv_str)
    if help_requested:
        # Since -h is passed as an argument, parse_args will simply print the help text and return
        parse_args(sys.argv)

    pat = r"(\s)(--)([\s\n]|$)"
    match = re.search(pat, argv_str)

    # If there's no '--' in the list of command line arguments, the script can't say which is the executable
    if match is None:
        raise MissingExecutableError(
            "Executable path is missing. Provide an executable path and its arguments after '--'.\n"
            "Example: ./memTracer.py -- /path/to/executable arg1 arg2 -opt1"
        )

    sys_args = argv_str.split(match[0])

    # If no executable is provided, raise an error
    if len(sys_args[1]) == 0:
        raise MissingExecutableError(
            "Executable path is missing. Provide an executable path and its arguments after '--'.\n"
            "Example: ./memTracer.py -- /path/to/executable arg1 arg2 -opt1"
        )

    # args is a Namespace object, containing the arguments parsed by parse_args
    args = parse_args(sys_args[0].split(" ")[1:])
    # Change keep_ld argument to reflect the type Intel PIN expects for the same flag
    args.keep_ld = "1" if args.keep_ld else "0"

    # executable is a string specifying the executable path followed by the arguments to be passed to it
    executable = parse_executable(sys_args[1])

    if args.no_fuzzing:
        e = t.Event()
        e.set()
        launchTracer(executable, args, e)
        return

    if args.single_exec:
        tracer_out = args.tracer_out
        tracer_out = os.path.realpath(tracer_out)
        if not os.path.exists(tracer_out):
            raise IOError("Folder {0} must exist".format(tracer_out))
        launcher_path = os.path.join(sys.path[0], "launcher")
        tracer_cmd = [launcher_path, "-o", os.path.join(tracer_out, "overlaps.bin"), "-u", args.heuristic_status, "--"] + executable
        proc = subp.Popen(tracer_cmd) #, stdout = subp.DEVNULL, stderr = subp.DEVNULL)
        proc.wait()
        return

    WORKING_DIR = os.getcwd()
    FUZZ_DIR = os.path.join(WORKING_DIR, args.fuzz_dir)
    TMP_FILE_PATH = os.path.join(FUZZ_DIR, "tmp")
    FUZZ_OUT = os.path.join(FUZZ_DIR, args.fuzz_out)
    FUZZ_IN = os.path.join(FUZZ_DIR, args.fuzz_in)
    FUZZ_OLDS = os.path.join(FUZZ_DIR, args.olds_dir)

    if args.argv_rand:
        executable = adjust_executable(executable, FUZZ_IN)
    else:
        for arg in executable[1:]:
            if os.path.exists(arg):
                initial_fixed_input_files.add(arg.encode('utf-8'))

    if not os.path.exists(FUZZ_DIR):
        raise IOError("Folder {0} must exist and must contain all the required files/folders for the fuzzer to work (e.g. initial testcases)".format(FUZZ_DIR))

    if not os.path.exists(FUZZ_IN):
        raise IOError("Folder {0} must exist and contain at least 1 initial testcase".format(FUZZ_IN))

    tracer_out = os.path.join(FUZZ_DIR, args.tracer_out)

    # If out (and therefore tracer_out) exists in the fuzzer folder, we need to move them away.
    # First, we need to update last_count value
    if os.path.exists(FUZZ_OUT) or os.path.exists(tracer_out):
        last_old = os.path.join(FUZZ_DIR, "last_old")
        last_count = None

        # If file last_old does not exist, create it. last_count value will be 0
        if not os.path.exists(last_old):
                last_count = 0
                with open(last_old, "w") as f:
                    f.write(str(last_count))
        # otherwise, read the value stored in the file last_old. If it is a number, set last_count to that 
        # value, otherwise, the file is malformed, so we need to actually count the folders already stored in
        # FUZZ_OLDS
        else:
            with open(last_old, "r+") as f:
                content = f.readline()
                if len(content) == 0:
                    last_count = 0
                else:
                    try:
                        last_count = int(content)
                        last_count += 1
                    except ValueError:
                        last_count = count_dir_content(FUZZ_OLDS)

                f.seek(0)
                f.write(str(last_count))
                f.truncate()

        backup_dir = os.path.join(FUZZ_OLDS, str(last_count))
        
        if os.path.exists(FUZZ_OUT):
            move_directory(FUZZ_OUT, backup_dir)

        if os.path.exists(tracer_out):
            move_directory(tracer_out, backup_dir)

        if os.path.exists(tracer_out):
            raise IOError("Directory {0} already exists. Please move or remove it before running the tracer.".format(tracer_out))
        
        if os.path.exists(FUZZ_OUT):
            raise IOError("Directory {0} already exists. Please move or remove it before running the tracer.".format(FUZZ_OUT))

    # Start AFL fuzzer
    if(args.admin_priv):
        p = subp.Popen(["sudo", "afl-system-config"])
        p.wait()
    launch_single_instance = args.slaves == 0
    dictionary = None
    if args.dictionary is not None and os.path.exists(args.dictionary):
        dictionary = os.path.realpath(args.dictionary)
    fuzz_cmd = [AFL_PATH, "-Q", "-S" if launch_single_instance else "-M", "Main", "-i", FUZZ_IN, "-o", FUZZ_OUT]
    if dictionary is not None:
        fuzz_cmd.extend(["-x", dictionary])
    fuzz_cmd.append("--")
    fuzz_cmd.extend(executable)

    cpus = os.cpu_count()
    if cpus is None:
        print("[Main Thread] Warning: cpu count cannot be determined and will be ignored.")
        args.ignore_cpu_count = True

    if not args.ignore_cpu_count:
        # +1 is required because args.slaves contains only the additional fuzzer instances. The +1 counts the main instance
        p = args.slaves + args.processes + 1
        if p > cpus:
            raise TooManyProcessesError("Available cpus ({0}) are less than the number of processes to be launched ({1})".format(cpus, p))

    stop_after_seconds = args.exec_time
    PROGRESS_UNIT = stop_after_seconds // PROGRESS_LEN
    if PROGRESS_UNIT <= 0:
        PROGRESS_UNIT = PROGRESS_LEN

    print("[Main Thread] Launching Main fuzzer instance...")
    ENV_VARS_COPY['FUZZ_INSTANCE_NAME'] = 'Main'
    p = subp.Popen(fuzz_cmd, env = ENV_VARS_COPY, stdout = subp.DEVNULL, stderr = subp.DEVNULL)
    slaves = list()
    for i in range(args.slaves):
        cmd, instance_name = build_slave_cmd(i, FUZZ_IN, FUZZ_OUT, args.experimental, dictionary, executable)
        ENV_VARS_COPY['FUZZ_INSTANCE_NAME'] = instance_name
        slaves.append(subp.Popen(cmd, env = ENV_VARS_COPY, stdout = subp.DEVNULL, stderr = subp.DEVNULL))

    print()
    print("[Main Thread] Fuzzer will be interrupted in {0} seconds...".format(stop_after_seconds))

    fuzzer_interrupted_event = t.Event()
    fuzzer_error_event = t.Event()
    int_timer = t.Timer(stop_after_seconds, send_int, [p, slaves])
    int_timer.start()
    print_empty_progress(fuzzer_interrupted_event)

    tracerThread = t.Thread(target = launchTracer, args = [executable, args, fuzzer_interrupted_event, fuzzer_error_event])
    tracerThread.start()

    # Wait for the fuzzer to be interrupted
    for slave in slaves:
        slave.wait()
    p.wait()

    num_instances = len(slaves) + 1
    num_failed_instances = functools.reduce(lambda acc, el: acc + (1 if el.returncode != 0 else 0), slaves, 0)
    if p.returncode != 0:
        num_failed_instances += 1

    if num_failed_instances > num_instances // 2:
        print("[Main Thread] Fuzzer interrupted due to an error")
        fuzzer_error_event.set()
        int_timer.cancel()
        LOCK.acquire()
        if LAST_PROGRESS_TASK is not None:
            LAST_PROGRESS_TASK.cancel()
        LOCK.release()
        clean()

    fuzzer_interrupted_event.set()
    print_progress(fuzzer_interrupted_event, True)
    # If the fuzzer terminates before the requested time (e.g. if there are no initial testcases)
    # it is not necessary to wait the whole time to send a SIGINT
    int_timer.cancel()
    print("[Main Thread] Fuzzer interrupted")
    print()

    restore_originals()

    # Wait for the tracer thread to run with all the inputs found by the fuzzer
    tracerThread.join()

    print("[Main Thread] All finished")


if __name__ == "__main__":
    main()
