#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>


//#define START 0x4005a6
//#define END 0x40062f

#define START 0x4005a6
#define END 0x40069f

std::ofstream OutFile;

class LoopStream {
    public:
      std::vector<UINT32> entryIterList;
      //std::vector<std::vector<UINT64>> iterBblList;
      //entryIterList.assign(10,0);
      UINT64 iter = 0;      
      UINT64 tailAddr;
      UINT64 prevAddr=0;
      UINT64 entries=0;
      
      
    
      //UINT64 totalIns = 0;
      //UINT64 totalLoads = 0;
      //UINT64 totalCalls = 0;
      //UINT64 totalArithmeticIns = 0;
};

// Map from endAddr to startaddr
std::unordered_map<UINT64, UINT64> bbls;

std::vector<UINT64> stack;
std::unordered_map<UINT64, UINT64> loops;

std::vector<UINT64> validBbls;
bool inLoop = false;

std::vector<LoopStream> loopList;

UINT64 curLoopIter = 0;

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

VOID loopDetection(UINT64 tailAddr) {
	//std::cout << tailAddr << std::endl;
	std::vector<UINT64>::iterator itr = std::find(stack.begin(), stack.end(), tailAddr);
	if(itr != stack.end()) { //if already on the stack 
		if(inLoop) {
			curLoopIter++;
			/*for(UINT32 i=0;i<loopList.size();i++) {
				if(loopList.at(i).tailAddr == tailAddr) {
					loopList.at(i).iter++;
					//std::cout << tailAddr << std::endl;
				}
			}*/
			stack.clear();
			stack.push_back(tailAddr);
		} else {
			/*bool contains = false;
			for(UINT64 j=0;j<loopList.size();j++) {
				if(loopList.at(j).tailAddr == tailAddr) {
					loopList.at(j).iter++;
					loopList.at(j).entries++;
					contains = true;
					break;
				}
			}*/
			curLoopIter=1;
			inLoop = true;
			UINT32 ind = std::distance(stack.begin(),itr);
			/*if(!contains) {
				LoopStream elem;
				elem.entries++;
				elem.iter = 1;
				elem.tailAddr = tailAddr;
				loopList.push_back(elem);
			}*/
			while(ind < stack.size()) {
				validBbls.push_back(stack.at(ind));
				ind++;
			}
			stack.clear();
			stack.push_back(tailAddr);
		}
		/*int ind = std::distance(stack.begin(),itr);
		bool foundLoopAlready = false;
		//std::cout << tailAddr << std::endl;
		for(UINT32 i=0;i<loopList.size();i++) {
			//std::cout << "test1" << std::endl;
			if(loopList.at(i).tailAddr == tailAddr) {
				foundLoopAlready = true;
				if(loopList.at(i).prevAddr == stack.at(stack.size() - 1)) {
					loopList.at(i).entryIterList[loopList.at(i).entryIterList.size()-1]++;//last entry should have an iter increase
                                       	//std::cout << loopList.at(i).entryIterList.at(loopList.at(i).entryIterList.size()-1) << std::endl;
				} else {
					if(loopList.at(i).entryIterList.size() == loopList.at(i).entryIterList.capacity()) {
						loopList.at(i).entryIterList.reserve(loopList.at(i).entryIterList.capacity() * 2); 	
					}
					loopList.at(i).entryIterList[loopList.at(i).entryIterList.size()]++;
				}
			}		
		}
		if(!foundLoopAlready) {
			inLoop = true;
			LoopStream elem;
			elem.iterBblList[0].push_back(NULL);
			while(ind < stack.size()) {
				elem.iterBblList.reserve(10);
				elem.iterBBlList[1].push_back(stack.at(ind));
				validBbl.push_back(stack.at(ind));
				ind++;
			}
			stack.clear();
			stack.push_back(tailAddr);
					
			elem.tailAddr = tailAddr;
			elem.entryIterList.reserve(10);
			elem.entryIterList.push_back(0);
			elem.entryIterList.push_back(1);
                        //std::cout << elem.entryIterList.at(1) << std::endl;
			elem.prevAddr = stack.at(stack.size()-1);
			loopList.push_back(elem);
		}*/
	} else {
		if(inLoop) {
			std::vector<UINT64>::iterator itrTwo = std::find(validBbls.begin(), validBbls.end(), tailAddr);
			if(itrTwo != validBbls.end()) {
				stack.push_back(tailAddr);
			} else {
				bool contains = false;
				for(UINT64 j=0;j<loopList.size();j++) {
					std::cout << curLoopIter << std::endl;
					if(loopList.at(j).tailAddr == tailAddr && loopList.at(j).iter == curLoopIter) {
					loopList.at(j).entries++;
					contains = true;
					break;
					}
				}
				if(!contains) {
					LoopStream elem;
					elem.entries++;
					elem.iter = curLoopIter;
					elem.tailAddr = stack.at(0);
					loopList.push_back(elem);
				}
				
				inLoop = false;
				validBbls.clear();
				stack.clear();
				stack.push_back(tailAddr);	
		       	}
		} else {	
			stack.push_back(tailAddr);
		}
	}
}

VOID processBbl(UINT64 startAddr, UINT64 endAddr, BBL *bbl)
{
    if (std::find(stack.begin(), stack.end(), endAddr) == stack.end()) {
        stack.push_back(endAddr);
    } else {
        while(stack.back() != endAddr){
            stack.pop_back();
        }
        if (loops.find(endAddr) == loops.end()){
            loops[endAddr] = 1;
        } else { 
            loops[endAddr] ++;
        }
    }
        
//    OutFile << startAddr << " " << endAddr << std::endl;
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
        //for(INS ins = std::cout << INS_Address(BBL_InsHead(*dbbl)) << std::endl;//; ins != BBL_InsTail(*dbbl); ins=INS_Next(ins)) {
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
        
	if(INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END) {
		// Insert a call to docount before every bbl, passing the tail address
        	BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)loopDetection, IARG_ADDRINT, INS_Address(BBL_InsTail(bbl)),IARG_END);
	}
	
	

	/*if(basicBlocks.top() == bbl) {
		//add to the loops map
	
		
		//do instruction accounting 
		for(INS ins = BBL_InsHead(bbl); true; ins=INS_Next(ins)) {
			if(ins == BBL_InsTail(bbl)) {
				doInstructionAccounting(ins, true);
				break;
			}
			doInstructionAccounting(ins,false);
		}
		//remove the basic block
		basicBlocks.pop();		
	} else {
		//if basic block not on top, just add it to the stack
		basicBlocks.push(bbl);
	}*/
    }
}


KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bblcount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
   
	OutFile << "Total number of Vectorizable Loops:" << loopList.size() << std::endl;
    for(UINT32 i=0;i<loopList.size();i++) {
 	UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries; 
	OutFile << "Total number of iteration for loop " << i << ": "<< totalIters << std::endl;
	OutFile << "Total number of entries for loop " << i << ": "<<  loopList.at(i).entries << std::endl;
	OutFile << "Iterations/entry for loop " << i << ": "<<  totalIters/loopList.at(i).entries << std::endl;
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
