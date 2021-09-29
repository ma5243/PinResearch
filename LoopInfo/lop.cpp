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

#define MAX_ADDR 0x900000
#define MAX_ADDR_TYPE UINT32

#define MAX_COUNT 0xffffffff
#define MAX_COUNT_TYPE UINT32

#define MAX_INSTS 9000000000

std::ofstream OutFile;

static UINT32         _lockAnalysis = !LOCKED;
static MAX_COUNT_TYPE _tabAddr[MAX_ADDR];
static std::string    _tabStr[MAX_ADDR];
UINT64 min = 0x700000000;
UINT64 max = 0;
UINT64 num_inst = 0;
bool overflow = false;

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
    return;
  }

  if (_tabAddr[insAddr ^ 0x400000] == MAX_COUNT) {
    overflow = true;
    return;
  }

  _tabAddr[insAddr ^ 0x400000] += 1;
  _tabStr[insAddr ^ 0x400000] = insDis;

}

VOID Instruction(INS ins, VOID *v)
{
  INS_InsertCall(
      ins, IPOINT_BEFORE, (AFUNPTR)insCallBack,
      IARG_ADDRINT, INS_Address(ins),
      IARG_PTR, new std::string(INS_Disassemble(ins)),
      IARG_END);
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
  };

  std::cout << "\nStats from Pin:" << std::endl;
  MAX_ADDR_TYPE prev_pc = 0;
  MAX_ADDR_TYPE total_loops = 0;
  UINT64 total_loops_dynamic = 0;
  UINT32 base_pc = 0x400000;

  std::stack<LoopStream> loops;

  for (MAX_ADDR_TYPE i = 0; i < MAX_ADDR; i++){
      if (_tabAddr[i]) {
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
            loops.pop();
        }
        prev_pc = i;
      }
    }
    std::cout << "Count ovewrflow? " << std::dec << overflow << std::endl;
    std::cout << "Static count = " << std::dec << total_loops << std::endl;
    std::cout << "Dynamic count = " << std::dec << total_loops_dynamic << std::endl;
    std::cout << "Min addr = 0x" << std::hex << min << std::endl;
    std::cout << "Max addr = 0x" << std::hex << max << " (Forced max 0x" << std::hex << base_pc + MAX_ADDR << ")" << std::endl;
    std::cout << "Total num inst = " << std::dec << num_inst << " (Stopped after " << std::dec << MAX_INSTS << ")" << std::endl;
  }

  int main(int argc, char *argv[])
  {
      PIN_InitSymbols();
      if(PIN_Init(argc, argv)){
          return Usage();
      }
    
      OutFile.open(KnobOutputFile.Value().c_str());

      PIN_SetSyntaxIntel();
      INS_AddInstrumentFunction(Instruction, 0);
      PIN_AddFiniFunction(Fini, 0);
      PIN_StartProgram();
    
      return 0;
}












  





