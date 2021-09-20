#! /usr/bin/env python3

import argparse
import os.path

def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("report_path",
        help =  "Path of the report from where the count is performed"
    )

    return parser.parse_args()


def main():
    args = parse_args()
    count = 0
    report_path = os.path.realpath(args.report_path)

    print("Opening file {0}".format(report_path))
    with open(report_path, "r") as f:
        line = f.readline()
        while line != "":
            line = line[:-1]
            if line == "===============================================":
                count += 1
            line = f.readline()
    
    # Note that in the report there are 4 lines of symbol  '=' for each reported overlap
    count //= 4
    print("Reported overlaps: {0}".format(count))


if __name__ == '__main__':
    main()
