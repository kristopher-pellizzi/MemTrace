import os
import shutil as su

def restore_private_cpy_files(testcase_path):
    testcase_basename = os.path.basename(testcase_path)
    fuzzer_instance_path = os.path.dirname(testcase_path)
    fuzzer_instance = os.path.basename(fuzzer_instance_path)
    out_path = os.path.dirname(os.path.dirname(fuzzer_instance_path))
    private_cpy_dir = os.path.join(out_path, b'private_cpy')
    testcase_private_cpy = os.path.join(private_cpy_dir, fuzzer_instance, testcase_basename)

    if os.path.exists(testcase_private_cpy):
        for f in os.scandir(testcase_private_cpy):
            name = f.name
            src_path = os.path.join(testcase_private_cpy, name)
            dst_path = os.path.join(testcase_path, name)
            su.copy(src_path, dst_path)