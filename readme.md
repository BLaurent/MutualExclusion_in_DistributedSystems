
Motivation:				- In a single system processes use locking mechanism to access shared resources aka critical section
						- In Distributed System (DS), resources can be shared between multiple processors (systems) over the network
						- We need mechanism such as the locking mechanism in single system to access shared resources in DS environment 

Goal:					Implement mutual exclusion mechanism for distributed systems
Algorithm:				Ricart-Agrawala mutual exclusion algorithm with Roucairol and Carvalho optimizations

Programming Language:	C++; socket programming; POSIX Thread
Programming Technique:	Structured
Development Platform:	VIM editor, command line, LINUX Server (net machines at UT Dallas)

Compilation:			- make clean
						- make all

System:					- 10 nodes in system
						- Each node enters critical section 40 times
						- For each Critical Section it uses implementation of the algorithm mentioned above
						
Algorithm:				- Each node sends Request to all the other n-1 nodes
						- Node receiving the Request Grants permission if it is not using the critical section or the requesting node has higher priority than itself
						- Otherwise the node receiving the request, defers the request and sends the grant after it has finished it's use of the critical section

TODO:					- code commenting
						
Results:				Node 0: 
(results will vary		- Min Messages = 8
for each execution;		- Max Messages = 18
messages include both	- Total Time = 626
requests and replies)						
						Node 1: 
						- Min Messages = 8
						- Max Messages = 18
						- Total Time = 610
						
						Node 2: 
						- Min Messages = 8
						- Max Messages = 18
						- Total Time = 628
						
						Node 3: 
						- Min Messages = 10
						- Max Messages = 18
						- Total Time = 614
						
						Node 4: 
						- Min Messages = 8
						- Max Messages = 18
						- Total Time = 626
						
						Node 5: 
						- Min Messages = 10
						- Max Messages = 18
						- Total Time = 616
						
						Node 6: 
						- Min Messages = 8
						- Max Messages = 18
						- Total Time = 620
						
						Node 7: 
						- Min Messages = 10
						- Max Messages = 18
						- Total Time = 616
						
						Node 8: 
						- Min Messages = 4
						- Max Messages = 18
						- Total Time = 612
						
						Node 9: 
						- Min Messages = 10
						- Max Messages = 18
						- Total Time = 614

Why There is significant variation in ellapsed time?
Ellapsed time is directly proportional to number of request made. In other words more the request are made, longer it will have to wait to get the replies for all the request
and longer it will have to wait.						
						