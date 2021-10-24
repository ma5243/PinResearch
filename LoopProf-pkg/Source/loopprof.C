#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "pin.H"
#include "loopprof.H"

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Knobs ///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static KNOB<BOOL> KnobTraceSharedLibs(KNOB_MODE_WRITEONCE,
					   "pintool",
					   "shared-libs",
					   "0",
					   "instrument shared libraries");

static KNOB<string> KnobMode(KNOB_MODE_WRITEONCE,
			     "pintool",
			     "mode",
			     "path",
			     "mode of loop detection [path|backedge|cfg]");

static KNOB<BOOL> KnobAppendPid(KNOB_MODE_WRITEONCE,
				"pintool",
				"pid",
				"1",
				"append pid to output files");

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Global Non-Reentrant Variables //////////////////////
///////////////////////////////////////////////////////////////////////////////

// Read-only after initialization
static bool TraceSharedLibs;
static bool AppendPid;
static string Mode;
static string LOOPPROF_HOME;
static string PIN_HOME;
static bool Instrument = true;
static string Outdir;
static string ProgramName;

static int ARGC;
static char **ARGV;
static char **ENVP;

static set<string> IgnorePrograms;
static set<string> OnlyExecPrograms;

map<ADDRINT, string> PltSymTab;

// Method-generic analysis functions
VOID (*A_DumpBuffer)(threadid_t);
VOID (*A_ProcessReturn)(ADDRINT, ThreadContext *, Activation *, threadid_t);

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Thread-local Storage ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static ThreadContext **Contexts;

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Global Reentrant Variables //////////////////////////
///////////////////////////////////////////////////////////////////////////////

// These are all protected by PIN_LockClient()
static UINT32 MaxNumContexts = 128;

map<ADDRINT, LoopStats *> Loops;
map<ADDRINT, Function *> Functions;
map<ADDRINT, FunctionStats *> FuncStats;

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Utility Functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static string Which(const string &prog)
{
  if( prog.size() == 0 ) {return "";}
  else if( prog[0] == '/' || prog[0] == '.' ) {return prog;}
  string cmd = "which " + prog;
  FILE *pipe = popen(cmd.c_str(), "r");
  if( !pipe ) {return prog;}
  char buf[1024];
  if( fgets(buf, 1024, pipe) == NULL ) {return prog;}
  if( strstr("buf", "which: ") ) {return prog;}
  char *s;
  if( (s = strrchr(buf, '\n')) != NULL ) {
    *s = '\0';    
  }
  if( pclose(pipe) == 0 ) {
    return string(buf);
  } else {
    return prog;
  }
}

static VOID
InitPltSymTab(string prog)
{
  const int bufsize = 4096;
  char buf[bufsize];
  char buf2[bufsize];

  prog = Which(prog);
  if( access(prog.c_str(), F_OK) == 0 ) {
    string cmd = "objdump -d -j .plt ";
    cmd += prog + " 2>&1 | grep ^0";
    FILE *pipe = popen(cmd.c_str(), "r");
    while( fgets(buf, bufsize, pipe) != NULL ) {
      unsigned long addr;
      if( sscanf(buf, "0%lx <%[^>]", &addr, buf2) == 2 ) {
	PltSymTab[addr] = string(buf2);
      }
    }
  }
}

static inline Function *
getFunction(ADDRINT addr)
{
  PIN_LockClient();
  if( Functions.find(addr) == Functions.end() ) {
    RTN rtn = RTN_FindByAddress(addr);
    Function *f;
    if( RTN_Valid(rtn) ) {
      IMG_TYPE imgType = IMG_Type(SEC_Img(RTN_Sec(rtn)));
      bool inMainImg = (imgType == IMG_TYPE_SHARED
			|| imgType == IMG_TYPE_STATIC);
      string name;
      if( RTN_Name(rtn) == ".plt" ) {
	if( PltSymTab.find(addr) != PltSymTab.end() ) {
	  name = PltSymTab[addr];
	} else {
	  name = ".plt";
	}
      } else {
	name = RTN_Name(rtn);
      }
      f = new Function(addr, name, inMainImg);
    } else {
      f = new Function(addr, "??", true);
    }
    ASSERTP(f != NULL);

    Functions[addr] = f;
    PIN_UnlockClient();
    return f;
  } else {
    Function *ret = Functions[addr];
    PIN_UnlockClient();
    return ret;
  }
}

static VOID
printFlatTrace(ofstream *outfile, map<ADDRINT, LoopStats *> &loops,
	       map<ADDRINT, FunctionStats *> &functions)
{
  *outfile << "m,flat" << endl;
  *outfile << "# l,addr,srcfile,lineno,iterations,self_ins,total_ins,"
	   << "hist0,hist1,hist2,hist3,hist4,hist5,"
	   << "hist6,hist7,hist8,hist9.hist10,histRest" << endl;

  map<ADDRINT, LoopStats *>::iterator li;

  for( li = loops.begin(); li != loops.end(); li++ ) {
    int lineno;
    const char *srcfile;
    PIN_FindLineFileByAddress(li->first, &lineno, &srcfile);
    if( !srcfile ) {
      srcfile = "??";
    }
    *outfile << "l"
	     << "," << hex << li->first << dec
	     << "," << srcfile << "," << lineno
	     << "," << li->second->iterations
	     << "," << li->second->selfIns
	     << "," << li->second->totalIns
	     << "," << li->second->iterationsHisto[0]
	     << "," << li->second->iterationsHisto[1]
	     << "," << li->second->iterationsHisto[2]
	     << "," << li->second->iterationsHisto[3]
	     << "," << li->second->iterationsHisto[4]
	     << "," << li->second->iterationsHisto[5]
	     << "," << li->second->iterationsHisto[6]
	     << "," << li->second->iterationsHisto[7]
	     << "," << li->second->iterationsHisto[8]
	     << "," << li->second->iterationsHisto[9]    
	     << "," << li->second->iterationsHisto[10]    
	     << "," << li->second->iterationsHisto[11]
	     << endl;
  }

  *outfile << "# f,addr,name,calls,selfIns,totalIns" << endl;

  
  PIN_LockClient();
  map<ADDRINT, FunctionStats *>::iterator fi;
  for( fi = functions.begin(); fi != functions.end(); fi++ ) {
    ADDRINT addr = fi->first;
    FunctionStats *fstats = fi->second;
    Function *func = Functions[addr];
    
    if( !TraceSharedLibs && !func->inMainImg ) {continue;}
    if( fstats->calls == 0 ) {continue;}
    *outfile << "f"
	     << "," << hex << addr << dec
	     << "," << func->name
	     << "," << fstats->calls
	     << "," << fstats->selfIns
	     << "," << fstats->totalIns
	     << endl;

  }
  PIN_UnlockClient();
}

static VOID
printGraph(ofstream *outfile, map<ADDRINT, LoopStats *> &loops,
	   map<ADDRINT, FunctionStats *> &functions)
{
  *outfile << "m,graph" << endl;
  map<ADDRINT, FunctionStats *>::iterator fi;
  
  for( fi = functions.begin(); fi != functions.end(); fi++ ) {
    *outfile << "f," << hex << fi->first << dec << ",f";
    set<ADDRINT>::iterator i;
    for( i = fi->second->childFunctions.begin();
	 i != fi->second->childFunctions.end();
	 i++ ) {
      *outfile << "," << hex << *i << dec;
    }
    *outfile << ",l";
    for( i = fi->second->childLoops.begin();
	 i != fi->second->childLoops.end();
	 i++ ) {
      *outfile << "," << hex << *i << dec;
    }
    *outfile << endl;
  }

  map<ADDRINT, LoopStats *>::iterator li;
  for( li = loops.begin(); li != loops.end(); li++ ) {
    *outfile << "l," << hex << li->first << dec << ",f";
    set<ADDRINT>::iterator i;
    for( i = li->second->childFunctions.begin();
	 i != li->second->childFunctions.end();
	 i++ ) {
      *outfile << "," << hex << *i << dec;
    }
    *outfile << ",l";
    for( i = li->second->childLoops.begin();
	 i != li->second->childLoops.end();
	 i++ ) {
      *outfile << "," << hex << *i << dec;
    }
    *outfile << endl;
  } 
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Class Methods ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////// Class ThreadContext Methods /////////////////////////

ThreadContext::ThreadContext(threadid_t _threadid) :
  bufferIndex(0),
  maxNumActivations(16),
  activationIndex(-1),
  threadid(_threadid),
  outfile(NULL)
{
  for( int i = 0; i < BUFFER_SIZE; i++ ) {
    buffer[i].type = BUFFER_TYPE_PATH_BBL;
  }

  activations = new Activation*[maxNumActivations];
  ASSERTP(activations != NULL);

  // Open output file
  ostringstream oss;
  oss << Outdir << "/" << ProgramName << ".";
  if( AppendPid ) {
    oss  << getpid() << ".";
  }
  oss << threadid << ".raw";

  outfile = new ofstream(oss.str().c_str());
  ASSERTP(outfile != NULL);
  ASSERTS(outfile->is_open(), "error opening file: %s\n", oss.str().c_str());
    
  MSG("thread %d created output file\n", threadid);
  
}

ThreadContext::~ThreadContext()
{
  map<ADDRINT, LoopStats *>::iterator li;
  for( li = loops.begin(); li != loops.end(); li++ ) {
    delete li->second;
  }
  loops.clear();

  map<ADDRINT, FunctionStats *>::iterator fi;
  for( fi = funcStats.begin(); fi != funcStats.end(); fi++ ) {
    delete li->second;
  }
  funcStats.clear();

  delete[] activations;
  activations = NULL;

  if( outfile ) {
    outfile->close();
  }
  delete outfile;
  outfile = NULL;
}

VOID
ThreadContext::pushActivation(Activation *a)
{
  a->calls++;
  activations[++activationIndex] = a;

  if( unlikely(activationIndex + 1 == maxNumActivations) ) {
    Activation **newActivations = new Activation*[maxNumActivations * 2];
    ASSERTP(newActivations != NULL);
    for( INT32 i = 0; i < maxNumActivations; i++ ) {
      newActivations[i] = activations[i];
    }
    maxNumActivations *= 2;
    delete activations;
    activations = newActivations;
  }
}

Activation *
ThreadContext::topActivation()
{
  return activations[activationIndex];
}

VOID
ThreadContext::popActivation()
{
  Activation *top = topActivation();
  top->clear();
  Function *func = top->function;
  func->freeActivation(top, threadid);
  activationIndex--;
}

VOID
ThreadContext::consolidateStats(VOID)
{
  map<ADDRINT, Function *>::iterator fi;
  PIN_LockClient();
  for( fi = Functions.begin(); fi != Functions.end(); fi++ ) {
    if( !TraceSharedLibs && !fi->second->inMainImg ) {continue;}
    fi->second->consolidateThreadStats(this);
  }
  PIN_UnlockClient();
  
  PIN_LockClient();
  map<ADDRINT, LoopStats *>::iterator li;
  for( li = loops.begin(); li != loops.end(); li++ ) {
    ADDRINT addr = li->first;
    LoopStats *localStats = li->second;

    if( Loops.find(li->first) == Loops.end() ) {
      Loops[addr] = new LoopStats(*localStats);
      ASSERTP(Loops[addr] != NULL);
    } else {    
      *Loops[addr] += *localStats;
    }
  }
  PIN_UnlockClient();
}

///////////////////////// Class Function Methods //////////////////////////////

Function::Function(ADDRINT _addr, string _name, bool _inMainImg) :
  addr(_addr),
  inMainImg(_inMainImg),
  name(_name),
  maxNumBbls(8),
  numThreads(8)
{
  InitLock(&lockvar);
  
  // Make dummy 0 for front of path.
  bblIndex(0);

  // We have an activation pool for each thread.
  size_t sz = numThreads * sizeof(vector<Activation *> *);
  ActivationPool = (vector<Activation *> **)malloc(sz);
  ASSERTP(ActivationPool != NULL);
  for( size_t i = 0; i < numThreads; i++ ) {
    ActivationPool[i] = new vector<Activation *>();
    ASSERTP(ActivationPool[i] != NULL);
  }
}

Function::~Function()
{
  for( size_t i = 0; i < numThreads; i++ ) {
    if( ActivationPool[i] != NULL ) {
      vector<Activation *>::iterator ai;
      for( ai = ActivationPool[i]->begin();
	   ai != ActivationPool[i]->end();
	   ai++ ) {
	delete *ai;
      }
      delete ActivationPool[i];
      ActivationPool[i] = NULL;
    }
  }
}

int
Function::bblIndex(ADDRINT bblHead)
{
  // no lock, assume instrumentation isn't reentrant
  //lock();
  if( bblIndices.find(bblHead) == bblIndices.end() ) {
    size_t index = bblIndices.size();
    bblIndices[bblHead] = index;
    if( index > maxNumBbls ) {
      maxNumBbls = index;
    }
  }
  int ret = bblIndices[bblHead];
  //unlock();

  return ret;
}

VOID
Function::lock(VOID)
{
  GetLock(&lockvar, 1);
}

VOID
Function::unlock(VOID)
{
  ReleaseLock(&lockvar);
}

VOID
Function::freeActivation(Activation *a, threadid_t threadid)
{
  vector<Activation *> *localAP;
  lock();
  localAP = ActivationPool[threadid];
  unlock();

  localAP->push_back(a);
}

Activation *
Function::getActivation(ADDRINT callReturnAddr, Activation *parent,
			threadid_t threadid)
{
  Activation *a;
  
  if( threadid >= numThreads ) {
    lock();
    size_t sz = numThreads * 2  * sizeof(vector<Activation *> *);
    ActivationPool = (vector<Activation *> **)realloc(ActivationPool, sz);
    ASSERTP(ActivationPool != NULL);
    for( size_t i = numThreads; i < (2 * numThreads); i++ ) {
      ActivationPool[i] = new vector<Activation *>();
      ASSERTP(ActivationPool[i] != NULL);
    }
    numThreads *= 2;
    unlock();
  }

  vector<Activation *> *localAP;
  lock();
  localAP = ActivationPool[threadid];
  unlock();

  if( unlikely(localAP->size() == 0) ) {
    a = new Activation(this);
    ASSERTP(a != NULL);
  } else {
    a = localAP->back();
    localAP->pop_back();
  }
  a->addToPath(a->bpis[0]);
  a->callReturnAddr = callReturnAddr;
  a->parent = parent;
  return a;
}


// Assume PIN_LockClient() held
VOID
Function::consolidateThreadStats(ThreadContext *context)
{
  threadid_t threadid = context->threadid;
  UINT32 calls = 0;
  UINT64 selfIns = 0;
  UINT64 totalIns = 0;
  FunctionStats *ctxFuncStats;

  if( context->funcStats.find(addr) == context->funcStats.end() ) {
    ctxFuncStats = new FunctionStats();
    ASSERTP(ctxFuncStats != NULL);
    context->funcStats[addr] = ctxFuncStats;
  } else {
    ctxFuncStats = context->funcStats[addr];
  }

  if (threadid >= numThreads) {
    return;
  }
  
  lock();
  vector<Activation *> *localAP = ActivationPool[threadid];
  unlock();
  
  // maxNumBbls could be changed by another thread, but this is guaranteed
  // to be big enough for this dead thread
  UINT32 maxNumBpis = maxNumBbls;
  BblPathInfo **accumBpis = new BblPathInfo*[maxNumBpis];
  ASSERTP(accumBpis != NULL);

  for( UINT32 i = 0; i < maxNumBpis; i++ ) {
    accumBpis[i] = NULL;
  }

  vector<Activation *>::iterator ai;
  for( ai = localAP->begin(); ai != localAP->end(); ai++ ) {
    Activation *a = *ai;

    // Per-thread self/total instruction counts for each function
    calls += a->calls;
    selfIns += a->sumSelfIns;
    totalIns += a->sumTotalIns;

    for( UINT32 i = 0; i < maxNumBpis; i++ ) {
      if( i >= a->maxNumBpis || a->bpis[i] == NULL ) {continue;}
      BblPathInfo *bpi = a->bpis[i];

      if( accumBpis[i] == NULL ) {
	accumBpis[i] = bpi;
	continue;
      }
      
      *accumBpis[i] += *bpi;
    }
  }

  for( UINT32 i = 0; i < maxNumBpis; i++ ) {
    if( accumBpis[i] == NULL ) {continue;}

    BblPathInfo *bpi = accumBpis[i];

    if( bpi->loopHead == NULL ) {
      // TOP LEVEL
      if( bpi->childFunctions != NULL && bpi->loopStats == NULL ) {
	// top level non-loop bbl that made calls
	set<ADDRINT>::iterator cfi = bpi->childFunctions->begin();
	for( ; cfi != bpi->childFunctions->end(); cfi++ ) {
	  ctxFuncStats->childFunctions.insert(*cfi);
	}
      } else if( bpi->loopStats != NULL ) {
	// top level loop
	ctxFuncStats->childLoops.insert(bpi->addr);
      }
    }

    if( bpi->loopHead != NULL && bpi->loopStats != NULL ) {
      bpi->loopHead->loopStats->childLoops.insert(bpi->addr);
    }

    if( bpi->childFunctions != NULL ) {
      BblPathInfo *head = NULL;
      if( bpi->loopStats != NULL ) {
	head = bpi;
      } else if( bpi->loopHead != NULL ) {
	head = bpi->loopHead;
      }

      if( head != NULL ) {
	set<ADDRINT>::iterator cfi = bpi->childFunctions->begin();
	for( ; cfi != bpi->childFunctions->end(); cfi++ ) {
	  head->loopStats->childFunctions.insert(*cfi);
	}
      }
    }
  }

  ctxFuncStats->calls += calls;
  ctxFuncStats->selfIns += selfIns;
  ctxFuncStats->totalIns += totalIns;  

  if( FuncStats.find(addr) == FuncStats.end() ) {
    FuncStats[addr] = new FunctionStats();
    ASSERTP(FuncStats[addr] != NULL);
  }
  *FuncStats[addr] += *ctxFuncStats;

  delete[] accumBpis;
}

VOID
Function::freeThreadStorage(threadid_t threadid)
{
  if (threadid >= numThreads) {
    return;
  }
  lock();
  vector<Activation *> *localAP = ActivationPool[threadid];
  unlock();
  
  vector<Activation *>::iterator ai;
  for( ai = localAP->begin(); ai != localAP->end(); ai++ ) {
    delete *ai;
  }
  localAP->clear();
}

///////////////////////// Class DBbl Methods //////////////////////////////////

VOID 
DBbl::addBbls(BBL bbl, set<ADDRINT> &splits)
{
  bbls.clear();
  UINT32 icount = 1;
  
  if( BBL_NumIns(bbl) > numIns ) {
    numIns = BBL_NumIns(bbl);
    head = BBL_Address(bbl);
  }
  
  for( INS ins = BBL_InsTail(bbl); INS_Valid(ins); ins = INS_Prev(ins) ) {
    ADDRINT iaddr = INS_Address(ins);
    if( splits.find(iaddr) != splits.end() ) {
      bbls.insert(bbls.begin(), Bbl(iaddr, icount));
      icount = 0;
      if( iaddr == head ) {
	return;
      }
    }
    icount++;
  }
  bbls.insert(bbls.begin(), Bbl(head, icount-1));
}

VOID
DBbl::insertBbl(BBL bbl)
{
  INT32 icount = 0;
  
  if( BBL_NumIns(bbl) > numIns ) {
    numIns = BBL_NumIns(bbl);
    head = BBL_Address(bbl);
  }
  if( bbls.size() == 0 ) {
    icount = BBL_NumIns(bbl);
  } else {
    icount = BBL_NumIns(bbl) - bbls[0].numIns;
  }
  if( icount < 0 ) {
    icount = BBL_NumIns(bbl);
  }

  bbls.insert(bbls.begin(), Bbl(BBL_Address(bbl), icount));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Analysis Callbacks //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////// Bbl Callbacks ///////////////////////////////////////

static int
A_BufferRegularBbl(ADDRINT addr, UINT32 numIns, UINT32 index,
		   threadid_t threadid)
{
  ThreadContext *context = Contexts[threadid];
  BufferBbl *bbl = &context->buffer[context->bufferIndex].u.bbl;

  bbl->addr = addr;
  bbl->index = index;
  bbl->numIns = numIns;

  context->bufferIndex++;
  return context->bufferIndex == BUFFER_SIZE;
}

/*
static int
A_BufferNonLoopHeadBbl(UINT32 numIns, UINT32 headIndex, threadid_t threadid)
{
  ThreadContext *context = Contexts[threadid];
  BufferObject *buffer = &context->buffer[context->bufferIndex];
  
  buffer->u.bbl.index = headIndex;
  buffer->u.bbl.numIns = numIns;
  buffer->type = BUFFER_TYPE_NONPATH_BBL;

  context->bufferIndex++;
  return context->bufferIndex == BUFFER_SIZE;
}
*/

static int
A_BufferSharedLibBbl(UINT32 numIns, threadid_t threadid)
{
  ThreadContext *context = Contexts[threadid];
  BufferObject *buffer = &context->buffer[context->bufferIndex];

  context->bufferIndex++;
  buffer->type = BUFFER_TYPE_LIB_BBL;
  buffer->u.bbl.numIns = numIns;

  return context->bufferIndex == BUFFER_SIZE;
}

///////////////////////// Callstack Callbacks /////////////////////////////////

static int
A_BufferCall(Function *dstFunc, ADDRINT callReturnAddr, threadid_t threadid)
{
  ThreadContext *context = Contexts[threadid];
  BufferObject *buffer = &context->buffer[context->bufferIndex];

  buffer->type = BUFFER_TYPE_CALL;
  buffer->u.callInfo.dstFunc = dstFunc;
  buffer->u.callInfo.callReturnAddr = callReturnAddr;

  context->bufferIndex++;
  return context->bufferIndex == BUFFER_SIZE;
}

static int
A_BufferReturn(ADDRINT returnTarget, threadid_t threadid)
{
  ThreadContext *context = Contexts[threadid];
  BufferObject *buffer = &context->buffer[context->bufferIndex];

  buffer->type = BUFFER_TYPE_RETURN;
  buffer->u.callInfo.callReturnAddr = returnTarget;

  context->bufferIndex++;
  return context->bufferIndex == BUFFER_SIZE;
}

static VOID
A_BufferIndirectCall(ADDRINT dstFuncAddr, ADDRINT callReturnAddr,
		     threadid_t threadid)
{
  Function *dstFunc = getFunction(dstFuncAddr);
  
  if( A_BufferCall(dstFunc, callReturnAddr, threadid) ) {
    A_DumpBuffer(threadid);
  }
}

///////////////////////// Thread Callbacks ////////////////////////////////////

static VOID
A_ThreadBegin(UINT32 threadid, VOID *sp, int flags, VOID *v)
{
  MSG("thread begin: pid=%d tid=%d\n", getpid(), threadid);
  
  PIN_LockClient();
  if( threadid > MaxNumContexts ) {
    MaxNumContexts *= 2;
    Contexts =
      (ThreadContext **)realloc(Contexts,
				MaxNumContexts * sizeof(ThreadContext *));
    
    PIN_UnlockClient();
    ASSERTP(Contexts != NULL);
  } else {
    PIN_UnlockClient();
  }

  ASSERTP((Contexts[threadid] = new ThreadContext(threadid)) != NULL);

  Function *dummyFunc;
  PIN_LockClient();
  if( Functions.find(0) == Functions.end() ) {
    dummyFunc = new Function(0, "root", true);
    ASSERTP(dummyFunc != NULL);
    Functions[0] = dummyFunc;
  } else {
    dummyFunc = Functions[0];
  }
  PIN_UnlockClient();
  Activation *a = dummyFunc->getActivation(0, NULL, threadid);
  Contexts[threadid]->pushActivation(a);
}

static VOID
A_ThreadEnd(UINT32 threadid, INT32 code, VOID *v)
{
  MSG("thread execution complete: pid=%d tid=%d\n", getpid(), threadid);
  ThreadContext *context = Contexts[threadid];

  A_DumpBuffer(threadid);
  A_ProcessReturn(0, context, context->topActivation(), threadid);

  context->consolidateStats();

  *context->outfile << "p," << ProgramName << endl;
  printFlatTrace(context->outfile, context->loops, context->funcStats);
  printGraph(context->outfile, context->loops, context->funcStats);
  
  delete context;
  Contexts[threadid] = NULL;

  MSG("thread processing complete: pid=%d tid=%d\n", getpid(), threadid);

  PIN_LockClient();
  map<ADDRINT, Function *>::iterator fi;
  for( fi = Functions.begin(); fi != Functions.end(); fi++ ) {
    fi->second->freeThreadStorage(threadid);
  }
  PIN_UnlockClient();
}

static VOID
Fini(INT32 code, VOID *v)
{
  A_ThreadEnd(0, code, 0);
  
  ofstream *outfile =
    new ofstream((Outdir + "/" + ProgramName + ".total.raw").c_str());
  *outfile << "p," << ProgramName << endl;

  PIN_LockClient();
  printFlatTrace(outfile, Loops, FuncStats);
  printGraph(outfile, Loops, FuncStats);

  // Hopefully no other threads are lying around
  // clean up the mess we've made
  map<ADDRINT, LoopStats *>::iterator li;
  for( li = Loops.begin(); li != Loops.end(); li++ ) {
    if( li->second != NULL ) {
      delete li->second;
    }
  }
  Loops.clear();

  map<ADDRINT, FunctionStats *>::iterator fi;
  for( fi = FuncStats.begin(); fi != FuncStats.end(); fi++ ) {
    if( fi->second != NULL ) {
      delete fi->second;
    }
  }
  FuncStats.clear();

  map<ADDRINT, Function *>::iterator fi2;
  for( fi2 = Functions.begin(); fi2 != Functions.end(); fi2++ ) {
    if( fi2->second != NULL ) {
      delete fi2->second;
    }
  }
  Functions.clear();

  PIN_UnlockClient();
}

///////////////////////// Syscall Callbacks ///////////////////////////////////

static VOID
A_SyscallBefore(int num,
		unsigned long arg0, unsigned long arg1,
		unsigned long arg2, unsigned long arg3,
		unsigned long arg4, unsigned long arg5,
		threadid_t threadid)
{
  if( SYS_execve == num && (VOID *)arg1 != NULL ) {
    if( access((char *)arg0, F_OK) < 0 ) {
      return;
    }
    char *bn = basename((char *)arg0);
    if( IgnorePrograms.find(string(bn)) != IgnorePrograms.end() ) {
      MSG("ignoring program: %s\n", (char *)arg0);
      return;
    }

    vector<string> argvec;
    argvec.push_back(PIN_HOME + "/Bin/pin");
    argvec.push_back("-mt");
    argvec.push_back("-t");
    argvec.push_back(LOOPPROF_HOME + "/Source/loopprof");
    argvec.push_back("-pid");
    argvec.push_back("--");

    char **prog_arg = (char **)arg1;
    while( *prog_arg != NULL ) {
      argvec.push_back(*prog_arg);
      prog_arg++;
    }
    
    const char **args = (const char **)calloc(argvec.size() + 1,
					      sizeof(char *));
    ASSERTP(args != NULL);
    for( size_t i = 0; i < argvec.size(); i++ ) {
      args[i] = argvec[i].c_str();
    }
    args[argvec.size()] = NULL;

    /*
    cerr << "LPEXEC: ";
    const char **argp = args;
    while( *argp != NULL ) {
      cerr << *argp << " ";
      argp++;
    }
    cerr << endl;
    */
    MSG("calling execv for: %s\n", (char *)arg0);
    // TODO: call threadend? A_ThreadEnd(threadid, 0, NULL);
    if( execv(args[0], (char* const*)args) != -1 ) {
      ASSERTP(0);
    }
    ASSERTP(0);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Instrumentation Functions ///////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////// Dynamic Instrumentation /////////////////////////////

static VOID
I_CallTrace(TRACE trace, INS tail)
{
  if( INS_IsProcedureCall(tail) && INS_IsDirectBranchOrCall(tail) ) {
    // direct call
    ADDRINT target = INS_DirectBranchOrCallTargetAddress(tail);
    ADDRINT callReturnAddr = INS_NextAddress(tail);
      
    Function *dstFunc = getFunction(target);
      
    INS_InsertIfCall(tail, IPOINT_BEFORE,
		     (AFUNPTR)A_BufferCall,
		     IARG_ADDRINT, dstFunc,
		     IARG_ADDRINT, callReturnAddr,
		     IARG_THREAD_ID,
		     IARG_END);
    INS_InsertThenCall(tail, IPOINT_BEFORE,
		       (AFUNPTR)A_DumpBuffer,
		       IARG_THREAD_ID,
		       IARG_END);
  } else if( INS_IsRet(tail) ) {
    // return
    INS_InsertIfCall(tail, IPOINT_BEFORE,
		     (AFUNPTR)A_BufferReturn,
		     IARG_BRANCH_TARGET_ADDR,
		     IARG_THREAD_ID,
		     IARG_END);
    INS_InsertThenCall(tail, IPOINT_BEFORE,
		       (AFUNPTR)A_DumpBuffer,
		       IARG_THREAD_ID,
		       IARG_END);
  } else if( INS_IsProcedureCall(tail) 
	     && INS_IsIndirectBranchOrCall(tail) ) {
    // indirect call
    INS_InsertCall(tail, IPOINT_BEFORE,
		   (AFUNPTR)A_BufferIndirectCall,
		   IARG_BRANCH_TARGET_ADDR,
		   IARG_ADDRINT, INS_NextAddress(tail),
		   IARG_THREAD_ID,
		   IARG_END);
  }
}

static VOID
I_Syscall(INS ins)
{
  if( INS_IsSyscall(ins) ) {
    INS_InsertCall(ins, IPOINT_BEFORE,
		   (AFUNPTR)A_SyscallBefore,
		   IARG_SYSCALL_NUMBER,
		   IARG_SYSCALL_ARG0,
		   IARG_SYSCALL_ARG1,
		   IARG_SYSCALL_ARG2,
		   IARG_SYSCALL_ARG3,
		   IARG_SYSCALL_ARG4,
		   IARG_SYSCALL_ARG5,
		   IARG_THREAD_ID,
		   IARG_END);
  }
}

static VOID
I_SyscallTrace(TRACE trace, VOID *v)
{
  for( BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl) ) {
    INS tail = BBL_InsTail(bbl);
    I_Syscall(tail);
  }
}

static VOID
I_Trace(TRACE trace, VOID *v)
{
  if( !Instrument ) {
    I_SyscallTrace(trace, v);
    return;
  }

  char type = BUFFER_TYPE_PATH_BBL;
  bool inMainImg = true;
  ADDRINT srcRtnAddr = TRACE_Address(trace);

  RTN srcRtn = TRACE_Rtn(trace);
  if( RTN_Valid(srcRtn) ) {
    srcRtnAddr = RTN_Address(srcRtn);    
    IMG_TYPE imgType = IMG_Type(SEC_Img(RTN_Sec(srcRtn)));
    inMainImg = imgType == IMG_TYPE_SHARED || imgType == IMG_TYPE_STATIC;
  }

  if( !inMainImg && !TraceSharedLibs ) {
    type = BUFFER_TYPE_LIB_BBL;
  }

  Function *srcFunc = getFunction(srcRtnAddr);

  for( BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl) ) {
    INS tail = BBL_InsTail(bbl);
    I_Syscall(tail);
    
    if( type == BUFFER_TYPE_LIB_BBL ) {
      BBL_InsertIfCall(bbl, IPOINT_BEFORE,
		       (AFUNPTR)A_BufferSharedLibBbl,
		       IARG_UINT32, BBL_NumIns(bbl),
		       IARG_THREAD_ID,
		       IARG_END);
      BBL_InsertThenCall(bbl, IPOINT_BEFORE,
			 (AFUNPTR)A_DumpBuffer,
			 IARG_THREAD_ID,
			 IARG_END);
      I_CallTrace(trace, tail);
      continue;
    }


    ADDRINT headAddr = BBL_Address(bbl);
    ADDRINT tailAddr = INS_Address(tail);
    
    map<ADDRINT, DBbl> &DBblMap = srcFunc->DBblMap;
    map<ADDRINT, set<ADDRINT> > &DBblSplits = srcFunc->DBblSplits;
    
    if( DBblMap.find(tailAddr) == DBblMap.end() ) {
      DBblMap[tailAddr] = DBbl(headAddr, tailAddr, BBL_NumIns(bbl));
      DBbl &dbbl = DBblMap[tailAddr];
      set<ADDRINT> &splits = DBblSplits[tailAddr];
      dbbl.addBbls(bbl, splits);
      DBblSplits.erase(tailAddr);
    } else if( headAddr < DBblMap[tailAddr].head ) {
      DBblMap[tailAddr].insertBbl(bbl);
    }
    
    vector<Bbl>::iterator bi;
    DBbl &dbbl = DBblMap[tailAddr];
    for( bi = dbbl.bbls.begin(); bi != dbbl.bbls.end(); bi++ ) {
      if( headAddr > bi->addr ) {continue;}
      
      UINT32 index = srcFunc->bblIndex(bi->addr);
      BBL_InsertIfCall(bbl, IPOINT_BEFORE,
		       (AFUNPTR)A_BufferRegularBbl,
		       IARG_ADDRINT, bi->addr,
		       IARG_UINT32, bi->numIns,
		       IARG_UINT32, index,
		       IARG_THREAD_ID,
		       IARG_END);

      BBL_InsertThenCall(bbl, IPOINT_BEFORE,
			 (AFUNPTR)A_DumpBuffer,
			 IARG_THREAD_ID,
			 IARG_END);
    }

    I_CallTrace(trace, tail);
  } // for each BBL
}

///////////////////////// AOTI Instrumentation ////////////////////////////////

static VOID
I_Image(IMG img, void *v)
{
  if( !Instrument ) {return;}
  static bool mainImgInstrumented = false;
  if( !TraceSharedLibs and mainImgInstrumented ) {return;}

  if( !mainImgInstrumented ) {
    // Pin doesn't give good symbols for things in the .plt section.  So
    // we objdump that section and make our own symtab.  We don't to
    // objdump the whole thing because it takes a while on larger
    // programs...
    //
    // We do this here because for some programs (i.e. scripts) the program
    // name is meaningless but the img that gets loaded is the shell
    InitPltSymTab(IMG_Name(img));
  
  }

  mainImgInstrumented = true;
  
  MSG("processing image: %s\n", IMG_Name(img).c_str());
  if( OnlyExecPrograms.find(string(basename(IMG_Name(img).c_str())))
      != OnlyExecPrograms.end() ) {
    MSG("not profiling: %s\n", ProgramName.c_str());
    Instrument = false;
    return;
  }

  for( SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    for( RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
      RTN_Open(rtn);
      
      ADDRINT rtnAddr = RTN_Address(rtn);
      Function *func = getFunction(rtnAddr);

      set<ADDRINT> targets;

      for( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) ) {
	// for each instruction in this rtn
	// first pass: find all direct branch targets
	if( INS_IsDirectBranchOrCall(ins) && !INS_IsCall(ins) ) {
	  targets.insert(INS_DirectBranchOrCallTargetAddress(ins));
	}
      }
      
      set<ADDRINT> targets_list;
      
      map<ADDRINT, set<ADDRINT> > &DBblSplits = func->DBblSplits;
      
      for( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) ) {
	// for each instruction in this rtn
	// second pass: find DBbls split
	if( targets.find(INS_Address(ins)) != targets.end() ) {
	  targets_list.insert(INS_Address(ins));
	}

	if( INS_IsDirectBranchOrCall(ins) && !INS_IsCall(ins)
	    && targets_list.size() != 0) {
	  DBblSplits[INS_Address(ins)] = targets_list;
	  targets_list.clear();
	}
      }
      
      RTN_Close(rtn);
    } // for each RTN
  } // for each SEC

  MSG("loaded image: %s\n", IMG_Name(img).c_str());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////// main ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static INT32
Usage()
{
  cerr << "pin -t loopprof -- program arguments" << endl;
  cerr << KNOB_BASE::StringKnobSummary();
  cerr << endl;

  return EXIT_FAILURE;
}

int
main(int pin_argc, char **pin_argv, char **envp)
{
  int argc = pin_argc - 1;
  char **argv = pin_argv + 1;
  
  if( PIN_Init(pin_argc, pin_argv) ) {
    return Usage();
  }

  // Get the instrumented program's args
  while( argc > 0 && strcmp(*(argv - 1), "--") != 0 ) {
    argv++;
    argc--;
  }
  ARGC = argc;
  ARGV = argv;
  ENVP = envp;

  ProgramName = string(basename(ARGV[0]));
  
  string args;
  for( int i = 0; i < argc; i++ ) {
    args += string(argv[i]) + " ";
  }
  MSG("program: %s\n", args.c_str());


  // Check for PIN_HOME and LOOPPROF_HOME env variables
  char *s;
  FAIL((s = getenv("PIN_HOME")) != NULL,
       "%s: set the environment variable PIN_HOME\n", TOOLNAME);
  PIN_HOME = string(s);

  FAIL((s = getenv("LOOPPROF_HOME")) != NULL,
       "%s: set the environment variable LOOPPROF_HOME\n", TOOLNAME);
  LOOPPROF_HOME = string(s);

  // Set programs to ignore (TODO: make config file)
  IgnorePrograms.insert("awk");
  IgnorePrograms.insert("basename");
  IgnorePrograms.insert("cat");
  IgnorePrograms.insert("consoletype");
  IgnorePrograms.insert("cut");
  IgnorePrograms.insert("dircolors");
  IgnorePrograms.insert("dirname");
  IgnorePrograms.insert("echo");
  IgnorePrograms.insert("expr");
  IgnorePrograms.insert("grep");
  IgnorePrograms.insert("id");
  IgnorePrograms.insert("ps");
  IgnorePrograms.insert("sed");
  IgnorePrograms.insert("stty");
  IgnorePrograms.insert("test");
  IgnorePrograms.insert("tput");
  IgnorePrograms.insert("tr");
  IgnorePrograms.insert("uname");

  // Set programs to only instrument for exec syscalls (TODO: make config file)
  OnlyExecPrograms.insert("bash");
  OnlyExecPrograms.insert("csh");
  OnlyExecPrograms.insert("exec");
  OnlyExecPrograms.insert("gmake");
  OnlyExecPrograms.insert("make");
  OnlyExecPrograms.insert("perl");
  OnlyExecPrograms.insert("sh");

  // Test to see if we should profile this program
  if( OnlyExecPrograms.find(ProgramName) != OnlyExecPrograms.end() ) {
    MSG("not profiling: %s\n", ProgramName.c_str());
    Instrument = false;
    TRACE_AddInstrumentFunction(I_SyscallTrace, 0);
    PIN_StartProgram();
  }
  
  if( IgnorePrograms.find(ProgramName) != IgnorePrograms.end() ) {
    MSG("detaching from: %s\n", ProgramName.c_str());
    PIN_Detach();
    PIN_StartProgram();
  }

  // Store Knobs locally
  TraceSharedLibs = KnobTraceSharedLibs.Value();
  AppendPid = KnobAppendPid.Value();
  Mode = KnobMode.Value();

  // Set up output directory
  ostringstream outdir;
  outdir << ProgramName << ".loopprof";
  if( AppendPid ) {
    outdir << "." << getpid();
  }
  Outdir = outdir.str();
  MSG("setting output directory: %s\n", Outdir.c_str());
  if( mkdir(Outdir.c_str(), 0777) != 0 ) {
    perror("error creating output directory");
    exit(EXIT_FAILURE);
  }
  
  // Set function pointers to do correct "mode" of loop detection
  if( Mode == "path" ) {
    A_DumpBuffer = LP_PATH::A_DumpBuffer;
    A_ProcessReturn = LP_PATH::A_ProcessReturn;
  } else if( Mode == "backedge" ) {

  } else if( Mode == "cfg" ) {

  } else {
    FAIL(0, "%s: unknown mode: %s\n", TOOLNAME, Mode.c_str());
  }

  // Initialize the symbol table
  PIN_InitSymbols();

  // Allocate thread contexts
  Contexts = (ThreadContext**)malloc(MaxNumContexts * sizeof(ThreadContext *));
  ASSERTP(Contexts != NULL);

  // Initialize "main" thread
  if( Instrument ) {
    A_ThreadBegin(0,0,0,0);
  }

  // Add instrumentation
  PIN_AddThreadBeginFunction(A_ThreadBegin, 0);
  PIN_AddThreadEndFunction(A_ThreadEnd, 0);
  IMG_AddInstrumentFunction(I_Image, 0);
  TRACE_AddInstrumentFunction(I_Trace, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Start program
  MSG("starting program: %s\n", ProgramName.c_str());
  PIN_StartProgram();
  
  // Should never reach here
  return EXIT_SUCCESS;
}

// So gcc will inline better, we include a .C file.
#include "loopprof_path.C"
