#ifndef _LOOPPROF_H_
#define _LOOPPROF_H_

#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <set>

#ifndef TOOLNAME
#define TOOLNAME "loopprof"
#endif

#define BUFFER_SIZE (2<<10)
#define ITERATIONS_HISTO_MAX 10

///////////////////////// Macro Functions /////////////////////////////////////

#define MSG_ON 1
#define MSG(fmt, ...) if( MSG_ON ) fprintf(stderr, "---" TOOLNAME "(%5d)--- "\
                                           fmt, getpid(), __VA_ARGS__);

#define ASSERTP(c) \
do { \
  if( !(c) ) { \
    char buf[4096]; \
    snprintf(buf, 4096, "%s:%s:%d", __FILE__, __FUNCTION__, __LINE__); \
    perror(buf); \
    exit(EXIT_FAILURE); \
  } \
}  while( 0 )

#define ASSERTS(c, s, ...) \
do { \
  if( !(c) ) { \
    char buf[4096]; \
    snprintf(buf, 4096, "%s:%s:%d: %s",__FILE__,__FUNCTION__,__LINE__, s); \
    fprintf(stderr, buf, __VA_ARGS__); \
    exit(EXIT_FAILURE); \
  } \
}  while( 0 )

#define FAIL(c, s, ...) \
do { \
  if( !(c) ) { \
    fprintf(stderr, s, __VA_ARGS__); \
    exit(EXIT_FAILURE); \
  } \
} while( 0 )

#define likely(c) __builtin_expect((c), 1)
#define unlikely(c) __builtin_expect((c), 0)

///////////////////////// Data Declarations ///////////////////////////////////

class Activation;
class Bbl;
class BblPathInfo;
class Function;
class LoopStats;
class FunctionStats;

typedef BblPathInfo ** Path;
typedef UINT32 threadid_t;

///////////////////////////////////////////////////////////////////////////////
///////////////////////// Class Definitions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////// Class BufferObject //////////////////////////////////


/*
 * Program events are buffered, and each event is packed into a
 * generic "BufferObject"
 */

#define BUFFER_TYPE_PATH_BBL    0
#define BUFFER_TYPE_NONPATH_BBL 1
#define BUFFER_TYPE_CALL        2
#define BUFFER_TYPE_RETURN      3
#define BUFFER_TYPE_LIB_BBL     4

typedef struct {
  ADDRINT addr;
  UINT32 index;
  UINT32 numIns;
} BufferBbl;

typedef struct {
  Function *dstFunc;
  ADDRINT callReturnAddr;
} BufferCallInfo;

typedef struct BufferObject
{
  char type; 
  union {
    BufferBbl bbl;
    BufferCallInfo callInfo;
  } u;
} BufferObject;

///////////////////////// Class BblPathInfo ///////////////////////////////////

/*
 * A BblPathInfo object contains accounting information for Bbls on
 * the path.  Each Activation has its own set of BblPathInfo objects.
 * Since there may be multiple activations of the same function on the
 * call stack, the same Bbl may be on the global path multiple time.
 * Thus the need for a local Bbl object.
 */
class BblPathInfo
{
public:
  BblPathInfo(ADDRINT _addr, UINT32 _numIns) :
    count(0),
    selfIns(0),
    totalIns(0),
    numIns(_numIns),
    iterations(0),
    loopStats(NULL),
    loopHead(NULL),
    childFunctions(NULL),
    addr(_addr)
  {}

  ~BblPathInfo()
  {
    if( childFunctions != NULL ) {
      delete childFunctions;
      childFunctions = NULL;
    }
  }

  /*
   * BblPathInfo::operator +=
   * 
   * This serves to consolidate Bbl edges when the program is finished.
   */
  VOID operator +=(BblPathInfo &bpi)
  {
    if( loopStats == NULL ) {
      loopStats = bpi.loopStats;
    }

    // Take the closest loopHead
    if( loopHead == NULL ) {
      loopHead = bpi.loopHead;
    } else if( bpi.loopHead != NULL ) {
      if( loopHead->addr < addr
	  && bpi.loopHead->addr < addr
	  && bpi.loopHead->addr > loopHead->addr ) {
	loopHead = bpi.loopHead;
      }
    }

    // Union childFunctions
    if( bpi.childFunctions != NULL ) {
      if( childFunctions == NULL ) {
	childFunctions = new set<ADDRINT>();
	ASSERTP(childFunctions != NULL);
      }
      set<ADDRINT>::iterator fi;
      for( fi = bpi.childFunctions->begin();
	   fi != bpi.childFunctions->end(); 
	   fi++ ) {
	childFunctions->insert(*fi);
      }
    }
  }

  VOID addChildFunction(ADDRINT c)
  {
    if( unlikely(childFunctions == NULL) ) {
      childFunctions = new set<ADDRINT>();
      ASSERTP(childFunctions != NULL);
    }

    childFunctions->insert(c);
  }

  UINT32 count;
  UINT64 selfIns;
  UINT64 totalIns;
  UINT32 numIns;

  UINT32 iterations;

  LoopStats *loopStats;
  BblPathInfo *loopHead;

  set<ADDRINT> *childFunctions;
  ADDRINT addr;
};

///////////////////////// Class LoopStats /////////////////////////////////////

class LoopStats
{
public:
  LoopStats() :
    selfIns(0),
    totalIns(0),
    iterations(0)
  {
    for( int i = 0; i < ITERATIONS_HISTO_MAX + 2; i++ ) {
      iterationsHisto[i] = 0;
    }
  }

  /*
   * LoopStats::operator +=
   *
   * When a thread ends, this operator serves to combine statistics
   * and edges of a local copy of a LoopStats with a global one.
   */
  VOID operator +=(const LoopStats &ls)
  {
    selfIns += ls.selfIns;
    totalIns += ls.totalIns;
    iterations += ls.iterations;

    for( int i = 0; i < ITERATIONS_HISTO_MAX + 2; i++ ) {
      iterationsHisto[i] += ls.iterationsHisto[i];
    }

    set<ADDRINT>::iterator li;
    for( li = ls.childLoops.begin(); li != ls.childLoops.end(); li++ ) {
      childLoops.insert(*li);
    }
    set<ADDRINT>::iterator fi;
    for( fi = ls.childFunctions.begin(); fi != ls.childFunctions.end(); fi++ ){
      childFunctions.insert(*fi);
    }  
  }

  UINT64 selfIns;
  UINT64 totalIns;
  UINT64 iterations;

  // An array to store histogram information for the number of entries
  // per number of iterations
  UINT64 iterationsHisto[ITERATIONS_HISTO_MAX + 2];

  set<ADDRINT> childLoops;
  set<ADDRINT> childFunctions;
};

///////////////////////// Class ThreadContext /////////////////////////////////

class ThreadContext
{
public:
  ///// Public Constructors and Destructors/////

  ThreadContext(threadid_t _threadid);

  ~ThreadContext();

  ///// Public Variables /////

  BufferObject buffer[BUFFER_SIZE];
  UINT32 bufferIndex;
  
  // the call stack
  Activation **activations;
  INT32 maxNumActivations;
  INT32 activationIndex;

  const threadid_t threadid;

  map<ADDRINT, LoopStats *> loops;
  map<ADDRINT, FunctionStats *> funcStats;

  ofstream *outfile;

  ///// Public Methods /////

  VOID pushActivation(Activation *a);
  Activation *topActivation();
  VOID popActivation();
  VOID consolidateStats();
};

///////////////////////// Class Bbl ///////////////////////////////////////////

// a static Bbl
class Bbl
{
public:
  Bbl(ADDRINT _addr, UINT32 _numIns) :
    addr(_addr),
    numIns(_numIns)
  {}
  
  Bbl() :
    addr(0),
    numIns(0)
  {}

  ADDRINT addr;
  UINT32 numIns;
};

///////////////////////// Class DBbl //////////////////////////////////////////

// a list of Bbls.  This is necessary since Pin gives us dynamic Bbls
// which could be jumped into.
class DBbl
{
public:
  DBbl(ADDRINT _head, ADDRINT _tail, UINT32 _numIns) :
    head(_head),
    tail(_tail),
    numIns(_numIns)
  {}

  DBbl() :
    head(0),
    tail(0),
    numIns(0)    
  {}

  VOID addBbls(BBL bbl, set<ADDRINT> &splits);
  VOID insertBbl(BBL bbl);

  ADDRINT head;
  ADDRINT tail;
  UINT32 numIns;

  vector<Bbl> bbls;

};

///////////////////////// Class FunctionStats /////////////////////////////////

class FunctionStats
{
public:
  FunctionStats() :
    calls(0),
    selfIns(0),
    totalIns(0)
  {}

  VOID operator+=(const FunctionStats &fs)
  {
    calls += fs.calls;
    selfIns += fs.selfIns;
    totalIns += fs.totalIns;
    
    set<ADDRINT>::iterator li;
    for( li = fs.childLoops.begin(); li != fs.childLoops.end(); li++ ) {
      childLoops.insert(*li);
    }
    set<ADDRINT>::iterator fi;
    for( fi = fs.childFunctions.begin(); fi != fs.childFunctions.end(); fi++ ){
      childFunctions.insert(*fi);
    }
  }
  
  UINT64 calls;
  UINT64 selfIns;
  UINT64 totalIns;

  set<ADDRINT> childLoops;
  set<ADDRINT> childFunctions;
};

///////////////////////// Class Function //////////////////////////////////////

// Each function keeps a cache of "activations" for the call stack
class Function
{
public:
  Function(ADDRINT _addr, string _name, bool _inMainImg);
  ~Function();

  const ADDRINT addr;
  const bool inMainImg;

  const string name;

  UINT32 maxNumBbls;
  map<ADDRINT, UINT32> bblIndices;

  // Unprotected, assume instrumentation code is non-reentrant
  map<ADDRINT, DBbl> DBblMap;
  map<ADDRINT, set<ADDRINT> > DBblSplits;

  int bblIndex(ADDRINT bblHead);

  VOID lock(VOID);
  VOID unlock(VOID);
  VOID freeActivation(Activation *a, threadid_t threadid);
  Activation *getActivation(ADDRINT callReturnAddr, Activation *parent,
			    threadid_t threadid);
  VOID consolidateThreadStats(ThreadContext *);
  VOID freeThreadStorage(threadid_t);
  
private:
  size_t numThreads;
  vector<Activation *> **ActivationPool;
  PIN_LOCK lockvar;
};

///////////////////////// Class Activation ////////////////////////////////////

// Activations keep a cache of BblPathInfo objects so they don't have
// to be reallocated each time an activation is pushed.
class Activation
{
public:
  Activation(Function *_function) :
    function(_function),
    path(NULL),
    pathSize(0),
    maxPathSize(32),
    bpis(NULL),
    maxNumBpis(0),
    tmpSelfIns(0),
    tmpTotalIns(0),
    sumSelfIns(0),
    sumTotalIns(0),
    calls(0)
  {
    if( _function->maxNumBbls == 0 ) {
      _function->maxNumBbls = 4;
    }
    maxNumBpis = _function->maxNumBbls;
    
    bpis = (BblPathInfo **)calloc(maxNumBpis, sizeof(BblPathInfo *));
    ASSERTP(bpis != NULL);

    path = (Path)malloc(maxPathSize * sizeof(BblPathInfo *));
    ASSERTP(path != NULL);

    // Make a dummy bpi for the front of the path
    bpis[0] = new BblPathInfo(0, 0);
    ASSERTP(bpis[0] != NULL);
  }

  ~Activation()
  {
    for( UINT32 i = 0; i < maxNumBpis; i++ ) {
      if( bpis[i] != NULL ) {
	delete bpis[i];
	bpis[i] = NULL;
      }
    }
    
    delete bpis;
    bpis = NULL;
    delete path;
    path = NULL;
  }

  VOID
  clear(void)
  {
    pathSize = 0;
    libInsCount = 0;
    sumSelfIns += tmpSelfIns;
    sumTotalIns += tmpTotalIns;
    tmpSelfIns = 0;
    tmpTotalIns = 0;
  }

  BblPathInfo *
  getBpi(UINT32 index)
  {
    if( unlikely(index >= maxNumBpis) ) {
      
      UINT32 oldMax = maxNumBpis;
      while( index >= maxNumBpis ) {
	maxNumBpis *= 2;
      }

      BblPathInfo **bpis2 = (BblPathInfo **)calloc(maxNumBpis ,
						   sizeof(BblPathInfo *));
      ASSERTP(bpis2 != NULL);
      memcpy(bpis2, bpis, oldMax * sizeof(BblPathInfo *));
      free(bpis);
      bpis = bpis2;
      function->lock();
      if( function->maxNumBbls < maxNumBpis ) {
	function->maxNumBbls = maxNumBpis;
      }
      function->unlock();
    }

    return bpis[index];
  }

  VOID
  addToPath(BblPathInfo *bpi)
  {
    if( unlikely(pathSize == maxPathSize) ) {
      maxPathSize *= 2;
      path = (Path)realloc(path, maxPathSize * sizeof(BblPathInfo *));
      ASSERTP(path != NULL);
    }
    path[pathSize++] = bpi;
  }

  ADDRINT callReturnAddr;
  Function *function;
  UINT64 libInsCount;

  Path path;
  UINT32 pathSize;
  UINT32 maxPathSize;

  BblPathInfo **bpis;
  UINT32 maxNumBpis;
  UINT64 tmpSelfIns;
  UINT64 tmpTotalIns;
  UINT64 sumSelfIns;
  UINT64 sumTotalIns;
  UINT64 calls;
  Activation *parent;
};

///////////////////////// Namespaces //////////////////////////////////////////

namespace LP_PATH {
  static inline VOID A_DumpBuffer(threadid_t);
  static inline VOID A_ProcessReturn(ADDRINT, ThreadContext *, Activation *,
				     threadid_t threadid);
  int AnalysisThread(void *v);
} // namespace LP_PATH

#endif
