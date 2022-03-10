#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>
#include <sstream>

#define START 0x400607
#define END 0x400750

std::ifstream instrFile;
std::ofstream OutFile;

class LoopStream {
    public:      
      UINT64 arithmeticIns;
      UINT64 memoryReadIns;
      UINT64 controlFlowIns; 
      UINT64 memoryWriteIns;       
};

std::vector<LoopStream> loopList;
std::vector<std::string> addr;

int arithmetic_instr = 0;
int mem_read_instr = 0;
int mem_write_instr = 0;
int control_flow_instr = 0;

uint i=0;
uint addrr;

VOID Application_Start(VOID *v) {
    instrFile.open("Instructions.txt");
    std::string myline;
    while(instrFile) {
        std::getline (instrFile,myline);
        addr.push_back(myline);
    }
    addr.pop_back();
}

//This method will keep track of all these instructions throughout and everytime a loop is detected
//it will attach these statistic to the loop, and then reset the values. 
/*void doInstructionAccountingBBL(BBL bbl,vector<vector<int>>& vec) {
	for( INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) ) {
		if(INS_IsMemoryRead(ins)) {
			mem_instr++;
		} else if(INS_IsCall(ins) || INS_IsBranch(ins)) {
			control_flow_instr++;
		} else if(INS_Opcode(ins) == XED_ICLASS_ADD || INS_Opcode(ins) == XED_ICLASS_SUB || 
                    INS_Opcode(ins) == XED_ICLASS_AND || INS_Opcode(ins) == XED_ICLASS_IMUL || 
                    INS_Opcode(ins) == XED_ICLASS_IDIV || INS_Opcode(ins) == XED_ICLASS_OR || 
                    INS_Opcode(ins) == XED_ICLASS_XOR || INS_Opcode(ins) == XED_ICLASS_SHL || 
                    INS_Opcode(ins) == XED_ICLASS_SHR || INS_Opcode(ins) == XED_ICLASS_NOT || 
                    INS_Opcode(ins) == XED_ICLASS_NEG || INS_Opcode(ins) == XED_ICLASS_INC || 
                    INS_Opcode(ins) == XED_ICLASS_DEC)  {
			arithmetic_instr++;
		}
	}
}*/


INT32 Usage()
{
    std::cerr << "Usage: Not Implemented" << std::endl;
    return -1;
}

VOID Trace(TRACE trace, VOID *v)
{
    //std::cout << myline << std::endl;
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {   
	    if(INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END) {
            if(i==addr.size()) {
                break;
            }
            //std::cout << addr.at(i) << std::endl;
            if(addr.at(i) != "end") {
                std::stringstream temp(addr.at(i));
                temp >> addrr;
            } else {
                LoopStream elem;
                elem.arithmeticIns = arithmetic_instr;
                elem.controlFlowIns = control_flow_instr;
                elem.memoryReadIns = mem_read_instr;
                elem.memoryWriteIns = mem_write_instr;

                loopList.push_back(elem);
                i++;

                arithmetic_instr = 0;
                control_flow_instr = 0;
                mem_read_instr = 0;
                mem_write_instr = 0;

                std::cout << "end" << std::endl;
            }
            //std::cout << temp << std::endl;
            if(addrr == INS_Address(BBL_InsTail(bbl))) {

                //std::cout << INS_Address(BBL_InsHead(bbl)) << std::endl;

                for( INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins) ) {
		            if(INS_IsMemoryRead(ins) || INS_Opcode(ins) == XED_ICLASS_POP) {
                        std::cout << "Memory Read Instruction: " << std::hex << INS_Address(ins) << std::endl;
			            mem_read_instr++;
		            } else if(INS_IsMemoryWrite(ins) || INS_Opcode(ins) == XED_ICLASS_PUSH) {
                        std::cout << "Memory Write Instruction: " << std::hex << INS_Address(ins) << std::endl;
			            mem_write_instr++;
                    } else if(INS_IsControlFlow(ins)) {
                        std::cout << "Control Flow Instruction: " << std::hex << INS_Address(ins) << std::endl;
			            control_flow_instr++;
		            } else if(INS_Opcode(ins) == XED_ICLASS_ADD || INS_Opcode(ins) == XED_ICLASS_SUB || 
                        INS_Opcode(ins) == XED_ICLASS_AND || INS_Opcode(ins) == XED_ICLASS_IMUL || 
                        INS_Opcode(ins) == XED_ICLASS_IDIV || INS_Opcode(ins) == XED_ICLASS_OR || 
                        INS_Opcode(ins) == XED_ICLASS_XOR || INS_Opcode(ins) == XED_ICLASS_SHL || 
                        INS_Opcode(ins) == XED_ICLASS_SHR || INS_Opcode(ins) == XED_ICLASS_NOT || 
                        INS_Opcode(ins) == XED_ICLASS_NEG || INS_Opcode(ins) == XED_ICLASS_INC || 
                        INS_Opcode(ins) == XED_ICLASS_DEC || INS_Opcode(ins) == XED_ICLASS_MOV)  {
                            std::cout << "Arithmetic Instruction: " << std::hex << INS_Address(ins) << std::endl;
			                arithmetic_instr++;
		            }
	            }
                i++;
            }     
        }
    }
}


KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bblcount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
   
	OutFile << "Total number of Vectorizable Loops: " << loopList.size() << std::endl;
	OutFile << "" << std::endl;
    for(UINT32 i=0;i<loopList.size();i++) {
 	    //UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries; 
	    OutFile << "Total number of arithmetic instructions for loop " << i << ": "<< loopList.at(i).arithmeticIns << std::endl;
	    OutFile << "Total number of read memory instructions for loop " << i << ": "<<  loopList.at(i).memoryReadIns << std::endl;
	    OutFile << "Total number of write memory instructions for loop " << i << ": "<<  loopList.at(i).memoryWriteIns << std::endl;
        OutFile << "Total number of control flow instructions for loop " << i << ": " << loopList.at(i).controlFlowIns  << std::endl;
	    OutFile << "" << std::endl;
    }
}



int main(int argc, char * argv[])
{
    
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) return Usage();

    PIN_AddApplicationStartFunction(Application_Start,0);

    OutFile.open(KnobOutputFile.Value().c_str());

    PIN_SetSyntaxIntel();
    
    TRACE_AddInstrumentFunction(Trace, 0);

    
    PIN_AddFiniFunction(Fini, 0);
    
    
    PIN_StartProgram();
    
    return 0;
}
