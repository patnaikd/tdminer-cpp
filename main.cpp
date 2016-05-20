#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <string>
#include <cstring>
#include <map>
#include <list>

#include "types.h"
#include "global.h"
#include "kernel.h"
#include "getopt.h"

using namespace std;

#define ALLOCATION_SIZE (1024)
#define ALLOCATION_INCR (1024*4)

UINT numCandidates;
UINT numCandidatesBuffer;
UBYTE* h_events;                  UINT h_event_size = ALLOCATION_SIZE;
FLOAT* h_start_times = NULL;
FLOAT* h_end_times = NULL;
UBYTE* h_cid = NULL;

UBYTE* h_episodeCandidates;       UINT h_episodeCandidates_size = ALLOCATION_SIZE;
UBYTE* h_episodeCandidatesBuffer; UINT h_episodeCandidatesBuffer_size = ALLOCATION_SIZE;

FLOAT* h_episodeIntervals;        UINT h_episodeIntervals_size = ALLOCATION_SIZE;
FLOAT* h_episodeIntervalsBuffer;  UINT h_episodeIntervalsBuffer_size = ALLOCATION_SIZE;

UINT* h_episodeSupport;           UINT h_episodeSupport_size = ALLOCATION_SIZE;

UINT eventSize;
FILE* dumpFile;
FLOAT* temporalConstraint;
UINT temporalConstraintSize;
UINT symbolSize = 0;

double min_supp = 0.01;
int num_threads = 1;
int time_flag = 0;
int max_level = -1;
int hflag = 1;
int cid_flag = 0;
int duration_flag = 0;
char ivl_file_name[256] = "ivl.txt";
double support;
UINT max_timestamp_per_level = 50;

int is_counting = 0;
char eps_file_name[256] = "episodes.txt";
FILE* f_occurrences = NULL;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

map<string, UINT> event2int;
map<UINT, string> int2event;
//UINT event_index = 0;
UINT getSymbol(char* sym)
{
	UINT symbol;
	if (event2int.find(string(sym)) == event2int.end())
	{
		symbol = symbolSize;
		event2int[string(sym)] = symbolSize;
		int2event[symbolSize] = string(sym);
		symbolSize ++;
	}
	else
	{
		symbol = event2int[string(sym)];
	}
	return symbol;
}

string getEvent(UINT symbol)
{
	return int2event[symbol];
}


////////////////////////////////////////////////////////////////////////////////
// declaration, forward
void resize_buffer(UINT level)
{
	if (level == 0) return;
	//printf("%d <= %d\n", h_episodeCandidatesBuffer_size, (numCandidatesBuffer+1)*level);
	if (h_episodeCandidatesBuffer_size <= (numCandidatesBuffer+1)*level)
	{
		//printf("Realloc: prev_size = %d required = %d (%d) ", 
		//	h_episodeCandidatesBuffer_size,
		//	(numCandidatesBuffer+1)*level, numCandidatesBuffer);fflush(stdout);
		while(h_episodeCandidatesBuffer_size <= (numCandidatesBuffer+1)*level)
			h_episodeCandidatesBuffer_size += ALLOCATION_INCR * level;
		//printf(" new_size = %d\n", h_episodeCandidatesBuffer_size);fflush(stdout);
		h_episodeCandidatesBuffer = (UBYTE*)realloc(h_episodeCandidatesBuffer, 
			h_episodeCandidatesBuffer_size * sizeof(UBYTE));
		if (h_episodeCandidatesBuffer == NULL)
		{
			printf("Realloc failed\n");fflush(stdout);
			exit(1);
		}
	}
	//printf("%d <= %d\n", h_episodeIntervalsBuffer_size, (numCandidatesBuffer+1)*(level-1)*2);
	if (h_episodeIntervalsBuffer_size <= (numCandidatesBuffer+1)*(level-1)*2)
	{
		//printf("Realloc: prev_size(ivl) = %d required = %d (%d) ", 
		//	h_episodeIntervalsBuffer_size,
		//	(numCandidatesBuffer+1)*(level-1)*2, numCandidatesBuffer);fflush(stdout);
		while(h_episodeIntervalsBuffer_size <= (numCandidatesBuffer+1)*(level-1)*2)
			h_episodeIntervalsBuffer_size += ALLOCATION_INCR * 2 * (level-1);
		//printf("new_size(ivl) = %d\n", h_episodeIntervalsBuffer_size);fflush(stdout);
		h_episodeIntervalsBuffer = (FLOAT*)realloc(h_episodeIntervalsBuffer,
			h_episodeIntervalsBuffer_size * sizeof(FLOAT));
		if (h_episodeIntervalsBuffer == NULL)
		{
			printf("Realloc failed\n");fflush(stdout);
			exit(1);
		}
	}
}

void roll_buffer(void)
{
	UBYTE* temp_episodeCandidates = h_episodeCandidates;
	h_episodeCandidates = h_episodeCandidatesBuffer;
	h_episodeCandidatesBuffer = temp_episodeCandidates;

	UINT temp_episodeCandidates_size = h_episodeCandidates_size;
	h_episodeCandidates_size = h_episodeCandidatesBuffer_size;
	h_episodeCandidatesBuffer_size = temp_episodeCandidates_size;

	FLOAT* temp_episodeIntervals = h_episodeIntervals;
	h_episodeIntervals = h_episodeIntervalsBuffer;
	h_episodeIntervalsBuffer = temp_episodeIntervals;

	UINT temp__episodeIntervals_size = h_episodeIntervals_size;
	h_episodeIntervals_size = h_episodeIntervalsBuffer_size;
	h_episodeIntervalsBuffer_size = temp__episodeIntervals_size;

	numCandidates = numCandidatesBuffer;
	numCandidatesBuffer = 0;
	if (h_episodeSupport_size <= numCandidates)
	{
		while(h_episodeSupport_size <= numCandidates)
			h_episodeSupport_size += ALLOCATION_INCR;
		h_episodeSupport = (UINT*)realloc(h_episodeSupport,
			h_episodeSupport_size * sizeof(UINT));
		if (h_episodeSupport == NULL)
		{
			printf("Realloc failed\n");fflush(stdout);
			exit(1);
		}
		//memset(h_episodeSupport, 0, h_episodeSupport_size * sizeof(UINT));
	}
}

void initEpisodeCandidates()
{
	map<UINT,UINT> events_map;
	UINT level = 1;

	for(UINT i = 0; i < eventSize; i++)
	{
		UINT event_i = h_events[i];
		events_map[event_i]++;
	}
	if (time_flag == 1)
		support = min_supp * (h_start_times[eventSize - 1] - h_start_times[0]);
	else
		support = min_supp * eventSize;
	printf("Support count = %.2f\n", support);

	list<UINT> event_types;
	for (map<UINT,UINT>::iterator it_map = events_map.begin(); it_map != events_map.end(); it_map++)
		event_types.push_back((*it_map).first);
	event_types.sort();
	numCandidatesBuffer = 0;
	// show content:
	for (list<UINT>::iterator it = event_types.begin(); it != event_types.end(); it++)
	{
		UINT event_i = (*it);
		//printf("Event type = %d\n", event_i);
		resize_buffer(level);
		h_episodeCandidatesBuffer[numCandidatesBuffer] = event_i;
		numCandidatesBuffer ++;
	}
	roll_buffer();

	for(UINT i = 0; i < numCandidates; i++)
	{
		h_episodeSupport[i] = events_map[h_episodeCandidates[i]];
		//printf("%d : %d\n", h_episodeCandidates[i], h_episodeSupport[i]);
	}
}

int getNextValidCandidate(int level, UINT currentIdx, UINT nextIdx)
{
	int prefixLength = level - 2;//level-1 is known
	//nextIdx++;
	for (UINT idx = nextIdx+1; idx < numCandidates; idx++)
	{
		if (currentIdx == idx) continue;
		// The condition below restricts candidates to no-repeated events
		//if (h_episodeSupport[idx] >= support 
		//	&& h_episodeCandidates[currentIdx*(prefixLength+1)+0] != 
		//		h_episodeCandidates[idx*(prefixLength+1)+prefixLength])
		if (h_episodeSupport[idx] >= support)
		{
			bool intervalMatch = true;
			for(UINT intIdx = 0; intIdx < prefixLength; intIdx++)
			{
				if (h_episodeCandidates[currentIdx*(prefixLength+1)+(intIdx+1)] != 
					h_episodeCandidates[idx*(prefixLength+1)+intIdx])
				{
					intervalMatch = false;
					break;
				}
				if (intIdx < prefixLength-1 &&
					(h_episodeIntervals[currentIdx*prefixLength*2+(intIdx+1)*2+0] != 
						h_episodeIntervals[idx*prefixLength*2+(intIdx)*2+0] ||
					h_episodeIntervals[currentIdx*prefixLength*2+(intIdx+1)*2+1] != 
						h_episodeIntervals[idx*prefixLength*2+intIdx*2+1]))
				{
					intervalMatch = false;
					break;
				}
			}
			if (intervalMatch) return idx;
		}
	}
	return -1;
}

void generateEpisodeCandidatesCPU(UINT level)
{
	numCandidatesBuffer = 0;
	if (level == 1) initEpisodeCandidates();
	else if (level == 2)
	{
		for (UINT candidateIdx1 = 0; candidateIdx1 < numCandidates; candidateIdx1++)
		{
			//printf("candidateIdx1 = %d\n", candidateIdx1);fflush(stdout);
			if (h_episodeSupport[candidateIdx1]<support) continue;
			for (UINT candidateIdx2 = 0; candidateIdx2 < numCandidates; candidateIdx2++)
			{
				//printf("candidateIdx2 = %d\n", candidateIdx2);fflush(stdout);
				if (h_episodeSupport[candidateIdx2] < support) continue;
				// The condition below restricts candidates to no-repeated events
				if (candidateIdx1 == candidateIdx2) continue;
				for (UINT idx = 0; idx < temporalConstraintSize; idx++)
				{
					resize_buffer(level);
					h_episodeCandidatesBuffer[numCandidatesBuffer*level+0] = h_episodeCandidates[candidateIdx1];
					h_episodeCandidatesBuffer[numCandidatesBuffer*level+1] = h_episodeCandidates[candidateIdx2];
					h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2+0] = temporalConstraint[idx*2+0];
					h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2+1] = temporalConstraint[idx*2+1];
					//printf("%d candidate %d [%f-%f] %d\n", numCandidatesBuffer,
					//	h_episodeCandidates[candidateIdx1], 
					//	temporalConstraint[idx*2+0],
					//	temporalConstraint[idx*2+1],
					//	h_episodeCandidates[candidateIdx2]);
					//fflush(stdout);
					numCandidatesBuffer++;
				}
			}
		}
		roll_buffer();
	}
	else
	{
		//printf("Generating %d-size candidates\n", level);
		for (UINT candidateIdx = 0; candidateIdx < numCandidates; candidateIdx++)
		{
			if (numCandidates == 0) break;
			if (h_episodeSupport[candidateIdx]<support) continue;

			int nextCandidateIdx = 0;
			while((nextCandidateIdx = getNextValidCandidate(level, candidateIdx, nextCandidateIdx)) != -1)
			{
				//printf("nextCandidateIdx = %d\n", nextCandidateIdx);
				resize_buffer(level);
				memcpy(&h_episodeCandidatesBuffer[numCandidatesBuffer*level],
					&h_episodeCandidates[candidateIdx*(level-1)],
					(level-1)*sizeof(UBYTE));
				h_episodeCandidatesBuffer[numCandidatesBuffer*level + (level-1)] = 
					h_episodeCandidates[nextCandidateIdx*(level-1) + (level-2)];

				memcpy(&h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2],
					&h_episodeIntervals[candidateIdx*(level-2)*2],
					(level-1)*2*sizeof(FLOAT) );
				memcpy(&h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2+(level-2)*2],
					&h_episodeIntervals[nextCandidateIdx*(level-2)*2+(level-3)*2],
					2*sizeof(FLOAT));
				numCandidatesBuffer++;
			}
		}
		roll_buffer();
	}
}

int cullCandidates(UINT level)
{
	int retValue = 0;
	numCandidatesBuffer = 0;
	for ( UINT candidateIdx = 0; candidateIdx < numCandidates; candidateIdx++ )
	{
		if (h_episodeSupport[candidateIdx] < support) continue;
		resize_buffer(level);
		memcpy(&h_episodeCandidatesBuffer[numCandidatesBuffer*level],
			&h_episodeCandidates[candidateIdx*level],
			level*sizeof(UBYTE));
		memcpy(&h_episodeIntervalsBuffer[numCandidatesBuffer*(level-1)*2],
			&h_episodeIntervals[candidateIdx*(level-1)*2],
			(level-1)*2*sizeof(FLOAT));
		h_episodeSupport[numCandidatesBuffer] = h_episodeSupport[candidateIdx];
		if (candidateIdx < numCandidatesBuffer)
		{
			printf(__LOC__"Something wrong here\n");
			exit(1);
		}
		numCandidatesBuffer++;
	}
	retValue = numCandidates - numCandidatesBuffer;
	roll_buffer();
	return retValue;
}

void saveResult(UINT level)
{
	fprintf( dumpFile, "-----------------------\nEpisodes of size = %d\n-----------------------\n", level );

	UINT newcount = 0;
	for (UINT idx = 0; idx < numCandidates; idx++)
	{
		//if (h_episodeSupport[idx] >= support)
		{
			newcount++;
			for (UINT levelIdx = 0; levelIdx < level; levelIdx++)
			{
				if (levelIdx > 0)
				{
					fprintf(dumpFile, "-[%f,%f]-", 
						h_episodeIntervals[idx*(level-1)*2+(levelIdx-1)*2+0], 
						h_episodeIntervals[idx*(level-1)*2+(levelIdx-1)*2+1]);
				}
				//fprintf(dumpFile, "%d", h_episodeCandidates[idx*level+levelIdx]);
				fprintf(dumpFile, "%s", getEvent(h_episodeCandidates[idx*level+levelIdx]).c_str());
			}
			fprintf( dumpFile, " : %d\n", h_episodeSupport[idx]);
		}
	}
	fprintf(dumpFile, "No. of %d node frequent episodes = %d\n\n", level, newcount);
}

UINT countLinesInFile(char* filename)
{
	int ch, prev = '\n', lines = 0;
	FILE* file;
	
	file = fopen(filename, "r");
	if (file)
	{
		while ((ch = fgetc(file)) != EOF) /* Read all chars in the file. */
		{
			if (ch == '\n') ++lines;
			prev = ch;
		}
		fclose(file);
		if (prev != '\n') ++lines;
	}
	else
	{
		printf("Unable to read file %s\n", filename);
		exit(1);
	}
	return lines;
}


void loadData(char* filename)
{
	FILE* eventFile;
	eventSize = 0;
	eventFile =  fopen(filename, "r");
	if (eventFile)
	{
		h_events = (UBYTE*)malloc(h_event_size * sizeof(UBYTE));
		h_start_times = (FLOAT*)malloc(h_event_size * sizeof(FLOAT));
		if (cid_flag == 1) h_cid = (UBYTE*)malloc(h_event_size * sizeof(UBYTE));
		if (duration_flag == 1) h_end_times = (FLOAT*)malloc(h_event_size * sizeof(FLOAT));

		char buf[1024];
		while (fgets(buf, sizeof(buf), eventFile) != NULL)
		{
			if (h_event_size <= eventSize)
			{
				while(h_event_size <= eventSize) h_event_size += 1024;
				h_events = (UBYTE*)realloc(h_events, h_event_size * sizeof(UBYTE));
				h_start_times = (FLOAT*)realloc(h_start_times, h_event_size * sizeof(FLOAT));
				if (duration_flag == 1) h_end_times = (FLOAT*)realloc(h_end_times, h_event_size * sizeof(FLOAT));
				if (cid_flag == 1) h_cid = (UBYTE*)realloc(h_cid, h_event_size * sizeof(UBYTE));
			}
			char *sym = NULL;
			if (cid_flag == 1)
			{
				h_cid[eventSize] = atoi(strtok(buf, ","));
				sym = strtok(NULL, ",");
			}
			else 
			{
				sym = strtok(buf, ",");
			}

			h_events[eventSize] = getSymbol(sym);
			h_start_times[eventSize] = atof(strtok(NULL, ","));
			if (duration_flag == 1) h_end_times[eventSize] = atof(strtok(NULL, ","));
			eventSize ++;
		}
		fclose(eventFile);
		printf("Total Number of events = %d\n", eventSize);
		printf("Time span of the data = %f\n", h_start_times[eventSize-1] - h_start_times[0]);
	}
	else
	{
		printf("Unable to read file %s\n", filename);
		exit(1);
	}
}

void loadTemporalConstraints(char* filename)
{
	FILE* temporalFile;
	char buffer[256] = "";

	temporalConstraintSize = countLinesInFile(filename);
	//printf("temporalConstraintSize = %d\n", temporalConstraintSize);
	temporalFile = fopen(filename, "r");
	if (temporalFile)
	{
		temporalConstraint = (FLOAT*)malloc(temporalConstraintSize * 2 * sizeof(FLOAT));
		for (UINT idx = 0; idx < temporalConstraintSize; idx++)
		{
			float startIvl, endIvl;
			fgets(buffer, 256, temporalFile);
			//printf("buffer = %s\n", buffer);
			sscanf(buffer, "%f %f", &startIvl, &endIvl);
			temporalConstraint[2*idx+0] = startIvl;
			temporalConstraint[2*idx+1] = endIvl;
			printf("Interval %d : %f - %f\n", idx+1, startIvl, endIvl);
		}
		fclose(temporalFile);
	}
	else
	{
		printf("Unable to read file %s\n", filename);
		exit(1);
	}
}

void setupCpu()
{
	h_episodeCandidates = (UBYTE*)malloc(h_episodeCandidates_size * sizeof(UBYTE));
	h_episodeCandidatesBuffer = (UBYTE*)malloc(h_episodeCandidates_size * sizeof(UBYTE));
	h_episodeIntervals = (FLOAT*)malloc(h_episodeIntervals_size * sizeof(FLOAT));
	h_episodeIntervalsBuffer = (FLOAT*)malloc(h_episodeIntervalsBuffer_size * sizeof(FLOAT));
	h_episodeSupport = (UINT*)malloc(h_episodeSupport_size * sizeof(UINT));
}

void cleanup()
{
	//printf("Cleaning up memory...\n");
	free( h_events );
	free( h_start_times );
	free( h_end_times );
	free( h_episodeSupport );
	free( h_episodeCandidates );
	free( h_episodeCandidatesBuffer );
	fclose(dumpFile);
	if (f_occurrences != NULL) fclose(f_occurrences);
}

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
void usage(void);
int main(int argc, char** argv) 
{
	int ch;
	while ((ch = getopt(argc, argv, "ht:i:m:sxcdz:o:f:")) != -1)
	{
		switch (ch) 
		{
			 case 'h' : usage();exit(1);
			 case 't' : num_threads = atoi(optarg); break;
			 case 'i' : strncpy(ivl_file_name, optarg, 255); break;
			 case 'm' : max_level = atoi(optarg); break;
			 case 's' : time_flag = 1; break;
			 case 'x' : hflag = 0; break;
			 case 'c' : cid_flag = 1; break;
			 case 'd' : duration_flag = 1; hflag = 0; break;
			 case 'z' : max_timestamp_per_level = atoi(optarg); break;
			 case 'o' : is_counting = 1; strncpy(eps_file_name, optarg, 255); break;
			 case 'f' : f_occurrences = fopen(optarg, "w"); break;
			 default  : printf("Option not available. Type -h or --help for help.\n"); exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
	{
		printf("\nThere are 2 mandatory arguments.");
		printf("\nType -h for more options.\n");
		exit(1);
	}
	
	printf("Input file: %s\n", argv[0]);
	printf("Output file: %s\n", argv[1]);


	setupCpu();	
	loadData(argv[0]);
	dumpFile = fopen(argv[1], "w");

	printf("Initializing...\n");
	printf("Number of threads = %d\n", num_threads);
	printf("Event stream size: %i\n", eventSize);

	if (!is_counting)
	{
		if (argc > 2)
			min_supp = atof(argv[2]);
		else
			min_supp = 0.01;

		printf("Support = %f\n", min_supp);
		printf("Use time span = %d\n", time_flag);
		printf("Maximum level = %d\n", max_level);
		printf("Turn on heuristics = %d\n", hflag);
		printf("Intervals file: %s\n", ivl_file_name);
		loadTemporalConstraints(ivl_file_name);

		// BEGIN LOOP
		UINT episodesCulled = 0;
		UINT level = 1;
		generateEpisodeCandidatesCPU(level);
		cullCandidates(level);
		saveResult(level);
		fflush(dumpFile);
		level ++;

		while(1)
		{
			if (max_level != -1 && level > max_level) break;
			printf("Generating episode candidates for level %d...\n", level);fflush(stdout);
			generateEpisodeCandidatesCPU(level);
			//saveResult(level);
			if (numCandidates == 0) break;

			printf("Executing kernel on %d candidates...\n", numCandidates);
			if (hflag == 0)
			{
				countCandidates(level);
				episodesCulled = 0;
			}
			else
			{
				countCandidatesStatic(level);
				episodesCulled = cullCandidates(level);
				if (numCandidates == 0) break;
				countCandidates(level);
			}
			//saveResult(level);
			cullCandidates(level);
			saveResult(level);
			fflush(dumpFile);

			// Print Statistics for this run
			printf("level, support, #candidates, #heuristic-culling\n");
			printf("%d, %.2f, %d, %d\n", level, support, numCandidates, episodesCulled);
			level ++;
		}
	}
	else
	{
		// Load candidates
		UINT level = read_candidates(eps_file_name);
		printf("level, #candidates\n");
		printf("%d, %d\n", level, numCandidates);
		// Count the candidates
		genericThreadLaunch(level, 0, find_occurrences_Threaded);
		// Save to file
		saveResult(level);
		fflush(dumpFile);
	}
	cleanup();
	pthread_exit(NULL);
}


void usage()
{
	printf("\nUsage: tdminer [ht:i:m:sxcdz:o:f:] eventsfile outcomefile [min_supp]\n");
	printf("\n eventsfile\t    file, that contains the event stream");
	printf("\n outcomefile\t    file to write the outcome");
	printf("\n min_supp\t    support threshold (default %.4f)", min_supp);

	printf("\n\nBasic options:");
	printf("\n -h\t\t\tGives this help display.");
	printf("\n -t <num>\t\tSpecifies the number of pthreads (default 1).");
	printf("\n -i <intervalfile>\tSpecifies the interval file (default ivl.txt).");
	printf("\n\t\t\tSee example ivl.txt for format");
	printf("\n -m <num>\t\tSpecifies the maximum level up to which mining is done.");
	printf("\n\t\t\t(default -1 i.e. no limit). Level corresponds to episode size.");
	printf("\n -s\t\t\tSpecifies minimum support as a ratio of #occurrences to total time span.");
	printf("\n\t\t\tBy default minimum support is the ratio of #occurrences");
	printf("\n\t\t\tto total number of events in the data.");
	printf("\n -x\t\t\tTurn off pre-count pruning heuristics.");
	printf("\n -c\t\t\tTakes the first column as customer id");
	printf("\n\t\t\tand prevents patterns from crossing over across customers..");
	printf("\n -d\t\t\tEnables duration of events. Reads the last column as end time of an event");
	printf("\n -z <num>\t\tSet the list size for storing time stamps.");
	printf("\n -o <epsfile>\t\tCounts the episodes specified in <epsfile>.");
	printf("\n\t\t\tThe result of counting is stored in <outcomefile>.");
	printf("\n -f <occurfile>\t\tSpecifies the file into which episode occurrences are written.");
	printf("\n\t\t\tThis works only in conjunction with -o");
	printf("\n\t\t\tThe occurrences are output as <episode_index> : <list of time stamps>.");

	printf("\n\nMining Example:");
	printf("\n$ tdminer sample_stream.csv test_episodes.txt 0.01");

	printf("\n\nCounting example:");
	printf("\n$ tdminer -o eps.txt -f occurrences.txt sample_stream.csv episodes-out.txt");
	printf("\n\"eps.txt\" contains the episodes to count. Note currently only episode of \nthe same size are supported.");
	printf("\n\nDependencies: PThreads library");

	printf("\n\nDebprakash Patnaik (debprakash@gmail.com)\n");
}

