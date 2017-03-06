/* src/write.c
* Write back stage of the pipeline
*/


#include "write.h"

extern int flags;


void writeback(control_t * memwb){
    //Determine the WB register based on regDst
    word_t writeRegister = 0;
    word_t writeRegisterValue = 0;
    if(memwb->regDst){
        writeRegister = memwb->regRd;
    }
    else{
        writeRegister = memwb->regRt;
    }


    //Get correct writeback register value
    if(memwb->memToReg){
        writeRegisterValue = memwb->memData;
    }
    else{
        writeRegisterValue = memwb->ALUresult;
    }

    //write back to the register file
    if(memwb->regWrite){

        if(flags & MASK_VERBOSE){
            printf("WB: writing 0x%08x to register %d(10)", writeRegisterValue, writeRegister);
        }

        reg_write(writeRegister, &writeRegisterValue);
    }

}