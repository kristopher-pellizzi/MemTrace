#!/usr/bin/env python3

import os
import shutil as su
import subprocess as subp
import threading as t
import signal as sig
import time

PROGRESS_UNIT = 0
PROGRESS_LEN = 30
PROGRESS = 0
LAST_PROGRESS_TASK = None


def launchTracer(exec_cmd, fuzz_dir, fuzz_int_event: t.Event):
    fuzz_out = os.path.join(fuzz_dir, "out")
    tracer_out = os.path.join(fuzz_dir, "tracer_out")
    inputs_dir = os.path.join(fuzz_out, "Main", "queue")
    launcher_path = os.path.join(os.getcwd(), "launcher")
    tracer_cmd = [launcher_path] + [exec_cmd]
    traced_inputs = {".state"}
    os.mkdir(tracer_out)
    new_inputs_found = True

    while not os.path.exists(inputs_dir):
        time.sleep(1)

    # Loop interrupts if and only if there are no new_inputs and the fuzzer has been interrupted
    while new_inputs_found or not fuzz_int_event.is_set():
        inputs = set()
        for f in os.scandir(inputs_dir):
            inputs.add(f.name)

        new_inputs = inputs.difference(traced_inputs)
        if(len(new_inputs) == 0):
            new_inputs_found = False
            print("Waiting the fuzzer for new inputs")
            time.sleep(10)
            continue

        new_inputs_found = True

        for f in new_inputs:
            print("{0} is running".format(f))
            input_folder = os.path.join(tracer_out, f)
            os.mkdir(input_folder)
            input_cpy_path = os.path.join(input_folder, "input")
            su.copy(os.path.join(inputs_dir, f), input_cpy_path)
            print()
            full_cmd = tracer_cmd + [input_cpy_path]
            p = subp.Popen(full_cmd)
            p.wait()
            su.move(os.path.join(os.getcwd(), "overlaps.bin"), os.path.join(input_folder, "overlaps.bin"))
            print()

        traced_inputs.update(new_inputs)
        time.sleep(10)


def main():
    global PROGRESS
    global PROGRESS_UNIT

    def send_int(process: subp.Popen):
        process.send_signal(sig.SIGINT)


    def print_empty_progress():
        global PROGRESS_LEN

        out = ["|"]
        out.append(" " * PROGRESS_LEN)
        out.append("|")
        print("".join(out))

        LAST_PROGRESS_TASK = t.Timer(10, print_progress)
        LAST_PROGRESS_TASK.start()


    def print_progress(finished = False):
        global PROGRESS
        global PROGRESS_LEN
        global LAST_PROGRESS_TASK

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

        if not finished:
            LAST_PROGRESS_TASK = t.Timer(10, print_progress)
            LAST_PROGRESS_TASK.start()
        elif not LAST_PROGRESS_TASK is None:
            LAST_PROGRESS_TASK.cancel()


    WORKING_DIR = os.getcwd()
    FUZZ_DIR = os.path.join(WORKING_DIR, "afl_test")
    FUZZ_OUT = os.path.join(FUZZ_DIR, "out")
    FUZZ_IN = os.path.join(FUZZ_DIR, "in")
    FUZZ_OLDS = os.path.join(FUZZ_DIR, "olds")

    tracer_out = os.path.join(FUZZ_DIR, "tracer_out")
    print(tracer_out)
    if os.path.exists(tracer_out):
        raise IOError("Directory {0} already exists. Please move or remove it before running the tracer.".format(tracer_out))

    # If folder "out" already exists in the fuzzing directory,
    # copy it in a subfolder "olds" so that a new one can be created
    if os.path.exists(FUZZ_OUT):
        last_old = os.path.join(FUZZ_DIR, "last_old")
        last_count = None

        if not os.path.exists(last_old):
                last_count = 0
                with open(last_old, "w") as f:
                    f.write(str(last_count))
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
                        last_count = 0

                f.seek(0)
                f.write(str(last_count))
                f.truncate()

        new_path = os.path.join(FUZZ_DIR, str(last_count))
        os.rename(FUZZ_OUT, new_path)
        su.move(new_path, os.path.join(FUZZ_OLDS, str(last_count)))

    # Start AFL fuzzer
    executable = os.path.join(WORKING_DIR, "Tests", "coreutils", "src", "od")
    minutes = 1
    p = subp.Popen(["sudo", "afl-system-config"])
    p.wait()
    fuzz_cmd = ["afl-fuzz", "-Q", "-M", "Main", "-i", FUZZ_IN, "-o", FUZZ_OUT, "--", executable, "@@"]
    stop_after_seconds = 60 * minutes
    PROGRESS_UNIT = stop_after_seconds // PROGRESS_LEN
    p = subp.Popen(fuzz_cmd, stdout = subp.PIPE, stderr = subp.PIPE)
    print()
    print("Fuzzer will be interrupted in {0} seconds...".format(stop_after_seconds))

    t.Timer(stop_after_seconds, send_int, [p]).start()
    print_empty_progress()

    fuzzer_interrupted_event = t.Event()
    tracerThread = t.Thread(target = launchTracer, args = [executable, FUZZ_DIR, fuzzer_interrupted_event])
    tracerThread.start()

    # Wait for the fuzzer to be interrupted
    p.wait()
    fuzzer_interrupted_event.set()
    print_progress(True)
    print("Fuzzer interrupted")
    print()

    # Wait for the tracer thread to run with all the inputs found by the fuzzer
    tracerThread.join()
    print("All finished")


if __name__ == "__main__":
    main()