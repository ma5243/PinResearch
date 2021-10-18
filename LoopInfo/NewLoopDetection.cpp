#include <iostream>
#include <fstream>
#include "pin.H"
#include <unordered_map>

std::ofstream OutFile;

// Map from endAddr to startaddr
std::unordered_map<UINT64, UINT64> bbls;
int arithmetic_instr = 0;
int mem_instr = 0;
int control_flow_instr = 0;

bool isLoop = false;
bool isLoopTail = false;
//This method will keep track of all these instructions throughout and everytime a loop is detected
//it will attach these statistic to the loop, and then reset the values. 
void doInstructionAccouting(INS ins) {
	if(isLoopTail) {
		//attach the arithmetic_instr, mem_instr, control_flow_instr to the loop 
		//depends on how the loop is being "stored" so can't implement rn
		//reset the values to 0 and break 
		arithmetic_instr = 0;
		mem_instr = 0;
		control_flow_instr = 0;
		isLoopTail = false;
		return;
	}
	if(isLoop && !isLoopTail) {
		if(INS_IsMemoryRead(ins) || INS_IsLea(ins)) {
			mem_instr++;
		} else if(INS_IsCall(ins) || INS_IsBranch(ins)) {
			control_flow_instr++;
		} else if(INS_Opcode(ins) == XED_ICLASS_ADD || INS_Opcode(ins) == XED_ICLASS_SUB || INS_Opcode(ins) == XED_ICLASS_AND || INS_Opcode(ins) == XED_ICLASS_IMUL ||
        	          INS_Opcode(ins) == XED_ICLASS_IDIV || INS_Opcode(ins) == XED_ICLASS_OR || INS_Opcode(ins) == XED_ICLASS_XOR || INS_Opcode(ins) == XED_ICLASS_SHL || INS_Opcode(ins) == XED_ICLASS_SHR 
        	          || INS_Opcode(ins) == XED_ICLASS_NOT || INS_Opcode(ins) == XED_ICLASS_NEG || INS_Opcode(ins) == XED_ICLASS_INC || INS_Opcode(ins) == XED_ICLASS_DEC)  {
			arithmetic_instr++;
		}
	}
	 /*else if(INS_Opcode(ins) == XED_ICLASS_SUB) {                                     
               	 	arithmetic_instr++;
        	} else if(INS_Opcode(ins) == XED_ICLASS_AND) {                                     
                	arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_IMUL) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_IDIV) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_OR) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_XOR) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_SHL) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_SHR) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_NOT) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_NEG) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_INC) {                                     
                arithmetic_instr++;
        } else if(INS_Opcode(ins) == XED_ICLASS_DEC) {                                     
                arithmetic_instr++;
        }*/
}

INT32 Usage()
{
    std::cerr << "Usage: Not Implemented" << std::endl;
    return -1;
}

VOID processBbl(UINT64 startAddr, UINT64 endAddr, BBL *bbl)
{
    OutFile << startAddr << " " << endAddr << std::endl;
}

VOID processDbbl(UINT64 startAddr, UINT64 endAddr, BBL *dbbl)
{
    if (bbls.find(endAddr) == bbls.end()) {
        bbls[endAddr] = startAddr;
    } else if (bbls[endAddr] < startAddr) {
        bbls[endAddr] = startAddr;
    } else if (bbls[endAddr] > startAddr) {
        // Split dbbls
        //UINT64 prev; //Maybe the type should be ADDRINT
        /*for(INS ins = */std::cout << INS_Address(BBL_InsHead(*dbbl)) << std::endl;//; ins != BBL_InsTail(*dbbl); ins=INS_Next(ins)) {
		//    if(INS_Address(ins) == bbls[endAddr]) {
		//	    break;
		//    }
        //    std::cout << INS_Address(ins); 
//	    }
	    //bbls[prev] = startAddr;
	    //processBbl(startAddr,prev,dbbl);
        // prev = address of ins just before bbls[endAddr]
        // bbls[prev] = startAddr
        // processBbl(startAdr, prev);
    } else {
        // all good
    }
    UINT64 newStartAddr = bbls[endAddr];
    processBbl(newStartAddr, endAddr, dbbl);
}


VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)processDbbl, IARG_ADDRINT, INS_Address(BBL_InsHead(bbl)), IARG_ADDRINT, INS_Address(BBL_InsTail(bbl)), IARG_PTR, &bbl, IARG_END);
    }
}


KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bblcount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	
}



int main(int argc, char * argv[])
{
    
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    PIN_SetSyntaxIntel();
    
    TRACE_AddInstrumentFunction(Trace, 0);

    
    PIN_AddFiniFunction(Fini, 0);
    
    
    PIN_StartProgram();
    
    return 0;
}
