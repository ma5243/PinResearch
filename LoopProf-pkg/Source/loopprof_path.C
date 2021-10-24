namespace LP_PATH {

static inline VOID
ProcessLoop(ADDRINT addr, BblPathInfo *headBpi, Path path,
	    Activation *top, ThreadContext *context)
{
  BblPathInfo *loopHead;
  UINT32 activationIns = 0;  // this number can't be that large, so UINT32

  // If loopStats is NULL, find the thread-local LoopStats object for
  // this loop
  if( unlikely(headBpi->loopStats == NULL) ) {
    if( unlikely(context->loops.find(addr) == context->loops.end()) ) {
      LoopStats *ls = new LoopStats();
      ASSERTP(ls != NULL);
      context->loops[addr] = ls;
      headBpi->loopStats = ls;
    } else {
      headBpi->loopStats = context->loops[addr];
    }
  }
  
  headBpi->iterations++;

  int pathIndex = top->pathSize - 1;
  BblPathInfo *curBpi = path[pathIndex];

  // Traverse the path in reverse until we reach the loop head
  while( curBpi != headBpi ) {
    // Update curBpi's self/total instructions and this activations count
    UINT64 c = curBpi->count * curBpi->numIns;
    curBpi->selfIns += c;
    curBpi->totalIns += c;
    activationIns += c;

    if( unlikely(!curBpi->loopHead)
	|| (unlikely(addr < curBpi->loopHead->addr)
	    && addr > curBpi->addr) ) {
      // If the current loop head is closer to curBpi than its
      // loopHead, swap them.
      curBpi->loopHead = headBpi;
    }

    // set loopHead to curbpi->loopHead
    loopHead = curBpi->loopHead;

    if( unlikely(curBpi->loopStats != NULL) ) {
      // if curBpi is a loop head, update the loops stats
      curBpi->loopStats->selfIns += curBpi->selfIns;
      curBpi->loopStats->totalIns += curBpi->totalIns;
      curBpi->loopStats->iterations += curBpi->iterations;

      if( unlikely(curBpi->iterations > ITERATIONS_HISTO_MAX) ) {
	curBpi->loopStats->iterationsHisto[ITERATIONS_HISTO_MAX + 1]++;
      } else {
	curBpi->loopStats->iterationsHisto[curBpi->iterations]++;
      }

      curBpi->iterations = 0;
    }

    // Since curBpi is about to be popped, add its stats to its loopHead.
    loopHead->totalIns += curBpi->totalIns;
    if( likely(!curBpi->loopStats) ) {
      loopHead->selfIns += curBpi->selfIns;
    }

    // Reset curBpi b/c it will be cached and used again.
    curBpi->totalIns = 0;
    curBpi->selfIns = 0;
    curBpi->count = 0;

    curBpi = path[--pathIndex];
  }

  // Pop the bpis on the path below headBpi.
  top->pathSize = pathIndex + 1;
  top->tmpSelfIns += activationIns;

  // Add to the top activations totalIns
  top->tmpTotalIns += activationIns;
}

static inline VOID
PopActivation(ThreadContext *context, threadid_t threadid)
{
  UINT64 selfIns = 0;
  UINT64 totalIns = 0;
  UINT32 activationIns = 0; // this number can't be that large

  Activation *activation = context->topActivation();

  Path &path = activation->path;
  
  int pathIndex = activation->pathSize;

  while( pathIndex-- > 0 ) {
    BblPathInfo *curBpi = path[pathIndex];

    UINT64 c = (UINT64)curBpi->count * curBpi->numIns;
    curBpi->selfIns += c;
    curBpi->totalIns += c;
    activationIns += c;

    if( likely(curBpi->loopStats != NULL) ) {
      curBpi->loopStats->selfIns += curBpi->selfIns;
      curBpi->loopStats->totalIns += curBpi->totalIns;
      curBpi->loopStats->iterations += curBpi->iterations;

      if( unlikely(curBpi->iterations > ITERATIONS_HISTO_MAX) ) {
	curBpi->loopStats->iterationsHisto[ITERATIONS_HISTO_MAX + 1]++;
      } else {
	curBpi->loopStats->iterationsHisto[curBpi->iterations]++;
      }

      curBpi->iterations = 0;    
    }

    if( curBpi->loopHead == NULL ) {
      // TOP LEVEL
      if( curBpi->loopStats == NULL ) {
	selfIns += curBpi->selfIns;
      }
      totalIns += curBpi->totalIns;

    } else {
      // NESTED
      if( !curBpi->loopStats ) {
	curBpi->loopHead->selfIns += curBpi->selfIns;
      }
      curBpi->loopHead->totalIns += curBpi->totalIns;
    }
    curBpi->count = 0;
    curBpi->selfIns = 0;
    curBpi->totalIns = 0;
  }

  activation->tmpSelfIns += activationIns;
  activation->tmpTotalIns += activationIns;

  Activation *parent = activation->parent;
  if( likely(parent != NULL) ) {
    // Propagate instructions for functions up call stack
    parent->tmpTotalIns += activation->tmpTotalIns;
    // Propagate instructions for loops up call stack
    BblPathInfo *pback = parent->path[parent->pathSize - 1];
    pback->selfIns += selfIns;
    pback->totalIns += totalIns;
    // Connect this function in graph
    pback->addChildFunction(activation->function->addr);
  }

  context->popActivation();
}

static inline VOID
A_ProcessBbl(ADDRINT addr, UINT32 numIns, UINT32 index,
	     ThreadContext *context, Activation *top)
{
  BblPathInfo *bpi = top->getBpi(index);
  Path path = top->path;

  if( unlikely(bpi == NULL) ) {
    BblPathInfo *b = new BblPathInfo(addr, numIns);
    ASSERTP(b != NULL);
    top->bpis[index] = b;
    bpi = b;
  }
  
  if( likely(!bpi->count) ) {
    bpi->count++;
    top->addToPath(bpi);
  } else {
    bpi->count++;
    ProcessLoop(addr, bpi, path, top, context);
  }
}

static inline VOID
A_ProcessCall(Function *dstFunc, ADDRINT callReturnAddr,
	      ThreadContext *context, Activation **top, threadid_t threadid)
{
  // Get an activation from the cache
  Activation *a = dstFunc->getActivation(callReturnAddr, *top, threadid);

  // Push it on the stack
  context->pushActivation(a);
  *top = a;
}

static inline VOID
A_ProcessReturn(ADDRINT returnTarget, ThreadContext *context,
		Activation *top, threadid_t threadid)
{
  if( likely(top->callReturnAddr == returnTarget) ) {
    // Pop the top activation
    PopActivation(context, threadid);
  } else {
    // Something strange happened, traverse up the function stack to
    // find the return addr
    INT32 i = context->activationIndex;
    while( i >= 0 ) {
      if( context->activations[i]->callReturnAddr == returnTarget ) {
	while( context->topActivation()->callReturnAddr != returnTarget ) {
	  PopActivation(context, threadid);
	}
	PopActivation(context, threadid);
	break;
      }
      i--;
    }
  }
}

static void
A_DumpBuffer(threadid_t threadid)
{ 
  ThreadContext *context = Contexts[threadid];
  Activation *top = context->topActivation();

  BufferBbl *bbl;
  BufferCallInfo *callInfo;
  for(UINT32 i = 0; i < context->bufferIndex; i++ ) {
    switch(context->buffer[i].type) {
    case BUFFER_TYPE_PATH_BBL:
      do {
	// This is likely to occur in long sequences, so stick a loop
	// around it.
	bbl = &context->buffer[i++].u.bbl;
	A_ProcessBbl(bbl->addr,
		     bbl->numIns,
		     bbl->index,
		     context, top);
	if( i == context->bufferIndex ) {
	  goto out;
	}
      } while( likely(context->buffer[i].type == BUFFER_TYPE_PATH_BBL) );
      i--;
      break;
    case BUFFER_TYPE_CALL:
      callInfo = &context->buffer[i].u.callInfo;
      A_ProcessCall(callInfo->dstFunc,
		    callInfo->callReturnAddr,
		    context,
		    &top, threadid);

      context->buffer[i].type = BUFFER_TYPE_PATH_BBL;
      break;
    case BUFFER_TYPE_RETURN:
      callInfo = &context->buffer[i].u.callInfo;
      LP_PATH::A_ProcessReturn(callInfo->callReturnAddr,
			       context, top, threadid);
      top = context->topActivation();
      context->buffer[i].type = BUFFER_TYPE_PATH_BBL;
      break;
    case BUFFER_TYPE_LIB_BBL:
      UINT64 count = 0;
      do {
	// This is likely to occur in long sequences, so stick a loop
	// around it.
	bbl = &context->buffer[i].u.bbl;
	count += bbl->numIns;
	context->buffer[i].type = BUFFER_TYPE_PATH_BBL;
	if( ++i == context->bufferIndex ) {
	  i++;
	  break;
	}
      } while( likely(context->buffer[i].type == BUFFER_TYPE_LIB_BBL) );
      i--;
      top->tmpSelfIns += count;
      top->tmpTotalIns += count;
      top->path[top->pathSize - 1]->selfIns += count;
      top->path[top->pathSize - 1]->totalIns += count;
      break;
    }
  out:;

  }

  context->bufferIndex = 0;
}

} // namespace LP_PATH
