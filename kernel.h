#ifndef KERNEL_H_
#define KERNEL_H_

#include "types.h"

extern UBYTE* h_events;
extern FLOAT* h_start_times;
extern FLOAT* h_end_times;
extern UBYTE* h_cid;
extern UBYTE* h_episodeCandidates;
extern UBYTE* h_episodeCandidatesBuffer;
extern FLOAT* h_episodeIntervals;
extern FLOAT* h_episodeIntervalsBuffer;
extern UINT*  h_episodeSupport;

extern int duration_flag;
extern int cid_flag;
extern UINT symbolSize;
extern UINT max_timestamp_per_level;
extern UINT eventSize;
extern UINT numCandidates;
extern int num_threads;
extern double support;

void countCandidates(UINT level);

void countCandidatesStatic(UINT level);

typedef struct ThreadParamStruct
{
	UINT level;
	UINT numCandidates;
	UINT offset;

	UINT** waits;
	UINT* waits_size;
	UINT* waits_max;
	FLOAT* timestamps;
	UINT* timestampSizeAll;
	

} ThreadParam;

// For use in finding inner-most occurrences
void* find_occurrences_Threaded(void *dummyParam);
UINT read_candidates(char *filename);

//private functions
void addToList(FLOAT* values, UINT* sizes, UINT listIdx, FLOAT newValue);
FLOAT getFromList(FLOAT* values, UINT listIdx, int valueIdx);
void clearListsAll(UINT* sizes, const UINT level, const UINT tnumCandidates);
void clearLists(UINT* sizes, const UINT level);
void clearCount(UINT* count, const UINT tnumCandidates);
void initWaits(UINT** waits, UINT* waits_size, UINT* waits_max, 
			   UINT offset, UINT tnumCandidates, UINT level );
void free_waits(UINT** waits);
void allocateResource(UINT tnumCandidates, UINT level, ThreadParam* th, UINT type);
void freeResource(ThreadParam* th);
void genericThreadLaunch(UINT level, UINT type, void* (*f)(void*));
void resetToZero( FLOAT* timestamps, UINT level );
void resetAllToZero( FLOAT* timestamps, UINT level, UINT tnumCandidates );

#endif //_KERNEL_H_

