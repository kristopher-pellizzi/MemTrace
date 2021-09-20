import gdb
import os
import sys

def import_modules(script_dir):
    modules_path = os.path.realpath(os.path.join(script_dir, "..", "python_modules"))
    bin_path = os.path.realpath(os.path.join(script_dir, "..", "bin"))
    sys.path.append(modules_path)
    sys.path.append(bin_path)

s = gdb.execute("print $cwd", to_string = True)
path = os.path.realpath(s.split("=")[1].strip()[1:-1])
print("$cwd set to \"", path, "\"")
sys.path.append(path)
import_modules(path)

from binOverlapParser import parse
from libFinder import findLib

gdb.execute("unset env LINES")
gdb.execute("unset env COLUMNS")
gdb.execute("set follow-fork-mode parent")
gdb.execute("b _start")

s = gdb.execute("print $testcase_path", to_string = True)
testcase_path = os.path.realpath(s.split("=")[1].strip()[1:-1])

overlaps_path = os.path.join(testcase_path, "overlaps.bin")
parsed_ret = parse(bin_report_dir = testcase_path)
load_bases = parsed_ret.load_bases

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
    s = gdb.execute("vmmap {0}".format(name), to_string = True)
    gdb_base = int(s.split("\n")[1].strip().split()[1].strip(), 0)
    return addr - load_base + gdb_base


def inferior_is_running():
    return gdb.selected_inferior().pid != 0


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
            addr = convert_addr(int(arg, 0))
        except ValueError as e:
            print("Cannot interpret \"{0}\" as an address".format(arg))
            return
        gdb.execute("b *{0}".format(addr))


class VmmapCommand(gdb.Command):
    def __init__(self):
        super(VmmapCommand, self).__init__("vmmap", gdb.COMMAND_USER)

    def invoke(self, arg: str, from_tty):
        # TODO: now add arg, and return only the requested addresses in case it is a string with the library name or
        # a string with an address
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
                lib = arg.strip()

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

                if lib is not None and lib != line_lib:
                    continue

                if addr is not None and (addr < low_addr or addr > high_addr):
                    continue

                # Note: the ANSI color code is just noise, used in order to allow function
                # |convert_addr| to use pwndbg's vmmap command as well (it prepends an ANSI color code).
                # Also, pwndbg prints a table header before printing the result of the command.
                # In order to get the correct line both if we are using pwndbg or our internal command,
                # let's add a header as well.
                if not printed_line:
                    print("\u001b[31mVMMAP:")
                print("\u001b[31m ", line)
                printed_line = True
        print("\u001b[0m")    


BreakOverlapCommand()
gdb.execute("alias bro=break-overlap")
gdb.execute("r")

# If there is no vmmap command (pwndbg is not installed), then use the internal
# definition of the vmmap command
try:
    s = gdb.execute("vmmap", to_string = True)
except gdb.error:
    VmmapCommand()

s = gdb.execute("vmmap /usr/lib/x86_64-linux-gnu/libc-2.31.so", to_string = True)
gdb_base = int(s.split("\n")[1].strip().split()[1].strip(), 0)
print("GDB_BASE: ", gdb_base)