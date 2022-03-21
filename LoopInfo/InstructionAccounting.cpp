#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>
#include <sstream>
#include <set>

#define START 0x400607
#define END 0x40073d

std::ifstream instrFile;
std::ofstream OutFile;

class LoopStream
{
public:
    UINT64 arithmeticIns;
    UINT64 memoryReadIns;
    UINT64 controlFlowIns;
    UINT64 memoryWriteIns;
    BOOL dependency;
};

static std::vector<LoopStream> loopList;
static std::vector<std::string> addr;
static std::vector<INS> instr;

UINT64 arithmetic_instr = 0;
UINT64 mem_read_instr = 0;
UINT64 mem_write_instr = 0;
UINT64 control_flow_instr = 0;

UINT64 i = 0;
UINT64 addrr;

VOID Application_Start(VOID *v)
{
    instrFile.open("Instructions.txt");
    std::string myline;
    while (instrFile)
    {
        std::getline(instrFile, myline);
        addr.push_back(myline);
    }
    addr.pop_back();
}

// This method will keep track of all these instructions throughout and everytime a loop is detected
// it will attach these statistic to the loop, and then reset the values.
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

BOOL containsInstruction(std::vector<INS> vec, INS in)
{
    for (uint j = 0; j < vec.size(); j++)
    {
        if (INS_Address(vec.at(j)) == INS_Address(in))
        {
            return true;
        }
    }
    return false;
}

VOID doLiveInLiveOutAnalysis(std::vector<INS> vec, LoopStream &elem)
{
    std::set<REG> liveIns;
    std::set<REG> liveOuts;

    int k = vec.size() - 1;
    // std::cout << k << std::endl;

    for (; k >= 0; k--)
    {
        //std::cout << k << " " << std::hex << INS_Address(vec.at(k)) << std::endl;
        int numOperands = INS_OperandCount(vec.at(k));
        for (int j = 0; j < numOperands; j++)
        {
            if (INS_OperandIsReg(vec.at(k), j))
            {
                REG rgstr = INS_OperandReg(vec.at(k), j);
                if (INS_OperandReadAndWritten(vec.at(k), j))
                { // corner case of a = a+b
                    liveIns.insert(rgstr);
                    liveOuts.insert(rgstr);
                    // std::cout << "Added to both: " << "Operand: " << k  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << std::endl;
                }
                else if (INS_OperandReadOnly(vec.at(k), j))
                {
                    liveIns.insert(rgstr);
                    // std::cout << "Added to Live Ins: " << "Operand: " << k  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << std::endl;
                }
                else if (INS_OperandWrittenOnly(vec.at(k), j))
                {
                    std::set<REG>::iterator iter = liveIns.find(rgstr);
                    if (iter != liveIns.end())
                    {
                        liveIns.erase(iter);
                        // std::cout << "Erased from Live Ins: " << "Operand: " << k  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << std::endl;
                    }
                    liveOuts.insert(rgstr);
                    // std::cout << "Added to Live Outs: " << "Operand: " << k  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << std::endl;
                }
            }
        }
    }
    // std::cout << "end\n";
    for (REG reg : liveIns)
    {
        std::set<REG>::iterator iter = liveOuts.find(reg);
        if (iter != liveOuts.end())
        {
            elem.dependency = true;
            break;
        }
    }
}

INT32 Usage()
{
    std::cerr << "Usage: Not Implemented" << std::endl;
    return -1;
}

VOID Trace(TRACE trace, VOID *v)
{
    // std::cout << myline << std::endl;
    //  Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        if (INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END)
        {
            if (i == addr.size())
            {
                break;
            }
            // std::cout << addr.at(i) << std::endl;
            if (addr.at(i) != "end")
            {
                std::stringstream temp(addr.at(i));
                temp >> addrr;

                if (addrr == INS_Address(BBL_InsTail(bbl)))
                {

                    // std::cout << INS_Address(BBL_InsHead(bbl)) << std::endl;

                    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
                    {
                        if (INS_IsMemoryRead(ins) || INS_Opcode(ins) == XED_ICLASS_POP)
                        {
                            // std::cout << "Memory Read Instruction: " << std::hex << INS_Address(ins) << std::endl;
                            mem_read_instr++;
                        }
                        else if (INS_IsMemoryWrite(ins) || INS_Opcode(ins) == XED_ICLASS_PUSH)
                        {
                            // std::cout << "Memory Write Instruction: " << std::hex << INS_Address(ins) << std::endl;
                            mem_write_instr++;
                        }
                        else if (INS_IsControlFlow(ins))
                        {
                            // std::cout << "Control Flow Instruction: " << std::hex << INS_Address(ins) << std::endl;
                            control_flow_instr++;
                        }
                        else if (INS_Opcode(ins) == XED_ICLASS_ADD || INS_Opcode(ins) == XED_ICLASS_SUB ||
                                 INS_Opcode(ins) == XED_ICLASS_AND || INS_Opcode(ins) == XED_ICLASS_IMUL ||
                                 INS_Opcode(ins) == XED_ICLASS_IDIV || INS_Opcode(ins) == XED_ICLASS_OR ||
                                 INS_Opcode(ins) == XED_ICLASS_XOR || INS_Opcode(ins) == XED_ICLASS_SHL ||
                                 INS_Opcode(ins) == XED_ICLASS_SHR || INS_Opcode(ins) == XED_ICLASS_NOT ||
                                 INS_Opcode(ins) == XED_ICLASS_NEG || INS_Opcode(ins) == XED_ICLASS_INC ||
                                 INS_Opcode(ins) == XED_ICLASS_DEC || INS_Opcode(ins) == XED_ICLASS_MOV)
                        {
                            // std::cout << "Arithmetic Instruction: " << std::hex << INS_Address(ins) << std::endl;
                            arithmetic_instr++;
                        }
                        // std::cout << std::hex << INS_Address(ins) << std::endl;

                        // if(std::find(instr.begin(), instr.end(), ins) == instr.end()) {
                        // if(!containsInstruction(instr,ins)) {
                        std::cout << std::hex << INS_Address(ins) << std::endl;
                        instr.push_back(ins);

                        std::cout << "Current size: " << instr.size() << std::endl;
                        for (uint val = 0; val < instr.size(); val++)
                        {
                            std::cout << std::hex << INS_Address(instr.at(val)) << std::endl;
                        }
                        //}

                        //}
                    }
                    i++;
                }
            }
            else
            {
                LoopStream elem;
                elem.arithmeticIns = arithmetic_instr;
                elem.controlFlowIns = control_flow_instr;
                elem.memoryReadIns = mem_read_instr;
                elem.memoryWriteIns = mem_write_instr;
                elem.dependency = false;

                for (uint val = 0; val < instr.size(); val++)
                {
                    std::cout << std::hex << INS_Address(instr.at(val)) << std::endl;
                }
                std::cout << "end\n";
                doLiveInLiveOutAnalysis(instr, elem);
                instr.clear();

                loopList.push_back(elem);
                i++;

                arithmetic_instr = 0;
                control_flow_instr = 0;
                mem_read_instr = 0;
                mem_write_instr = 0;
            }
            // std::cout << temp << std::endl;
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
        if (loopList.at(i).dependency == false)
        {
            // UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries;
            OutFile << "Total number of arithmetic instructions for loop " << i << ": " << loopList.at(i).arithmeticIns << std::endl;
            OutFile << "Total number of read memory instructions for loop " << i << ": " << loopList.at(i).memoryReadIns << std::endl;
            OutFile << "Total number of write memory instructions for loop " << i << ": " << loopList.at(i).memoryWriteIns << std::endl;
            OutFile << "Total number of control flow instructions for loop " << i << ": " << loopList.at(i).controlFlowIns << std::endl;
            OutFile << "" << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{

    PIN_InitSymbols();
    if (PIN_Init(argc, argv))
        return Usage();

    PIN_AddApplicationStartFunction(Application_Start, 0);

    OutFile.open(KnobOutputFile.Value().c_str());

    PIN_SetSyntaxIntel();

    TRACE_AddInstrumentFunction(Trace, 0);

    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
