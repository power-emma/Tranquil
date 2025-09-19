
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

// Arm Emulator
// Emma Power - 2025


uint8_t byteIsolate(uint32_t opcode, uint8_t position, uint8_t size, uint8_t offset) {
    //position right to left
    uint32_t mask = (1 << size) - 1;

    opcode = opcode >> offset;

    for (int i = 0; i < opcode / size; i ++) {

        if (i == position) {
            return opcode & mask;
        } else {
            opcode = opcode >> size;
        }

    }

    return opcode & mask;
}

// Register 12 is print
int main () {
    // Turn on step by step instructions
    int debug = 1;

    uint32_t* registers = (uint32_t*) malloc(17*sizeof(uint32_t));       
    uint32_t* memory = (uint32_t*) malloc(1024*sizeof(uint32_t));
    registers[15] = 0; // PC


    FILE *fptr;
    fptr = fopen("../fib.bin", "r");
    if (fptr == NULL) {
        printf("File not found");
        return 1;
    }

    u_int32_t nextLine[1];
    int fileReadIndex = 0;
    while (fread(nextLine, 4, 1, fptr)) {
        memory[fileReadIndex] = nextLine[0];
        fileReadIndex ++;
    }

    uint32_t nextInstruction = memory[registers[15]];

    while (nextInstruction != (uint32_t) 0xD4400000) {
        uint32_t printToggle = registers[12];

        if (debug) {printf("Next Instruction Is %X\n", nextInstruction);}

        // Data Processing Instruction
        if (nextInstruction == (nextInstruction & 0xF3FFFFFF)) {
            //printf("^ Data Processing \n");

            
            uint8_t opcode = byteIsolate(nextInstruction, 5, 4, 1);
            uint8_t operand1 = byteIsolate(nextInstruction, 4, 4, 0);
            uint8_t operand2 = byteIsolate(nextInstruction, 0, 12, 0);
            uint8_t destination = byteIsolate(nextInstruction, 3, 4, 0);
            if (debug) {printf("Opcode: %X, o1 = %X, o2 = %X, dr = %X\n", opcode, operand1, operand2, destination);}
            uint8_t immBit = (nextInstruction << 6) >> 31;
            
            switch(opcode) {
                case 0:
                case 1:
                case 2:
                case 3:
                    break;
                case 4:
                    // ADD
                    if (!immBit) {
                        registers[destination] = registers[operand1] + operand2;
                    } else {
                        registers[destination] = registers[operand1] + registers[operand2];
                    }
                    if (debug) {printf("Performing ADD instruction \"%X\"\n", opcode);}

                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    break;
                case 10:
                    if (debug) {printf("Performing CMP instruction immedate = %X, \"%X\" on %X and %X\n", immBit, opcode, operand1, operand2);}
                    if (immBit) {
                        if (registers[operand1] == operand2) {
                            registers[16] = 0x80000000;
                        } else {
                            registers[16] = 0x00000000;
                        }
                    } else {
                        if (registers[operand1] == registers[operand2]) {
                            registers[16] = 0x80000000;
                        } else {
                            registers[16] = 0x00000000;

                        }
                    }
                    break;
                default:
                    break;
            }

        } else if (nextInstruction == (nextInstruction & 0xF7DFFFFF)) {
            //printf("^ Single Data Transfer \n");
            char loadBit = byteIsolate(nextInstruction, 0, 1, 20);
            uint8_t immBit = (nextInstruction << 6) >> 31;
            int16_t offset = byteIsolate(nextInstruction, 0, 12, 0);
            uint8_t destination = byteIsolate(nextInstruction, 3, 4, 0);
            if (immBit) {
                offset = registers[offset];
            }
            if (debug) {printf("Save/Load: %X, Immediate: %X, into register %X with Value %X \n", loadBit, immBit, destination, offset);}
            registers[destination] = offset;
        } else if (nextInstruction == (nextInstruction & 0xFAFFFFFF)) {
            //printf("Branch Instruction");
            char type = byteIsolate(nextInstruction, 7, 4, 0);

            int32_t offset = (int32_t) (nextInstruction << 8) >> 8;

            char branch = 0;

            switch(type) {
                case 0xA:
                    branch = (registers[16]) >> 31;
                    break;
                case 0xE:
                    // Unconditional
                    branch = 1;
                    break;
                default:
                    break;
            }
            if (debug) {printf("Branch of type %X to offset %X\n", type, offset);}
            if (branch) {
                registers[15] += offset;
            }

        }

        if (debug) {printf("Register Output: R0 = %X, R1 = %X, R2 = %X, R3 = %X, PSX = %X\n", registers[0], registers[1], registers[2], registers[3], registers[16]);}
        registers[15] ++;
        nextInstruction = memory[registers[15]];
        if (printToggle != registers[12]) {
            printf("Print Register Updated: \"0x%X\" \"#%d\"\n", registers[12], registers[12]);
        }
    }
    
    



    return 0;
}