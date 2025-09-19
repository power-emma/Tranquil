
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


int main () {

    uint32_t* registers = (uint32_t*) malloc(4*sizeof(uint32_t));       


    FILE *fptr;
    fptr = fopen("../file.bin", "r");
    if (fptr == NULL) {
        printf("File not found");
        return 1;
    }

    u_int32_t nextLine[1];

    while (fread(nextLine, 4, 1, fptr)) {
        printf("%X\n", nextLine[0]);

        // Data Processing Instruction
        if (nextLine[0] == (nextLine[0] & 0xF3FFFFFF)) {
            //printf("^ Data Processing \n");

            
            uint8_t opcode = byteIsolate(nextLine[0], 5, 4, 1);
            uint8_t operand1 = byteIsolate(nextLine[0], 4, 4, 0);
            uint8_t operand2 = byteIsolate(nextLine[0], 0, 12, 0);
            uint8_t destination = byteIsolate(nextLine[0], 3, 4, 0);
            printf("Opcode: %X, o1 = %X, o2 = %X, dr = %X\n", opcode, operand1, operand2, destination);
            
            switch(opcode) {
                case 0:
                case 1:
                case 2:
                case 3:
                    break;
                case 4:
                    // ADD
                    printf("Performing ADD instruction \"%X\"\n", opcode);
                    registers[destination] = registers[operand1] + registers[operand2];
                    break;
                case 5:
                default:
                    break;
            }

        } else if (nextLine[0] == (nextLine[0] & 0xF7FFFFFF)) {
            //printf("^ Single Data Transfer \n");
            char loadBit = byteIsolate(nextLine[0], 20, 1, 0);
            uint16_t offset = byteIsolate(nextLine[0], 0, 12, 0);
            uint8_t destination = byteIsolate(nextLine[0], 3, 4, 0);
            printf("Save/Load: %X, into register %X with Value %X \n", loadBit, destination, offset);
            registers[destination] = offset;
        }

        printf("Register Output: R0 = %X, R1 = %X, R2 = %X, R3 = %X\n", registers[0], registers[1], registers[2], registers[3]);
    }
    
    



    return 0;
}