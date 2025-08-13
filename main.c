#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#if defined(__APPLE__) || defined(__linux__)
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
#else
#include <Windows.h>
#include <conio.h> // _kbhit
#endif

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

/**
 *
 */
enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

/**
 * Memory mapped register
 */
enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

/**
 * Update register R_COND
 */
void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15 /* a `1` in the left-most bit indicates negative number in `Two's complement` */)
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1 /* a `1` in the left-most bit indicates negative number in `Two's complement` */)
    {
        x |= (0xFFFF << bit_count); /* Fill missing bit to 1 upto 16 bits for negative number */
    }
    return x /* Keep x if x is positive*/;
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

/**
 * Load program into address
 *
 * The first 16 bits of the program file specify the address in memory where the program should start.
 * This address is called the origin. It must be read first,
 * after which the rest of the data can be read from the file into memory starting at the origin address.
 */
void read_image_file(FILE *file)
{
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t *p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    /* LC-3 programs are big-endian, but most modern computers are little-endian. So, we need to swap each uint16 that is loaded. */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char *image_path)
{
    FILE *file = fopen(image_path, "rb");
    if (!file)
    {
        return 0;
    };

    read_image_file(file);
    fclose(file);
    return 1;
}

#if defined(__APPLE__) || defined(__linux__)
struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
#else
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode);     /* save old mode */
    fdwMode = fdwOldMode ^ ENABLE_ECHO_INPUT /* no input echo */
              ^ ENABLE_LINE_INPUT;           /* return when one or
                                                more characters are available */
    SetConsoleMode(hStdin, fdwMode);         /* set new mode */
    FlushConsoleInputBuffer(hStdin);         /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}
#endif

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

void mem_write(uint16_t addr, uint16_t val)
{
    memory[addr] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

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

#pragma region setup
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();
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
        uint16_t op = instr >> 12; // 4 most significant bits is opcode

        switch (op)
        {
        case OP_ADD: /* 0001 */
        {
            uint16_t r0 = (instr >> 9) & 0x7;       /* destination register (DR) - bit 9..11 */
            uint16_t r1 = (instr >> 6) & 0x7;       /* first operand (SR1) - bit 6..8 */
            uint16_t imm_flag = (instr >> 5) & 0x1; /* bit 5, whether we are in immediate mode */

            if (imm_flag /* immediate mode */)
            {
                uint16_t imm5 = sign_extend(
                    instr & 0x1F /* Mask with 0001 1111 (or 0x1F). Or in other words, keep only instr's lowest 5 bits, set all other bits to 0. */,
                    5);
                reg[r0] = reg[r1] + imm5;
            }
            else /* register mode */
            {
                uint16_t r2 = instr & 0x7; /* right-most 3 bits is second operand (SR2) in register mode */
                reg[r0] = reg[r1] + reg[r2];
            }

            update_flags(r0);

            break;
        }
        case OP_AND: /* 0101*/
        {
            uint16_t r0 = (instr >> 9) & 0x7;       /* destination register (DR) - bit 9..11 */
            uint16_t r1 = (instr >> 6) & 0x7;       /* first operand (SR1) - bit 6..8 */
            uint16_t imm_flag = (instr >> 5) & 0x1; /* bit 5, whether we are in immediate mode */

            if (imm_flag /* immediate mode */)
            {
                uint16_t imm5 = sign_extend(
                    instr & 0x1F /* Mask with 0001 1111 (or 0x1F). Or in other words, keep only instr's lowest 5 bits, set all other bits to 0. */,
                    5);
                reg[r0] = reg[r1] & imm5;
            }
            else /* register mode */
            {
                uint16_t r2 = instr & 0x7; /* right-most 3 bits is second operand (SR2) in register mode */
                reg[r0] = reg[r1] & reg[r2];
            }

            update_flags(r0);

            break;
        }
        case OP_NOT: /* 1001 */
        {
            uint16_t r0 = (instr >> 9) & 0x7; // bit [9:11]
            uint16_t r1 = (instr >> 6) & 0x7; // bit [6:8]

            reg[r0] = ~reg[r1];
            update_flags(r0);

            break;
        }
        case OP_BR: /* 0000 */
        {
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9); // pc offset, bit [0:8]
            uint16_t cond_flag = (instr >> 9) & 0x7;            // condition flag, bit [9:11] (bit 9 is p, bit 10 is z, bit 11 is n)
            if (cond_flag & reg[R_COND])
            {
                reg[R_PC] += pc_offset;
            }

            break;
        }
        case OP_JMP: /* 1100 */
        {
            /**
             * RET is a special case of JMP. RET happens whenever R1 is 7
             */

            uint16_t r1 = (instr >> 6) & 0x7;
            reg[R_PC] = reg[r1];

            break;
        }
        case OP_JSR: /* 0100 */
        {
            uint16_t long_flag = (instr >> 11) & 1; // bit 11
            reg[R_R7] = reg[R_PC];

            if (long_flag)
            {
                uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                reg[R_PC] += long_pc_offset; /* JSR */
            }
            else
            {
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r1]; /* JSRR */
            }

            break;
        }
        case OP_LD: /* 0010 */
        {
            uint16_t r0 = (instr >> 9) & 0x7;                   // bit [9:11]
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9); // bit [0:8]
            reg[r0] = mem_read(reg[R_PC] + pc_offset);
            update_flags(r0);

            break;
        }
        case OP_LDI: /* 1010 */
        {
            /**
             * Load value from a location of memory into a register.
             * An address is computed by sign-extending bits [8:0] to 16 bits and adding this value to the incremented PC.
             * The resulting sum is an address to a location in memory, and that address contains, yet another value which is the address of the value to load.
             * Also, the condition codes are set based on whether the value loaded is negative, zero, or positive.
             */

            uint16_t r0 = (instr >> 9) & 0x7; /* destination register (DR) - bit 9..11 */
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

            reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
            update_flags(r0);

            break;
        }
        case OP_LDR: /* 0110 */
        {
            uint16_t r0 = (instr >> 9) & 0x7;               // bit [9:11]
            uint16_t r1 = (instr >> 6) & 0x7;               // bit [6:8]
            uint16_t offset = sign_extend(instr & 0x3F, 6); // bit [0:5]
            reg[r0] = mem_read(reg[r1] + offset);
            update_flags(r0);

            break;
        }
        case OP_LEA: /* 1110 */
        {
            uint16_t r0 = (instr >> 9) & 0x7; // bit [9:11]
            uint16_t pc_offset = sign_extend(instr & 0x1FF /* bit [0:8] */, 9);
            reg[r0] = reg[R_PC] + pc_offset;
            update_flags(r0);

            break;
        }
        case OP_ST: /* 0011 */
        {
            uint16_t r0 = (instr >> 9) & 0x7; // bit [9:11]
            uint16_t pc_offset = sign_extend(instr & 0x1FF /* bit [0:8] */, 9);
            mem_write(reg[R_PC] + pc_offset, reg[r0]);

            break;
        }
        case OP_STI: /* 1011 */
        {
            uint16_t r0 = (instr >> 9) & 0x7; // bit [9:11]
            uint16_t pc_offset = sign_extend(instr & 0x1FF /* bit [0:8] */, 9);
            mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);

            break;
        }
        case OP_STR: /* 0111 */
        {
            uint16_t r0 = (instr >> 9) & 0x7;               // bit [9:11]
            uint16_t r1 = (instr >> 6) & 0x7;               // bit [6:8]
            uint16_t offset = sign_extend(instr & 0x3F, 6); // bit [0:5]

            mem_write(reg[r1] + offset, reg[r0]);

            break;
        }
        case OP_TRAP: /* 1111 */
        {
            reg[R_R7] = reg[R_PC];

            switch (instr & 0xFF /*  */)
            {
            case TRAP_GETC:
            {
                /* read a single ASCII char */
                reg[R_R0] = (uint16_t)getchar();
                update_flags(R_R0);

                break;
            }
            case TRAP_OUT:
            {
                /* Output character */
                putc((char)reg[R_R0], stdout);
                fflush(stdout);

                break;
            }
            case TRAP_PUTS:
            {
                /* one char per word */

                uint16_t *c = memory + reg[R_R0];
                while (*c)
                {
                    putc((char)*c, stdout);
                    ++c;
                }
                fflush(stdout);

                break;
            }
            case TRAP_IN:
            {
                /* Prompt for input character */
                printf("Enter a character: ");
                char c = getchar();
                putc(c, stdout);
                fflush(stdout);
                reg[R_R0] = (uint16_t)c;
                update_flags(R_R0);

                break;
            }
            case TRAP_PUTSP:
            {
                /**
                 * one char per byte (two bytes per word)
                 * here we need to swap back to
                 * big endian format
                 */

                uint16_t *c = memory + reg[R_R0];
                while (*c)
                {
                    char char1 = (*c) & 0xFF;
                    putc(char1, stdout);
                    char char2 = (*c) >> 8;
                    if (char2)
                        putc(char2, stdout);
                    ++c;
                }
                fflush(stdout);

                break;
            }
            case TRAP_HALT:
            {
                puts("HALT");
                fflush(stdout);
                running = 0;

                break;
            }
            }

            break;
        }
        case OP_RES: /* unused */
        case OP_RTI: /* unused*/
        default:
            abort(); // Bad opcode
            break;
        }
    }

    restore_input_buffering(); // shutdown

    return EXIT_SUCCESS;
}