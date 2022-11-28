// Count number of bits in a 32-bit word, faster version than a while loop
// see: https://www.johndcook.com/blog/2020/02/21/popcount/

#include "popc.h"

unsigned int popcount(unsigned int x) {
	x -= ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x += (x >> 8);
	x += (x >> 16);
	return x & 0x0000003F;
}
