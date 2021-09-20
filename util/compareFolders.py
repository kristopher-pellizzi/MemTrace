#! /usr/bin/env python3

import argparse
import os
import subprocess as subp

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("folder",
        nargs = "+",
        help =  "List of directory paths to be compared",
    )

    parser.add_argument("--folders-only", "-f",
        action = "store_true",
        help =  "Flag used to specify there's no need to check files content. "
                "With this flag enabled, the tool will simply check if the folders have the same structure, and, therefore, "
                "contain the same files and folders in the same way.",
        dest = "folders_only"
    )

    return parser.parse_args()

def strip_final_seps(path):
    if os.sep not in path:
        return path
    i = -1
    while(path[i] == os.sep):
        i = i -1
    
    return path[:i + 1]

def main():
    args = parse_args()

    same = set()
    different = set()
    reason = dict()

    l = args.folder
    l = list(map(strip_final_seps, l))
    original_set = set(l).difference({l[0]})
    reference_folder = l[0]
    reference_initial_depth = len(reference_folder.split(os.sep))
    initial_depths = dict()

    reference = list(map(lambda x: os.path.join(l[0], x), os.listdir(l[0])))
    same.update([l[0]])
    l = list(map(lambda x: (x, list(map(lambda y: os.path.join(x, y), os.listdir(x)))), l[1:]))
    for key, lst in l:
        initial_depths[key] = len(lst[0].split(os.sep)) - 1

    while len(reference) > 0:
        curr_ref = reference.pop()
        curr_basename = os.path.basename(curr_ref)
        l = list(filter(lambda x: curr_ref.split(os.sep)[reference_initial_depth:] in list(map(lambda y: y.split(os.sep)[initial_depths[x[0]]:], x[1])), l))
        tmp_same = set(map(lambda x: x[0], l))
        tmp_diff = original_set.difference(tmp_same)
        for node in tmp_diff.difference(different):
            reason[node] = "{0} contained in reference folder {1}, but not in {2}".format(curr_basename, os.path.dirname(curr_ref), os.path.join(os.path.dirname(curr_ref), node))
        different.update(tmp_diff)

        # Get the paths of folders or files with the same basename as curr_basename
        nodes_to_remove = list(map(lambda y: list(filter(lambda z: z.split(os.sep)[initial_depths[y[0]]:] == curr_ref.split(os.sep)[reference_initial_depth:], y[1]))[0], l))
        if os.path.isdir(curr_ref):
            # Get the children list for each folder with the same name as curr_basename
            children_list = list(map(lambda w: list(map(lambda x: os.path.join(w, x), os.listdir(w))), nodes_to_remove))
            reference.extend(list(map(lambda x: os.path.join(curr_ref, x), os.listdir(curr_ref))))
        elif not args.folders_only and os.path.isfile(curr_ref):
            to_remove = list()
            for i in range(len(l)):
                p = subp.Popen(["diff", curr_ref, nodes_to_remove[i]], stdout = subp.DEVNULL, stderr = subp.STDOUT)
                ret_code = p.wait()
                if ret_code != 0:
                    to_remove.append(l[i])
                    reason[l[i][0]] = "Content of {0} and reference file {1} is different".format(nodes_to_remove[i], curr_ref)
            for el in to_remove:
                l.remove(el)
                different.add(el[0])


        for i in range(len(l)):
            l[i][1].remove(nodes_to_remove[i])
            if os.path.isdir(curr_ref):
                l[i][1].extend(children_list[i])
        
    tmp_diff = set(map(lambda y: y[0], filter(lambda x: len(x[1]) > 0, l)))
    different.update(tmp_diff)
    l = list(filter(lambda x: len(x[1]) > 0, l))

    for node, remained_list in l:
        if len(remained_list) == 1:
            verb = "is"
        else:
            verb = "are"
        reason[node] = "{0} {1} contained in {2}, but not in the reference folder {3}".format(", ".join(remained_list), verb, node, reference_folder)

    same.update(original_set.difference(different))

    print("Are the same:")
    for folder in same:
        print("[*] {0}".format(folder))
    print()
    if len(different) > 0:
        print("Are different:")
        for folder in different:
            print("[*] {0} ({1})".format(folder, reason[folder]))

if __name__ == '__main__':
    main()
        