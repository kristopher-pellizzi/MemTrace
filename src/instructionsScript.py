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
    include_str = '#include "instructions/{0}/{1}"\n'
    template_path = os.path.join(cwd, "EmulatorTemplate.h")
    emulator_template = None
    existing_instr = set()
    objdir = "debug" if args.debug else "tool"
    instr_obj_dir = os.path.realpath(os.path.join("..", objdir, "instructions"))

    args = {
        "mem": "(MemoryAccess& ma, set<REG>* srcRegs, set<REG>* dstRegs)",
        "reg": "(OPCODE opcode, set<REG>* srcRegs, set<REG>* dstRegs)"
    }
    
    comment = "/*\nThis header file has been automatically generated with a script\n\
Please DO NOT modify this header manually\n*/\n\n"

    with open(template_path, "r") as f:
        emulator_template = f.read()

    for dir in os.scandir(instr_path):
        out_header_file = out_path.format(dir.name.capitalize())
        if os.path.exists(out_header_file):
            mode = "a"
        else:
            mode = "w"
        with open(out_header_file, mode) as f:
            if mode == "w":
                f.write(comment)
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
            for file in os.scandir(obj_dir):
                name, ext = file.name.split(".")
                if not name in existing_instr:
                    path = os.path.join(instr_obj_dir, file.name)
                    os.remove(os.path.join(instr_obj_dir, path))


if __name__ == '__main__':
    main()