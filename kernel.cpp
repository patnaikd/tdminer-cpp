#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "types.h"
#include "global.h"
#include "kernel.h"


void addToList(FLOAT* values, UINT* sizes, UINT listIdx, FLOAT newValue)
{
	if (sizes[listIdx] >= max_timestamp_per_level)
	{
		fprintf(stderr, "Overflow in time stamp array.");
		exit(1);
	}
	values[listIdx*max_timestamp_per_level + sizes[listIdx]] = newValue;
	sizes[listIdx]++;
}

FLOAT getFromList(FLOAT* values, UINT listIdx, int valueIdx)
{
	if (valueIdx < 0)
	{
		fprintf(stderr, "Accessing empty time stamp array");
		exit(1);
	}
	return values[listIdx*max_timestamp_per_level+valueIdx];
}

void clearListsAll(UINT* sizes, const UINT level, const UINT tnumCandidates)
{
	for (UINT idx = 0; idx < level * tnumCandidates; idx++ )
		sizes[idx] = 0;
}

void clearLists(UINT* sizes, const UINT level)
{
	for (UINT idx = 0; idx < level; idx++ )
		sizes[idx] = 0;
}

void clearCount(UINT* count, const UINT tnumCandidates)
{
	for (UINT idx = 0; idx < tnumCandidates; idx++ )
		count[idx] = 0;
}


void initWaits(UINT** waits, UINT* waits_size, UINT* waits_max, 
			   UINT offset, UINT tnumCandidates, UINT level )
{
	for (UINT i = 0; i < symbolSize; i++)
	{
		waits[i] = NULL;
		waits_size[i] = 0;
		waits_max[i] = 0;
	}

	for (UINT idx = offset; idx < offset + tnumCandidates; idx++)
	{
		for(UINT j = 0; j < level; j++)
		{
			UBYTE sym = h_episodeCandidates[idx * level + j];
			if (waits_max[sym] == waits_size[sym])
			{
				waits_max[sym] += 64;
				waits[sym] = (UINT*)realloc(waits[sym], waits_max[sym] * sizeof(UINT));
			}
			waits[sym][waits_size[sym]] = idx;
			waits_size[sym]++;
		}
	}
}




void free_waits(UINT** waits)
{
	for (UINT i = 0; i < symbolSize; i++)
	{
		if (waits[i] != NULL) 
		{
			free(waits[i]);
			waits[i] = NULL;
		}
	}
}


void allocateResource(UINT tnumCandidates, UINT level, ThreadParam* th, UINT type)
{
	th->waits = (UINT **)malloc(symbolSize * sizeof(UINT *));
	th->waits_size = (UINT *)malloc(symbolSize * sizeof(UINT));
	th->waits_max = (UINT *)malloc(symbolSize * sizeof(UINT));

	if (type == 0)
	{
		UINT timestampMatrixSize = level * max_timestamp_per_level;
		th->timestamps = (FLOAT *)malloc(tnumCandidates * timestampMatrixSize * sizeof(FLOAT));
		th->timestampSizeAll = (UINT*)malloc(tnumCandidates * level * sizeof(UINT));
	}
	else
	{
		th->timestamps = (FLOAT *)malloc(tnumCandidates * level * sizeof(FLOAT));
		th->timestampSizeAll = NULL;
	}
}

void freeResource(ThreadParam* th)
{
	free_waits(th->waits);
	free(th->waits);
	free(th->waits_size);
	free(th->waits_max);
	free(th->timestamps);
	if (th->timestampSizeAll != NULL) free(th->timestampSizeAll);
}

void genericThreadLaunch(UINT level, UINT type, void* (*f)(void*))
{
	pthread_t* thread = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
	ThreadParam** thArray = (ThreadParam**)malloc(num_threads * sizeof(ThreadParam*));
	pthread_attr_t attr;
	UINT rc;
	void *status;

	/* Initialize and set thread detached attribute */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	UINT numCand = numCandidates;
	UINT tnumCandidates = numCand/num_threads;
	if (numCand % num_threads != 0) tnumCandidates++;
	UINT toffset = 0;
	UINT thread_count = 0;
	for(UINT t = 0; t < num_threads; t++) 
	{
		//printf("Main: creating thread %ld\n", t);
		if (numCand < tnumCandidates) tnumCandidates = numCand;
		if (tnumCandidates > 0)
		{
			thArray[t] = (ThreadParam*)malloc(sizeof(ThreadParam));
			ThreadParam* th = thArray[t];
			th->level = level;
			th->numCandidates = tnumCandidates;
			th->offset = toffset;
			allocateResource(tnumCandidates, level, th, type);
			rc = pthread_create(&thread[t], &attr, f, th);
			thread_count ++;
		}
		else
			break;

		if (rc) 
		{
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
		toffset += tnumCandidates;
		numCand -= tnumCandidates;
	}

	/* Free attribute and wait for the other threads */
	pthread_attr_destroy(&attr);
	for(UINT t = 0; t < thread_count; t++) 
	{
		rc = pthread_join(thread[t], &status);
		if (rc) 
		{
			printf("ERROR; return code from pthread_join() is %d\n", rc);
			exit(-1);
		}
		//printf("Main: completed join with thread %ld having a status of %ld\n",t,(long)status);
	}

	for(UINT t = 0; t < thread_count; t++) 
	{
		freeResource(thArray[t]);
		free(thArray[t]);
		thArray[t] = NULL;
	}
	//printf("Main: program completed. Exiting.\n");
	free(thArray);
	free(thread);
}


void* countCandidatesThreaded(void *dummyParam)
{
	//printf("DYNAMIC ALGO\n");
	ThreadParam* th = (ThreadParam*)dummyParam;
	UINT level = th->level; 
	UINT tnumCandidates = th->numCandidates;
	UINT offset = th->offset;

	UINT** waits = th->waits;
	UINT* waits_size = th->waits_size;
	UINT* waits_max = th->waits_max;
	FLOAT* timestamps = th->timestamps;
	UINT* timestampSizeAll = th->timestampSizeAll;

	UINT timestampMatrixSize = level * max_timestamp_per_level;

	// Initialize sizes of timestamp arrays
	clearListsAll(timestampSizeAll, level, tnumCandidates);
	clearCount(h_episodeSupport+offset, tnumCandidates);
	initWaits(waits, waits_size, waits_max, offset, tnumCandidates, level);

	for (UINT eventIdx = 0; eventIdx < eventSize; eventIdx++ )
	{
		UBYTE eventSymbol = h_events[eventIdx];
		if (cid_flag == 1 && eventIdx > 0)
		{
			if (h_cid[eventIdx]!= h_cid[eventIdx-1]) 
				clearListsAll(timestampSizeAll, level, tnumCandidates);
		}

		UINT* eps_list = waits[eventSymbol];
		for( UINT eps_idx = 0; eps_idx < waits_size[eventSymbol]; eps_idx++)
		{
			UINT eps_index = eps_list[eps_idx];
			FLOAT* timestampMatrix = &timestamps[timestampMatrixSize * (eps_index - offset)];
			UINT* timestampSize = &timestampSizeAll[level * (eps_index - offset)];
			UINT gCandidateIdx = level * eps_index;
			UINT gIntervalIdx = 2*(level-1)* eps_index;
			bool breakOuterLoop = false;

			// Check other symbols in the episode for matches to the current event
			for (int symbolIdx = level-1; symbolIdx >= 0 && !breakOuterLoop; symbolIdx-- )
			{
				if ( eventSymbol == h_episodeCandidates[gCandidateIdx + symbolIdx] )
				{
					if (symbolIdx == 0)
					{
						if ( symbolIdx == level-1 )
							h_episodeSupport[eps_index]++;
						else
						{
							if (timestampSize[0] > 0 &&
								h_start_times[eventIdx] 
									- getFromList(timestampMatrix, 0, timestampSize[0]-1) 
								> h_episodeIntervals[gIntervalIdx+1])
								timestampSize[0] = 0;

							if (duration_flag == 1)
								addToList(timestampMatrix, timestampSize, symbolIdx, h_end_times[eventIdx]);
							else
								addToList(timestampMatrix, timestampSize, symbolIdx, h_start_times[eventIdx]);
						}
					}
					else if (timestampSize[symbolIdx-1] > 0)
					{
						FLOAT distance = h_start_times[eventIdx] - 
							getFromList(timestampMatrix, symbolIdx-1, timestampSize[symbolIdx-1]-1);
						FLOAT lowerBound = h_episodeIntervals[gIntervalIdx + (symbolIdx-1)*2+0];
						FLOAT upperBound = h_episodeIntervals[gIntervalIdx + (symbolIdx-1)*2+1];

						if ( distance > upperBound )
						{
							// Clear list
							timestampSize[symbolIdx-1] = 0;
						}
						else 
						{
							// Check previous for acceptable interval
							for (int prevIdx = timestampSize[symbolIdx-1]-1; prevIdx >= 0; prevIdx--)
							{
								distance = h_start_times[eventIdx] - getFromList(timestampMatrix, symbolIdx-1, prevIdx);
								if (distance > lowerBound && distance <= upperBound)
								{
									if (symbolIdx == level-1)
									{
										// The final symbol has been found, clear all lists
//#pragma message (__LOC__"### Some thing wrong in usage of offset ###")
										//episodeSupport[eps_index - offset]++;
										h_episodeSupport[eps_index]++;
										clearLists(timestampSize, level);
										breakOuterLoop = true;
									}
									else
									{
										if ( timestampSize[symbolIdx] > 0 &&
											h_start_times[eventIdx] - 
											getFromList(timestampMatrix, symbolIdx, timestampSize[symbolIdx]-1) > 
											h_episodeIntervals[gIntervalIdx + 2*(symbolIdx)+1] )
											timestampSize[symbolIdx] = 0;

										if (duration_flag == 1)
											addToList(timestampMatrix, timestampSize, symbolIdx, h_end_times[eventIdx]);
										else
											addToList(timestampMatrix, timestampSize, symbolIdx, h_start_times[eventIdx]);
									}
									break;
								}
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

void countCandidates(UINT level)
{
	genericThreadLaunch(level, 0, countCandidatesThreaded);
}


void resetToZero( FLOAT* timestamps, UINT level )
{
	for( UINT idx = 0; idx < level; idx++ )
		timestamps[idx] = -1.0;
}

void resetAllToZero( FLOAT* timestamps, UINT level, UINT tnumCandidates )
{
	for( UINT idx = 0; idx < level * tnumCandidates; idx++ )
		timestamps[idx] = -1.0;
}


void* countCandidatesStaticThreaded(void *dummyParam)
{
	//printf("STATIC ALGO\n");
	ThreadParam* th = (ThreadParam*)dummyParam;
	UINT level = th->level; 
	UINT tnumCandidates = th->numCandidates;
	UINT offset = th->offset;

	UINT** waits = th->waits;
	UINT* waits_size = th->waits_size;
	UINT* waits_max = th->waits_max;
	FLOAT* timestamps = th->timestamps;

	clearCount(h_episodeSupport+offset, tnumCandidates);
	initWaits(waits, waits_size, waits_max, offset, tnumCandidates, level);

	// Initialize sizes of timestamp arrays
	resetAllToZero( timestamps, level, tnumCandidates );

	for (UINT eventIdx = 0; eventIdx < eventSize; eventIdx++ )
	{
		UBYTE eventSymbol = h_events[eventIdx];
		if (cid_flag == 1 && eventIdx > 0)
		{
			if (h_cid[eventIdx]!= h_cid[eventIdx-1]) 
				resetAllToZero(timestamps, level, tnumCandidates);
		}

		UINT* eps_list = waits[eventSymbol];
		for( UINT eps_idx = 0; eps_idx < waits_size[eventSymbol]; eps_idx++)
		{
			UINT eps_index = eps_list[eps_idx];
			FLOAT* myTimestamps = &timestamps[(eps_index - offset) * level];
			UINT gCandidateIdx = level * eps_index;
			UINT gIntervalIdx = 2*(level-1) * eps_index;
			// Check other symbols in the episode for matches to the current event
			for (int symbolIdx = level-1; symbolIdx >= 0; symbolIdx--)
			{
				if ( eventSymbol == h_episodeCandidates[gCandidateIdx + symbolIdx] )
				{
					if ( symbolIdx == 0 )
					{
						if ( symbolIdx == level-1 )
							h_episodeSupport[eps_index]++;//1-node episode
						else
						{
							myTimestamps[symbolIdx] = h_start_times[eventIdx];//record for future
						}
					}
					else
					{
						if (myTimestamps[symbolIdx-1] < 0.0) continue;
						FLOAT distance = h_start_times[eventIdx] - myTimestamps[symbolIdx-1];
						FLOAT upperBound = h_episodeIntervals[gIntervalIdx + (symbolIdx-1)*2+1];
						if (distance <= upperBound || symbolIdx == 0)
						{
							if ( symbolIdx == level-1 )
							{
								// The final symbol has been found, clear all lists
								//episodeSupport[eps_index - offset]++;
								h_episodeSupport[eps_index]++;
								resetToZero(myTimestamps, level);
								break;
							}
							else
							{
								myTimestamps[symbolIdx] = h_start_times[eventIdx];
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}

void countCandidatesStatic(UINT level)
{
	genericThreadLaunch(level, 1, countCandidatesStaticThreaded);
}
