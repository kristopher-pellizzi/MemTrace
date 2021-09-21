import gdb
import os
import sys
from functools import reduce
from time import sleep

def import_modules(script_dir):
    modules_path = os.path.realpath(os.path.join(script_dir, "..", "python_modules"))
    bin_path = os.path.realpath(os.path.join(script_dir, "..", "bin"))
    sys.path.append(modules_path)
    sys.path.append(bin_path)

s = gdb.execute("print $cwd", to_string = True)
path = os.path.realpath(s.split("=")[1].strip()[1:-1])
sys.path.append(path)
import_modules(path)

from binOverlapParser import parse
from libFinder import findLib, findLibByName

gdb.execute("unset env LINES")
gdb.execute("unset env COLUMNS")
gdb.execute("set follow-fork-mode parent")
gdb.execute("b _start")

s = gdb.execute("print $testcase_path", to_string = True)
testcase_path = os.path.realpath(s.split("=")[1].strip()[1:-1])

overlaps_path = os.path.join(testcase_path, "overlaps.bin")
parsed_ret = parse(bin_report_dir = testcase_path)
load_bases = parsed_ret.load_bases
stack_base = parsed_ret.stack_base

def breakpoint_created(ev):
    def is_watchpoint():
        return ev.location is None and ev.expression is not None

    def is_breakpoint():
        return ev.location is not None and ev.expression is None

    def has_address_location():
        if is_breakpoint():
            return ev.location.strip().startswith("*")
        return is_watchpoint()

    print("Breakpoint set")
    print("It is a watchpoint" if is_watchpoint() else "It is a breakpoint")
    print("It has an address location" if has_address_location() else "It has not an address location")


def convert_addr(addr):
    name, load_base = findLib(addr, load_bases)
    s = gdb.execute("memmap {0}".format(name), to_string = True)
    gdb_base = int(s.split("\n")[0].strip().split()[1].strip(), 0)
    return int(addr, 16) - int(load_base, 16) + gdb_base


def inferior_is_running():
    return gdb.selected_inferior().pid != 0


def on_start(ev):
    # Whenever we hit the breakpoint set to "_start"
    if isinstance(ev, gdb.BreakpointEvent):
        stack_cmd.set_stack_addr()
        gdb.events.stop.disconnect(on_start)
        gdb.execute("r")


def try_get_heap_base(ev):
    if isinstance(ev, gdb.BreakpointEvent):
        if heap_cmd.set_heap_addr():
            gdb.events.stop.disconnect(try_get_heap_base)



class BreakOverlapCommand(gdb.Command):
    def __init__(self):
        super(BreakOverlapCommand, self).__init__("break-overlap", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        if not inferior_is_running():
            gdb.execute("enable 1")
            gdb.execute("r")
        arg = arg.strip()
        arg = "".join(arg.split())
        if arg[0] == '*':
            arg = arg[1:]
        
        try:
            addr = convert_addr(arg)
        except ValueError as e:
            print("Cannot interpret \"{0}\" as an address".format(arg))
            return
        gdb.execute("b *{0}".format(addr))
        gdb.execute("x/i {0}".format(addr))


class OverlapStackCommand(gdb.Command):
    def __init__(self):
        super(OverlapStackCommand, self).__init__("overlap-stack-base", gdb.COMMAND_USER)
        self.stack_addr = None

    def invoke(self, arg, from_tty):
        print(hex(self.stack_addr))

    def set_stack_addr(self):
        s = gdb.execute("print/x ${0}".format(sp), to_string = True)
        addr = int(s.split('=')[1].strip(), 16)
        self.stack_addr = addr


class OverlapHeapCommand(gdb.Command):
    def __init__(self):
        super(OverlapHeapCommand, self).__init__("overlap-heap-base", gdb.COMMAND_USER)
        self.heap_addr = None

    def invoke(self, arg, from_tty):
        if self.heap_addr is None:
            print("Heap is not initialized yet")
        else:
            print(hex(self.heap_addr))

    def set_heap_addr(self):
        def get_sbrk_base():
            nonlocal s
            left_brace_idx = s.find("{")
            right_brace_idx = s.find("}")
            s = s[left_brace_idx + 1 : right_brace_idx]
            splitted = map(lambda x: x.split('='), s.split(','))
            d = dict()
            for key, val in splitted:
                d[key.strip()] = val.strip().split()[0].strip()
            return int(d['sbrk_base'], 16)

        s: str = gdb.execute("print mp_", to_string = True)
        sbrk_base = get_sbrk_base()
        if sbrk_base != 0:
            self.heap_addr = sbrk_base
            return True
        else:
            return False


class OverlapHeapAddrCommand(gdb.Command):
    def __init__(self):
        super(OverlapHeapAddrCommand, self).__init__("overlap-heap-addr", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        if not arg:
            print("Command requires an address as parameter")
            return
        
        if heap_cmd.heap_addr is None:
            print("Heap base address not found yet")
            print("Set a breakpoint somewhere the heap is already initialized and hit it.")
            return

        try:
            addr = int(arg.strip(), 0)
            pin_heap_base = int(findLibByName("Heap", load_bases), 16)
            print(hex(addr - pin_heap_base + heap_cmd.heap_addr))
        except ValueError:
            print("Not valid address")


class OverlapStackAddrCommand(gdb.Command):
    def __init__(self):
        super(OverlapStackAddrCommand, self).__init__("overlap-stack-addr", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        if not arg:
            print("Command requires an address as parameter")
            return

        try:
            addr = int(arg.strip(), 0)
            pin_stack_base = int(stack_base, 16)
            print(hex(addr - pin_stack_base + stack_cmd.stack_addr))
        except ValueError:
            print("Not valid address")


class VmmapCommand(gdb.Command):
    def __init__(self):
        super(VmmapCommand, self).__init__("memmap", gdb.COMMAND_USER)

    def invoke(self, arg: str, from_tty):
        if not inferior_is_running():
            print("No running process found")
            return

        pid = gdb.selected_inferior().pid
        locations = [
            '/proc/%s/maps' % pid,
            '/proc/%s/map'  % pid,
            '/usr/compat/linux/proc/%s/maps'  % pid
        ]

        file_path = None

        for location in locations:
            if not os.path.exists(location):
                continue

            file_path = location
            break

        lib = None
        addr = None
        to_find = None

        if arg:
            try:
                addr = int(arg, 0)
            except ValueError:
                lib = os.path.basename(os.path.realpath(arg.strip()))

            if addr is not None:
                to_find = addr
            else:
                to_find = lib

        printed_line = False
        with open(file_path, "rb") as f:
            while True:
                line = f.readline().decode('utf-8')
                if not line:
                    if not printed_line and to_find is not None:
                        print("{0} not found".format(to_find))
                    break

                splitted = line.strip().split()
                line_lib = splitted[5].strip() if len(splitted) > 5 else ""
                low_addr, high_addr = splitted[0].strip().split('-')
                low_addr = int("0x{0}".format(low_addr), 16)
                high_addr = int("0x{0}".format(high_addr), 16)
                splitted[0] = "\t".join((hex(low_addr), hex(high_addr)))
                line = "\t".join(splitted)

                if lib is not None and lib != os.path.basename(os.path.realpath(line_lib)):
                    continue

                if addr is not None and (addr < low_addr or addr > high_addr):
                    continue

                print("\u001b[31m ", line)
                printed_line = True
        print("\u001b[0m")    

# Define new commands
BreakOverlapCommand()
gdb.execute("alias bro=break-overlap", to_string = True)
VmmapCommand()
stack_cmd = OverlapStackCommand()
OverlapStackAddrCommand()
gdb.execute("alias saddr=overlap-stack-addr")
heap_cmd = OverlapHeapCommand()
OverlapHeapAddrCommand()
gdb.execute("alias haddr=overlap-heap-addr")

# Setup required information
gdb.execute("r", to_string = True)
s: str = gdb.execute("info registers", to_string = True)
regs = set(map(lambda x: x.split()[0], s.splitlines()))
arch = None
sp = None
if "rsp" in regs:
    arch = 64
    sp = "rsp"
elif "esp" in regs:
    arch = 32
    sp = "esp"

# Setup stack and heap base addresses
gdb.events.stop.connect(on_start)
gdb.events.stop.connect(try_get_heap_base)
gdb.execute("r")