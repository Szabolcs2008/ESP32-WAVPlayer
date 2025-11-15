#!/usr/bin/env python3

import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("IN")
parser.add_argument("OUT")
parser.add_argument("-s", "--sample-rate", dest="sample_rate", type=int, default=22050)
parser.add_argument("-S", "--stereo", action="store_true")

args = parser.parse_args()

subprocess.run(["ffmpeg", "-i", args.IN, "-ac", '2' if args.stereo else "1", "-ar", str(args.sample_rate), "-c:a", "pcm_u8", args.OUT])
