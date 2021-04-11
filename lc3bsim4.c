/***************************************************************/
/*                                                             */
/*   LC-3b Simulator                                           */
/*                                                             */
/*   EE 460N                                                   */
/*   The University of Texas at Austin                         */
/*                                                             */
/***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************/
/*                                                             */
/* Files:  ucode        Microprogram file                      */
/*         isaprogram   LC-3b machine language program file    */
/*                                                             */
/***************************************************************/

/***************************************************************/
/* These are the functions you'll have to write.               */
/***************************************************************/

void eval_micro_sequencer();
void cycle_memory();
void eval_bus_drivers();
void drive_bus();
void latch_datapath_values();
int signExt(int input, int numSize);



/***************************************************************/
/* A couple of useful definitions.                             */
/***************************************************************/
#define FALSE 0
#define TRUE  1

/***************************************************************/
/* Use this to avoid overflowing 16 bits on the bus.           */
/***************************************************************/
#define Low16bits(x) ((x) & 0xFFFF)

/***************************************************************/
/* Definition of the control store layout.                     */
/***************************************************************/
#define CONTROL_STORE_ROWS 64
#define INITIAL_STATE_NUMBER 18

/***************************************************************/
/* Definition of bit order in control store word.              */
/***************************************************************/
enum CS_BITS {                                                  
    IRD,
    COND1, COND0,
    J5, J4, J3, J2, J1, J0,
    LD_MAR,
    LD_MDR,
    LD_IR,
    LD_BEN,
    LD_REG,
    LD_CC,
    LD_PC,
    GATE_PC,
    GATE_MDR,
    GATE_ALU,
    GATE_MARMUX,
    GATE_SHF,
    PCMUX1, PCMUX0,
    DRMUX,
    SR1MUX,
    ADDR1MUX,
    ADDR2MUX1, ADDR2MUX0,
    MARMUX,
    ALUK1, ALUK0,
    MIO_EN,
    R_W,
    DATA_SIZE,
    LSHF1,
/* MODIFY: you have to add all your new control signals */
    CONTROL_STORE_BITS
} CS_BITS;

/***************************************************************/
/* Functions to get at the control bits.                       */
/***************************************************************/
int GetIRD(int *x)           { return(x[IRD]); }
int GetCOND(int *x)          { return((x[COND1] << 1) + x[COND0]); }
int GetJ(int *x)             { return((x[J5] << 5) + (x[J4] << 4) +
				      (x[J3] << 3) + (x[J2] << 2) +
				      (x[J1] << 1) + x[J0]); }
int GetLD_MAR(int *x)        { return(x[LD_MAR]); }
int GetLD_MDR(int *x)        { return(x[LD_MDR]); }
int GetLD_IR(int *x)         { return(x[LD_IR]); }
int GetLD_BEN(int *x)        { return(x[LD_BEN]); }
int GetLD_REG(int *x)        { return(x[LD_REG]); }
int GetLD_CC(int *x)         { return(x[LD_CC]); }
int GetLD_PC(int *x)         { return(x[LD_PC]); }
int GetGATE_PC(int *x)       { return(x[GATE_PC]); }
int GetGATE_MDR(int *x)      { return(x[GATE_MDR]); }
int GetGATE_ALU(int *x)      { return(x[GATE_ALU]); }
int GetGATE_MARMUX(int *x)   { return(x[GATE_MARMUX]); }
int GetGATE_SHF(int *x)      { return(x[GATE_SHF]); }
int GetPCMUX(int *x)         { return((x[PCMUX1] << 1) + x[PCMUX0]); }
int GetDRMUX(int *x)         { return(x[DRMUX]); }
int GetSR1MUX(int *x)        { return(x[SR1MUX]); }
int GetADDR1MUX(int *x)      { return(x[ADDR1MUX]); }
int GetADDR2MUX(int *x)      { return((x[ADDR2MUX1] << 1) + x[ADDR2MUX0]); }
int GetMARMUX(int *x)        { return(x[MARMUX]); }
int GetALUK(int *x)          { return((x[ALUK1] << 1) + x[ALUK0]); }
int GetMIO_EN(int *x)        { return(x[MIO_EN]); }
int GetR_W(int *x)           { return(x[R_W]); }
int GetDATA_SIZE(int *x)     { return(x[DATA_SIZE]); } 
int GetLSHF1(int *x)         { return(x[LSHF1]); }
/* MODIFY: you can add more Get functions for your new control signals */

/***************************************************************/
/* The control store rom.                                      */
/***************************************************************/
int CONTROL_STORE[CONTROL_STORE_ROWS][CONTROL_STORE_BITS];

/***************************************************************/
/* Main memory.                                                */
/***************************************************************/
/* MEMORY[A][0] stores the least significant byte of word at word address A
   MEMORY[A][1] stores the most significant byte of word at word address A 
   There are two write enable signals, one for each byte. WE0 is used for 
   the least significant byte of a word. WE1 is used for the most significant 
   byte of a word. */

#define WORDS_IN_MEM    0x08000 
#define MEM_CYCLES      5
int MEMORY[WORDS_IN_MEM][2];

/***************************************************************/

/***************************************************************/

/***************************************************************/
/* LC-3b State info.                                           */
/***************************************************************/
#define LC_3b_REGS 8

int RUN_BIT;	/* run bit */
int BUS;	/* value of the bus */

/*
 * BUS DRIVER REGISTERS
 */

int MARMUX_VAL;
int PC_VAL;
int ALU_VAL;
int SHF_VAL;
int MDR_VAL;

int IntermediateMDR;
int AdderOut;

typedef struct System_Latches_Struct{

int PC,		/* program counter */
    MDR,	/* memory data register */
    MAR,	/* memory address register */
    IR,		/* instruction register */
    N,		/* n condition bit */
    Z,		/* z condition bit */
    P,		/* p condition bit */
    BEN;        /* ben register */

int READY;	/* ready bit */
  /* The ready bit is also latched as you dont want the memory system to assert it 
     at a bad point in the cycle*/

int REGS[LC_3b_REGS]; /* register file. */

int MICROINSTRUCTION[CONTROL_STORE_BITS]; /* The microintruction */

int STATE_NUMBER; /* Current State Number - Provided for debugging */ 

/* For lab 4 */
int INTV; /* Interrupt vector register */
int EXCV; /* Exception vector register */
int SSP; /* Initial value of system stack pointer */
/* MODIFY: You may add system latches that are required by your implementation */

} System_Latches;

/* Data Structure for Latch */

System_Latches CURRENT_LATCHES, NEXT_LATCHES;

/***************************************************************/
/* A cycle counter.                                            */
/***************************************************************/
int CYCLE_COUNT;

/***************************************************************/
/*                                                             */
/* Procedure : help                                            */
/*                                                             */
/* Purpose   : Print out a list of commands.                   */
/*                                                             */
/***************************************************************/
void help() {                                                    
    printf("----------------LC-3bSIM Help-------------------------\n");
    printf("go               -  run program to completion       \n");
    printf("run n            -  execute program for n cycles    \n");
    printf("mdump low high   -  dump memory from low to high    \n");
    printf("rdump            -  dump the register & bus values  \n");
    printf("?                -  display this help menu          \n");
    printf("quit             -  exit the program                \n\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : cycle                                           */
/*                                                             */
/* Purpose   : Execute a cycle                                 */
/*                                                             */
/***************************************************************/
void cycle() {                                                

  eval_micro_sequencer();   
  cycle_memory();
  eval_bus_drivers();
  drive_bus();
  latch_datapath_values();

  CURRENT_LATCHES = NEXT_LATCHES;

  CYCLE_COUNT++;
}

/***************************************************************/
/*                                                             */
/* Procedure : run n                                           */
/*                                                             */
/* Purpose   : Simulate the LC-3b for n cycles.                 */
/*                                                             */
/***************************************************************/
void run(int num_cycles) {                                      
    int i;

    if (RUN_BIT == FALSE) {
	printf("Can't simulate, Simulator is halted\n\n");
	return;
    }

    printf("Simulating for %d cycles...\n\n", num_cycles);
    for (i = 0; i < num_cycles; i++) {
	if (CURRENT_LATCHES.PC == 0x0000) {
	    RUN_BIT = FALSE;
	    printf("Simulator halted\n\n");
	    break;
	}
	cycle();
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : go                                              */
/*                                                             */
/* Purpose   : Simulate the LC-3b until HALTed.                 */
/*                                                             */
/***************************************************************/
void go() {                                                     
    if (RUN_BIT == FALSE) {
	printf("Can't simulate, Simulator is halted\n\n");
	return;
    }

    printf("Simulating...\n\n");
    while (CURRENT_LATCHES.PC != 0x0000)
	cycle();
    RUN_BIT = FALSE;
    printf("Simulator halted\n\n");
}

/***************************************************************/ 
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(FILE * dumpsim_file, int start, int stop) {          
    int address; /* this is a byte address */

    printf("\nMemory content [0x%0.4x..0x%0.4x] :\n", start, stop);
    printf("-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
	printf("  0x%0.4x (%d) : 0x%0.2x%0.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    printf("\n");

    /* dump the memory contents into the dumpsim file */
    fprintf(dumpsim_file, "\nMemory content [0x%0.4x..0x%0.4x] :\n", start, stop);
    fprintf(dumpsim_file, "-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
	fprintf(dumpsim_file, " 0x%0.4x (%d) : 0x%0.2x%0.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */   
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump(FILE * dumpsim_file) {                               
    int k; 

    printf("\nCurrent register/bus values :\n");
    printf("-------------------------------------\n");
    printf("Cycle Count  : %d\n", CYCLE_COUNT);
    printf("PC           : 0x%0.4x\n", CURRENT_LATCHES.PC);
    printf("IR           : 0x%0.4x\n", CURRENT_LATCHES.IR);
    printf("STATE_NUMBER : 0x%0.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    printf("BUS          : 0x%0.4x\n", BUS);
    printf("MDR          : 0x%0.4x\n", CURRENT_LATCHES.MDR);
    printf("MAR          : 0x%0.4x\n", CURRENT_LATCHES.MAR);
    printf("CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    printf("Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
	printf("%d: 0x%0.4x\n", k, CURRENT_LATCHES.REGS[k]);
    printf("\n");

    /* dump the state information into the dumpsim file */
    fprintf(dumpsim_file, "\nCurrent register/bus values :\n");
    fprintf(dumpsim_file, "-------------------------------------\n");
    fprintf(dumpsim_file, "Cycle Count  : %d\n", CYCLE_COUNT);
    fprintf(dumpsim_file, "PC           : 0x%0.4x\n", CURRENT_LATCHES.PC);
    fprintf(dumpsim_file, "IR           : 0x%0.4x\n", CURRENT_LATCHES.IR);
    fprintf(dumpsim_file, "STATE_NUMBER : 0x%0.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    fprintf(dumpsim_file, "BUS          : 0x%0.4x\n", BUS);
    fprintf(dumpsim_file, "MDR          : 0x%0.4x\n", CURRENT_LATCHES.MDR);
    fprintf(dumpsim_file, "MAR          : 0x%0.4x\n", CURRENT_LATCHES.MAR);
    fprintf(dumpsim_file, "CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    fprintf(dumpsim_file, "Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
	fprintf(dumpsim_file, "%d: 0x%0.4x\n", k, CURRENT_LATCHES.REGS[k]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : get_command                                     */
/*                                                             */
/* Purpose   : Read a command from standard input.             */  
/*                                                             */
/***************************************************************/
void get_command(FILE * dumpsim_file) {                         
    char buffer[20];
    int start, stop, cycles;

    printf("LC-3b-SIM> ");

    scanf("%s", buffer);
    printf("\n");

    switch(buffer[0]) {
    case 'G':
    case 'g':
	go();
	break;

    case 'M':
    case 'm':
	scanf("%i %i", &start, &stop);
	mdump(dumpsim_file, start, stop);
	break;

    case '?':
	help();
	break;
    case 'Q':
    case 'q':
	printf("Bye.\n");
	exit(0);

    case 'R':
    case 'r':
	if (buffer[1] == 'd' || buffer[1] == 'D')
	    rdump(dumpsim_file);
	else {
	    scanf("%d", &cycles);
	    run(cycles);
	}
	break;

    default:
	printf("Invalid Command\n");
	break;
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : init_control_store                              */
/*                                                             */
/* Purpose   : Load microprogram into control store ROM        */ 
/*                                                             */
/***************************************************************/
void init_control_store(char *ucode_filename) {                 
    FILE *ucode;
    int i, j, index;
    char line[200];

    printf("Loading Control Store from file: %s\n", ucode_filename);

    /* Open the micro-code file. */
    if ((ucode = fopen(ucode_filename, "r")) == NULL) {
	printf("Error: Can't open micro-code file %s\n", ucode_filename);
	exit(-1);
    }

    /* Read a line for each row in the control store. */
    for(i = 0; i < CONTROL_STORE_ROWS; i++) {
	if (fscanf(ucode, "%[^\n]\n", line) == EOF) {
	    printf("Error: Too few lines (%d) in micro-code file: %s\n",
		   i, ucode_filename);
	    exit(-1);
	}

	/* Put in bits one at a time. */
	index = 0;

	for (j = 0; j < CONTROL_STORE_BITS; j++) {
	    /* Needs to find enough bits in line. */
	    if (line[index] == '\0') {
		printf("Error: Too few control bits in micro-code file: %s\nLine: %d\n",
		       ucode_filename, i);
		exit(-1);
	    }
	    if (line[index] != '0' && line[index] != '1') {
		printf("Error: Unknown value in micro-code file: %s\nLine: %d, Bit: %d\n",
		       ucode_filename, i, j);
		exit(-1);
	    }

	    /* Set the bit in the Control Store. */
	    CONTROL_STORE[i][j] = (line[index] == '0') ? 0:1;
	    index++;
	}

	/* Warn about extra bits in line. */
	if (line[index] != '\0')
	    printf("Warning: Extra bit(s) in control store file %s. Line: %d\n",
		   ucode_filename, i);
    }
    printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : init_memory                                     */
/*                                                             */
/* Purpose   : Zero out the memory array                       */
/*                                                             */
/***************************************************************/
void init_memory() {                                           
    int i;

    for (i=0; i < WORDS_IN_MEM; i++) {
	MEMORY[i][0] = 0;
	MEMORY[i][1] = 0;
    }
}

/**************************************************************/
/*                                                            */
/* Procedure : load_program                                   */
/*                                                            */
/* Purpose   : Load program and service routines into mem.    */
/*                                                            */
/**************************************************************/
void load_program(char *program_filename) {                   
    FILE * prog;
    int ii, word, program_base;

    /* Open program file. */
    prog = fopen(program_filename, "r");
    if (prog == NULL) {
	printf("Error: Can't open program file %s\n", program_filename);
	exit(-1);
    }

    /* Read in the program. */
    if (fscanf(prog, "%x\n", &word) != EOF)
	program_base = word >> 1;
    else {
	printf("Error: Program file is empty\n");
	exit(-1);
    }

    ii = 0;
    while (fscanf(prog, "%x\n", &word) != EOF) {
	/* Make sure it fits. */
	if (program_base + ii >= WORDS_IN_MEM) {
	    printf("Error: Program file %s is too long to fit in memory. %x\n",
		   program_filename, ii);
	    exit(-1);
	}

	/* Write the word to memory array. */
	MEMORY[program_base + ii][0] = word & 0x00FF;
	MEMORY[program_base + ii][1] = (word >> 8) & 0x00FF;
	ii++;
    }

    if (CURRENT_LATCHES.PC == 0) CURRENT_LATCHES.PC = (program_base << 1);

    printf("Read %d words from program into memory.\n\n", ii);
}

/***************************************************************/
/*                                                             */
/* Procedure : initialize                                      */
/*                                                             */
/* Purpose   : Load microprogram and machine language program  */ 
/*             and set up initial state of the machine.        */
/*                                                             */
/***************************************************************/
void initialize(char *argv[], int num_prog_files) { 
    int i;
    init_control_store(argv[1]);

    init_memory();
    for ( i = 0; i < num_prog_files; i++ ) {
	load_program(argv[i + 2]);
    }
    CURRENT_LATCHES.Z = 1;
    CURRENT_LATCHES.STATE_NUMBER = INITIAL_STATE_NUMBER;
    memcpy(CURRENT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[INITIAL_STATE_NUMBER], sizeof(int)*CONTROL_STORE_BITS);
    CURRENT_LATCHES.SSP = 0x3000; /* Initial value of system stack pointer */

    NEXT_LATCHES = CURRENT_LATCHES;

    RUN_BIT = TRUE;
}

/***************************************************************/
/*                                                             */
/* Procedure : main                                            */
/*                                                             */
/***************************************************************/
int main(int argc, char *argv[]) {                              
    FILE * dumpsim_file;

    /* Error Checking */
    if (argc < 3) {
	printf("Error: usage: %s <micro_code_file> <program_file_1> <program_file_2> ...\n",
	       argv[0]);
	exit(1);
    }

    printf("LC-3b Simulator\n\n");

    initialize(argv, argc - 2);

    if ( (dumpsim_file = fopen( "dumpsim", "w" )) == NULL ) {
	printf("Error: Can't open dumpsim file\n");
	exit(-1);
    }

    while (1)
	get_command(dumpsim_file);

}

/***************************************************************/
/* Do not modify the above code, except for the places indicated 
   with a "MODIFY:" comment.

   Do not modify the rdump and mdump functions.

   You are allowed to use the following global variables in your
   code. These are defined above.

   CONTROL_STORE
   MEMORY
   BUS

   CURRENT_LATCHES
   NEXT_LATCHES

   You may define your own local/global variables and functions.
   You may use the functions to get at the control bits defined
   above.

   Begin your code here 	  			       */
/***************************************************************/


void eval_micro_sequencer() {

//get MicroInstruction
    int *x = CURRENT_LATCHES.MICROINSTRUCTION;

    setbuf(stdout, 0);
    printf("State %d, cycle %d\n", CURRENT_LATCHES.STATE_NUMBER, CYCLE_COUNT);

    //check IRD bit
    int IRD = GetIRD(x);
    //If IRD = 1, process left side of MUX
    if(IRD){
        //next state is opcode of IR

        setbuf(stdout, 0);
        printf("decoding\n");

        int nextState = (CURRENT_LATCHES.IR >> 12) & 0b0000000000001111;
        //get next Microinstruction from Control Store
        //Latch next MicroInstruction
        memcpy(NEXT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[nextState], sizeof(int)*CONTROL_STORE_BITS);
        NEXT_LATCHES.STATE_NUMBER = nextState;
    }
        //If IRD != 1
    else{
        //check Cond0 and Cond1
        int condBits = GetCOND(x);
        int nextInstruction;
        switch(condBits){
            //unconditional
            case(0):
            {
                //get J bits
                int nextState = GetJ(x);
                //get next Microinstruction from Control Store
                //latch next MicroInstruction
                memcpy(NEXT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[nextState], sizeof(int)*CONTROL_STORE_BITS);
                NEXT_LATCHES.STATE_NUMBER = nextState;
                break;
            }
                //Memory Ready
            case(1):
            {
                //calculate next state from J bits and Ready Bit
                int nextState = GetJ(x) + (CURRENT_LATCHES.READY << 1);
                //get next Microinstruction from Control Store
                //Latch next MicroInstruction
                memcpy(NEXT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[nextState], sizeof(int)*CONTROL_STORE_BITS);
                NEXT_LATCHES.STATE_NUMBER = nextState;
                break;
            }
                //Branch
            case(2):
            {
                //calculate next state from J bits and BEN
                int nextState = GetJ(x) + (CURRENT_LATCHES.BEN << 2);
                //get next Microinstruction from Control Store
                //Latch next MicroInstruction
                memcpy(NEXT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[nextState], sizeof(int)*CONTROL_STORE_BITS);
                NEXT_LATCHES.STATE_NUMBER = nextState;
                break;
            }
                //Addressing Mode
            case(3):
            {
                //calculate next state from J bits and Addressing Mode
                int nextState = GetJ(x) + ((CURRENT_LATCHES.IR >> 11) & 1);
                //get next Microinstruction from Control Store
                //Latch next MicroInstruction
                memcpy(NEXT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[nextState], sizeof(int)*CONTROL_STORE_BITS);
                NEXT_LATCHES.STATE_NUMBER = nextState;
                break;
            }
            default:
                break;

        }
    }

}


void cycle_memory() {

    static int cycles = MEM_CYCLES;

    //fetch current microInstruction
    int *x = CURRENT_LATCHES.MICROINSTRUCTION;
    //check condition bits to make sure that we should be checking memory
    if(GetCOND(x) == 1){    //1 signifies memory ready
        cycles--;
        if(cycles == 1){
            NEXT_LATCHES.READY = 1;
        }
        else if(cycles == 0){
            //two cases: read and write
            if(GetR_W(x) == 1){ //high = Write
                //take value of MDR and put it in M[MAR]
                int MDR_val_write = Low16bits(CURRENT_LATCHES.MDR);
                int MAR_val_write = Low16bits(CURRENT_LATCHES.MAR);

                if(GetDATA_SIZE(x) == 0){   //8 bits
                    //take first 8 bits of MDR
                    int storeVal = (MDR_val_write & 0b0000000011111111);
                    MEMORY[(MAR_val_write >> 1)][(MAR_val_write & 1)] = storeVal;

                }
                else if(GetDATA_SIZE(x) == 1){
                    //store entirity of MDR
                    //split val into two bytes
                    int msb = (MDR_val_write >> 8) & 0b0000000011111111;
                    int lsb = MDR_val_write & 0b0000000011111111;
                    MEMORY[(MAR_val_write >> 1)][0] = lsb;
                    MEMORY[(MAR_val_write >> 1)][1] = msb;
                }

            }
            else{   //low = read
                int MAR_val_read = CURRENT_LATCHES.MAR;

                if(GetDATA_SIZE(x) == 0){   //byte
                    IntermediateMDR = MEMORY[(MAR_val_read >> 1)][(MAR_val_read & 1)];
                }
                else if(GetDATA_SIZE(x) == 1){ //word
                    IntermediateMDR = (MEMORY[(MAR_val_read >> 1)][1] << 8);
                    IntermediateMDR += MEMORY[(MAR_val_read >> 1)][0];
                }
            }
            //we can reset cycles at the end of this if block
            cycles = MEM_CYCLES;        //reset values
            NEXT_LATCHES.READY = 0;     //reset values
//            setbuf(stdout, 0);
//            printf("Memory Access completed\n");
        }

    }



}


/*
 * Datapath routine emulating operations before driving the bus.
 * Evaluate the input of tristate drivers
 *             Gate_MARMUX,
 *		 Gate_PC,
 *		 Gate_ALU,
 *		 Gate_SHF,
 *		 Gate_MDR.
 */

void eval_bus_drivers() {
    int *x = CURRENT_LATCHES.MICROINSTRUCTION;

    //Gate_MARMUX
    int AddrMux2Out;
    int SR1Out;
    int ZextOut;
    int Addr1MuxOut;

    ZextOut = (CURRENT_LATCHES.IR & 0b0000000011111111) << 1;

    if(GetADDR2MUX(x) == 0){
        AddrMux2Out = 0;

    }
    else if(GetADDR2MUX(x) == 1){
        AddrMux2Out = signExt((CURRENT_LATCHES.IR & 0b0000000000111111),6);
    }
    else if(GetADDR2MUX(x) == 2){
        AddrMux2Out = signExt((CURRENT_LATCHES.IR & 0b0000000111111111),9);
    }
    else if(GetADDR2MUX(x) == 3){
        AddrMux2Out = signExt((CURRENT_LATCHES.IR & 0b0000011111111111),11);
    }
    if(GetLSHF1(x)){
        AddrMux2Out = AddrMux2Out << 1;
    }
    int SR1Num;
    if(GetSR1MUX(x) == 0){      //IR[11:9]
        SR1Num = (CURRENT_LATCHES.IR >> 9) & 0b0000000000000111;
        SR1Out = signExt(CURRENT_LATCHES.REGS[SR1Num],16);
    }
    else if(GetSR1MUX(x) == 1){     //IR[8:6]
        SR1Num = (CURRENT_LATCHES.IR >> 6) & 0b0000000000000111;
        SR1Out = signExt(CURRENT_LATCHES.REGS[SR1Num],16);
    }
    //Now Calculate MARMUX Value
    if(GetADDR1MUX(x) == 0){
        Addr1MuxOut = CURRENT_LATCHES.PC;
    }
    else{
        Addr1MuxOut = SR1Out;
    }

    AdderOut = AddrMux2Out + Addr1MuxOut;

    if(GetMARMUX(x) == 0){
        MARMUX_VAL = ZextOut;
    }
    else if(GetMARMUX(x) == 1){
        MARMUX_VAL = AdderOut;
    }

    //Gate_PC
    PC_VAL = CURRENT_LATCHES.PC;

    //Gate_ALU
    //find sr2
    int SR2MuxVal;
    int SR2Num;
    int steerBit = (CURRENT_LATCHES.IR >> 5) & 1;

    if(steerBit == 0){
        SR2Num = (CURRENT_LATCHES.IR & 7);
        SR2MuxVal = signExt(CURRENT_LATCHES.REGS[SR2Num], 16);
    }
    else if(steerBit == 1){
        SR2MuxVal = signExt(CURRENT_LATCHES.IR & 0b0000000000011111, 5);
    }
    //Perform Calculation
    int ALUKbits = GetALUK(x);
    if(ALUKbits == 0){
        ALU_VAL = SR1Out + SR2MuxVal;
    }
    else if(ALUKbits == 1){
        ALU_VAL = SR1Out & SR2MuxVal;
    }
    else if(ALUKbits == 2){
        ALU_VAL = SR1Out ^ SR2MuxVal;
    }
    else if(ALUKbits == 3){
        ALU_VAL = SR1Out;
    }

    //Gate_SHF

    int ShfControl = (CURRENT_LATCHES.IR >> 4) & 0b0000000000000011;
    int ShfAmnt = CURRENT_LATCHES.IR & 0b0000000000001111;
    if(ShfControl == 0){
        SHF_VAL = SR1Out << ShfAmnt;
    }
    else if(ShfControl == 1){
        int intermediate = SR1Out;

        for(int i = 0; i < ShfAmnt; i++){
            intermediate = (intermediate >> 1) & 0b0111111111111111;
        }
        SHF_VAL = Low16bits(intermediate);
    }
    else if(ShfControl == 3){
        SHF_VAL = Low16bits(SR1Out >> ShfAmnt);
    }

    //Gate_MDR **addresses MAR[0], but I dont see where this is useful

    int datasize = GetDATA_SIZE(x);
    if(datasize == 0){
        MDR_VAL = signExt(CURRENT_LATCHES.MDR & 0b0000000011111111, 8);
    }
    else if(datasize == 1){
        MDR_VAL = signExt(CURRENT_LATCHES.MDR, 16);
    }



}


void drive_bus() {

    int *x = CURRENT_LATCHES.MICROINSTRUCTION;

    int g_mdr_bit = GetGATE_MDR(x);
    int g_marmux_bit = GetGATE_MARMUX(x);
    int g_pc_bit = GetGATE_PC(x);
    int g_shf_bit = GetGATE_SHF(x);
    int g_alu_bit = GetGATE_ALU(x);

    if(g_alu_bit + g_marmux_bit + g_mdr_bit + g_pc_bit + g_shf_bit > 1){
//        setbuf(stdout, 0);
//        printf("Too many signals are being gated onto the bus\n");
    }
    else{
        if(g_mdr_bit){
            BUS = MDR_VAL;
        }
        else if(g_shf_bit){
            BUS = SHF_VAL;
        }
        else if(g_pc_bit){
            BUS = PC_VAL;
        }
        else if(g_marmux_bit){
            BUS = MARMUX_VAL;
        }
        else if(g_alu_bit){
            BUS = ALU_VAL;
        }
        else{
            BUS = 0;
        }

    }


}

/*
 * Datapath routine for computing all functions that need to latch
 * values in the data path at the end of this cycle.  Some values
 * require sourcing the bus; therefore, this routine has to come
 * after drive_bus.
 */

void latch_datapath_values() {

    int *x = CURRENT_LATCHES.MICROINSTRUCTION;
    //What are all the values that could possibly be latched
    // PC
    if(GetLD_PC(x) == 1){
        int pcMuxBits = GetPCMUX(x);
        if(pcMuxBits == 0){
            NEXT_LATCHES.PC = CURRENT_LATCHES.PC + 2;
        }
        else if(pcMuxBits == 1){
            NEXT_LATCHES.PC = Low16bits(BUS);
        }
        else if(pcMuxBits == 2){
            NEXT_LATCHES.PC = Low16bits(AdderOut); //global value that is always calculated in previous subroutine
        }
    }
    // REG File
    if(GetLD_REG(x) == 1) {
        if (GetDRMUX(x) == 0) {
            int DrNum = (CURRENT_LATCHES.IR >> 9) & 0b0000000000000111;
            NEXT_LATCHES.REGS[DrNum] = Low16bits(BUS);
        }
        else if(GetDRMUX(x) == 1){
            NEXT_LATCHES.REGS[7] = Low16bits(BUS);
        }
    }
    // IR
    if(GetLD_IR(x) == 1){
        NEXT_LATCHES.IR = Low16bits(BUS);
    }
    // CC
    if(GetLD_CC(x) == 1){
        BUS = signExt(BUS, 16);
        if(BUS < 0){
            NEXT_LATCHES.N = 1;
            NEXT_LATCHES.Z = 0;
            NEXT_LATCHES.P = 0;
        }
        if(BUS == 0){
            NEXT_LATCHES.N = 0;
            NEXT_LATCHES.Z = 1;
            NEXT_LATCHES.P = 0;
        }
        if(BUS > 0){
            NEXT_LATCHES.N = 0;
            NEXT_LATCHES.Z = 0;
            NEXT_LATCHES.P = 1;
        }
    }
    // MAR
    if(GetLD_MAR(x) == 1){
        NEXT_LATCHES.MAR = Low16bits(BUS);
    }
    // MDR
    if(GetLD_MDR(x) == 1){
        if(GetMIO_EN(x) == 0){
            if(GetDATA_SIZE(x) == 0){
                NEXT_LATCHES.MDR = Low16bits(signExt(BUS & 0b0000000011111111, 8));
            }
            else if(GetDATA_SIZE(x) == 1){
                NEXT_LATCHES.MDR = Low16bits(BUS);
            }
        }
        else if(GetMIO_EN(x) == 1){
            NEXT_LATCHES.MDR = Low16bits(IntermediateMDR);
        }
    }
    // BEN
    if(GetLD_BEN(x) == 1){
        int nBit = (CURRENT_LATCHES.IR >> 11) & 1;
        int zBit = (CURRENT_LATCHES.IR >> 10) & 1;
        int pBit = (CURRENT_LATCHES.IR >> 9) & 1;
        NEXT_LATCHES.BEN = (CURRENT_LATCHES.N & nBit) | (CURRENT_LATCHES.Z & zBit) | (CURRENT_LATCHES.P & pBit);
    }

}

int signExt(int input, int numSize){
    int signBit = (input >> (numSize - 1)) & 1;   // minus 1 or else the bit gets shifted off
    int negativeFlag = 0;
    int flipVal = input;
    int mask = 1;
    if(signBit != 0){
        for(int i = 0; i < numSize - 1; i++){
            mask = mask << 1;
            mask++;
        }

        negativeFlag = 1;
        input = input & mask;
        flipVal = input ^ mask; //2's compliment
        flipVal++;
    }
    if(negativeFlag == 1){
        flipVal *= -1;
        return flipVal;
    }
    else return input;

}