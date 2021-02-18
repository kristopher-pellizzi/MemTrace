from binascii import b2a_hex

class ParseError(Exception):
    def __init__(self, file, message="Error while parsing"):
        super().__init__(message)
        print("Error @ byte " + str(file.tell()))
        file.seek(-6, 1)
        print(b"... " + file.read(50))
        print("            ^")
        print("            |")


class ReportWriter():
    
    def __init__(self):
        self.file = open("overlaps.log", "w")

    def __del__(self):
        self.file.close()
    
    def writelines(self, lines):
        for line in lines:
            self.file.writelines([line, "\n"])



def accept(f, acceptable):
    if(f.read(1) == acceptable):
        return True
    f.seek(-1, 1)
    return False


def expect(f, expected_list):
    read_byte = f.read(1)
    for expected_val in expected_list:
        if read_byte == expected_val:
            return read_byte
    raise ParseError(f, "Expected byte not found.")


def parse_integer(f):
    int_val = b""
    is_negative = accept(f, b"-")

    read_byte = f.read(1)
    while(read_byte != b";"):
        if read_byte == b"":
            raise ParseError(f, "EOF reached while parsing integer")
        int_val += read_byte
        read_byte = f.read(1)
    
    if int_val == b"":
        return 0
    
    ret = int(int_val, 16)
    return - ret if is_negative else ret


def parse_address(f, reg_size):
    addr = f.read(reg_size)
    if len(addr) != reg_size:
        raise ParseError(f, "EOF reached while parsing an address")
    addr = int(b2a_hex(addr[::-1]), 16)
    return hex(addr)


def parse_string(f):
    string = b""
    read_byte = f.read(1)
    while read_byte != b";":
        if(read_byte == b""):
            raise ParseError(f, "EOF reached while parsing a string")
        string += read_byte
        read_byte = f.read(1)
    return string.decode("utf-8")


def print_table_header(header):
    global rw
    l1 = "==============================================="
    l2 = header
    rw.writelines([l1, l2, l1])


def print_table_entry(entry):
    global rw
    rw.writelines([entry])


def print_table_footer():
    global rw
    l1 = "==============================================="
    rw.writelines([l1, l1, "", "", "", ""])


def parse_full_overlap_entry(f, reg_size):
    # Seek required because the call to 'accept' previously done to check there were other entries moved the cursor
    # ahead of one byte. So we need to set its position 1 byte behind in order to verify whether it is an 
    # uninitialized read or not
    f.seek(-1, 1) 
    is_uninitialized_read = True if f.read(1) == b"\x0a" else False
    ip = parse_address(f, reg_size)
    actual_ip = parse_address(f, reg_size)
    disassembled_inst = parse_string(f)
    access_type = expect(f, [b"\x1a", b"\x1b"])
    access_size = parse_integer(f)
    sp_offset = parse_integer(f)
    bp_offset = parse_integer(f)

    # Emit full overlap table entry
    str_list = []
    str_list.append("*" if is_uninitialized_read else "")
    str_list.append(ip + " (" + actual_ip + "):")
    str_list.append("\t" if len(disassembled_inst) > 0 else " ")
    str_list.append("W " if access_type == b"\x1a" else "R ")
    str_list.append(str(access_size) + " B @ ")
    str_list.append("(sp ")
    str_list.append("+ " if sp_offset >= 0 else "- ")
    str_list.append(str(abs(sp_offset)) + "); ")
    str_list.append("(bp ")
    str_list.append("+ " if bp_offset >= 0 else "- ")
    str_list.append(str(abs(bp_offset)) + ")")

    print_table_entry("".join(str_list))



def parse_full_overlap(f, reg_size):
    addr = parse_address(f, reg_size)
    access_size = parse_integer(f)
    # Emit table header
    header = addr + " - " + str(access_size)
    print_table_header(header)
    while(accept(f, b"\x0a") or accept(f, b"\x0b")):
        parse_full_overlap_entry(f, reg_size)


rw = ReportWriter()

with open("overlaps.bin", "rb") as f:
    read_byte = f.read(1)
    while(read_byte != b"\x00"):
        if(read_byte == b""):
            raise ParseError(f, "Overlap beginning (byte 0x00) not found")
        read_byte = f.read(1)
    
    reg_size = parse_integer(f)

    # While there are overlaps...
    while not accept(f, b"\x01"):
        parse_full_overlap(f, reg_size)

print("Finished parsing binary file. Textual reports created")