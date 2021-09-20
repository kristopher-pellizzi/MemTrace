import sys
import os

modules_path = os.path.realpath(os.path.join(sys.path[0], "..", "python_modules"))
bin_path = os.path.realpath(os.path.join(sys.path[0], "..", "bin"))
sys.path.append(modules_path)
sys.path.append(bin_path)