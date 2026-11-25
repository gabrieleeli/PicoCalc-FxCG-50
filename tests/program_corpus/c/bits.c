/* interi senza segno, bit, esadecimale, binario */
#include <stdio.h>
#include <stdint.h>

void binary(unsigned v, int bits) {
    for (int i = bits - 1; i >= 0; i--) putchar((v >> i) & 1 ? '1' : '0');
    putchar('\n');
}
int popcount(uint32_t v) {
    int n = 0;
    while (v) {
        v &= v - 1;
        n++;
    }
    return n;
}

int main(void) {
    unsigned a = 0xF0F0u, b = 0x0FF0u;
    printf("%04X %04X %04X %08X\n", a & b, a | b, a ^ b, ~a);
    binary(37, 8);
    binary(a >> 4, 16);
    printf("%d %d %d\n", popcount(255), popcount(0x80000001u), popcount(0));
    uint8_t x = 200;
    x = (uint8_t)(x * 2);
    int8_t y = (int8_t)200;
    printf("%u %d\n", x, y);
    uint32_t h = 2166136261u;
    const char* s = "PicoCalc";
    for (const char* p = s; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    printf("FNV-1a(%s) = %08x\n", s, h);
    unsigned long long big = 1ULL << 63;
    printf("%llu %llx\n", big, big - 1);
    int flags = 0;
    flags |= 1 << 2;
    flags |= 1 << 5;
    flags &= ~(1 << 2);
    printf("%d %d %d\n", flags, (flags >> 5) & 1, (flags >> 2) & 1);
    printf("%d %d\n", -17 >> 1, (int)(-17u >> 28));
    return 0;
}
