#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>

#define START 0x400607
#define END 0x40073d

std::ofstream OutFile;
std::ofstream bblTracker;

class LoopStream
{
public:
	UINT64 iter = 0;
	UINT64 tailAddr;
	UINT64 prevAddr = 0;
	UINT64 entries = 0;
	UINT64 numIns = 0;

	std::vector<UINT64> bbls;
};

std::vector<UINT64> stack;

std::vector<UINT64> validBbls;
std::vector<UINT64> validBblsIter;
bool inLoop = false;

// contains all loops that we found vectorizable
std::vector<LoopStream> loopList;

UINT64 curLoopIter = 0;
UINT64 curLoopNumInsts = 0;
UINT64 curLoopNumInstsPrev = curLoopNumInsts;
// std::stack<BBL> basicBlocks;

INT32 Usage()
{
	std::cerr << "Usage: Not Implemented" << std::endl;
	return -1;
}

VOID loopDetection(UINT64 tailAddr, UINT32 numIns)
{
	std::vector<UINT64>::iterator itr = std::find(stack.begin(), stack.end(), tailAddr);
	if (itr != stack.end())
	{ // if already on the stack
		if (inLoop)
		{ // If already in a loop and the bbl is on the stack, update statistics (this is the end of an iteration)
			if (validBblsIter != validBbls)
			{
				LoopStream elem;
				elem.entries++;
				elem.iter = curLoopIter;
				elem.tailAddr = stack.at(0);
				elem.numIns = curLoopNumInsts;
				elem.bbls = validBbls;
				loopList.push_back(elem);
				curLoopNumInsts = 0;
				curLoopIter = 1;
				validBbls = validBblsIter;

				for (UINT32 i = 0; i < validBbls.size(); i++)
				{
					bblTracker << validBbls.at(i) << "\n";
				}
				bblTracker << "\n";
			}
			else
			{
				curLoopIter++;
			}
			curLoopNumInstsPrev = curLoopNumInsts;
			curLoopNumInsts = numIns;
			stack.clear();
			stack.push_back(tailAddr);
			validBblsIter.clear();
			validBblsIter.push_back(tailAddr);
		}
		else
		{ // If not already in loop, that means we found a loop
			curLoopIter = 1;
			inLoop = true;
			UINT32 ind = std::distance(stack.begin(), itr);
			while (ind < stack.size())
			{
				validBbls.push_back(stack.at(ind));
				ind++;
			}
			validBblsIter.clear();
			validBblsIter.push_back(tailAddr);
			stack.clear();
			stack.push_back(tailAddr);
		}
	}
	else
	{ // if not already on the stack
		if (inLoop)
		{ // if in a loop and current bbl not on the stack (either we're in the middle of the loop or the end)
			validBblsIter.push_back(tailAddr);
			std::vector<UINT64>::iterator itrTwo = std::find(validBbls.begin(), validBbls.end(), tailAddr);
			if (itrTwo != validBbls.end())
			{ // if the current bbl is on the valid bbl list (we're in the middle of the loop)
				stack.push_back(tailAddr);
				curLoopNumInsts += numIns;
			}
			else
			{ // if its not on valid bbl list, end of loop object
				bool contains = false;
				for (UINT64 j = 0; j < loopList.size(); j++)
				{
					if (loopList.at(j).tailAddr == stack.at(stack.size() - 1) && loopList.at(j).iter == curLoopIter && loopList.at(j).bbls == validBbls)
					{
						loopList.at(j).entries++;
						contains = true;
						break;
					}
				}
				if (!contains)
				{ // create loop object on the end if doesn't already exist and set statistics
					LoopStream elem;
					elem.entries++;
					elem.iter = curLoopIter;
					elem.tailAddr = stack.at(0);
					elem.numIns = curLoopNumInstsPrev;
					elem.bbls = validBbls;
					loopList.push_back(elem);

					for (UINT32 i = 0; i < validBbls.size(); i++)
					{
						bblTracker << validBbls.at(i) << std::endl;
					}
					bblTracker << "end" << std::endl;
				}

				curLoopNumInsts = 0;
				inLoop = false;
				validBbls.clear();
				stack.clear();
				stack.push_back(tailAddr);
			}
		}
		else
		{ // if not in a loop currently, this is just another bbl so push to stack
			stack.push_back(tailAddr);
		}
	}
}

VOID Trace(TRACE trace, VOID *v)
{
	// Visit every basic block  in the trace
	for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
	{
		if (INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END)
		{
			// Insert a call to docount before every bbl, passing the tail address
			BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)loopDetection, IARG_ADDRINT, INS_Address(BBL_InsTail(bbl)), IARG_UINT32, BBL_NumIns(bbl), IARG_END);
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
	for (UINT32 i = 0; i < loopList.size(); i++)
	{
		OutFile << "Number of iterations for loop " << i << ": " << loopList.at(i).iter << std::endl;
		OutFile << "Total number of entries for loop " << i << ": " << loopList.at(i).entries << std::endl;
		OutFile << "Instructions/iteration for loop " << i << ": " << loopList.at(i).numIns << std::endl;
		OutFile << "" << std::endl;
	}
}

int main(int argc, char *argv[])
{

	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
		return Usage();

	OutFile.open(KnobOutputFile.Value().c_str());
	bblTracker.open("Instructions.txt");

	PIN_SetSyntaxIntel();

	TRACE_AddInstrumentFunction(Trace, 0);

	PIN_AddFiniFunction(Fini, 0);

	PIN_StartProgram();

	bblTracker.close();

	return 0;
}
