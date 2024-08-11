#include <stdint.h>
uint32_t seed;

uint32_t PCG_hash(uint32_t input) {
    int32_t state = input * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint32_t fastrand() {
    seed = PCG_hash(seed);
    return seed;
}

void set_rand_seed(uint32_t input) {
    seed = input;
}