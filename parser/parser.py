#!/usr/bin/python3

import os
import sys

# Hardcoded list of input files (adjust paths as needed)
INPUT_FILES = [
    "log1.txt",
    "log2.txt",
    "results.log",
    # add more files here
]

# Pattern definitions:
# (tag, output_name, num_fields, ignore_extra_brackets)
PATTERNS = [
    ("[MPIAlltoallv]",      "mpi_alltoallv",        3,      False),
    ("[ExcOr]",             "xor",                  3,      False),
    ("[OMPI_pairwise]",     "ompi_pairwise",        3,      False),
    ("[OMPI_linear]",       "ompi_linear",          3,      False),
    ("[MPICH_scattered]",   "mpi_scattered",        4,      False),
    ("[Rbruckv]",           "parlogna",             5,      False),
    ("[TTPL]",              "parlinna_coalesced",   5,      True),
    ("[TTPL_S1]",           "parlinna_staggered",   5,      True),
]

def process_line(line):
    """Return a cleaned CSV line (string) if line matches a pattern, else None."""
    line = line.rstrip('\n\r')
    if not line:
        return None

    for tag, out_name, num_fields, ignore_extra in PATTERNS:
        if not line.startswith(tag):
            continue

        # After the tag there must be a space
        rest = line[len(tag):].lstrip()
        if not rest:
            continue

        # Split by comma and clean each part
        parts = [p.strip() for p in rest.split(',')]
        if ignore_extra:
            # For TTPL/TTPL_S1: take first num_fields parts,
            # and if a part contains a bracket, keep only the part before the first space.
            cleaned = []
            for p in parts[:num_fields]:
                if '[' in p:
                    p = p.split()[0]   # discard everything after the first space
                cleaned.append(p)
            if len(cleaned) < num_fields:
                continue
            fields = cleaned[:num_fields]
        else:
            # Exact match: exactly num_fields parts, and no stray brackets
            if len(parts) != num_fields:
                continue
            if any('[' in p for p in parts):
                continue
            fields = parts

        # Build CSV line: output_name,field1,field2,...
        return f"{out_name},{','.join(fields)}"

    return None

def main():
    for input_path in INPUT_FILES:
        if not os.path.isfile(input_path):
            print(f"Warning: File not found, skipping: {input_path}", file=sys.stderr)
            continue

        # Output file name: input_base + "_cleaned.csv"
        base, _ = os.path.splitext(input_path)
        output_path = f"{base}_cleaned.csv"

        with open(input_path, 'r') as infile, open(output_path, 'w') as outfile:
            for line in infile:
                cleaned = process_line(line)
                if cleaned is not None:
                    outfile.write(cleaned + '\n')

        print(f"Processed: {input_path} -> {output_path}")

if __name__ == "__main__":
    main()