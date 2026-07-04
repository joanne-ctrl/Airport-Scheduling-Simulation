#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

// runway:ensures mutual exclusion, only one plane can land or take off at a time
sem_t runway;
// taxiway:acts as a queue, limits number of plane waiting after landing
sem_t gates; 
// gates:limits number of planes at gates, only 3 plane can dock at the same time
sem_t taxiway; 

// gate_mutex:protects shared variable(gate_index), prevents race condition during gate assignment
pthread_mutex_t gate_mutex;

int gate_index=0;
int total_planes;
char *gate_names[]={"A","B","C"};
volatile int running=1;

// random sleep helper
int rand_range(int min,int max){
	return rand()%(max-min+1)+min;
}

#define TOWER_COLOR "\033[1;36m"
const char *getColor(int id){
	switch(id){
	case 1: return "\033[1;33m";
	case 2: return "\033[1;35m";
	case 3: return "\033[1;32m";
	case 4: return "\033[1;33m";
	case 5: return "\033[1;35m";
	case 6: return "\033[1;32m";
	case 7: return "\033[1;33m";
	case 8: return "\033[1;35m";
	case 9: return "\033[1;32m";
	case 10: return "\033[1;33m";
	default: return "\033[0m";
	}
}

void *plane(void *arg){
	int id=*(int *)arg;

	const char *color=getColor(id);

	printf("%sPlane-%d: Approaching\033[0m\n",color,id);

	// wait taxiway space, queue control
	sem_wait(&taxiway);
	// wait runway, only one plane can land
	sem_wait(&runway);

	printf("%sTower: Clear to land\033[0m\n",TOWER_COLOR);
	printf("%sPlane-%d: Landing\033[0m\n",color,id);

	sleep(1);
	
	// release runway after landing to allow next plane
	sem_post(&runway);
	// wait for gate, only 3 planes can dock
	sem_wait(&gates);
	
	// critical section: assign gate safely
	pthread_mutex_lock(&gate_mutex);
	char gate='A'+ gate_index;
	gate_index=(gate_index+1)%3;
	pthread_mutex_unlock(&gate_mutex);

	printf("%sPlane-%d: Docked at gate %c\033[0m\n",color,id,gate);
	
	// leave taxiway after docking, free queue slot
	sem_post(&taxiway);
	
	sleep(rand_range(4,5));

	// wait runway for take off, mutual exclusion
	sem_wait(&runway);
	
	printf("%sPlane-%d: Taking off\033[0m\n",color,id);
	
	sleep(1);

	// release runway for next plane
	sem_post(&runway);
	// release gates for next plane
	sem_post(&gates);
	
	pthread_exit(NULL);
}

void *tower(void *arg){
	while(running){
		sleep(2);
	}
	return NULL;
}
void *operator(void *arg){
	while(running){
		sleep(3);
	}
	return NULL;
}

int main(int argc, char *argv[]){
	printf("%sTower: Ready\033[0m\n",TOWER_COLOR);

	if(argc!=2){
		printf("Usage: %s<number_of_planes>\n",argv[0]);
		return 1;
	}
	
	total_planes=atoi(argv[1]);
	if(total_planes<4||total_planes>10){
		printf("Planes must be between 4 and 10\n");
		return 1;
	}
	
	srand(time(NULL));
	
	pthread_t planes[total_planes];
	pthread_t tower_thread,operator_thread;
	int ids[total_planes];

	// binary semaphore
	sem_init(&runway,0,1);
	// queue capacity=3 planes
	sem_init(&gates,0,3);
	// 3 gates available
	sem_init(&taxiway,0,3);

	// initialize the mutex before use for thread synchronization
	pthread_mutex_init(&gate_mutex,NULL);

	// create tower threads
	pthread_create(&tower_thread,NULL,tower,NULL);
	// create operator threads
	pthread_create(&operator_thread,NULL,operator,NULL);

	// create plane threads
	for(int i=0;i<total_planes;i++){
		ids[i]=i+1;
		pthread_create(&planes[i],NULL,plane,&ids[i]);
		sleep(rand_range(2,4));
	}
	
	// wait all planes to finish
	for(int i=0;i<total_planes;i++){
		pthread_join(planes[i],NULL);
	}

	running=0;
	
	pthread_join(tower_thread,NULL);
	printf("Tower thread joined\n");

	pthread_join(operator_thread,NULL);
	printf("Operator thread joined\n");

	printf("All plane threads joined\n");
	printf("Program terminated");

	// release semaphore resource for runway
	sem_destroy(&runway);
	// release semaphore resource for taxiway
	sem_destroy(&taxiway);
	// release semaphore resource for gates
	sem_destroy(&gates);
	// destroy mutex to free system resource
	pthread_mutex_destroy(&gate_mutex);
	
	return 0;
}
