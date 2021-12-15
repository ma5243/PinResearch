#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>


//#define START 0x4005a6
//#define END 0x40062f

#define START 0x4005a6
#define END 0x40064c

std::ofstream OutFile;

class LoopStream {
    public:
      //std::vector<UINT32> entryIterList;
      //std::vector<std::vector<UINT64>> iterBblList;
      //entryIterList.assign(10,0);
      UINT64 iter = 0;      
      UINT64 tailAddr;
      UINT64 prevAddr=0;
      UINT64 entries=0;
      UINT64 numIns=0;
      std::vector<UINT64> bbls;          
};

// Map from endAddr to startaddr
//std::unordered_map<UINT64, UINT64> bbls;

std::vector<UINT64> stack;
//std::unordered_map<UINT64, UINT64> loops;

std::vector<UINT64> validBbls;
std::vector<UINT64> validBblsIter;
bool inLoop = false;

std::vector<LoopStream> loopList;

UINT64 curLoopIter = 0;
UINT64 curLoopNumInsts = 0;
//std::stack<BBL> basicBlocks;


int arithmetic_instr = 0;
int mem_instr = 0;
int control_flow_instr = 0;



//This method will keep track of all these instructions throughout and everytime a loop is detected
//it will attach these statistic to the loop, and then reset the values. 
void doInstructionAccouting(INS ins, bool isLoopTail) {
	if(INS_IsMemoryRead(ins) || INS_IsLea(ins)) {
			mem_instr++;
	} else if(INS_IsCall(ins) || INS_IsBranch(ins)) {
			control_flow_instr++;
	} else if(INS_Opcode(ins) == XED_ICLASS_ADD || INS_Opcode(ins) == XED_ICLASS_SUB || INS_Opcode(ins) == XED_ICLASS_AND || INS_Opcode(ins) == XED_ICLASS_IMUL || INS_Opcode(ins) == XED_ICLASS_IDIV || INS_Opcode(ins) == XED_ICLASS_OR || INS_Opcode(ins) == XED_ICLASS_XOR || INS_Opcode(ins) == XED_ICLASS_SHL || INS_Opcode(ins) == XED_ICLASS_SHR || INS_Opcode(ins) == XED_ICLASS_NOT || INS_Opcode(ins) == XED_ICLASS_NEG || INS_Opcode(ins) == XED_ICLASS_INC || INS_Opcode(ins) == XED_ICLASS_DEC)  {
			arithmetic_instr++;
	}	
	if(isLoopTail) {
		//attach the arithmetic_instr, mem_instr, control_flow_instr to the loop 
		//depends on how the loop is being "stored" so can't implement rn
		//reset the values to 0 
		arithmetic_instr = 0;
		mem_instr = 0;
		control_flow_instr = 0;
	}
}

INT32 Usage()
{
    std::cerr << "Usage: Not Implemented" << std::endl;
    return -1;
}
//Want a similar structure to validBBL that gets created at the beginning of an iteration and gets destroyed at the end of an 
//iteration
//At the end of an iteration, if the current list is different than the validBBL list, then we create a new LoopStream object
//and set the validBBL to the new iteration bbl list. Reset the iteration/instruction counters
VOID loopDetection(UINT64 tailAddr,UINT32 numIns) {
	std::vector<UINT64>::iterator itr = std::find(stack.begin(), stack.end(), tailAddr);
	//std::cout << tailAddr << std::endl;
	if(itr != stack.end()) { //if already on the stack 
		if(inLoop) { //If already in a loop and the bbl is on the stack, update statistics
			if(validBblsIter != validBbls) {
				//need to store the iteration that ran in each iteration and store those
				//need a way to set instructions later 			

				LoopStream elem;
				elem.entries++;
				elem.iter = curLoopIter;
				elem.tailAddr = stack.at(0);
				//std::cout << tailAddr << std::endl;
				elem.numIns = curLoopNumInsts;
				elem.bbls = validBbls;
				loopList.push_back(elem);

				curLoopIter = 1;
				//Need a way to keep track of instructions in each iteration
				validBbls = validBblsIter;
				
			} else {
				curLoopIter++;
				curLoopNumInsts += numIns;
			}
				stack.clear();
				stack.push_back(tailAddr);
				validBblsIter.clear();
				validBblsIter.push_back(tailAddr);
		} else { //If not already in loop, that means we found a loop
			curLoopIter=1;
			curLoopNumInsts += numIns;
			inLoop = true;
			UINT32 ind = std::distance(stack.begin(),itr);
			while(ind < stack.size()) {
				validBbls.push_back(stack.at(ind));
				ind++;
			}
			validBblsIter.push_back(tailAddr);
			stack.clear();
			stack.push_back(tailAddr);
		}
	} else { //if not already on the stack
		if(inLoop) { //if in a loop and current bbl not on the stack
			validBblsIter.push_back(tailAddr);
			std::vector<UINT64>::iterator itrTwo = std::find(validBbls.begin(), validBbls.end(), tailAddr);
			if(itrTwo != validBbls.end()) { //if the current bbl is on the valid bbl list
				stack.push_back(tailAddr);
				curLoopNumInsts += numIns;
			} else { //if its not on valid bbl list, end of loop object
				bool contains = false;
				curLoopIter++;
				for(UINT64 j=0;j<loopList.size();j++) {
					//std::cout << curLoopIter << std::endl;
					//std::cout << tailAddr << std::endl;
					if(loopList.at(j).tailAddr == stack.at(stack.size()-1) && loopList.at(j).iter == curLoopIter && loopList.at(j).bbls == validBbls) {
					loopList.at(j).entries++;
					contains = true;
					break;
					}
				}
				if(!contains) {//create loop object on the end if doesn't already exist and set statistics
					LoopStream elem;
					elem.entries++;
					elem.iter = curLoopIter;
					elem.tailAddr = stack.at(0);
					//std::cout << tailAddr << std::endl;
					elem.numIns = curLoopNumInsts;
					elem.bbls = validBbls;
					loopList.push_back(elem);
				}
				
				curLoopNumInsts = 0;
				inLoop = false;
				validBbls.clear();
				stack.clear();
				stack.push_back(tailAddr);	
		       	}
		} else { //if not in a loop currently, this is just another bbl so push to stack	
			stack.push_back(tailAddr);
		}
	}
}



VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        
	if(INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END) {
		// Insert a call to docount before every bbl, passing the tail address
        	BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)loopDetection, IARG_ADDRINT, INS_Address(BBL_InsTail(bbl)),IARG_UINT32, BBL_NumIns(bbl),IARG_END);
	/*std::stringstream ss;
	ss<< std::hex << INS_Address(BBL_InsHead(bbl));
	std::string res ( ss.str() );
	std::cout << res << std::endl;	
	
	std::stringstream sss;
	sss<< std::hex << INS_Address(BBL_InsTail(bbl));
	std::string resTwo ( sss.str() );
	std::cout << resTwo << std::endl; */
	

	//std::cout << INS_Address(BBL_InsHead(bbl)) << std::endl;
	//std::cout << INS_Address(BBL_InsTail(bbl)) << std::endl;
	}
    }
}


KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bblcount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
   
	OutFile << "Total number of Vectorizable Loops: " << loopList.size() << std::endl;
    for(UINT32 i=0;i<loopList.size();i++) {
 	//UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries; 
	OutFile << "Number of iterations for loop " << i << ": "<< loopList.at(i).iter << std::endl;
	OutFile << "Total number of entries for loop " << i << ": "<<  loopList.at(i).entries << std::endl;
	//OutFile << "Instructions/iteration for loop " << i << ": " << loopList.at(i).numIns / (loopList.at(i).iter - 1) << std::endl;
	OutFile << "" << std::endl;
    }
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
