#include <iostream>
#include <fstream>
#include "pin.H"


VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)processBbl, addr_t, addr_t, BBL_NumIns(bbl), IARG_END);
    }
}


KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "inscount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	
}



int main(int argc, char * argv[])
{
    
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    
    TRACE_AddInstrumentFunction(Trace, 0);

    
    PIN_AddFiniFunction(Fini, 0);
    
    
    PIN_StartProgram();
    
    return 0;
}
