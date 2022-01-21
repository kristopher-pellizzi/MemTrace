# MemTrace

*MemTrace* is a dynamic binary analysis tool designed to report **memory overlaps**, that is uninitialized memory reads together with all the memory write accesses overlapping the same memory location.

*MemTrace* makes use of Dynamic Binary Instrumentation to analyze all the instructions performed during a program's execution and keep track of every memory access. Eventually, the tool will generate a report containing all the detected overlaps.

The main goal of *MemTrace* is to help the user identify instructions that can possibly lead to information disclosure (i.e. leaks). Indeed, by inspetting the generated report, the user will know where an uninitialized memory read is performed and which instructions previously wrote in the same memory location, thus understanding whether it is possible to control **what** or **where** to read.



## Supported platforms

Currently, *MemTrace* only supports 64 bits x86 machines with a Linux distribution installed.
The tool works as is if the installed operating system has *glibc* installed; otherwise, it will be required to implement a custom **malloc handler** header.
More info can be found in section ["Malloc handlers"](#Malloc-handlers).

Intel PIN, which is used as a DBI framework, only support x86 and x86_64 architectures.
Support for more operating systems will be added in the future.



## Pre-requisites

- Install from the system package manager (e.g. *apt* in Ubuntu) the following packages:

  - ninja-build 
  - libglib2.0-dev 
  - make 
  - gcc 
  - g++ 
  - pkg-config 
  - python3 
  - python3-pip
  - gdb

  In Ubuntu, this is done running the command "*sudo apt-get install ninja-build libglib2.0-dev make gcc g++ pkg-config python3 python3-pip gdb*".

- Using *pip*, install the packages listed in *requirements.txt*.
  This can be easily done by running "*python3 -m pip install -r requirements.txt*".

  

## Building

1. Download *MemTrace* repository as '*git clone https://github.com/kristopher-pellizzi/MemTraceThesis*'

2. Digit '*cd MemTraceThesis*'

3. Run '*make*'. ***Note***: *this operation may take a while.*

   

## Usage

After building, the tool and all the third-party tools (i.e. [AFL++](https://github.com/AFLplusplus/AFLplusplus) and [Intel PIN](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html)) will be compiled and *MemTrace* is ready to run.

The launcher script can be found in folder *bin* and it is called *memTracer.py*.
There are 2 modes of launching *MemTrace*:

- As a standalone tool:
  run '*memTracer.py -x -- path/to/executable arg1 arg2 --opt1 optarg1 --opt2*'.

  This way, *MemTrace* will analyze the given executable using the passed arguments and will generate a report containing all the memory overlaps detected during that specific execution.
  The generated report will be in a custom binary format. To generate the human-readable version, launch script *binOverlapsParser.py* from folder *bin* passing the binary report as a parameter.

- Combined with *AFL++:*
  this mode will launch some instances of the fuzzer to generate an input corpus for the given executable. The generated inputs will be used by some parallel processes of *MemTrace* to analyze the executable.
  There are many options available to configure this mode. The format of the command to use this mode is the following:
  *memTracer.py [OPTIONS] -- path/to/executable [FIXED_ARGS] [@@]*.

  - FIXED_ARGS is an optional list of arguments to pass to the executable which are always the same over all the executions
  - @@ is an input file placeholder. When it is used, the tool will replace it with the path of a generated input, thus changing the content of the input file at each execution. 
    ***NOTE***: *if @@ is not used, the tool will implicitly infer that the executable reads from stdin. So, both AFL++ and MemTrace will use the generated inputs as stdin for the executable*.

  

## Manual

Usage: memTracer.py [-h] [--disable-argv-rand] [--single-execution] [--keep-ld] [--unique-access-sets] [--disable-string-filter] [--str-opt-heuristic {OFF,ON,LIBS}] [--fuzz-out FUZZ_OUT] [--out TRACER_OUT] [--fuzz-dir FUZZ_DIR] [--fuzz-in FUZZ_IN] [--backup OLDS_DIR] [--admin-priv] [--time EXEC_TIME] [--slaves SLAVES] [--processes PROCESSES] [--ignore-cpu-count] [--experimental] [--no-fuzzing] [--stdin] [--store-tracer-out] [--dict DICTIONARY] -- /path/to/executable [EXECUTABLE_ARGS]

optional arguments:

- -h, --help:	show this help message and exit

- --disable-argv-rand, -r:	flag used to disable command line argument randomization (default: True)

- --single-execution, -x:	flag used to specify that the program should be executed only once with the given parameters.This may be useful in case the program does not read any input (it is useless to run the fuzzer in these cases) and it requires to run with very specific argv arguments (e.g. utility cp from coreutils requires correctly formatted, existing paths to work). Options *-f, -d, -i, -b, -a, -t, -s, -p, --ignore-cpu-count, -e and --no-fuzzing* (i.e. all options related to the fuzzing task) will be ignored. (default: False)

- --keep-ld:    flag used to specify the tool should not ignore instructions executed from the loader's library (ld.so in Linux). By default, they are ignored because there may be a degree of randomness which makes the instructions from that library always change between executions of the same command. This may cause some confusion in the final report. For this reason they are ignored. (default: False)

- --unique-access-sets, -q:    this flag is used to report also those uninitialized read accesses which have a unique access set. Indeed, by default, the tool only reports read accesses that have more access sets, which are those that can read different bytes according to the input. This behaviour is thought to report only those uninitialized reads that are more likely to be interesting. By enabling this flag, even the uninitialized read accesses that always read from the same set of write accesses are reported in the merged report. (default: False)

- --disable-string-filter:    this flag allows to disable the filter which removes all the uninitialized read accesses which come from a string function (e.g. strcpy, strcmp...). This filter has been designed because string functions are optimized, and because of that, they very often read uninitialized bytes, but those uninitialized reads are not relevant. An heuristic is already used to try and reduce this kind of false positives. However, when we use the fuzzer and merge all the results, the final report may still contain a lot of them. (default: False)

- --str-opt-heuristic {OFF,ON,LIBS}, -u {OFF,ON,LIBS}:    option used to specify whether to enable the heuristic thought to remove the high number of not relevant uninitialized reads due to the optimized versions of string operations (e.g. strcpy). The aforementioned heuristic can be either disabled, enabled or partially enabled. If the heuristic is disabled, it will never be applied. If it is enabled, it will be always applied. If it is partially enabled, it is applied only for accesses performed by code from libraries (e.g. libc). By default, the heuristic is partially enabled. Possible choices are: 
  OFF => Disabled
  ON => Enabled
  LIBS => Partially enabled. 
  WARNING: when the heuristic is enabled, many false negatives may arise. If you want to avoid false negatives due to the application of the heuristic, you can simply disable it, but at a cost. Indeed, in a single program there may be many string operations, and almost all of them will generate uninitialized reads due to the optimizations (e.g. strcpy usually loads 32 bytes at a time, but then checks are performed on the loaded bytes to avoid copying junk). So, the execution of the program with the heuristic disabled may generate a huge number of uninitialized reads. Those reads actually load uninitialized bytes from memory to registers, but they may be not relevant. Example: strcpy(dest, src), where |src| is a 4 bytes string, may load 32 bytes from memory to SIMD registers. Then, after some checks performed on the loaded bytes, only the 4 bytes belonging to |src| are copied in the pointer |dest|. (default: LIBS)

- --fuzz-out FUZZ_OUT, -f FUZZ_OUT:    Name of the folder containing the results generated by the fuzzer (default: fuzzer_out)

- --out TRACER_OUT, -o TRACER_OUT:    Name of the folder containing the results generated by the tracer (default: tracer_out)

- --fuzz-dir FUZZ_DIR, -d FUZZ_DIR:    Name of the folder containing all the requirements to run the fuzzer. This folder must already exist and contain all the files/directories required by the fuzzer. (default: out)

- --fuzz-in FUZZ_IN, -i FUZZ_IN: Name of the folder containing the initial testcases for the fuzzer. This folder must already exist and contain the testcases (default: in)

- --backup OLDS_DIR, -b OLDS_DIR: Name of the folder used to move old results of past executions before running the tracer again. (default: olds)

- --admin-priv, -a:    Flag used to specify the user has administration privileges (e.g. can use sudo on Linux). This can be used by the launcher in order to execute a configuration script that, according to the fuzzer's manual, should speedup the fuzzing task. (default: False)

- --time EXEC_TIME, -t EXEC_TIME:    Specify fuzzer's execution time. By default, the value is measured in seconds. The following modifiers can be used: 's', 'm', 'h', 'd', 'w', 'M', to specify time respectively in seconds, minutes, hours, days, weeks, Months (intended as 30 days months) (default: 60)

- --slaves SLAVES, -s SLAVES:    Specify the number of slave fuzzer instances to run. The fuzzer always launches at least the main instance. Launching more instances uses more resources, but allows to find more inputs in a smaller time span. It is advisable to use this option combined with -p, if possible. Note that the total amount of launched processes won't be higher than the total number of available cpus, unless --ignore-cpu-count flag is enabled. However, it is very advisable to launch at least 1 slave instance. (default: 0)

- --processes PROCESSES, -p PROCESSES:    Specify the number of processes executing the tracer. Using more processes allows to launch the tracer with more inputs in the same time span. It is useless to use many processes for the tracer if the fuzzer finds new inputs very slowly. If there are few resources available, it is therefore advisable to dedicate them to fuzzer instances rather then to tracer processes. Note that the total amount of launched processes won't be higher than the total number of available cpus, unless --ignore-cpu-count flag is enabled. (default: 1)

- --ignore-cpu-count :    Flag used to ignore the number of available cpus and force the number of processes specified with -s and -p to be launched even ifthey are more than that. (default: False)

- --experimental, -e :    Flag used to specify whether to use or not experimental power schedules (default: False)

- --no-fuzzing     Flag used to avoid launching the fuzzer. WARNING: this flag assumes a fuzzing task has already been executed and generated the required folders and files. It is possible to use a fuzzer different from the default one. However, the structure of the output folder must be the same as the one generated by AFL++. NOTE: this flag discards any fuzzer-related option (i.e. -s, -e, -t, -a). The other options may be still used to specify the paths of specific folders. (default: False)

- --stdin:    Flag used to specify that the input file should be read as stdin, and not as an input file. Note that this is meaningful **only when '--no-fuzzing' is enabled**.If this flag is used, but '--no-fuzzing' is not, it is simply ignored. (default: False)

- --store-tracer-out  This option allows the tracer thread to redirect both stdout and stderr of every spawned tracer process to a file saved in the same folder where the input file resides. (default: False)

- --dict DICTIONARY: Path of the dictionary to be used by the fuzzer in order to produce new inputs (default: None)

After the arguments for the script, the user must pass '--' followed by the executable path and the arguments that should be passed to it. If it reads from an input file, write '@@' instead of the input file path. It will be automatically replaced by the fuzzer.

Example: ./memTracer.py -- /path/to/the/executable arg1 arg2 --opt1 @@

Remember to NOT PASS the input file as an argument for the executable, but use @@. It will be automatically replaced by the fuzzer starting from the initial testcases and followed by the generated inputs.



## Extending *MemTrace*

There are a few ways that allow to extend the tool very easily, if it is needed.



### System call handlers

*MemTrace* uses a **system call manager** to keep track of memory accesses performed during the execution of system calls.

Since system calls are defined by the platform ABI, the system call manager in turns requires to have knowledge about the system calls available in the underlying operating system and about their behavior.
So, a **system call handler** is implemented for each available system call which performs any memory access.

The system call handlers for Linux x86-64 machines are already implemented (at least most of them) into the header file *src/x86_64_linux_syscall_handlers.h*.
If the user requires to add new syscall handlers or modify any of the existing ones, it is sufficient to modify that header file.

If instead the user wants to run *MemTrace* on a not supported platform, it is required to manually implement the required system call handlers. To do this, it is enough to copy **src/x86_64_linux_syscall_handlers.h*, remove the existing handlers and implement the handlers for the platform in use.
Then, it is necessary to include the created header file into *src/SyscallHandler.h* according to the platform in use (see *src/Platform.h* to check the macro defined when a certain architecture or OS is detected, if required).

Of course, after all the changes have been done, it is required to re-build *MemTrace* to apply them.



### Malloc handlers

Similarly to what we said for system calls, *MemTrace* requires some specific information also about *dynamic allocation functions* like *malloc, realloc, free*, etc...
More specifically, *MemTrace* requires to have information about the layout of allocated chunks and about how those functions store chunks metadata.

Dynamic allocations functions are not stadard, so every operating system may have its own implementation, and it is also possible to create a custom implementation.
For this reason, *src/x86_64_linux_malloc_handlers.h* will implement some functions whose goal is to return to *MemTrace* the information it needs, when it requires it.

Currently, the malloc handlers for x86_64 machines using *glibc-2.31* are implemented. If needed, it is possible to copy that header file and implement the handlers for different implementations of the dynamic allocation functions. 
After the required functions have been implemented, it is necessary to include the header file into *src/MallocHandler.h* according to the platform in use.

*MemTrace* must be re-built to apply the changes.

If a custom implementation of malloc is used, *MemTrace* must be built with the following command to define macro CUSTOM_ALLOCATOR:
'*make tool_custom_malloc*'.



### Instruction handlers

*MemTrace* performs a taint analysis to keep track of the transfers of uninitialized bytes.

Each instruction transferring bytes may have a different way of performing the data transfer.
For this reason, *MemTrace* makes use of **instruction handlers** which are meant to manage transfers performed by the executed instructions.

Instruction handlers are divided in 2 types:

- **Memory instruction handlers** (*set/instructions/mem*): they manage instructions performing at least 1 memory access (either reading or writing)
- **Register instruction handlers** (set/instructions/reg): they manage instructions transferring bytes from some src register to some est registers

For each type of instruction handler, there's a default handler designed to handle **most** of the instructions.
For those instructions not correctly handled by the default ones, a specific instruction handler has been implemented.

Of course, it is not feasible to implement a specific handler for all the instructions of the ISA.
So, only the most frequent instructions requiring a specific handler have been implemented.

If it is required to add more instructions handler, it is sufficient to copy one of the handlers in the corresponding folder, according to the type, and implement the handler so that it mimics the transfer of bytes performed by instruction itself.

After that, it is sufficient to re-build *MemTrace*.



# Docker

*MemTrace* repository also contains a *Dockerfile* which easily allows to create a container configured to use *MemTrace* immediately.

The *Dockerfile* has also been used to create a testing environment, and therefore perform all the tests for the validation of the tool in a reproducible way.

In order to create the container, it is of course required to have *Docker* installed (https://www.docker.com/).
Then, move into the root folder of the repository (where the *Dockerfile* is stored) and run '*docker build -t memtrace .*'.
***NOTE***: *this operation may take a while to complete*

Finally, simply run '*docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --rm -it memtrace*'.

- --cap-add=SYS_PTRACE is required to allow *Intel PIN* call *ptrace* to instrument binaries
- --security-opt seccomp=unconfined is required to allow *AFL++* to fuzz binaries
- --rm will remove the container when it is closed
- -it allows the container to use the terminal as stdin/stdout



# Tests

As mentioned in section [Docker](#Docker), *MemTrace* have been tested in a docker container in order to make them easily reproducible.



## Timing test

The script *test.sh* which is found in the root folder of the repository will execute *MemTrace* with all the utilities from *Coreutils-8.32* which can be found in folder *Tests*.

**Before** launching the script, it is required to manually *untar* the archive and compile the project.
When the script finished executing, a file called *execution_times.csv* will be available in folder *Tests*.
In order to be clearer, it is possible to import the csv file into a spreadsheet program, such as *MS Excel*.



## Functional tests

### Uninitialized reads detection

*MemTrace* has been tested using some binaries with known uninitialized read vulnerabilities.
In this set of tests, the goal was to check whether *MemTrace* was able to report the known vulnerability when the binary was executed with an input triggering it.

The script *Tests/detect_test.py* will automatically perform the same set of tests, generating folder *Tests/detect_results*, which contains a directory for each executed test, in turns containing the generated binary report.



### Fuzzing efficacy

The same set of binaries used for [uninitialized reads detection](#Uninitialized-reads-detection) test has been used to verify the efficacy of the combined execution of *MemTrace* with *AFL++*.

In this case, the goal was verifying whether the fuzzer was able to generate inputs allowing *MemTrace* to detect an existing vulnerability automatically, using, as initial inputs, some random inputs.

The script *Tests/fuzzing_test.py* will automatically perform the same set of tests.
For each binary, 4 instances of the fuzzer will be launched, which will fuzz the binary for **8 hours**. In the meanwhile, each input generated by AFL++ is used to execute the binary with *MemTrace*.

Unfortunately, different binaries required different configurations of *MemTrace*, so it is not possible to perform **all** the tests in parallel.

Eventually, folder *Tests/fuzzing_results* will contain a directory for each binary, containing, among other things, the report merging the results of all the executions of the binary.

***NOTE***: *as mentioned, the fuzzer will fuzz the binary for 8 hours using 4 parallel instances. Moreover, 4 parallel instances of *MemTrace* will handle the inputs generated by AFL++. So, this test should be done on machines with many resources available (e.g. a remote server) and, since some tests require to be serialized because of the different configuration of *MemTrace*, it may require many hours to complete.