#include <iostream>
#include <fstream>
#include <algorithm>
#include "pin.H"
#include <unordered_map>
#include <stack>
#include <sstream>
#include <set>
#include <list>
//#include "cvp.h"

//#define START 0x400607
//#define END 0x40073d

std::ifstream instrFile;
std::ofstream OutFile;

class LoopStream
{
public:
    UINT64 arithmeticIns;
    UINT64 memoryReadIns;
    UINT64 controlFlowIns;
    UINT64 memoryWriteIns;
    UINT64 dependency;
};

static std::vector<LoopStream> loopList;
static std::vector<std::string> addr;
static std::vector<INS> instr;
//static std::set<INS> predictInstructions;
// static std::list<INS> instr;

UINT64 arithmetic_instr = 0;
UINT64 mem_read_instr = 0;
UINT64 mem_write_instr = 0;
UINT64 control_flow_instr = 0;

UINT64 i = 0; //counter to loop through all addresses
UINT64 tailAddrInt; //convert addresses from strings to ints 
UINT64 headAddrInt;
UINT64 numSeen = 0;

KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                                 "o", "bblcount.out", "specify output file name");

KNOB<std::string> KnobInputFile(KNOB_MODE_WRITEONCE, "pintool",
                                 "i", "Instructions.txt", "specify input file name");


VOID Application_Start(VOID *v)
{
    instrFile.open(KnobInputFile.Value().c_str());
    std::string myline;
    while (instrFile)
    {
        //std::cout << myline << std::endl;
        std::getline(instrFile, myline);
        addr.push_back(myline);
    }
    addr.pop_back();
    //beginPredictor(0,NULL);
}

VOID doLiveInLiveOutAnalysis(std::vector<INS> vec, LoopStream &elem)
{
    std::set<REG> liveIns;
    std::set<REG> liveOuts;

    //std::vector<int> instructionNumsIn;
    //std::vector<int> instructionNumsOut;

    int k = vec.size() - 1;
    //std::cout << k << std::endl;

    for (; k >= 0; k--)
    {
        INS ins = vec.at(k);
        //std::cout << std::hex << INS_Address(ins) << std::endl;
        //bool erased = false;
        // vec.pop_back();
        // std::cout << k << " " << std::hex << INS_Address(vec.at(k)) << std::endl;
        int numOperands = INS_OperandCount(ins);
        for (int j = 0; j < numOperands; j++)
        {
            if (INS_OperandIsReg(ins, j))
            {
                REG rgstr = INS_OperandReg(ins, j);
                if(REG_is_gr_type(rgstr)) {    
                    if (INS_OperandReadAndWritten(ins, j))
                    { // corner case of a = a+b
                        liveIns.insert(rgstr);
                        liveOuts.insert(rgstr);

                        //instructionNumsIn.push_back(k);
                        //instructionNumsOut.push_back(k);
          //              std::cout << "Added to both: " << "Operand: " << j  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << " Register: " << rgstr << std::endl;
                    }
                    else if (INS_OperandReadOnly(ins, j))
                    {
                        liveIns.insert(rgstr);
                        //instructionNumsIn.Push_back(k);
            //            std::cout << "Added to Live Ins: " << "Operand: " << j  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << " Register: " << rgstr << std::endl;
                    }
                    else if (INS_OperandWrittenOnly(ins, j))
                    {
                        std::set<REG>::iterator iter = liveIns.find(rgstr);
                        //std::vector<int>::iterator iterTwo = instructionNumsIn.find(k);
                        if (iter != liveIns.end())
                        {
                            liveIns.erase(iter);
                            //instructionNumsIn.erase(iterTwo);
                            //erased = true;
                           // std::cout << "Erased from Live Ins: " << "Operand: " << j  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << " Register: " << rgstr << std::endl;
                        }
                        liveOuts.insert(rgstr);
                        //instructionNumsOut.insert(k);
                        //std::cout << "Added to Live Outs: " << "Operand: " << j  << " Instruction: " << std::hex << INS_Address(instr.at(k)) << " Register: " << rgstr << std::endl;
                    }
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
            //std::cout << "Common: " << reg << std::endl;
            elem.dependency ++;
            //break;
        }
    }
    /*for(int check: instructionNumsIn) {
        std::vector<int>::iterator iter = instructionNumsOut.find(check);
        if(iter != instructionNumsOut.end()) {
            predictInstructions.insert(vec.at(k));
        }
    }*/
    // std::cout << "test\n";
}

INT32 Usage()
{
    std::cerr << "Usage: Not Implemented" << std::endl;
    return -1;
}

VOID Trace(TRACE trace, VOID *v)
{
    //  Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
/*if (INS_Address(BBL_InsTail(bbl)) < 0x7fffffff){
                std::cout << INS_Address(BBL_InsTail(bbl)) << " " << INS_Address(BBL_InsHead(bbl)) <<std::endl;
            }
            continue;*/
       // if (INS_Address(BBL_InsHead(bbl)) >= START && INS_Address(BBL_InsTail(bbl)) <= END)
       // {
            if (i == addr.size())
            {
                break;
            }
            //Current solution is to have the list of instructions ready and add seperate block 
            //here to check if those instructions need to be predicted and if they do then predict
            //However, this requires instrumenting each instruction which would be really inefficient
            //The other way is to somehow figure out when the last iteration of the loop is being run 
            //and do my i++ in the other analsyis implementation accordingly instead of after the 1st
            //iteration like it currently does. However, not sure how to refactor in such a manner. 

            // std::cout << temp << std::endl;
            // std::cout << addr.at(i) << std::endl;
            /*std::stringstream temp(addr.at(i));
            temp >> tailAddrInt;
            while (tailAddrInt > 0x7fffffff){
                while (addr.at(i) != "end"){
                    i ++;
                }
                i++;
                std::stringstream temp(addr.at(i));
                temp >> tailAddrInt;
            }*/
            
            if (addr.at(i) != "end")
            {
                std::stringstream temp(addr.at(i));
                temp >> tailAddrInt;

                std::stringstream tempTwo(addr.at(i+1));
                tempTwo >> headAddrInt;
                //std::cout << "addr: " << addr.at(i+1) << " - " << addr.at(i) << std::endl;
                //std::cout << "bbl: " << INS_Address(BBL_InsHead(bbl)) << " - " << INS_Address(BBL_InsTail(bbl)) <<std::endl;
                if (tailAddrInt == INS_Address(BBL_InsTail(bbl)) && headAddrInt == INS_Address(BBL_InsHead(bbl)))
                {
                    numSeen = 0;
                    //std::cout << "addr two: " << addr.at(i) << std::endl;

                    // std::cout << INS_Address(BBL_InsHead(bbl)) << std::endl;

                    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
                    {
                        //how to classify cltq
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
                                 INS_Opcode(ins) == XED_ICLASS_DEC || INS_Opcode(ins) == XED_ICLASS_MOV || 
                                 INS_Mnemonic(ins) == "cltq")
                        {
                            // std::cout << "Arithmetic Instruction: " << std::hex << INS_Address(ins) << std::endl;
                            arithmetic_instr++;
                        }
                        
                            instr.push_back(ins);

                        /*std::cout << "Current size: " << instr.size() << std::endl;
                        for (uint val = 0; val < instr.size(); val++)
                        {
                            std::cout << std::hex << INS_Address(instr.at(val)) << std::endl;
                        }*/
                        //}

                        //}
                    }
                    i+=2;
                    //std::cout << "addr 3: " << addr.at(i) << std::endl;
                    if (addr.at(i) == "end")
                    {
                        LoopStream elem;
                        elem.arithmeticIns = arithmetic_instr;
                        elem.controlFlowIns = control_flow_instr;
                        elem.memoryReadIns = mem_read_instr;
                        elem.memoryWriteIns = mem_write_instr;
                        elem.dependency = 0;

                        /*std::cout << "check start\n";
                        for (uint val = 0; val < instr.size(); val++)
                        {
                        std::cout << std::hex << INS_Address(instr.at(val)) << std::endl;
                        }*/
                        // std::cout << "check end\n";
                        /*for (uint val = 0; val < instr.size(); val++)
                        {
                        std::cout << std::hex << INS_Address(instr.at(val)) << std::endl;
                        }*/
                        doLiveInLiveOutAnalysis(instr, elem); //this also gives me the set of instruction I need to run predictions on 
                        instr.clear();

                        loopList.push_back(elem);
                        i++;

                        arithmetic_instr = 0;
                        control_flow_instr = 0;
                        mem_read_instr = 0;
                        mem_write_instr = 0;
                    }
                }
           // }
        }
    }
}


// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{

    OutFile << "Total number of Loops: " << loopList.size() << std::endl;
    OutFile << "" << std::endl;
    for (UINT32 i = 0; i < loopList.size(); i++)
    {
        if (loopList.at(i).dependency == 0)
        {
            // UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries;
            OutFile << "Total number of arithmetic instructions for loop " << i << ": " << loopList.at(i).arithmeticIns << std::endl;
            OutFile << "Total number of read memory instructions for loop " << i << ": " << loopList.at(i).memoryReadIns << std::endl;
            OutFile << "Total number of write memory instructions for loop " << i << ": " << loopList.at(i).memoryWriteIns << std::endl;
            OutFile << "Total number of control flow instructions for loop " << i << ": " << loopList.at(i).controlFlowIns << std::endl;
            OutFile << "" << std::endl;
        }
    }
    OutFile << "\nCould not vectorize:" << std::endl;
    for (UINT32 i = 0; i < loopList.size(); i++)
    {
        if (loopList.at(i).dependency > 0)
        {
            // UINT32 totalIters = loopList.at(i).iter + loopList.at(i).entries;
            OutFile << "Total number of dependencies " << i << ": " << loopList.at(i).dependency << std::endl;
            OutFile << "Total number of arithmetic instructions for loop " << i << ": " << loopList.at(i).arithmeticIns << std::endl;
            OutFile << "Total number of read memory instructions for loop " << i << ": " << loopList.at(i).memoryReadIns << std::endl;
            OutFile << "Total number of write memory instructions for loop " << i << ": " << loopList.at(i).memoryWriteIns << std::endl;
            OutFile << "Total number of control flow instructions for loop " << i << ": " << loopList.at(i).controlFlowIns << std::endl;
            OutFile << "" << std::endl;
        }
    }
    //endPredictor();
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
