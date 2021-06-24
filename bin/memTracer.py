#!/usr/bin/env python3

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

from binOverlapParser import parse, print_table_header, print_table_footer, PartialOverlapsWriter
from parsedData import *
from collections import deque
from functools import partial, reduce
from typing import List, Tuple, Deque, Dict

class MissingExecutableError(Exception):
    pass

class TooManyProcessesError(Exception):
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

    parser.add_argument("--rand-argv", "-r",
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

    parser.epilog = "After the arguments for the script, the user must pass '--' followed by the executable path and the arguments that should be passed to it. "\
                    "If it reads from an input file, write '@@' instead of the input file path. It will be "\
                    "automatically replaced by the fuzzer\n\n"\
                    "Example: ./memTracer.py -- /path/to/the/executable arg1 arg2 --opt1 @@\n\n"\
                    "Remember to NOT PASS the input file as an argument for the executable, but use @@. It will be automatically replaced by the fuzzer starting "\
                    "from the initial testcases and followed by the generated inputs."

    ret = parser.parse_args(args)

    if ret.slaves < default_slaves_nr:
        print(  "WARNING: The number of slave fuzzer instances must be greater or equal to {0}. "
                "Falling back to default value: {0} slave instances will be launched".format(default_slaves_nr))
        ret.slaves = default_slaves_nr

    if ret.processes < default_processes_nr:
        print(  "WARNING: The number of slave fuzzer instances must be greater or equal to {0}. "
                "Falling back to default value: {0} tracer process will be launched".format(default_processes_nr))
        ret.processes = default_processes_nr

    return ret


def launchTracer(exec_cmd, args, fuzz_int_event: t.Event):

    def remove_terminated_processes(proc_list):
        ongoing_indices = [i for i, el in enumerate(map(lambda x: x.poll(), proc_list)) if el is None]
        ret = [el for i, el in enumerate(proc_list) if i in ongoing_indices]
        return ret

    def wait_process_termination(proc_list):
        ret = proc_list
        while len(ret) == len(proc_list):
            print("Waiting for a process to terminate")
            time.sleep(3)
            ret = remove_terminated_processes(proc_list)
        print("Process terminated")
        return ret



    print("Tracer thread started...")
    fuzz_dir = args.fuzz_dir
    fuzz_out = os.path.join(fuzz_dir, args.fuzz_out)
    tracer_out = os.path.join(fuzz_dir, args.tracer_out)
    inputs_dir = os.path.join(fuzz_out, "Main", "queue")
    launcher_path = os.path.join(os.getcwd(), "launcher")
    tracer_cmd = [launcher_path, "-o", "./overlaps.bin", "-u", args.heuristic_status, "--"] + exec_cmd
    traced_inputs = set()
    # If this expression evaluates to True, it means the user used --no-fuzzing option, but the tracer output folder
    # already exists, so he probably already launched the script.
    # This is a special case. The default usage should be to launch the script to perform fuzzing and tracing in parallel,
    # so, it is user's responsibility to manually get rid of the tracer output folder.
    if os.path.exists(tracer_out):
        raise IOError("Folder {0} already exists. Either remove or move it and try again.".format(tracer_out))
    os.mkdir(tracer_out)
    new_inputs_found = True

    processes = list()
    inputs_dir_set = set()

    if not args.no_fuzzing:
        inputs_dir_set.add(inputs_dir)
        for i in range(args.slaves):
            slave_name = "Slave_" + str(i)
            inputs_dir = os.path.join(fuzz_out, slave_name, "queue")
            inputs_dir_set.add(inputs_dir)

        instance_names = set(map(lambda x: os.path.basename(os.path.dirname(x)), inputs_dir_set)) 

    else:
        instance_names = os.listdir(fuzz_out)
        print(instance_names)
        for name in instance_names:
            path = os.path.join(fuzz_out, name, "queue")
            inputs_dir_set.add(path)   

    for directory in inputs_dir_set:
        while not os.path.exists(directory):
            print("Waiting for inputs directories to be generated by the fuzzer...")
            if fuzz_int_event.is_set():
                print()
                print("Fuzzer died before creating inputs directory.")
                print("This probably means there has been an error during fuzzer's startup.")
                print("Try to launch the script again using -a option. NOTE: you must have admin privileges (i.e. sudo user on Linux)")
                print()
                print("Tracer thread will be shutdown")
                return
            time.sleep(1)

    fuzz_artifact_name = ".state"
    for directory in instance_names:
        fuzz_artifact_path = os.path.join(fuzz_out, directory, "queue", fuzz_artifact_name)
        traced_inputs.add((directory, fuzz_artifact_path))
        new_folder = os.path.join(tracer_out, directory)
        os.mkdir(new_folder)

    pat = r"sync"
    # Loop interrupts if and only if there are no new_inputs and the fuzzer has been interrupted
    while new_inputs_found or not fuzz_int_event.is_set():
        processes = remove_terminated_processes(processes)
        # This set will contain tuples of type (<Instance_Name>, <Input_File_Name>)
        inputs = set()
        for directory in instance_names:
            inputs_dir = os.path.join(fuzz_out, directory, "queue")
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
            print("Waiting the fuzzer for new inputs")
            time.sleep(10)
            continue

        new_inputs_found = True

        for t in new_inputs:
            input_folder = os.path.join(tracer_out, t[0], os.path.basename(t[1]))
            os.mkdir(input_folder)
            input_cpy_path = os.path.join(input_folder, "input")
            su.copy(t[1], input_cpy_path)
            print()
            tracer_cmd[2] = os.path.join(input_folder, "overlaps.bin")
            full_cmd = tracer_cmd + [input_cpy_path]
            if len(processes) == args.processes:
                processes = wait_process_termination(processes)
            processes.append(subp.Popen(full_cmd, stdout = subp.DEVNULL, stderr = subp.DEVNULL))
            print("{0} is running".format(t[1]))
            print()

        traced_inputs.update(new_inputs)
        time.sleep(10)


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


def merge_reports(tracer_out_path: str):

    def merge_ma_sets(accumulator: Deque[Tuple[Deque[str], Deque[MemoryAccess]]], element: Tuple[str, Deque[MemoryAccess]]):
        filtered_acc = list(filter(lambda x: x[1] == element[1], accumulator))

        # If there are no elements with the same ma_set as |element|
        if len(filtered_acc) == 0:
            refs = deque()
            refs.append(element[0])
            accumulator.append((refs, element[1]))
        # If there's at least 1, we are sure there's exactly one (as a consequence of the applied reduce
        # that uses this function)
        else:
            t = list(filtered_acc)[0]
            t[0].append(element[0])

        return accumulator

    load_bases: Deque[Tuple[str, List[Tuple[str, str]]]] = deque()
    stack_bases: Deque[Tuple[str, str]] = deque()
    partial_overlaps: Dict[AccessIndex, Deque[Tuple[Deque[str], Deque[MemoryAccess]]]] = dict()

    tmp_partial_overlaps: Dict[AccessIndex, Deque[Tuple[str, Deque[MemoryAccess]]]] = dict()
    for instance in os.listdir(tracer_out_path):
        instance_path = os.path.join(tracer_out_path, instance)
        for input_dir in os.listdir(instance_path):
            input_dir_path = os.path.join(instance_path, input_dir)
            parse_res: ParseResult = parse(bin_report_dir = input_dir_path)
            cur_load_bases = parse_res.load_bases
            cur_stack_base = parse_res.stack_base
            overlaps = parse_res.partial_overlaps
            input_ref = os.path.join(instance, input_dir)

            # Fill partial_overlaps, load_bases and stack_bases
            for ai, ma_set in overlaps:
                if ai not in tmp_partial_overlaps:
                    tmp_partial_overlaps[ai] = deque()

                tmp_partial_overlaps[ai].append((input_ref, ma_set))

            load_bases.append((input_ref, cur_load_bases))
            stack_bases.append((input_ref, cur_stack_base))

    # Sort partial_overlaps by AccessIndex (increasing address, decreasing access size)
    sorted_partial_overlaps = sorted(tmp_partial_overlaps.items(), key=lambda x: x[0])

    # Try to merge those (inputRef, MemoryAccess) tuples whose memory access set is the same
    for ai, access_set in sorted_partial_overlaps:
        ma_sets = reduce(merge_ma_sets, access_set, deque())
        partial_overlaps[ai] = ma_sets

    # Generate textual report files: 1 with only the accesses, 1 with address bases

    # Generate base addresses report
    with open("base_addresses.log", "w") as f:
        while(len(load_bases) != 0):
            imgs = load_bases.popleft()
            stack = stack_bases.popleft()

            input_ref, img_list = imgs
            f.write("===============================================\n")
            f.write(input_ref)
            f.write("\n===============================================\n")
            for img_name, img_addr in img_list:
                f.write("{0} base address: {1}\n".format(img_name, img_addr))
            
            f.write("Stack base address: {0}\n".format(stack[1]))
            f.write("===============================================\n")
            f.write("===============================================\n\n")

    # Generate partial overlaps textual report

    po = PartialOverlapsWriter()

    if len(partial_overlaps) == 0:
        po.writelines(["** NO OVERLAPS DETECTED **"])
        return

    for ai, access_set in partial_overlaps.items():
        header = " ".join([str(ai.address), "-", str(ai.size)])
        print_table_header(po, header)

        for input_refs, ma_set in access_set:
            lines = deque()
            lines.append("Generated by inputs:")
            lines.extend(map(lambda x: " ".join(["[*]", x]), input_refs))
            lines.append("***********************************************")

            for entry in ma_set:
                # Print overlap entries
                str_list = []
                if entry.isPartialOverlap:
                    str_list.append("=> ")
                else:
                    str_list.append("   ")

                if entry.isUninitializedRead:
                    str_list.append("*")

                str_list.append(entry.ip + " (" + entry.actualIp + "):")
                str_list.append("\t" if len(entry.disasm) > 0 else " ")
                str_list.append(entry.disasm)
                str_list.append(" W " if entry.accessType == AccessType.WRITE else " R ")
                str_list.append(str(entry.accessSize) + " B ")
                if entry.memType == MemType.STACK:
                    str_list.append("@ (sp ")
                    str_list.append("+ " if entry.spOffset >= 0 else "- ")
                    str_list.append(str(abs(entry.spOffset)))
                    str_list.append("); ")
                    str_list.append("(bp ")
                    str_list.append("+ " if entry.bpOffset >= 0 else "- ")
                    str_list.append(str(abs(entry.bpOffset)))
                    str_list.append("); ")
                
                while len(entry.uninitializedIntervals) > 0:
                    lower_bound, upper_bound = entry.uninitializedIntervals.popleft()
                    str_list.append("[" + str(lower_bound) + " ~ " + str(upper_bound) + "]")

                lines.append("".join(str_list))

            lines.append("***********************************************\n")
            po.writelines(lines)
        print_table_footer(po)
                

def main():
    global PROGRESS
    global PROGRESS_UNIT

    ### MAIN HELPER FUNCTIONS ###

    def build_slave_cmd(slave_id, fuzz_in, fuzz_out, experimental_enabled, executable):
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

        print("Launching slave instance nr {0} with power schedule {1}".format(slave_id, selected))
        cmd = ["afl-fuzz", "-Q", "-S", "Slave_{0}".format(slave_id), "-p", selected, "-i", fuzz_in, "-o", fuzz_out, "--"] + executable
        return cmd


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
        print("".join(out), end="\r")

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
        end_char = "\n" if finished else "\r"
        print("".join(out), end=end_char)

        LOCK.acquire()
        if not finished and not fuzz_int_ev.is_set():
            LAST_PROGRESS_TASK = t.Timer(10, print_progress, [fuzz_int_ev])
            LAST_PROGRESS_TASK.start()
        elif not LAST_PROGRESS_TASK is None:
            LAST_PROGRESS_TASK.cancel()
        LOCK.release()


    def adjust_executable(executable: list, fuzz_in: str):
        exec_name = executable[0]
        args = executable[1:]
        # wrapper_args is a list containing additional arguments for the wrapper program.
        # More specifically, it will contain the indices of list args where a '@@' is found
        wrapper_args = list()

        # Append to args all the indices where a '@@' is found. Those locations will be replaced
        # by the input file path in the wrapper program.
        for i in range(len(args)):
            if args[i].strip() == '@@':
                # Append i + 1, because argv[0] is always the executable name
                wrapper_args.append(str(i + 1))

        # Append an empty string so that last argument will be terminated by a \x00 byte
        args.append("")

        # If there are no initial testcases provided, provide an empty one, so that the tool can at least randomize argv
        initial_testcases = os.listdir(fuzz_in)
        if len(initial_testcases) == 0:
            print(
                "WARNING: no initial testcases found in {0}. By default, an empty file will be created "
                "to be used as an initial testcase (to randomize input file content and argv)."
            )
            initial_testcases.append("testcase")
            path = os.path.join(fuzz_in, "testcase")
            with open(path, "w"):
                # Do nothing, the file is opened only to create it
                pass

        for file in os.listdir(fuzz_in):
            file_path = os.path.join(fuzz_in, file)

            # Convert the arg list into a byte sequence, with each argument terminated by a \x00 byte
            # Adjust length of the byte sequence to 128B (as required by the wrapper program), padding with \x00 bytes
            # if needed. Note that longer byte sequences are truncated to 128 bytes.
            args_bytes = b"\x00".join(map(lambda x: x.encode('utf-8'), args))
            if len(args_bytes) <= 128:
                args_bytes = args_bytes.ljust(128, b"\x00")
            else:
                args_bytes = args_bytes[:128]

            # Append |args_bytes| to every initial testcase in fuzz_in folder
            with open(file_path, "ab") as f:
                f.write(args_bytes)

        # Add custom mutator in the environment variables of process launching fuzzer instances
        ENV_VARS_COPY['PYTHONPATH'] = CUSTOM_MUTATOR_DIR_PATH
        ENV_VARS_COPY['AFL_PYTHON_MODULE'] = CUSTOM_MUTATOR_NAME
        ret = ["./wrapper", exec_name, "@@"]
        ret.extend(wrapper_args)
        return ret


    
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
        launcher_path = os.path.join(os.getcwd(), "launcher")
        tracer_cmd = [launcher_path, "-o", os.path.join(tracer_out, "overlaps.bin"), "-u", args.heuristic_status, "--"] + executable
        proc = subp.Popen(tracer_cmd) #, stdout = subp.DEVNULL, stderr = subp.DEVNULL)
        proc.wait()
        return

    WORKING_DIR = os.getcwd()
    FUZZ_DIR = os.path.join(WORKING_DIR, args.fuzz_dir)
    FUZZ_OUT = os.path.join(FUZZ_DIR, args.fuzz_out)
    FUZZ_IN = os.path.join(FUZZ_DIR, args.fuzz_in)
    FUZZ_OLDS = os.path.join(FUZZ_DIR, args.olds_dir)

    if args.argv_rand:
        executable = adjust_executable(executable, FUZZ_IN)

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
    fuzz_cmd = ["afl-fuzz", "-Q", "-S" if launch_single_instance else "-M", "Main", "-i", FUZZ_IN, "-o", FUZZ_OUT, "--"] +  executable

    cpus = os.cpu_count()
    if cpus is None:
        print("Warning: cpu count cannot be determined and will be ignored.")
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

    print("Launching Main fuzzer instance...")
    p = subp.Popen(fuzz_cmd, env = ENV_VARS_COPY, stdout = subp.DEVNULL, stderr = subp.DEVNULL)
    slaves = list()
    for i in range(args.slaves):
        cmd = build_slave_cmd(i, FUZZ_IN, FUZZ_OUT, args.experimental, executable)
        slaves.append(subp.Popen(cmd, env = ENV_VARS_COPY, stdout = subp.DEVNULL, stderr = subp.DEVNULL))

    print()
    print("Fuzzer will be interrupted in {0} seconds...".format(stop_after_seconds))

    fuzzer_interrupted_event = t.Event()
    int_timer = t.Timer(stop_after_seconds, send_int, [p, slaves])
    int_timer.start()
    print_empty_progress(fuzzer_interrupted_event)

    tracerThread = t.Thread(target = launchTracer, args = [executable, args, fuzzer_interrupted_event])
    tracerThread.start()

    # Wait for the fuzzer to be interrupted
    for slave in slaves:
        slave.wait()
    p.wait()
    fuzzer_interrupted_event.set()
    print_progress(fuzzer_interrupted_event, True)
    # If the fuzzer terminates before the requested time (e.g. if there are no initial testcases)
    # it is not necessary to wait the whole time to send a SIGINT
    int_timer.cancel()
    print("Fuzzer interrupted")
    print()

    # Wait for the tracer thread to run with all the inputs found by the fuzzer
    tracerThread.join()

    print("Generating textual report...")
    merge_reports(tracer_out)
    print("All finished")


if __name__ == "__main__":
    main()