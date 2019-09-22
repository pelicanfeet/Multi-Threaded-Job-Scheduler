/* Programming Assignment #5 - Multi-Threaded Job Scheduler
** Operating Systems - Fall 2018
** Author: Sean McTiernan
** Date: 10-24-2018
** File name: asgn5-mctiernans3.c
** Compile: cc asgn5-mctiernans3.c -o asgn5
** Run: ./asgn5
**
** This program simulates a multi-threaded job scheduler. It allows the user to
** actively add jobs to the list, remove jobs from the list, and enumerate the list.
** The list will display the Jobs in ascending order based upon either their 
** start times or, in the case of identical start times, their submission times.
** Removal of a job is based upon its index. The index of the list is zero-based.
** The expected input format for adding/removing a Job is as follows:
** Adding a job:
** add [startTime] [number of parameters] [name] [parameter1] [parameter2] ... [parameter4]
** Removing a job:
** rm [index]
** The program can only terminate by the user pressing Ctrl+C (killing the program).
** This program has three threads: the dispatcher, the scheduler, and the executer threads.
** The dispatcher thread determines when the jobs scheduled by the scheduler thread are 
** executed, and the executer thread actually executes these jobs. All of these threads are
** concurrent (running in parallel).
**
** Known flaws: None at this time
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

/* Each job will be represented as a "node" in a linked list.
*/
typedef struct JOB {
   int startTime;       // domain = { positive integer }
   int numOfParams;     // domain = { positive integer }
   char job[5][25];     // domain = { [a-zA-Z][a-zA-Z0-9_\.]* }
   int submissionTime;  // domain = { positive integer }
   struct JOB *next;
   int index;
} Job;

/* This function is responsible for pointing to the head
** of the linked the list.
*/
typedef struct HEADER {
	Job *headPtr;
	int count;
} Header;

// DECLARATIONS (in order)
void insert(Header *headerPtr, Job *nodePtr);
Job* Remove(Header *headerPtr, int Index);
void *dispatcher(void *ptr);
void *scheduler(void *ptr);
void *execute(void *ptr);
void printList(Header *headerPtr);
void initHeader(Header *headerPtr);
void updateIndices(Header *headerPtr);
void printJob(Job *node);
int delay();
void newLine(int i);

int main() {
	pthread_t thread1, thread2;
	int iret1, iret2;
	
	Header header;
	Header *headerPtr = &header;
	initHeader(headerPtr);
	
	iret1 = pthread_create(&thread1, NULL, scheduler, (void *) headerPtr);
	iret2 = pthread_create(&thread2, NULL, dispatcher, (void *) headerPtr);
	
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	exit(0);
}

/* This function will insert the Job passed into the list headed by the Job pointed
** to by 'headerPtr' in ascending order based upon its 'startTime' attribute.
*/
void insert(Header *headerPtr, Job *nodePtr) {
	Job *currentNode; // A POINTER to the current node
	currentNode = headerPtr->headPtr;
	Job *previousNode; // A POINTER to the previous node
	// The case of the list currently being empty
	if (headerPtr->count == 0) {
		headerPtr->headPtr = nodePtr;
	}
	// The case of 'nodePtr' being the new header
	else if (currentNode->startTime > nodePtr->startTime) {
		nodePtr->next = currentNode;
		headerPtr->headPtr = nodePtr;
	}
	else { // Loop through the list until we find its proper location or until
		   // we reach the end of the list
		while((currentNode->startTime < nodePtr->startTime) && (currentNode->next != NULL)) {
			previousNode = currentNode;
			currentNode = currentNode->next;
		}  // The case of inserting the node somewhere in the middle of the list
		if (currentNode->startTime > nodePtr->startTime) {
			previousNode->next = nodePtr;
			nodePtr->next = currentNode;
		}
		else if (currentNode->startTime == nodePtr->startTime) { // The case of multiple nodes having the same start time
			while ((currentNode->startTime == nodePtr->startTime) && (currentNode->next != NULL)) {
				previousNode = currentNode;
				currentNode = currentNode->next;
			}
			if (currentNode->startTime == nodePtr->startTime && currentNode->next == NULL) {
				currentNode->next = nodePtr;
				nodePtr->next = NULL;
			}
			else {
				previousNode->next = nodePtr;
				nodePtr->next = currentNode;
			}
		}
		else { // The case of appending the node to the end of the list
			currentNode->next = nodePtr;
			nodePtr->next = NULL;
		}
	}
	headerPtr->count = headerPtr->count + 1;
	printf("ADD: ");
	printJob(nodePtr);
	printf("\n\n");
}

/* This function removes the specified Node from the linked list.
*/
Job* Remove(Header *headerPtr, int Index) {
	if (headerPtr->count != 0) { // A check to see if we have any jobs in the list
		int maxIndex = headerPtr->count - 1;
		Job *temp;
		Job *currentNode;
		currentNode = headerPtr->headPtr;
		Job *previousNode;
		if (Index > maxIndex || Index < 0) { // Index out of bounds
			printf("Invalid index\n");
			return NULL;
		}
		else {
			if (currentNode->index == Index) { // The case of removing the header
				headerPtr->headPtr = currentNode->next;
				temp = currentNode;
				currentNode = NULL;
			}
			else {
				while (currentNode->index != Index) { // Locate the node to be removed
					previousNode = currentNode;
					temp = currentNode;
					currentNode = currentNode->next;
				}
				if (currentNode->next != NULL) { 			// The case of removing a node
					previousNode->next = currentNode->next; // somewhere between the header
					temp = currentNode;						// and the tail (or last node)
					currentNode = NULL;
				}
				else { // The case of removing the node at end of list
					previousNode->next = NULL;
					temp = currentNode;
					currentNode = NULL;
				}
			}
		}
		printf("DELETE(%d): ", Index);
		printJob(temp);
		newLine(1);
		headerPtr->count = headerPtr->count - 1;
		return temp;
	}
	else {
		return NULL;
	}
}

/* This thread function is responsible for determining which job
** should be started and when it should be started.
*/
void *dispatcher(void *ptr) {
	pthread_t thread3; // For the execution thread
	time_t app_start_time = time(NULL);
	time_t app_current_time = 0;
	Header *headerPtr = (Header *) ptr;
	Job *temp;
	int keepGoing = 1;
	
	while(keepGoing) { // Intentional infinite loop
		if (headerPtr->count != 0 && (headerPtr->headPtr->startTime <= app_current_time)) {
			temp = Remove(headerPtr, 0);
			updateIndices(headerPtr);
			pthread_create(&thread3, NULL, execute, (void *) temp);
		}
		else {
			delay();
		}
		app_current_time = time(NULL) - app_start_time;
	}
	return NULL;
}

/* This thread function is responsible for scheduling jobs (i.e. adding jobs and 
** setting their start times).
*/
void *scheduler(void *ptr) {
	char add[4] = "add";
	char list[5] = "list";
	char rm[3] = "rm";
	char command[5];
	int keepGoing = 1;
	int rmIndex;
	int i, j;
	time_t currentTime;
	
	Header *headerPtr = (Header *) ptr;
	Job *currentNodePtr = NULL;
	
	scanf("%s", command);
	while(keepGoing) {
		printf("\n");
		if (strcmp(command, add) == 0) { // Adding a job to the list
			currentTime = time(NULL);
			currentNodePtr = (Job *) (malloc(sizeof(Job)));
			scanf("%d%d", &(currentNodePtr->startTime), &(currentNodePtr->numOfParams));
			for(j = 0; j < currentNodePtr->numOfParams; j++) {
				scanf("%s", currentNodePtr->job[j]);
			}
			currentNodePtr->submissionTime = currentTime;
			insert(headerPtr, currentNodePtr);
			updateIndices(headerPtr);
		}
		else if (strcmp(command, list) == 0) { // Enumerating the list
			printList(headerPtr);
			printf("\n");
		}
		else if (strcmp(command, rm) == 0) { // Removing a job from the list
			scanf("%d", &(rmIndex));
			currentNodePtr = headerPtr->headPtr;
			if (rmIndex < 0 || rmIndex >= headerPtr->count) {
				printf("Invalid index!\n\n");
			}
			else {
				for(i = 0; i != rmIndex; i++) {
					currentNodePtr = currentNodePtr->next;
				}
				Remove(headerPtr, rmIndex);
				free(currentNodePtr);
				updateIndices(headerPtr);
				printf("\n");
			}
		}
		scanf("%s", command);
	}
	return NULL;
}

/* This function is responsible for executing the specified job ('node').
*/
void *execute(void *ptr) {
	Job *currentJob = (Job *) ptr;
	int child_pid, i, k;
	int status = 999;
	char *cmdString;
	char *argv[6];
	cmdString = currentJob->job[0];
	argv[0] = currentJob->job[0];
	for(i = 1; i < currentJob->numOfParams; i++) {
		argv[i] = currentJob->job[i];
	}
	argv[currentJob->numOfParams] = NULL;
	child_pid = fork();
	if (child_pid == 0) {
		newLine(1);
		k = execvp(cmdString, argv);
		exit(EXIT_SUCCESS);
	}
	else {
		newLine(1);
		waitpid(child_pid, &status, WEXITED);
	}
	newLine(1);
	printf("Job information: ");
	printJob(currentJob);
	newLine(2);
	free(currentJob);
	return NULL;
}

/* This function will accept a given linked list and print out the contents of each node,
** starting from the header.
*/
void printList(Header *headerPtr) {
   int i;
   int k;
   printf("LIST:\n");
   if (headerPtr->count != 0) {
      // The list is not empty, so we print it.
      Job *tempPtr = headerPtr->headPtr;
      for (i = 0; i < headerPtr->count; i++) {
         printf("%d, %d", tempPtr->startTime, tempPtr->numOfParams);
		 for (k = 0; k < tempPtr->numOfParams; k++) {
			printf(", %s", tempPtr->job[k]);
		 }
		 printf(", %ld\n", (long)tempPtr->submissionTime);
         tempPtr = tempPtr->next;
      }
   }
   else {
      // The list is empty.
      printf("EMPTY\n");
   }
}

/* This function is responsible for initializing the header.
*/
void initHeader(Header *headerPtr) {
   headerPtr->headPtr = NULL;
   headerPtr->count = 0;
}

/* This function is responsible for updating the linked list so that
** all nodes have the correct index value.
*/
void updateIndices(Header *headerPtr) {
	Job* currentNode = headerPtr->headPtr;
	int maxIndex = headerPtr->count - 1;
	int i;
	for(i = 0; i <= maxIndex; i++) {
		currentNode->index = i;
		currentNode = currentNode->next;
	}
}

/* This function prints a node's information.
*/
void printJob(Job *node) {
	int k;
	printf("%d, %d, ", node->startTime, node->numOfParams);
	for (k = 0; k < node->numOfParams; k++) {
		printf("%s, ", node->job[k]);
	}
	printf("%d", node->submissionTime);
}

/* This function will cause a 1 second delay.
*/
int delay() {
	int sec = 1;
	sleep(sec);
	return sec;
}

/* This function prints 'i' new line characters.
*/
void newLine(int i) {
	int j;
	for(j = 0; j<i; j++) {
		printf("\n");
	}
}