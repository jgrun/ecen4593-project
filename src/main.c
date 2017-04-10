/* src/main.c
 * Simulator top level
 */

#include "main.h"

int flags = 0; // Global flags register, shared across all files

// CPU state
control_t* ifid  = NULL; // IF/ID pipeline register
control_t* idex  = NULL; // ID/EX pipeline register
control_t* exmem = NULL; // EX/MEM pipeline register
control_t* memwb = NULL; // MEM/WB pipeline register
pc_t pc = 0;             // Program counter

uint32_t temp = 0;

int main(int argc, char *argv[]) {
    int i;
    // Validate args, if they exist
    if (argc == 1) {
        printf("Nothing to execute.\nUsage:\n");
        printf("\tsim [options] infile\n");
        printf("Options:\n");
        printf("\t-a: Alternate program format\n");
        printf("\t-d: Enable debug mode\n");
        printf("\t-i: Interactive stepping mode\n");
        printf("\t-s: Enable sanity checks\n");
        printf("\t-v: Enable verbose output\n");
        printf("\n");
        return 0; // exit without errors
    } else {
        // Stone-age command-line argument parsing
        for (i=0; i<argc-2; ++i) {
            switch (argv[i+1][1]) { // add command line option flags here
                case 'a': // -a: alternate format
                    flags |= MASK_ALTFORMAT;
                    printf("Alternate assembly format selected (flags = 0x%04x).\n",flags);
                    break;
                case 'd': // -d: debug
                    flags |= MASK_DEBUG;
                    printf("Debug mode enabled (flags = 0x%04x).\n",flags);
                    break;
                case 'i': // -i: interactive debug (step)
                    flags |= MASK_INTERACTIVE;
                    printf("Interactive mode enabled (flags = 0x%04x).\n",flags);
                    break;
                case 's': // -s: sanity
                    flags |= MASK_SANITY;
                    printf("Sanity checks enabled (flags = 0x%04x).\n",flags);
                    break;
                case 'v': // -v: verbose
                    flags |= MASK_VERBOSE;
                    printf("Verbose output enabled (flags = 0x%04x).\n",flags);
                    break;
                default:
                    printf("Option not recognized: %c\n",argv[i+1][1]);
                    break;
            }
        }
    }
    // Read in the assembly file to program space
    // The assembly file is always the last argument to the simulator
    FILE *asm_fp = fopen(argv[argc-1], "r");
    if (!asm_fp) {
        printf("Unable to open assembly file. Exiting..\n");
        return 1; // exit with errors
    }
    /**************************************************************************
     * Beginning the actual simulation                                        *
     * All initialization and state configuration happens below here          *
     **************************************************************************/
    // Initialize the register file
    reg_init();
    // Create an array to hold all the debug information
    asm_line_t lines[MEMORY_SIZE];
    for (i = 0; i < MEMORY_SIZE; ++i) lines[i].type = 0; // initialize all invalid
    // Parse the ASM file, parse() initializes the memory
    parse(asm_fp, lines);
    mem_dump();
    // Initialize the pipeline registers
    pipeline_init(&ifid, &idex, &exmem, &memwb, &pc,  (pc_t)mem_start());
    if (flags & MASK_ALTFORMAT) {
        // set the program counter based on the fifth word of memory
        mem_read_w(5<<2, &temp);
        pc = temp * 4;
    }
    // Run the simulation
    int cycles = 0;
    while (1) {
        // Run a pipeline cycle
        writeback(memwb);
        memory(exmem, memwb);
        execute(idex, exmem);
        decode(ifid, idex);
        fetch(ifid, &pc);
        hazard(ifid, idex, exmem, memwb, &pc);
        ++cycles;
        // Check for a magic halt number (beq zero zero -1 or jr zero)
        if (ifid->instr == 0x1000ffff || ifid->instr == 0x00000008 || pc == 0) break;
        if (flags & MASK_INTERACTIVE) { // Run interactive step
            if (interactive(lines) !=0) return 1;
        }
    }
    printf("\nPipeline halted after %d cycles (address 0x%08x)\n",cycles,pc);
    // Dump registers and the first couple words of memory so we can see what's going on
    reg_dump();
    mem_dump_cute(0,16);
    // Close memory, and cleanup register files (we don't need to clean up registers)
    pipeline_destroy(&ifid, &idex, &exmem, &memwb);
    mem_close();
    return 0; // exit without errors
}

int parse(FILE *fp, asm_line_t *lines) {
    uint32_t addr, inst, data, start;
    int count = 0;
    char buf[180]; // for storing a line from the source file
    char str[120]; // for the comment part of a line from the source file
    if (flags & MASK_ALTFORMAT) { // .txt "array" format
        addr = 0;
        mem_init(MEMORY_SIZE,0); // memory is assumed to start at 0x0
        // Iterate through file line-by-line
        while (fgets(buf, sizeof(buf), fp) != NULL ) {
            // Read the instruction into memory
            if (sscanf(buf,"0x%x",&inst) == 1) {
                mem_write_w(addr,&inst);
                lines[count].addr = addr;
                lines[count].inst = inst;
                lines[count].type = 2;
                addr += 4;
                // Read the comment if it exists
                if (sscanf(buf,"0x%*x, // %[^\n]", str) == 1) {
                    strcpy(lines[count].comment, str);
                    lines[count].type = 3;
                }
                ++count;
            }
        }
        // Set registers
        mem_read_w(0x0,&data);
        reg_write(REG_SP, &data);
        mem_read_w(0x1,&data);
        reg_write(REG_FP, &data);
        // (program counter is set after initializing pipeline)
    } else { // .s format
        // iterate through file line-by-line
        while (fgets(buf, sizeof(buf), fp) != NULL ) {
            // scanf magic to extract an address, colon, instruction, and the remaining line
            if (sscanf(buf,"%x: %x %[^\n]",&addr,&inst,str) == 3) {
                if (count == 0) { // first instruction, set offset and initialize memory
                    if (flags & MASK_VERBOSE) printf("First instruction found. %s",buf);
                    mem_init(MEMORY_SIZE,addr);
                    start = addr;
                }
                // write extracted instruction into memory and also into lines array
                mem_write_w(addr,&inst);
                lines[(addr>>2)-(start>>2)].addr = addr;
                lines[(addr>>2)-(start>>2)].inst = inst;
                strcpy(lines[(addr>>2)-(start>>2)].comment, str);
                lines[(addr>>2)-(start>>2)].type = 3;
                ++count;
            } else if (sscanf(buf,"%x: %x\n",&addr,&data) == 2) {
                // write extracted data into memory and also into lines array
                mem_write_w(addr,&data);
                lines[(addr>>2)-(start>>2)].addr = addr;
                lines[(addr>>2)-(start>>2)].inst = data;
                lines[(addr>>2)-(start>>2)].type = 2;
                ++count;
            }
        }
    }
    fclose(fp); // close the file
    printf("Successfully extracted %d lines\n",count);
    return count;
}

// Provides a crude interactive debugger for the simulator
int interactive(asm_line_t* lines) {
    uint32_t i_addr = 0, i_data;
    asm_line_t line;
PROMPT: // LOL gotos
    printf(ANSI_C_GREEN "(interactive) > " ANSI_C_RESET);
    system ("/bin/stty raw"); // set terminal to raw/unbuffered
    char c = getchar();
    system ("/bin/stty sane"); // set back to sane
    printf("%c\n",c);
    switch(c) {
        case 'd': // disable interactive
            flags &= ~(MASK_INTERACTIVE);
            printf(ANSI_C_GREEN "Interactive stepping disabled.\n" ANSI_C_RESET);
            break;
        case '?': // help
            printf("Available interactive commands: \n" \
                "\td: disable interactive mode\n" \
                "\tl: print the original disassembly for a given memory address\n" \
                "\tm: print a memory word for a given memory address\n" \
                "\to: print 11 words of memory surrounding a given memory address\n" \
                "\ts: single-step the pipeline\n" \
                "\tr: dump registers\n" \
                "\tx: exit simulation run\n");
            goto PROMPT;
        case 'l': // print the original disassembly for a given address
            printf(ANSI_C_GREEN "input address: " ANSI_C_RESET);
            scanf("%x",&i_addr); getchar();
            line = lines[(i_addr>>2)-(mem_start()>>2)];
            if (line.type == 3) {
                printf("\t0x%08x: 0x%08x %s\n",line.addr,line.inst,line.comment);
            } else if (line.type == 2) {
                printf("\t0x%08x: 0x%08x\n",line.addr,line.inst);
            } else {
                printf("\tNot a valid input line\n");
            }
            goto PROMPT;
        case 'm': // view a word of memory
            printf(ANSI_C_GREEN "memory address: " ANSI_C_RESET);
            scanf("%x",&i_addr); getchar();
            if (i_addr < mem_start() || i_addr > mem_end()) {
                printf("Address out of range\n");
                goto PROMPT;
            }
            mem_read_w(i_addr, &i_data);
            printf("mem[0x%08x]: 0x%08x (0d%d)\n",i_addr,i_data,i_data);
            goto PROMPT;
        case 'o': // view a region of memory
            printf(ANSI_C_GREEN "memory address: " ANSI_C_RESET);
            scanf("%x",&i_addr); getchar();
            if (i_addr < mem_start() || i_addr > mem_end()) {
                printf("Address out of range\n");
                goto PROMPT;
            }
            if (i_addr < mem_start()+(5<<2)) i_addr = mem_start()+(5<<2);
            if (i_addr > mem_end()-(5<<2)) i_addr = mem_end()-(5<<2);
            mem_dump_cute(i_addr-(5<<2),11);
            goto PROMPT;
        case 's': // step
            break;
        case 'r': // dump registers
            reg_dump();
            goto PROMPT;
        case 'x': // exit
            printf(ANSI_C_GREEN "Simulation halted in interactive mode.\n" ANSI_C_RESET);
            return 1;
        default:
            printf("Unrecognized interactive command \"%c\", press \"?\" for help.\n", c);
            goto PROMPT;
    }
    return 0;
}
