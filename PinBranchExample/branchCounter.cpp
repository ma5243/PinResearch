#include <iostream>
#include <fstream>
#include "pin.H"

using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;


ofstream OutFile;

static UINT64 icount = 0;
static UINT64 takenCount = 0;

VOID docount() { icount++; }
VOID countTaken() {takenCount++;}

VOID Instruction(INS ins, VOID *v) {
	if(INS_IsBranch(ins) && INS_HasFallThrough(ins)) { //HasFallThrough checks if it is a conditional branch
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount,IARG_BRANCH_TAKEN,IARG_END);//IARG_BRANCH_TAKEN sets a flag of whether the branch was taken or not
	}
	if(IARG_BRANCH_TAKEN){
		countTaken();
	}
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "brcount.out", "specify output file name");

VOID Fini(INT32 code, VOID *v) {
    	OutFile.setf(ios::showbase);
    	OutFile << "Total Branch Count " << icount << endl;
	OutFile << "Branches Taken " << takenCount << endl;
	OutFile << "Branches Not Taken " << icount - takenCount << endl;
    	OutFile.close();
}

INT32 Usage() {
    	cerr << "This tool counts the number of dynamic instructions executed" << endl;
    	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    	return -1;
}

int main(int argc, char * argv[]) {
    	if (PIN_Init(argc, argv)) return Usage();

    	OutFile.open(KnobOutputFile.Value().c_str());

    	INS_AddInstrumentFunction(Instruction, 0);

    	PIN_AddFiniFunction(Fini, 0);

    	PIN_StartProgram();
    
    	return 0;
}
