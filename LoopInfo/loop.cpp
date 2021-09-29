//
//  Jonathan Salwan - 2013-08-13
// 
//  http://shell-storm.org
//  http://twitter.com/JonathanSalwan
//
//  Note: Pin tool - Simple loop detection via the instruction counter.
//

#include "pin.H"
#include <asm/unistd.h>
#include <fstream>
#include <iostream>
#include <list>
#include <stack> 
#include <vector>
#include <algorithm>

#define LOCKED    1
#define UNLOCKED  !LOCKED

#define MAX_ADDR 0x900000 // + 0x400000  (we ignore 0x7... anyway)
#define MAX_ADDR_TYPE UINT32 // smallest type needed to hold (MAX_ADDR + 0x400000)

#define MAX_COUNT 0xffffffff // max value in _tabAddr
#define MAX_COUNT_TYPE UINT32 // smallest type needed to hold MAX_COUNT

#define MAX_INSTS 9000000000 // 5 billion (max number of insts run)

#define START 0x4004b6
#define END 0x4004d3

std::ofstream OutFile;

static UINT32         _lockAnalysis = !LOCKED; /* unlock -> without sym */
static MAX_COUNT_TYPE _tabAddr[MAX_ADDR];
static std::string    _tabStr[MAX_ADDR];
UINT64 min = 0x700000000;
UINT64 max = 0;
UINT64 num_inst = 0;
bool overflow = false;

static INS insArr [MAX_ADDR];


INT32 Usage()
{
    std::cerr << "Foo test" << std::endl;
    return -1;
}

VOID insCallBack(UINT64 insAddr, std::string insDis)
{
  if (_lockAnalysis)
    return ;

  num_inst ++;

  if (insAddr > 0x700000000000 || num_inst > MAX_INSTS)
    return;
  
  min = std::min(min, insAddr);
  max = std::max(max, insAddr);

  if ((insAddr ^ 0x400000) >= MAX_ADDR) {
    //std::cerr << "Instruction run past MAX_ADDR" << std::endl;
    return;
  }

  if (_tabAddr[insAddr ^ 0x400000] == MAX_COUNT) {
    //std::cerr << "Overflow of MAX_COUNT" << std::endl;
    overflow = true;
    return;
  }
 
  _tabAddr[insAddr ^ 0x400000] += 1;
  _tabStr[insAddr ^ 0x400000] = insDis;

}

VOID Instruction(INS ins, VOID *v)
{
  UINT64 insAddr = INS_Address(ins);
  if(insAddr >= START && insAddr <= END) {
      INS_InsertCall(
      ins, IPOINT_BEFORE, (AFUNPTR)insCallBack,
      IARG_ADDRINT, insAddr,
      IARG_PTR, new std::string(INS_Disassemble(ins)),
      IARG_END);
     
      std::cout << std::string(INS_Disassemble(ins)) << std::endl;
      insArr[insAddr ^ 0x400000] = ins;
  }
}

VOID unlockAnalysis(void)
{
  _lockAnalysis = UNLOCKED;
}

VOID lockAnalysis(void)
{
  _lockAnalysis = LOCKED;
}

VOID Image(IMG img, VOID *v)
{
  RTN mainRtn = RTN_FindByName(img, "main");

  if (RTN_Valid(mainRtn)){
    RTN_Open(mainRtn);
    RTN_InsertCall(mainRtn, IPOINT_BEFORE, (AFUNPTR)unlockAnalysis, IARG_END);
    RTN_InsertCall(mainRtn, IPOINT_AFTER, (AFUNPTR)lockAnalysis, IARG_END);
    RTN_Close(mainRtn);
  }
}

KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "loop_stats_pin.out", "specify output file name");


VOID Fini(INT32 code, VOID *v)
{

  class LoopStream {
    public:
      MAX_ADDR_TYPE start;
      MAX_COUNT_TYPE iter;
      MAX_COUNT_TYPE count;
      UINT64 totalIns = 0;
      UINT64 totalLoads = 0;
      UINT64 totalCalls = 0;
      UINT64 totalArithmeticIns = 0;
  };

  std::cout << "\nStats from Pin:" << std::endl;
  MAX_ADDR_TYPE prev_pc = 0;
  MAX_ADDR_TYPE total_loops = 0;
  UINT64 total_loops_dynamic = 0;
  UINT32 base_pc = 0x400000;

  UINT64 totalIns = 0;
  UINT64 totalLoads = 0;
  UINT64 totalCalls = 0;


  std::stack<LoopStream> loops; //start, iter, count
  for (MAX_ADDR_TYPE i = 0; i < MAX_ADDR; i++){
    if (_tabAddr[i]) {
      std::cout << " " << std::endl;
      std::cout << _tabAddr[i] << std::endl;
      std::cout << _tabStr[i] << std::endl;
      //std::cout << std::string(INS_Disassemble(insArr[i])) << std::endl;
	
      MAX_COUNT_TYPE cur_loop_iter = 1;
      if (!loops.empty()){
        cur_loop_iter = loops.top().count;
      }
      if (_tabAddr[i] > cur_loop_iter) { // Start of new loop
          LoopStream elem;
          elem.start = base_pc + i;
          elem.iter = _tabAddr[i]/cur_loop_iter;
          elem.count = _tabAddr[i];
          loops.push(elem);
      } else if (_tabAddr[i] < cur_loop_iter) { //end loop
          LoopStream elem = loops.top();
          if (elem.start != base_pc + prev_pc) {
            MAX_COUNT_TYPE calls = elem.count/elem.iter;
            total_loops ++;
            total_loops_dynamic += calls;

            OutFile << std::dec << elem.start << " "  << std::dec << base_pc + prev_pc;
            OutFile << " "  << std::dec << elem.iter << " "  << std::dec << calls;
            OutFile << std::endl;
          }
	  //Before I pop, need to add all the instructions counted in the inner loop to the outer loop too so would save those values here
	  loops.top().totalIns++;
	 
	  if(INS_IsMemoryRead(insArr[i])) {loops.top().totalLoads++;}
          if(INS_IsCall(insArr[i])) {loops.top().totalCalls++;}
	  
	  totalIns += loops.top().totalIns;
	  totalLoads += loops.top().totalLoads;
	  totalCalls += loops.top().totalCalls;

	  //std::cout << "Total Instuctions: "  << totalIns << std::endl;
          //std::cout << "Total Loads: "  << totalLoads << std::endl;
          //std::cout << "Total Calls: "  << totalCalls << std::endl;

          loops.pop();
   //       std::cout << "End loop:\tpc=" << std::hex << base_pc  + prev_pc << std::endl;
      }
      if(!loops.empty()) {
	 
	 //Adding the instruction count of inner loop to outer loop
	 loops.top().totalIns += totalIns;
	 loops.top().totalLoads += totalLoads;
	 loops.top().totalCalls += totalCalls;

	 totalIns = 0;
	 totalLoads = 0;
	 totalCalls = 0;

	 loops.top().totalIns++;
     	 if(INS_IsMemoryRead(insArr[i])) {loops.top().totalLoads++;}
     	 if(INS_IsCall(insArr[i])) {loops.top().totalCalls++;}
         
         /*std::cout << std::string(INS_Disassemble(insArr[i])) << std::endl;
         std::cout << "Total Instuctions: "  << loops.top().totalIns << std::endl;
         std::cout << "Total Loads: "  << loops.top().totalLoads << std::endl;
         std::cout << "Total Calls: "  << loops.top().totalCalls << std::endl;
         std::cout << " " << std::endl;*/
 
      }
      prev_pc = i;
    }
  }
 /* std::cout << "Count ovewrflow? " << std::dec << overflow << std::endl;
  std::cout << "Static count = " << std::dec << total_loops << std::endl;
  std::cout << "Dynamic count = " << std::dec << total_loops_dynamic << std::endl;
  std::cout << "Min addr = 0x" << std::hex << min << std::endl;
  std::cout << "Max addr = 0x" << std::hex << max << " (Forced max 0x" << std::hex << base_pc + MAX_ADDR << ")" << std::endl;
  std::cout << "Total num inst = " << std::dec << num_inst << " (Stopped after " << std::dec << MAX_INSTS << ")" << std::endl;*/
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if(PIN_Init(argc, argv)){
        return Usage();
    }
    
    OutFile.open(KnobOutputFile.Value().c_str());

    PIN_SetSyntaxIntel();
    //IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    
    return 0;
}

