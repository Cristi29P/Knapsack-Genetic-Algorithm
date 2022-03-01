#include <stdlib.h>
#include <stdio.h>
#include "genetic_algorithm.h"

int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;

	// number of objects
	int object_count = 0;

	// maximum weight that can be carried in the sack
	int sack_capacity = 0;

	// number of generations
	int generations_count = 0;
	
	// number of threads
	int P = 0;

    // Declare an array of threads
    pthread_t *threads;
    // Declare an array of arguments
    thread_info *arguments;
    // Declare a status pointer (debugging purposes)
    void *status;
    // Allocating the necessary threads and checking for any malloc failures
    threads = (pthread_t*) malloc(P * sizeof(pthread_t));
    if (threads == NULL) {
        fprintf(stderr,"Dynamic allocation of threads array failed!\n");
        return -1;
    }
    // Allocating the necessary arguments and checking for any malloc failures
    arguments = (thread_info *)malloc(P * sizeof(struct thread_info));
    if (arguments == NULL) {
        fprintf(stderr,"Dynamic allocation of arguments array failed!\n");
        return -1;
    }
    // Input parsing
	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, &P, argc, argv)) {
		return 0;
	}

    // Declaration and initialization of the barrier
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, P);

    // Declaration and initialization of generations (we do it here, in main, because we
    // don't want each thread to create its own current and next generation
    individual *current_generation = (individual*) calloc(object_count, sizeof(individual));
    individual *next_generation = (individual*) calloc(object_count, sizeof(individual));
    // Checking if the allocation was successful
    if (current_generation == NULL || next_generation == NULL) {
        fprintf(stderr, "Curr_gen and next_gen allocations failed!\n");
        return -1;
    }
    int r;
    for (int i = 0; i < P; i++) {
        // Passing the arguments to each thread
        arguments[i].thread_id = i;
        arguments[i].P = P;
        arguments[i].barrier = &barrier;
        arguments[i].objects = objects;
        arguments[i].object_count = object_count;
        arguments[i].generations_count = generations_count;
        arguments[i].sack_capacity = sack_capacity;
        arguments[i].current_generation = current_generation;
        arguments[i].next_generation = next_generation;

        r = pthread_create(&threads[i], NULL, run_genetic_algorithm, &arguments[i]);
        if (r) {
            fprintf(stderr, "Error while creating thread %d\n", i);
            exit(-1);
        }
    }

    // Waiting for the threads to finish
    for (int i = 0; i < P; i++) {
        r = pthread_join(threads[i], &status);

        if (r) {
            printf("Error while waiting for thread %d\n", i);
            exit(-1);
        }
    }
    ///////// MEMORY CLEAN-UP ////////////

    // Destroy the barrier upon completion
    pthread_barrier_destroy(&barrier);

    // free resources for old generation
    free_generation(current_generation);
    free_generation(next_generation);

    // free resources
    free(current_generation);
    free(next_generation);

    free(threads);
	free(objects);

	return 0;
}
