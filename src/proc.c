#include <string.h>
#include <stdint.h>

#include "internal/proc.h"
#include "internal/sbox.h"
#include "internal/gf256.h"

// word granularity (32 bits)
// used by key sched
uint32_t _aes_proc_sub_word(uint32_t word) {
    uint32_t res = 0;
    uint8_t* in = (uint8_t*) &word;
    uint8_t* out = (uint8_t*) &res;

    for(int i = 0; i < 4; i++) {
        out[i] = SUB_BYTE(in[i]);
    }

    return res;
}

uint32_t _aes_proc_rot_word(uint32_t word, int bytes) {
    int bitshift = bytes * 8;
    return (word >> bitshift) | (word << (32 - bitshift));
}

// block granularity (128 bits)
// used by encryption
void _aes_proc_add_round_key(uint32_t* state, uint32_t* round_key) {
	for(int i = 0; i < 4; i++) {
		state[i] ^= round_key[i];
	}
}

void _aes_proc_sub_bytes(uint32_t* state) {
	uint8_t* state_bytes = (uint8_t*) state;

	for(int i = 0; i < 16; i++) {
		state_bytes[i] = SUB_BYTE(state_bytes[i]);
	}
}

// index into a 16 byte block as a 4x4 byte matrix (for shift rows and inv shift rows)
#define IDX(x, y) (y + x * 4)

void _aes_proc_shift_rows(uint32_t* state) {
	uint8_t* state8 = (uint8_t*) state; // this step requires a byte level access
	uint8_t tmp;

	// 1st row - nothing

	// 2nd row
	tmp = state8[IDX(0, 1)];
	state8[IDX(0, 1)] = state8[IDX(1, 1)];
	state8[IDX(1, 1)] = state8[IDX(2, 1)];
	state8[IDX(2, 1)] = state8[IDX(3, 1)];
	state8[IDX(3, 1)] = tmp;

	// 3rd row
	tmp = state8[IDX(0, 2)];
	state8[IDX(0, 2)] = state8[IDX(2, 2)];
	state8[IDX(2, 2)] = tmp;

	tmp = state8[IDX(1, 2)];
	state8[IDX(1, 2)] = state8[IDX(3, 2)];
	state8[IDX(3, 2)] = tmp;

	// 4th row
	tmp = state8[IDX(3, 3)];
	state8[IDX(3, 3)] = state8[IDX(2, 3)];
	state8[IDX(2, 3)] = state8[IDX(1, 3)];
	state8[IDX(1, 3)] = state8[IDX(0, 3)];
	state8[IDX(0, 3)] = tmp;
}

void _aes_proc_mix_columns(uint32_t* state) {
	for(int i = 0; i < 4; i++) {
		uint8_t* state8 = (uint8_t*) state;
		uint8_t result[4] = { 0 };

		result[0] = _aes_GF256_mul(state8[0], 0x2) ^ _aes_GF256_mul(state8[1], 0x3) ^               state8[2]        ^               state8[3];
		result[1] =               state8[0]        ^ _aes_GF256_mul(state8[1], 0x2) ^ _aes_GF256_mul(state8[2], 0x3) ^               state8[3];
		result[2] =               state8[0]        ^               state8[1]        ^ _aes_GF256_mul(state8[2], 0x2) ^ _aes_GF256_mul(state8[3], 0x3);
		result[3] = _aes_GF256_mul(state8[0], 0x3) ^               state8[1]        ^               state8[2]        ^ _aes_GF256_mul(state8[3], 0x2);

		memcpy(state, result, 4);
		state++; // next column
	}
}

// block granularity (128 bits), inverse operations
// used by decryption

// because AddRoundKey uses XOR to mix key with the state, and XOR is its own inverse
// so InvAddRoundKey and AddRoundKey are literally the same operation
void _aes_proc_inv_sub_bytes(uint32_t* state) {
	uint8_t* state_bytes = (uint8_t*) state;

	for(int i = 0; i < 16; i++) {
		state_bytes[i] = ISUB_BYTE(state_bytes[i]);
	}
}

// This reuses the IDX() macro defined right above shift_rows()
void _aes_proc_inv_shift_rows(uint32_t* state) {
	uint8_t* state8 = (uint8_t*) state; // this step requires a byte level access
	uint8_t tmp;

	// 1st row - nothing

	// 2nd row
	tmp = state8[IDX(3, 1)];
	state8[IDX(3, 1)] = state8[IDX(2, 1)];
	state8[IDX(2, 1)] = state8[IDX(1, 1)];
	state8[IDX(1, 1)] = state8[IDX(0, 1)];
	state8[IDX(0, 1)] = tmp;

	// 3rd row
	tmp = state8[IDX(2, 2)];
	state8[IDX(2, 2)] = state8[IDX(0, 2)];
	state8[IDX(0, 2)] = tmp;

	tmp = state8[IDX(3, 2)];
	state8[IDX(3, 2)] = state8[IDX(1, 2)];
	state8[IDX(1, 2)] = tmp;

	// 4th row
	tmp = state8[IDX(0, 3)];
	state8[IDX(0, 3)] = state8[IDX(1, 3)];
	state8[IDX(1, 3)] = state8[IDX(2, 3)];
	state8[IDX(2, 3)] = state8[IDX(3, 3)];
	state8[IDX(3, 3)] = tmp;
}

void _aes_proc_inv_mix_columns(uint32_t* state) {
	for(int i = 0; i < 4; i++) {
		uint8_t* state8 = (uint8_t*) state;
		uint8_t result[4] = { 0 };

		result[0] = _aes_GF256_mul(state8[0], 0xe) ^ _aes_GF256_mul(state8[1], 0xb) ^ _aes_GF256_mul(state8[2], 0xd) ^ _aes_GF256_mul(state8[3], 0x9);
		result[1] = _aes_GF256_mul(state8[0], 0x9) ^ _aes_GF256_mul(state8[1], 0xe) ^ _aes_GF256_mul(state8[2], 0xb) ^ _aes_GF256_mul(state8[3], 0xd);
		result[2] = _aes_GF256_mul(state8[0], 0xd) ^ _aes_GF256_mul(state8[1], 0x9) ^ _aes_GF256_mul(state8[2], 0xe) ^ _aes_GF256_mul(state8[3], 0xb);
		result[3] = _aes_GF256_mul(state8[0], 0xb) ^ _aes_GF256_mul(state8[1], 0xd) ^ _aes_GF256_mul(state8[2], 0x9) ^ _aes_GF256_mul(state8[3], 0xe);

		memcpy(state, result, 4);
		state++; // next column
	}
}
