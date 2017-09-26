#!/usr/bin/env python


import sys
import os
from enum import Enum

class Encoding(Enum):
	_BPSK_1_2 = 0
	_BPSK_3_4 = 1
	_QPSK_1_2 = 2
	_QPSK_3_4 = 3
	_16QAM_1_2 = 4
	_16QAM_3_4 = 5
	_64QAM_2_3 = 6
	_64QAM_3_4 = 7



def display_encoding(encoding_f):
	encodings = encoding_f.read()
	encodings = encodings.split("\n")[2:-1]

	if len(encodings) == 0:
		print("ERROR: encoding file is not correct")
		return

	n_encoding = [0, 0, 0, 0, 0, 0, 0, 0]
	for encoding in encodings:
		n_encoding[int(encoding)]+=1

	i= 0
	for name, _ in Encoding.__members__.items():	
		if n_encoding[i] != 0:
			encoding_percent = 100 * n_encoding[i]/float(len(encodings))
			print ("{0}: {1:.2f}%".format(name[1:],encoding_percent))
		i+=1	

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("usage: ./display_encoding.py encoding_file")
		sys.exit(-1)
	try:	
		encoding_f = open(sys.argv[1])
	except IOError as e:
		print ("I/O error: {0}").format(e)
		sys.exit(-1)
	display_encoding(encoding_f)
	encoding_f.close()