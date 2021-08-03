import gdb

libc_path = input("")
addr_str = input("")

gdb.execute("file " + libc_path)
o = gdb.execute("info symbol " + addr_str, to_string = True)
print(o.split()[0])
gdb.execute("quit")