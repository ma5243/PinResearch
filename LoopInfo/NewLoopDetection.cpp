#include <iostream>
#include <fstream>
#include "pin.H"
#include <unordered_map>

std::ofstream OutFile;

// Map from endAddr to startaddr
std::unordered_map<UINT64, UINT64> bbls;

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