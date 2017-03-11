/* src/types.h
 * Internal types for the simulator
 */

#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Represents a 32-bit instruction
typedef uint32_t inst_t;
// Represents a 32-bit program counter
typedef uint32_t pc_t;
// Represents a single word (32b) of memory, with ambiguous signedness
typedef uint32_t word_t;

// Ignored MIPS I instructions
// BGEZ: Branch on Greater Than or Equal to Zero
// BGEZAL: Branch on Greater Than or Equal to Zero and Link
// BGTZ: Branch on Greater Than Zero
// BLEZ: Branch on Less Than or Equal to Zero
// BLTZ: Branch on Less Than Zero
// BLTZAL: Branch on Less Than Zero And Link
// BREAK: Breakpoint
// COPz: Coprocessor Operation
// DIV: Divide Word
// DIVU: Divide Unsigned Word
// JALR: Jump And Link Register
// LWCz: Load Word To Coprocessor
// LWL: Load Word Left
// LWR: Load Word Right
// MFHI: Move From HI Register
// MFLO: Move from LO Register
// MTHI: Move To HI Register
// MTLO: Move To LO Register
// MULT: Multiply Word
// MULTU: Multiply Unsigned Word
// SLLV: Shift Word Left Logical Variable
// SRA: Shift Word Right Arithmetic
// SRAV: Shift Word Right Arithmetic Variable
// SRLV: Shift Word Right Logical Variable
// SUBU: Subtract Unsigned Word
// SWCZ: Store Word From Coprocessor
// SWL: Store Word Left
// SWR: Store Word Right
// SYSCALL: System Call

// Mapping opcode values to mnemonic
typedef enum OpCodes {
    OPC_RTYPE   = 0x00,
    OPC_ADDI    = 0x08, // 0b001000, Add Immediate Word
    OPC_ADDIU   = 0x09, // 0b001001, Add Immediate Unsigned Word
    OPC_ANDI    = 0x0c, // 0b001100, And Immediate
    OPC_BEQ     = 0x04, // 0b000100, Branch on Equal
    OPC_BNE     = 0x05, // 0b000101, Branch on Not Equal
    OPC_J       = 0x02, // 0b000010, Jump
    OPC_JAL     = 0x03, // 0b000011, Jump And Link
    OPC_LB      = 0x20, // 0b100000, Load Byte
    OPC_LBU     = 0x24, // 0b100100, Load Byte Unsigned
    OPC_LH      = 0x21, // 0b100001, Load Halfword
    OPC_LHU     = 0x25, // 0b100101, Load Halfword Unsigned
    OPC_LUI     = 0x0f, // 0b001111, Load Upper Immediate
    OPC_LW      = 0x23, // 0b100011, Load Word
    OPC_ORI     = 0x0d, // 0b001101, Or Immediate
    OPC_SB      = 0x28, // 0b101000, Store Byte
    OPC_SH      = 0x29, // 0b101001, Store Halfword
    OPC_SLTI    = 0x0a, // 0b001010, Set on Less Than Immediate
    OPC_SLTIU   = 0x0b, // 0b001011, Set on Less Than Immediate Unsigned
    OPC_SW      = 0x2b, // 0b101011, Store Word
    OPC_XORI    = 0x0e  // 0b001110, Exclusive OR Immediate
} opcode_t;

// Mapping funct values to mnemonic (R-type, opcode=0x0)
typedef enum FunctCodes {
    FNC_ADD     = 0x20, // 0b100000, Add Word
    FNC_ADDU    = 0x21, // 0b100001, Add Unsigned Word
    FNC_AND     = 0x24, // 0b100100, And
    FNC_JR      = 0x08, // 0b001000, Jump Register
    FNC_NOR     = 0x27, // 0b100111, Not Or
    FNC_OR      = 0x25, // 0b100101, Or
    FNC_SLL     = 0x00, // 0b000000, Shift Word Left Logical
    FNC_SLT     = 0x2a, // 0b101010, Set On Less Than
    FNC_SLTU    = 0x2b, // 0b101011, Set on Less Than Unsigned
    FNC_SRL     = 0x02, // 0b000010, Shift Word Right Logical
    FNC_SUB     = 0x22, // 0b100010, Subtract Word
    FNC_XOR     = 0x26  // 0b100110, Exclusive OR
} funct_t;

// Enumerate all "operations" (R/J/I type instruction action)
// See http://alumni.cs.ucr.edu/~vladimir/cs161/mips.html
// These are used internally to represent operations between stages
typedef enum Operations {
    // Arithmetic and logical
    OPR_ADD,
    OPR_ADDU,
    OPR_ADDI,
    OPR_ADDIU,
    OPR_AND,
    OPR_ANDI,
    OPR_DIV,    // Probably not supported
    OPR_DIVU,   // Probably not supported
    OPR_MULT,
    OPR_MULTU,
    OPR_NOR,
    OPR_OR,
    OPR_ORI,
    OPR_SLL,
    OPR_SLLV,
    OPR_SRA,
    OPR_SRAV,
    OPR_SRL,
    OPR_SRLV,
    OPR_SUB,
    OPR_SUBU,
    OPR_XOR,
    OPR_XORI,
    // Constant-manipulating
    OPR_LHI,
    OPR_LLO,
    // Comparison
    OPR_SLT,
    OPR_SLTU,
    OPR_SLTI,
    OPR_SLTIU,
    // Branch
    OPR_BEQ,
    OPR_BTGZ,
    OPR_BLEZ,
    OPR_BNE,
    // Jump
    OPR_J,
    OPR_JAL,
    OPR_JALR,
    OPR_JR,
    // Load
    OPR_LB,
    OPR_LBU,
    OPR_LH,
    OPR_LHU,
    OPR_LW,
    // Store
    OPR_SB,
    OPR_SH,
    OPR_SW,
    // Data movement
    OPR_MFHI,
    OPR_MFLO,
    OPR_MTHI,
    OPR_MTLO,
    // Exception/interrupt
    OPR_TRAP
} operation_t;

typedef struct CONTROL_REGISTER {
    char * regName;

    // These are control register definitions that come from Figure 4.16 on page 264 of the Hennessy textbook
    bool regDst;        // regDst ? destination register is Rd : destination register is Rt
    bool regWrite;      // regWrite ? Register on the write register input is written with the value of the Write data input : nothing
    bool ALUSrc;        // ALUSrc ? The second ALU operand comes from Immediate 16 : The second ALU operand comes from Rt
    bool PCSrc;         // This has been implemented in the ID stage, so PCSrc true means branch taken
    bool memRead;       // memRead ? Data memory contents given by address input are put on Read data output : Nothing
    bool memWrite;      // memWrite ? Data memory contents designated by the address input replace by data on Write data input : Nothing
    bool memToReg;      // memToReg ? Value from Write data input comes from the data memory : value fed to register Write data input comes from ALU
    operation_t ALUop;  // ALU operation
    bool jump;          // Override PC with shifted and concatenated address

    inst_t instr;       // Raw instruction

    opcode_t opCode;
    uint32_t regRs;
    uint32_t regRt;
    uint32_t regRd;
    uint32_t immed;
    uint32_t address;
    funct_t funct;
    uint32_t shamt;

    uint32_t regRsValue;
    uint32_t regRtValue;

    uint32_t ALUresult;
    uint32_t pcNext;
    uint32_t memData;

} control_t;

#endif /* _TYPES_H */
