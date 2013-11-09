#include "HeapProfiler.h"

#include <Windows.h>
#include <stdio.h>
#include "dbghelp.h"

#include <algorithm>


StackTrace::StackTrace() : hash(0){
	memset(backtrace, 0, sizeof(void*)*backtraceSize);
}

void StackTrace::trace(){
	CaptureStackBackTrace(0, backtraceSize, backtrace, &hash);
}

void StackTrace::print() const {
	HANDLE process = GetCurrentProcess();

	const int MAXSYMBOLNAME = 128 - sizeof(IMAGEHLP_SYMBOL64);
	char symbol64_buf[sizeof(IMAGEHLP_SYMBOL64) + MAXSYMBOLNAME] = {0};
	IMAGEHLP_SYMBOL64 *symbol64 = reinterpret_cast<IMAGEHLP_SYMBOL64*>(symbol64_buf);
	symbol64->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	symbol64->MaxNameLength = MAXSYMBOLNAME - 1;

	printf("Start stack trace print.\n");
	for(int i = 0; i < backtraceSize; ++i){
		if(backtrace[i]){
			if(SymGetSymFromAddr(process, (DWORD64)backtrace[i], 0, symbol64))
				printf("%08x, %s\n", backtrace[i], symbol64->Name);
		}else{
			break;
		}
	}
}

void HeapProfiler::malloc(void *ptr, size_t size, const StackTrace &trace){
	std::lock_guard<std::mutex> lk(mutex);

	if(allocations.find(trace.hash) == allocations.end()){
		allocations[trace.hash].trace = trace;
	}

	allocations[trace.hash].allocations[ptr] = size;
	ptrs[ptr] = trace.hash;
}

void HeapProfiler::free(void *ptr, const StackTrace &trace){
	std::lock_guard<std::mutex> lk(mutex);
	auto it = ptrs.find(ptr);
	if(it != ptrs.end()){
		StackHash stackHash = it->second;
		allocations[stackHash].allocations.erase(ptr); 
		ptrs.erase(it);
	}else{
		// Do anything with wild pointer frees?
	}
}

void HeapProfiler::getAllocationSiteReport(std::vector<std::pair<StackTrace, size_t>> &allocs){
	std::lock_guard<std::mutex> lk(mutex);
	allocs.clear();

	// For each allocation 
	for(auto &traceInfo : allocations){
		size_t sumOfAlloced = 0;
		for(auto &alloc : traceInfo.second.allocations)
			sumOfAlloced += alloc.second;

		allocs.push_back(std::make_pair(traceInfo.second.trace, sumOfAlloced));
	}
}