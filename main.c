#include <stdlib.h>
#include <stdint.h>

// LC-3 has 65536 memory locations,
// each of which stores a 16-bit value.

#define MEMORY_MAX (1 << 16 /* 65_536 */)
uint16_t memory[MEMORY_MAX]; // 65_536 memory locations

/**
 * LC-3 has 10 total register, each of which is 16 bits.
 * Most of them are general purpose, but a few have designated roles:
 * - 8 general purpose registers (R0-R7) can be used to perform any program calculations.
 * - 1 program counter (PC) register.
 * - 1 condition flags (COND) register tells us information about the previous calculation.
 */
enum
{
#pragma region General purpose registers
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
#pragma endregion
    R_PC,   /* Program counter */
    R_COND, /* Condition flags. The R_COND register stores condition flags which provide information about the most recently executed calculation. This allows programs to check logical conditions such as if (x > 0) { ... }. */

    R_COUNT /* Just contain total number of registers */
};
uint16_t reg[R_COUNT];

/**
 * There are just 16 opcodes in LC-3.
 * Each instruction is 16 bits long, with the left 4 bits storing the opcode.
 * The rest of the bits are used to store the parameters.
 */
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

/**
 * The LC-3 uses only 3 condition flags which indicate the sign of the previous calculation.
 */
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

int main(int argc, const char *argv[])
{
    /**
     * Steps:
     * 1. Load one instruction from memory at the address of the PC register.
     * 2. Increment the PC register.
     * 3. Look at the opcode to determine which type of instruction it should perform.
     * 4. Perform the instruction using the parameters in the instruction.
     * 5. Go back to step 1.
     */

#pragma region Load arguments
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }

    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
#pragma endregion

    /** since exactly one condition flag should be set at any given time, set the Z flag  */
    reg[R_COND] = FL_ZRO;

    enum
    {
        PC_START = 0x3000
    };
    /** set the PC to starting position, 0x3000 is the default */
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH*/
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12; // 4 most significant bit is opcode

        switch (op)
        {
        case OP_ADD:
            break;
        case OP_AND:
            break;
        case OP_NOT:
            break;
        case OP_BR:
            break;
        case OP_JMP:
            break;
        case OP_JSR:
            break;
        case OP_LD:
            break;
        case OP_LDI:
            break;
        case OP_LDR:
            break;
        case OP_LEA:
            break;
        case OP_ST:
            break;
        case OP_STI:
            break;
        case OP_STR:
            break;
        case OP_TRAP:
            break;
        case OP_RES:
        case OP_RTI:
        default:
            break;
        }
    }

    return EXIT_SUCCESS;
}