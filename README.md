#TDMiner C++: Frequent episode mining with inter-event gap constraints. 

```
Usage: tdminer [ht:i:m:sxcdz:o:f:] eventsfile outcomefile [min_supp]

 eventsfile	    file, that contains the event stream
 outcomefile	    file to write the outcome
 min_supp	    support threshold (default 0.0100)

Basic options:
 -h			Gives this help display.
 -t <num>		Specifies the number of pthreads (default 1).
 -i <intervalfile>	Specifies the interval file (default ivl.txt).
			See example ivl.txt for format
 -m <num>		Specifies the maximum level up to which mining is done.
			(default -1 i.e. no limit). Level corresponds to episode size.
 -s			Specifies minimum support as a ratio of #occurrences to total time span.
			By default minimum support is the ratio of #occurrences
			to total number of events in the data.
 -x			Turn off pre-count pruning heuristics.
 -c			Takes the first column as customer id
			and prevents patterns from crossing over across customers..
 -d			Enables duration of events. Reads the last column as end time of an event
 -z <num>		Set the list size for storing time stamps.
 -o <epsfile>		Counts the episodes specified in <epsfile>.
			The result of counting is stored in <outcomefile>.
 -f <occurfile>		Specifies the file into which episode occurrences are written.
			This works only in conjunction with -o
			The occurrences are output as <episode_index> : <list of time stamps>.

Mining Example:
$ tdminer sample_stream.csv test_episodes.txt 0.01

Counting example:
$ tdminer -o eps.txt -f occurrences.txt sample_stream.csv episodes-out.txt
"eps.txt" contains the episodes to count. Note currently only episode of 
the same size are supported.
```

Dependencies: PThreads library

