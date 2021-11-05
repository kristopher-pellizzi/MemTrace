import os
import argparse

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("--debug", "-d",
        action = "store_true",
        help = "Flag to produce debug enabled object files",
        dest = "debug"
    )

    return parser.parse_args()

def main():
    args = parse_args()
    cwd = os.getcwd()
    out_path = os.path.join(cwd, "{0}Instructions.h")

    instr_path = os.path.join(cwd, "instructions")
    mem_header_path = os.path.join(cwd, "MemInstructions.h")
    reg_header_path = os.path.join(cwd, "RegInstructions.h")
    included_headers = set()

    comment = "/*\nThis header file has been automatically generated with a script\n\
Please DO NOT modify this header manually\n*/\n\n"

    if not os.path.exists(mem_header_path):
        with open(mem_header_path, "w") as f:
            f.write(comment)
            base_path = os.path.join("instructions", "mem")
            f.write("#include \"{0}\"\n".format(os.path.join(base_path, "DefaultLoadInstruction.h")))
            f.write("#include \"{0}\"\n".format(os.path.join(base_path, "DefaultStoreInstruction.h")))

    if not os.path.exists(reg_header_path):
        with open(reg_header_path, "w") as f:
            f.write(comment)
            base_path = os.path.join("instructions", "reg")
            f.write("#include \"{0}\"\n".format(os.path.join(base_path, "DefaultPropagateInstruction.h")))

    with open(mem_header_path, "r") as f:
        lines = f.readlines()
        lines = map(lambda x: x.split()[1], filter(lambda y: y.startswith("#include"), lines))
        headers = set(map(lambda x: os.path.split(x)[1][:-1], lines))
        included_headers.update(headers)

    with open(reg_header_path, "r") as f:
        lines = f.readlines()
        lines = map(lambda x: x.split()[1], filter(lambda y: y.startswith("#include"), lines))
        headers = set(map(lambda x: os.path.split(x)[1][:-1], lines))
        included_headers.update(headers)

    include_str = '#include "{0}"\n'.format(os.path.join("instructions", "{0}", "{1}"))
    template_path = os.path.join(cwd, "EmulatorTemplate.h")
    emulator_template = None
    existing_instr = set()
    objdir = "debug" if args.debug else "tool"
    instr_obj_dir = os.path.realpath(os.path.join("..", objdir, "instructions"))

    args = {
        "mem": "(MemoryAccess& ma, list<REG>* srcRegs, list<REG>* dstRegs)",
        "reg": "(OPCODE opcode, list<REG>* srcRegs, list<REG>* dstRegs)"
    } 

    with open(template_path, "r") as f:
        emulator_template = f.read()

    for dir in os.scandir(instr_path):
        out_header_file = out_path.format(dir.name.capitalize())
        with open(out_header_file, "a") as f:
            src_path = os.path.join(instr_path, dir.name)
            for file in os.scandir(src_path):
                name, ext = file.name.split(".")
                if ext != "cpp":
                    continue
                header_file = "{0}.h".format(name)
                include_guard = name.upper()
                header_path = os.path.join(src_path, header_file)
                existing_instr.add(name)

                # If the header file already exists, it has already been generated and added to the header file
                if os.path.exists(header_path):
                    print("{0} already exists".format(header_path))
                    if not header_file in included_headers:
                        f.write(include_str.format(dir.name, header_file))
                    continue

                # Generate header file
                print("Generating header file for {0}".format(file.name))
                with open(header_path, "w") as header:
                    header.write(emulator_template.format(name, include_guard, dir.name.capitalize(), args[dir.name]))
                
                # Insert include statement in Instructions.h
                f.write(include_str.format(dir.name, header_file))
            f.write("")

        # Remove not-existing instructions object files (if any)
        obj_dir = os.path.join(instr_obj_dir, dir.name)
        deleted_lines = False

        for file in os.scandir(obj_dir):
            name, ext = file.name.split(".")
            if not name in existing_instr:
                path = os.path.join(obj_dir, file.name)
                os.remove(path)
                header_name = "{0}.h".format(name)
                if header_name in included_headers:
                    deleted_lines = True
                included_headers.remove(header_name)

        if not deleted_lines:
            continue

        header_path = os.path.join(cwd, "{0}Instructions.h".format(dir.name.capitalize()))
        lines = None
        with open(header_path, "r") as f:
            lines = f.readlines()

        lines = list(filter(lambda x: os.path.split(x.split()[1])[1] in included_headers, filter(lambda y: y.startswith("#include"), lines)))

        with open(header_path, "w") as f:
            f.write(comment)
            for line in lines:
                f.write(line)


if __name__ == '__main__':
    main()