/*
 * Copyright (C) 2016 	Samuel Rey <samuel.rey.escudero@gmail.com>
 *                  	Bastian Bloessl <bloessl@ccs-labs.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "utils.h"

#include <cassert>
#include <cstring>
#include <math.h>


ofdm_param::ofdm_param(std::vector<int> pilots_enc) {
	resource_blocks_e = pilots_enc;
	n_bpsc = 0;
	n_cbps = 0;
	n_dbps = 0;


	// Rate field will not be used. The header sends the codification of each resource blocks directly. 
	// Each resource block have 12 carriers 
	for (int i = 0; i < 4; i++) {
		switch(pilots_enc[i]) {
		case BPSK_1_2:
			n_bpcrb[i] = 1; 
			n_bpsc += 1;
			n_cbps += 12;
			break;
		case QPSK_1_2:
			n_bpcrb[i] = 2;
			n_bpsc += 2;
			n_cbps += (12*2);
			break;

		case QAM16_1_2:
			n_bpcrb[i] = 4;
			n_bpsc += 4;
			n_cbps += (12*4);
			break;

		case QAM64_1_2:
			n_bpcrb[i] = 6;
			n_bpsc += 6;
			n_cbps += (12*6);
			break;
		default:
			assert(false);
			break;
		}
	}
	// Mean of the four resource blocks
	n_bpsc = n_bpsc / 4;

	// All posible coding schemes uses a 1/2 channel coding.
	n_dbps = n_cbps / 2;
}


void
ofdm_param::print() {
	std::cout << std::endl;
	std::cout << "OFDM Symbol Parameters:" << std::endl;
	std::cout << "n_bpsc: " << n_bpsc << std::endl;
	std::cout << "n_cbps: " << n_cbps << std::endl;
	std::cout << "n_dbps: " << n_dbps << std::endl;

	for (int i = 0; i < 4; i++) {
		std::cout << "Resource block " << i << " encoding: " << resource_blocks_e[i] << std::endl;
		std::cout << "Resource block " << i << " bits per carrier: " << n_bpcrb[i] << std::endl;
	}
	std::cout << std::endl;
}


frame_param::frame_param(ofdm_param &ofdm, int psdu_length) {
	psdu_size = psdu_length;

	// number of symbols (17-11)
	n_sym = (int) ceil((16 + 8 * psdu_size + 6) / (double) ofdm.n_dbps);

	n_data_bits = n_sym * ofdm.n_dbps;

	// number of padding bits (17-13)
	n_pad = n_data_bits - (16 + 8 * psdu_size + 6);

	n_encoded_bits = n_sym * ofdm.n_cbps;
}
void
frame_param::print() {
	std::cout << std::endl;
	std::cout << "FRAME Parameters:" << std::endl;
	std::cout << "psdu_size (bytes): " << psdu_size << std::endl;
	std::cout << "n_sym: " << n_sym << std::endl;
	std::cout << "n_pad: " << n_pad << std::endl;
	std::cout << "n_encoded_bits: " << n_encoded_bits << std::endl;
	std::cout << "n_data_bits: " << n_data_bits << std::endl << std::endl;
}


void scramble(const char *in, char *out, frame_param &frame, char initial_state) {
    int state = initial_state;
    int feedback;

    for (int i = 0; i < frame.n_data_bits; i++) {
		feedback = (!!(state & 64)) ^ (!!(state & 8));
		out[i] = feedback ^ in[i];
		state = ((state << 1) & 0x7e) | feedback;
    }
}


void reset_tail_bits(char *scrambled_data, frame_param &frame) {
	memset(scrambled_data + frame.n_data_bits - frame.n_pad - 6, 0, 6 * sizeof(char));
}


int ones(int n) {
	int sum = 0;
	for(int i = 0; i < 8; i++) {
		if(n & (1 << i)) {
			sum++;
		}
	}
	return sum;
}


void convolutional_encoding(const char *in, char *out, frame_param &frame) {
	int state = 0;

	for(int i = 0; i < frame.n_data_bits; i++) {
		assert(in[i] == 0 || in[i] == 1);
		state = ((state << 1) & 0x7e) | in[i];
		out[i * 2]     = ones(state & 0155) % 2;
		out[i * 2 + 1] = ones(state & 0117) % 2;
	}
}


void puncturing(const char *in, char *out, frame_param &frame, ofdm_param &ofdm) {

	int mod;

	for (int i = 0; i < frame.n_data_bits * 2; i++) {
		/*switch(ofdm.encoding) {
			case BPSK_1_2:
			case QPSK_1_2:
			case QAM16_1_2:
			case QAM64_1_2:
				*out = in[i];
				out++;
				break;

			defaut:
				assert(false);
				break;
		}*/
		*out = in[i];
		out++;
	}
}


void interleave(const char *in, char *out, frame_param &frame, ofdm_param &ofdm, bool reverse) {
	int n_cbps = ofdm.n_cbps;
	int first[n_cbps];
	int second[n_cbps];
	int s = floor(std::max(int(ofdm.n_bpsc) / 2, 1));

	for(int j = 0; j < n_cbps; j++) {
		first[j] = s * (j / s) + ((j + int(floor(16.0 * j / n_cbps))) % s);
	}

	for(int i = 0; i < n_cbps; i++) {
		second[i] = 16 * i - (n_cbps - 1) * int(floor(16.0 * i / n_cbps));
	}

	for(int i = 0; i < frame.n_sym; i++) {
		for(int k = 0; k < n_cbps; k++) {
			if(reverse) {
				out[i * n_cbps + second[first[k]]] = in[i * n_cbps + k];
			} else {
				out[i * n_cbps + k] = in[i * n_cbps + second[first[k]]];
			}
		}
	}
}


void split_symbols(const char *in, char *out, frame_param &frame, ofdm_param &ofdm) {
	int symbols = frame.n_sym * 48;
	int bpsc;

	for (int i = 0; i < symbols; i++) {
		if ((i % 48) < 12){
			bpsc = ofdm.n_bpcrb[0];
		}else if ((i % 48) < 24){
			bpsc = ofdm.n_bpcrb[1];
		}else if((i % 48) < 36){
			bpsc = ofdm.n_bpcrb[2];
		}else if((i % 48) < 48){
			bpsc = ofdm.n_bpcrb[3];
		}else{
			assert(false);
		}

		out[i] = 0;
		for(int k = 0; k < bpsc; k++) {
			assert(*in == 1 || *in == 0);
			out[i] |= (*in << k);
			in++;
		}
	}
}


void generate_bits(const char *psdu, char *data_bits, frame_param &frame) {
	// first 16 bits are zero (SERVICE/DATA field)
	memset(data_bits, 0, 16);
	data_bits += 16;

	for(int i = 0; i < frame.psdu_size; i++) {
		for(int b = 0; b < 8; b++) {
			data_bits[i * 8 + b] = !!(psdu[i] & (1 << b));
		}
	}
}

void 
print_bytes(std::string tag, char bytes[], int size)
{
    std::cout << tag << std::endl;
    for(int i = 0; i < size; i++)
    {
    	std::cout << std::setw(2) << std::setfill('0') << int(bytes[i]);
        std::cout << " ";
 	}
    std::cout << std::endl << std::endl;
}