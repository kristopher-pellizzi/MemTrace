gdb_libc_base_val = None
gdb_stack_base_val = None
pin_libc_base_val = None
pin_stack_base_val = None

def libc_addr(pin_addr):
    return hex(gdb_libc_base_val + (pin_addr - pin_libc_base_val))


def stack_addr(pin_addr):
    return hex(gdb_stack_base_val - (pin_stack_base_val - pin_addr))


def set_gdb_libc_base(addr):
    global gdb_libc_base_val
    gdb_libc_base_val = addr


def set_gdb_stack_base(addr):
    global gdb_stack_base_val
    gdb_stack_base_val = addr

def set_pin_libc_base(addr):
    global pin_libc_base_val
    pin_libc_base_val = addr


def set_pin_stack_base(addr):
    global pin_stack_base_val
    pin_stack_base_val = addr

def gdb_libc_base():
    return gdb_libc_base_val


def gdb_stack_base():
    return gdb_stack_base_val

def pin_libc_base():
    return pin_libc_base_val

def pin_stack_base():
    return pin_stack_base_val
