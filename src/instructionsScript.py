import os

def main():
    cwd = os.getcwd()
    out_path = os.path.join(cwd, "Instructions.h")

    if os.path.exists(out_path):
        mode = "a"
    else:
        mode = "w"

    instr_path = os.path.join(cwd, "instructions")
    include_str = '#include "instructions/{0}"\n'
    template_path = os.path.join(cwd, "EmulatorTemplate.h")
    emulator_template = None
    with open(template_path, "r") as f:
        emulator_template = f.read()

    with open(out_path, mode) as f:
        for file in os.scandir(instr_path):
            name, ext = file.name.split(".")
            if ext != "cpp":
                continue
            header_file = "{0}.h".format(name)
            include_guard = name.upper()
            header_path = os.path.join(instr_path, header_file)

            # If the header file already exists, it has already been generated and added to the header file
            if os.path.exists(header_path):
                print("{0} already exists".format(header_path))
                continue

            # Generate header file
            print("Generating header file for {0}".format(file.name))
            with open(header_path, "w") as header:
                header.write(emulator_template.format(name, include_guard))
            
            # Insert include statement in Instructions.h
            f.write(include_str.format(header_file))


if __name__ == '__main__':
    main()