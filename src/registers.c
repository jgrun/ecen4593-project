/* src/registers.c
 * Provides access to register file
 */

#include "registers.h"

static word_t regfile[32];

void reg_init(void) {
    for (int i = 0; i < 32; ++i) regfile[i] = 0;
}

void reg_read(int reg, word_t *value) {
    *value = regfile[reg];
}

void reg_write(int reg, word_t *value) {
    if (reg) regfile[reg] = *value;
}

void reg_dump(void) {
    int i;
    printf("Dumping registers:\n");
    printf("  $zero: 0x%08x (%d)\n",regfile[REG_ZERO],regfile[REG_ZERO]);
    printf("  $at:   0x%08x (%d)\n",regfile[REG_AT],regfile[REG_AT]);
    printf("  $v0:   0x%08x (%d)\n",regfile[REG_V0],regfile[REG_V0]);
    printf("  $v1:   0x%08x (%d)\n",regfile[REG_V1],regfile[REG_V1]);
    for (i =  4; i <=  7; ++i) printf("  $a%d:   0x%08x (%d)\n",i- 4,regfile[i],regfile[i]);
    for (i =  8; i <= 15; ++i) printf("  $t%d:   0x%08x (%d)\n",i- 8,regfile[i],regfile[i]);
    for (i = 16; i <= 23; ++i) printf("  $s%d:   0x%08x (%d)\n",i-16,regfile[i],regfile[i]);
    printf("  $t8:   0x%08x (%d)\n",regfile[REG_T8],regfile[REG_T8]);
    printf("  $t9:   0x%08x (%d)\n",regfile[REG_T9],regfile[REG_T9]);
    printf("  $k0:   0x%08x (%d)\n",regfile[REG_K0],regfile[REG_K0]);
    printf("  $k1:   0x%08x (%d)\n",regfile[REG_K1],regfile[REG_K1]);
    printf("  $gp:   0x%08x (%d)\n",regfile[REG_GP],regfile[REG_GP]);
    printf("  $sp:   0x%08x (%d)\n",regfile[REG_SP],regfile[REG_SP]);
    printf("  $fp:   0x%08x (%d)\n",regfile[REG_FP],regfile[REG_FP]);
    printf("  $ra:   0x%08x (%d)\n",regfile[REG_RA],regfile[REG_RA]);
}
