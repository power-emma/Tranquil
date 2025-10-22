
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h> 

// Arm Emulator
// Emma Power - 2025


uint32_t byteIsolate(uint32_t opcode, uint8_t position, uint8_t size, uint8_t offset) {
    //position right to left
    uint32_t mask = (1 << size) - 1;
    opcode = opcode >> ((size * position) + offset);
    return opcode & mask;
}

volatile int uart_running = 1;
volatile uint32_t UART_TX, UART_RX, UART_CTS, UART_RTS;
volatile uint32_t* UART_buffer;

void* UART_thread() {
    uint8_t readIndex = 0;
    printf("Terminal Emulator Started\n");
    // UART Thread
    while (uart_running || UART_buffer[readIndex] != 0) {
        if (UART_buffer[readIndex] != 0) {
            printf("%c", (char) UART_buffer[readIndex]);
            UART_buffer[readIndex] = 0;
            readIndex++;
        } else {
            //usleep(1); // Sleep for 0.1ms to reduce CPU usage
        }
    }
    printf("Terminal Emulator Exiting\n");
    return NULL;
}

// Register 12 is print
int main () {

    // Turn on step by step instructions
    int performance = 0;
    int debug = 0;

    // char write_msg[BUFFER_SIZE];
    // char read_msg[BUFFER_SIZE];

    // Create POSIX shared memory object for UART_buffer
    int shm_fd = shm_open("/uart_buffer", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }
    if (ftruncate(shm_fd, 256 * sizeof(uint32_t)) == -1) {
        perror("ftruncate failed");
        exit(1);
    }
    UART_buffer = (uint32_t*) mmap(NULL, 256 * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (UART_buffer == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    memset((void*)UART_buffer, 0, 256 * sizeof(uint32_t));

    pthread_t uart_tid;
    pthread_create(&uart_tid, NULL, (void*)UART_thread, NULL);
  
    uint32_t* registers = (uint32_t*) malloc(17*sizeof(uint32_t));       
    uint32_t* memory = (uint32_t*) malloc(65535*sizeof(uint32_t));

    // memory addresses 0x
    uint32_t TX_FIFO, RX_FIFO;
    uint8_t UART_buffer_write = 0;
    registers[15] = 0; // PC


    FILE *fptr;
    fptr = fopen("../Neurotic/test/print_num.asm.bin", "r");
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
        printf("-------CPU  START--------\n");
    // For runtime stats
    long instructionsElapsed = 0;
    struct timeval start, end;
    gettimeofday(&start, NULL);


    while (nextInstruction != (uint32_t) 0xD4400000 && registers[13] < 300 && (registers[13] > 100 || registers[13] < 10)) {
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
                case 0:
                    // AND
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] & (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] & operand2;
                    }
                    break;
                case 1:
                    // EOR
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] ^ (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] ^ operand2;
                    }
                    break;
                case 2:
                    //SUB
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] - (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] - operand2;
                    }

                    break;
                case 3:
                    // RSB
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand2] - (uint32_t)registers[operand1];
                    } else {
                        registers[destination] = operand2 - registers[operand1];
                    }
                    break;
                case 4:
                    // ADD
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] + (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] + operand2;
                    }

                    if (debug) {printf("Performing ADD instruction \"%X\" on %X and %X equals %X\n", opcode, operand1, operand2, registers[destination]);}

                    break;
                case 5:
                    // ADC
                    // TODO: ADD CARRY FLAG
                    // = O1 + O2 + C
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] + (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] + operand2;
                    }
                    break;
                case 6:
                    // SBC
                    // TODO: ADD CARRY FLAG
                    // = O1 - O2 + C - 1
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] - (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] - operand2;
                    }
                    break;
                case 7:
                    // RSC
                    // TODO: ADD CARRY FLAG
                    // = O2 - O1 + C - 1
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] - (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] - operand2;
                    }
                    break;
                case 8:
                    // TST
                    // TODO: SET FLAGS ON O1 & O2
                    break;
                case 9:
                    // TEQ
                    // TODO: SET FLAGS ON O1 ^ O2
                    break;
                case 10:
                    // CMP
                    // SETS FLAG ON O1 - O2
                    if (debug) {printf("Performing CMP instruction immedate = %X, \"%X\" on %X and %X\n", immBit, opcode, operand1, operand2);}
                    if (debug) {printf("R%d = %X\n", operand1, registers[operand1]);}
                    if (debug) {printf("R%d = %X\n", operand2, registers[operand2]);}
                    // N (Negative) Flag
                    if (((registers[operand1] - operand2) >> 31) == 1 && immBit) {
                        registers[16] |= 0x80000000;
                        if (debug) {printf("Negative Flag Set\n"); }
                    } else if (((registers[operand1] - registers[operand2]) >> 31) == 1 && !immBit) {
                        registers[16] |= 0x80000000;
                        if (debug) {printf("Negative Flag Set\n"); }
                    } else {
                        registers[16] &= 0x4FFFFFFF; // Clear Zero Flag
                    }
                    // Z (Zero) Flag
                    if (registers[operand1] == operand2 && immBit) {
                        registers[16] |= 0x40000000;
                        if (debug) {printf("Zero Flag Set Main\n"); }
                    } else if (registers[operand1] == registers[operand2] && !immBit) {
                        registers[16] |= 0x40000000;
                        if (debug) {printf("Zero Flag Set Reg\n"); }
                    } else {
                        registers[16] &= 0xBFFFFFFF; // Clear Zero Flag
                    }
                    // C (Carry) Flag
                    if (registers[operand1] >= operand2 && immBit) {
                        registers[16] |= 0x20000000; // Set Overflow Flag
                        if (debug) {printf("Carry Flag Set\n"); }
                    } else if (registers[operand1] >= registers[operand2] && !immBit) {
                        registers[16] |= 0x20000000; // Set Overflow Flag
                        if (debug) {printf("Carry Flag Set\n"); }
                    } else {
                        registers[16] &= 0xDFFFFFFF; // Clear Overflow Flag
                    }

                    // V (Overflow) Flag
                    if (registers[destination] > 0 && registers[operand1] < 0 && registers[operand2] < 0) {
                        registers[16] |= 0x10000000; // Set Overflow Flag
                        if (debug) {printf("Overflow Flag Set\n"); }
                    } else if (registers[destination] < 0 && registers[operand1] > 0 && registers[operand2] > 0) {
                        registers[16] |= 0x10000000; // Set Overflow Flag
                        if (debug) {printf("Overflow Flag Set\n"); }
                    } else {
                        registers[16] &= 0xEFFFFFFF; // Clear Overflow Flag
                    }
                    
                    break;
                case 11:
                    // CMN
                    // TODO: SET FLAGS ON  O1 + O2
                    break;
                case 12:
                    // ORR
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] | (uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] | operand2;
                    }
                    break;
                case 13:
                // MOV
                    if (immBit) {
                        if (debug) {printf("Performing MOV instruction \"%X\" on %X equals %X\n", opcode, operand2, (uint32_t)registers[operand2]);}
                        registers[destination] = (uint32_t)registers[operand1];
                    } else {
                        if (debug) {printf("Performing MOV instruction \"%X\" on %X equals %X\n", opcode, operand1, (uint32_t)registers[operand1]);}

                        registers[destination] = (uint32_t)registers[operand1];
                    }
                    break;
                case 14:
                    // BIC
                    // O1 AND NOT O2
                    if (immBit) {
                        registers[destination] = (uint32_t) registers[operand1] & !(uint32_t)registers[operand2];
                    } else {
                        registers[destination] = registers[operand1] & !operand2;
                    }
                    break;
                case 15:
                    //MVN
                    if (immBit) {
                        registers[destination] = !(uint32_t)registers[operand2];
                    } else {
                        registers[destination] = !operand2;
                    }
                    break;
                default:
                    break;
            }

            //Shifts
            uint32_t shiftType = byteIsolate(nextInstruction, 0, 2, 5);
            uint32_t shiftFromReg = byteIsolate(nextInstruction, 0, 1, 4);
            if (byteIsolate(nextInstruction, 0,5, 7) == 0 || !immBit) {
                if (debug) {printf("No Shift Applied\n");}
                // No shift
            }
            else if (shiftFromReg == 0) {
                
                uint32_t shiftAmount = byteIsolate(nextInstruction, 0,5, 7);
                if (debug) {printf("Shift Opcode = %X\n", byteIsolate(nextInstruction, 0,8,4));}
                if (debug) {printf("Destination Register is currently %X\n", registers[destination]);}
                if (debug) {printf("Shifting register %d by %d of type %d\n", destination, shiftAmount, shiftType);}
                switch(shiftType) {
                    case 0:
                        // LSL
                        registers[destination] = registers[destination] << shiftAmount;
                        break;
                    case 1:
                        // LSR
                        registers[destination] = registers[destination] >> shiftAmount;
                        break;
                    case 2:
                        // ASR
                        registers[destination] = (int32_t)registers[destination] >> shiftAmount;
                        break;
                    case 3:
                        // ROR
                        registers[destination] = (registers[destination] >> shiftAmount) | (registers[destination] << (32 - shiftAmount));
                        break;
                    default:
                        break;
                }
            } else {
                uint8_t shiftReg = byteIsolate(nextInstruction, 8, 4, 0);
                switch(shiftType) {
                    case 0:
                        // LSL
                        registers[destination] = registers[destination] << (registers[shiftReg] & 0xFF);
                        break;
                    case 1:
                        // LSR
                        registers[destination] = registers[destination] >> (registers[shiftReg] & 0xFF);
                        break;
                    case 2:
                        // ASR
                        registers[destination] = (int32_t)registers[destination] >> (registers[shiftReg] & 0xFF);
                        break;
                    case 3:
                        // ROR
                        registers[destination] = (registers[destination] >> (registers[shiftReg] & 0xFF)) | (registers[destination] << (32 - (registers[shiftReg] & 0xFF)));
                        break;
                    default:
                        break;
                }
            }


        } else if (nextInstruction == (nextInstruction & 0xF7FFFFFF)) {
            //printf("^ Single Data Transfer \n");
            char loadBit = byteIsolate(nextInstruction, 0, 1, 20);
            uint8_t immBit = byteIsolate(nextInstruction, 0, 1, 25);
            uint32_t offset = byteIsolate(nextInstruction, 0, 12, 0);
            uint8_t destination = byteIsolate(nextInstruction, 3, 4, 0);
            if (debug) {printf("Load/Store Instruction LoadBit: %X, ImmBit: %X, Offset: %X, Destination: %d\n", loadBit, immBit, offset, destination);}

            if (loadBit) {
                if (!immBit) {
                    if (debug) {printf("Loading value %X into register %d from memory address %d\n", memory[registers[offset]], destination, registers[offset]);}

                    offset = memory[registers[offset]];
                    if (debug) {printf("Offset from memory address %X is %X\n", memory[registers[offset]], registers[offset]);}
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
                        UART_buffer[UART_buffer_write] = TX_FIFO;
                        //printf("Wrote %c to UART buffer at index %d\n", (char) UART_buffer[UART_buffer_write], UART_buffer_write);
                        UART_buffer_write++;
                        if (debug) {
                            printf("UART Transmitting: %c - %X\n", (char) TX_FIFO, TX_FIFO);

                        } else {
                            //printf("%c", (char) TX_FIFO);

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
                    memory[registers[offset]] = registers[destination];

                    if (debug) {printf("Storing %X into memory address at r %d, is %d with value %X\n", registers[destination], offset, registers[offset], memory[registers[offset]]);};
                }
            }
        
        } else if (nextInstruction == (nextInstruction & 0xFAFFFFFF)) {
            //printf("Branch Instruction");
            
            char type = byteIsolate(nextInstruction, 7, 4, 0);

            int32_t offset = (int32_t) (nextInstruction << 8) >> 8;

            char branch = 0;

            switch(type) {
                // BEQ
                case 0x0:
                    branch = byteIsolate(registers[16], 30, 1, 0);
                    break;
                //BNE
                case 0x1:
                    branch = !byteIsolate(registers[16], 30, 1, 0);
                    break;
                // BHS
                case 0x2:
                    branch = byteIsolate(registers[16], 29, 1, 0);
                    break;
                // BHS
                case 0x3:
                    branch = !byteIsolate(registers[16], 29, 1, 0);
                    break;
                case 0xE:
                    // Unconditional
                    branch = 1;
                    break;
                default:
                    break;
            }
            if (debug) {printf("Branch of type %d from %d to offset %d condition is %X \n", type, registers[15], offset, branch);}
            if (branch) {
                if (offset >= 0) {
                    registers[15] += offset;
                    registers[15] --; // Offset the increment at end of loop
                } else {
                    registers[15] += offset;
                    registers[15] --;
                }
                if (debug) {printf("Branch Taken to address %X\n", registers[15]);}
            }

        }

        if (debug) {printf("Register Output: R0 = %X, R1 = %X, R2 = %X, R3 = %X, R12 = %X, PSX = %X, PC = %X, SP = %d\n", registers[0], registers[1], registers[2], registers[3], registers[12], registers[16], registers[15], registers[13]);}
        
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
    printf("\n-------CPU HALTED--------\n");
    printf("EMU STATS: Completed %d instructions in %d microseconds\n", instructionsElapsed, mtime);
    printf("EMU STATS: Clock Speed = %f MHz\n", effectiveClock);
    uart_running = 0; // Signal UART thread to exit
    usleep(1000000); // Give UART thread time to print remaining buffer

    return 0;
}