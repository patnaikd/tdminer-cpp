#include "occurrences.h"
#include <pthread.h>

void print_recursive(int symbol_idx, int list_idx, 
	FLOAT* timestampMatrix, UINT* timestampSize, UINT level, UINT eps_index);

void* find_occurrences_Threaded(void *dummyParam)
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
	clearCount(h_episodeSupport + offset, tnumCandidates);
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
						if (symbolIdx == level-1)
						{
							h_episodeSupport[eps_index]++;
							if (f_occurrences != NULL)
							{
								pthread_mutex_lock(&file_lock);
								if (duration_flag)
									fprintf(f_occurrences, "%d : %f,%f\n", eps_index, 
										h_start_times[eventIdx], h_end_times[eventIdx]);
								else
									fprintf(f_occurrences, "%d : %f\n", eps_index, 
										h_start_times[eventIdx]);
								pthread_mutex_unlock(&file_lock);
							}
						}
						else
						{
							addToList(timestampMatrix, timestampSize, symbolIdx, h_start_times[eventIdx]);
							if (duration_flag == 1) 
								addToList(timestampMatrix, timestampSize, symbolIdx, h_end_times[eventIdx]);
						}
					}//(symbolIdx == 0)
					else
					{
						FLOAT lowerBound = h_episodeIntervals[gIntervalIdx + (symbolIdx-1)*2+0];
						FLOAT upperBound = h_episodeIntervals[gIntervalIdx + (symbolIdx-1)*2+1];
						{
							// Check previous for acceptable interval
							int decr = 1;
							if (duration_flag == 1) decr = 2;
							for (int prevIdx = timestampSize[symbolIdx-1]-1; prevIdx >= 0; prevIdx -= decr)
							{
								FLOAT distance = h_start_times[eventIdx] - getFromList(timestampMatrix, symbolIdx-1, prevIdx);
								if (distance > lowerBound && distance <= upperBound)
								{
									if (symbolIdx == level-1)
									{
										h_episodeSupport[eps_index]++;
										if (f_occurrences != NULL)
										{
											pthread_mutex_lock(&file_lock);
											fprintf(f_occurrences, "%d : ", eps_index);
											print_recursive(symbolIdx-1, prevIdx, 
												timestampMatrix, timestampSize, level, eps_index);
											if (duration_flag)
												fprintf(f_occurrences, "%f,%f\n", h_start_times[eventIdx], h_end_times[eventIdx]);
											else
												fprintf(f_occurrences, "%f\n", h_start_times[eventIdx]);
											pthread_mutex_unlock(&file_lock);
										}
										clearLists(timestampSize, level);
										breakOuterLoop = true;
									}
									else
									{
										addToList(timestampMatrix, timestampSize, symbolIdx, h_start_times[eventIdx]);
										if (duration_flag == 1) 
											addToList(timestampMatrix, timestampSize, symbolIdx, h_end_times[eventIdx]);
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


void print_recursive(int symbol_idx, int list_idx, 
	FLOAT* timestampMatrix, UINT* timestampSize, UINT level, UINT eps_index)
{
	FLOAT t_start = 0.0, t_end = 0.0;
	if (duration_flag)
	{
		t_end = getFromList(timestampMatrix, symbol_idx, list_idx);
		t_start = getFromList(timestampMatrix, symbol_idx, list_idx - 1);
	}
	else
	{
		t_start = getFromList(timestampMatrix, symbol_idx, list_idx);
	}
	if (symbol_idx > 0)
	{
		UINT gIntervalIdx = 2*(level-1)* eps_index;
		FLOAT lowerBound = h_episodeIntervals[gIntervalIdx + (symbol_idx-1)*2+0];
		FLOAT upperBound = h_episodeIntervals[gIntervalIdx + (symbol_idx-1)*2+1];
		// Check previous for acceptable interval
		int decr = 1;
		if (duration_flag == 1) decr = 2;
		for (int prevIdx = timestampSize[symbol_idx-1]-1; prevIdx >= 0; prevIdx -= decr)
		{
			FLOAT distance = t_start - getFromList(timestampMatrix, symbol_idx-1, prevIdx);
			if (distance > lowerBound && distance <= upperBound)
			{
				print_recursive(symbol_idx-1, prevIdx, 
					timestampMatrix, timestampSize, level, eps_index);
				break;
			}//distance
		}//for prevIdx
	}
	if (duration_flag)
		fprintf(f_occurrences, "%f,%f ", t_start, t_end);
	else
		fprintf(f_occurrences, "%f ", t_start);
}


UINT read_candidates(char *filename)
{
	UINT level = 0, first_flag = 1;
	numCandidatesBuffer = 0;

	FILE *fp = fopen(filename, "r");
	if (fp != NULL)
	{
		char buf[1024];
		while(fgets(buf, sizeof(buf), fp) != NULL)
		{
			printf("Line: %s\n", buf);
			char *colon_pos = strstr(buf, ":");
			//if (colon_pos != NULL)
			{
				if (colon_pos != NULL) *colon_pos = '\0';
				char *ptr = NULL;
				char *sym = strtok(buf, "-[,]");
				sym = strstrip(sym);
				int index = 0;
				//00-[0.004000,0.006000]-01-[0.004000,0.006000]-02-[0.004000,0.006000]-03
				if (sym != NULL)
				{
					resize_buffer(level);
					printf("%s - %d\n", sym, getSymbol(sym));
					h_episodeCandidatesBuffer[numCandidatesBuffer*level+index] = getSymbol(sym);
					index ++;
					while ((ptr = strtok(NULL, "-[,]")) != NULL)
					{
						h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2+2*(index-1)] = atof(ptr);
						h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2+2*(index-1)+1] = atof(strtok(NULL, "-[,]"));
						sym = strtok(NULL, "-[,]");
						sym = strstrip(sym);
						if (sym != NULL)
						{
							printf("%s - %d\n", sym, getSymbol(sym));
							h_episodeCandidatesBuffer[numCandidatesBuffer*level+index] = getSymbol(sym);
						}
						else
						{
							fprintf(stderr, "Unable to read episode:\n%s\n", buf);
							exit(1);
						}
						index ++;
					}
					if (first_flag == 1)
					{
						level = index;
						first_flag = 0;
					}
					else if (index != level)
					{
						fprintf(stderr, "Candidate size mismatch for level=%d (%d).\n", level, index);
						exit(1);
					}
					numCandidatesBuffer++;
				}// if (sym != NULL)
			}// if (strstr(buf, ":") != NULL)
		}//while(fgets(buf, sizeof(buf), fp) != NULL)
		fclose(fp);
		roll_buffer();
	}
	else
	{
		fprintf(stderr, "Unable to read episodes list file.\n");
		exit(1);
	}
	return level;
}



char *strstrip(char *s)
{
        size_t size;
        char *end;

        size = strlen(s);

        if (!size)
                return s;

        end = s + size - 1;
        while (end >= s && isspace(*end))
                end--;
        *(end + 1) = '\0';

        while (*s && isspace(*s))
                s++;

        return s;
}

