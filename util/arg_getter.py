def get_argv_from_file(path):
    ret = []
    with open(path, "rb") as f:
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