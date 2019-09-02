#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import argparse
import re
from mmap import ACCESS_READ, mmap

def hname(name):
    name = re.sub('.*[\\/]', '', name)
    return re.sub('\.[^.]+$', '', name) + '.h'

# https://stackoverflow.com/questions/3303312/how-do-i-convert-a-string-to-a-valid-variable-name-in-python
def vname(name):
    name = re.sub('.*[\\/]', '', name)
    return re.sub('\W|^(?=\d)', '_', name)

parser = argparse.ArgumentParser(prog='bin2h.py')
parser.add_argument('-i', '--input', metavar='FILE', help='input file', required=True);
parser.add_argument('-o', '--output', metavar='HEADER.h', help='output file')
parser.add_argument('-n', '--name', metavar='VARIABLE', help='variable name in C header')
parser.add_argument('-u', '--unsigned', action='store_true', help='use unsigned char')
parser.add_argument('-v', '--verbose', action='store_true', help='noisy output')

args = parser.parse_args()

ifile = args.input
ofile = args.output or hname(args.input)
name = args.name or vname(args.input)
unsigned = args.unsigned
verbose = args.verbose


size = 0
# https://stackoverflow.com/questions/1035340/reading-binary-file-and-looping-over-each-byte
with open(ifile, 'rb', 0) as f, mmap(f.fileno(), 0, access=ACCESS_READ) as s:
    with open(ofile, 'w') as out:
        if unsigned:
            out.write('const unsigned char %s[] =\n  "' % name)
        else:
            out.write('const char %s[] =\n  "' % name)

        for byte in s:
            size = size + 1

            v = hex(ord(byte))[2:].zfill(2)
            out.write('\\x%s' % v)

            if size % 16 == 0:
                out.write('"\n  "')

        out.write('"\n;\nconst unsigned int %s_len = %s;\n' % (name, size))

if verbose:
    print("""bin2h.py: '%s' (%s bytes) -> '%s' => %s[] + %s_len""" % (ifile, size, ofile, name, name))

