
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

// Arm Emulator
// Emma Power - 2025


uint32_t byteIsolate(uint32_t opcode, uint8_t position, uint8_t size, uint8_t offset) {
    //position right to left
    uint32_t mask = (1 << size) - 1;
    opcode = opcode >> ((size * position) + offset);
    return opcode & mask;
}

// Register 12 is print
int main () {
    // Turn on step by step instructions
    int performance = 0;
    int debug = 0;
    uint32_t* registers = (uint32_t*) malloc(17*sizeof(uint32_t));       
    uint32_t* memory = (uint32_t*) malloc(1024*sizeof(uint32_t));
    // memory addresses 0x
    uint32_t UART_TX, UART_RX, UART_CTS, UART_RTS;
    uint32_t TX_FIFO, RX_FIFO;
    
    registers[15] = 0; // PC


    FILE *fptr;
    fptr = fopen("../Neurotic/test/hello_world.asm.bin", "r");
    if (fptr == NULL) {
        printf("File not found");
        return 1;
    }

    u_int32_t nextLine[1];
    int fileReadIndex = 0;
    while (fread(nextLine, 4, 1, fptr)) {
        memory[fileReadIndex] = nextLine[0];
        fileReadIndex ++;
        if (debug) {printf("Read %X to memory address %d\n", nextLine[0], fileReadIndex-1);}
    }
    uint32_t nextInstruction = memory[registers[15]];
    
    // For runtime stats
    long instructionsElapsed = 0;
    struct timeval start, end;
    gettimeofday(&start, NULL);


    while (nextInstruction != (uint32_t) 0xD4400000) {
        uint32_t printToggle = registers[12];

        if (debug) {printf("Next Instruction Is %X\n", nextInstruction);}

        // Data Processing Instruction
        if (nextInstruction == (nextInstruction & 0xF3FFFFFF)) {

            
            uint8_t opcode = byteIsolate(nextInstruction, 5, 4, 1);
            uint8_t operand1 = byteIsolate(nextInstruction, 4, 4, 0);
            uint32_t operand2 = byteIsolate(nextInstruction, 0, 12, 0);
            uint8_t destination = byteIsolate(nextInstruction, 3, 4, 0);
            if (debug) {printf("Opcode: %X, o1 = %X, o2 = %X, dr = %X\n", opcode, operand1, operand2, destination);}
            uint8_t immBit = (nextInstruction << 6) >> 31;
            
            switch(opcode) {
                case 4:
                    // ADD
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] + (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] + operand2;
                    }

                    if (debug) {printf("Performing ADD instruction \"%X\" on %X and %X equals %X\n", opcode, operand1, operand2, registers[destination]);}


                    break;
                case 10:
                    if (debug) {printf("Performing CMP instruction immedate = %X, \"%X\" on %X and %X\n", immBit, opcode, operand1, operand2);}

                    if (registers[operand1] == operand2 && immBit) {
                        registers[16] = 0x80000000;
                    } else if (registers[operand1] == registers[operand2] && !immBit) {
                        registers[16] = 0x80000000;
                    } else {
                        registers[16] = 0x00000000;

                    }
                    
                    break;
                default:
                    break;
            }

        } else if (nextInstruction == (nextInstruction & 0xF7FFFFFF)) {
            //printf("^ Single Data Transfer \n");
            char loadBit = byteIsolate(nextInstruction, 0, 1, 20);
            uint8_t immBit = byteIsolate(nextInstruction, 25, 1, 0);
            uint32_t offset = byteIsolate(nextInstruction, 0, 12, 0);
            uint8_t destination = byteIsolate(nextInstruction, 3, 4, 0);
            if (loadBit) {
                if (immBit) {
                    offset = memory[registers[offset]];
                } else {
                    offset = memory[registers[15] + offset];
                }
            } else {
                // if (immBit) {
                //     offset = registers[destination];
                // }
            }


            if (debug) {printf("Save/Load: %X, Immediate: %X, into register %d with Value %X \n", loadBit, immBit, destination, offset);}

            if (loadBit) {
                // Load
                registers[destination] = offset;
            } else {
                if (registers[offset] > 0xEFFFFFFF) {
                    // UART
                    if (registers[offset] == 0xF0000000) {
                        // TX FIFO
                        TX_FIFO = registers[destination];

                        if (debug) {
                            printf("UART Transmitting: %c - %X\n", (char) TX_FIFO, TX_FIFO);

                        } else {
                            printf("%c", (char) TX_FIFO);

                        }
                    } else if (registers[offset] + registers[15] == 0xF0000004) {
                        // RX FIFO
                        RX_FIFO = registers[destination];;
                        printf("UART Receiving: %c\n", (char) RX_FIFO);

                    } else if (registers[offset] == 0xF0000008) {
                        // CTS
                        UART_CTS = registers[destination];;

                    } else if (registers[offset] == 0xF000000C) {
                        // RTS
                        UART_RTS = registers[destination];

                    }
                } else {
                    // Store
                    memory[registers[offset] + registers[15]] = registers[destination];

                    printf("Storing %X into memory address %X\n", registers[destination], registers[offset] + registers[15]);
                }
            }
        
        } else if (nextInstruction == (nextInstruction & 0xFAFFFFFF)) {
            //printf("Branch Instruction");
            
            char type = byteIsolate(nextInstruction, 7, 4, 0);

            int32_t offset = (int32_t) (nextInstruction << 8) >> 8;

            char branch = 0;

            switch(type) {
                case 0x0:
                    branch = byteIsolate(registers[16], 31, 1, 0);
                    break;
                case 0xE:
                    // Unconditional
                    branch = 1;
                    break;
                default:
                    break;
            }
            if (debug) {printf("Branch of type %X to offset %X condition is %X \n", type, offset, branch);}
            if (branch) {
                registers[15] += offset;
            }

        }

        if (debug) {printf("Register Output: R0 = %X, R1 = %X, R2 = %X, R3 = %X, R12 = %X, PSX = %X, PC = %X\n", registers[0], registers[1], registers[2], registers[3], registers[12], registers[16], registers[15]);}
        registers[15] ++; // Increment PC
        nextInstruction = memory[registers[15]]; // Get Next Instruction
        instructionsElapsed++; // For Stats
        if (!performance && printToggle != registers[12]) {
            printf("Print Register Updated: \"0x%X\" \"#%d\"\n", registers[12], registers[12]);
        }
        if (debug) {("Memory Address 0x10 contains: %X\n", memory[16]);}
    }
    
    gettimeofday(&end, NULL);
    long mtime, seconds, useconds;

    seconds = end.tv_sec - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    // Handle cases where the microsecond component of end is less than start
    if (useconds < 0) {
        seconds--;
        useconds += 1000000;
    }

    mtime = (seconds * 1000000) + useconds; // Total elapsed time in microseconds

    float effectiveClock = (float) instructionsElapsed/(mtime);
    
    printf("Completed %d instructions in %d microseconds\n", instructionsElapsed, mtime);
    printf("Clock Speed = %f MHz\n", effectiveClock);

    return 0;
}