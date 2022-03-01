#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm.h"

int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count, int *P, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 4) {
		fprintf(stderr, "Usage:\n\t./tema1_par in_file generations_count P\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

    // We also read the number of needed processors
    *P = (int) strtol(argv[3], NULL, 10);

    if (*P == 0) {
        return 0;
    }

	return 1;
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(int start, int end, const sack_object *objects, individual *generation, int sack_capacity)
{
    int chromosome_count;
    int weight;
	int profit;

	for (int i = start; i < end; ++i) {
        chromosome_count = 0;
        weight = 0;
        profit = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
                ++chromosome_count;
                weight += objects[j].weight;
				profit += objects[j].profit;
			}
		}
        generation[i].chromosome_count = chromosome_count;
		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		res = first->chromosome_count - second->chromosome_count; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index; // decreasing by index
		}
	}
	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void* run_genetic_algorithm(void *arg)
{
    thread_info* data = (thread_info *)arg;
	individual *tmp = NULL;

    int count, cursor;

	// set initial generation (composed of object_count individuals with a single item in the sack)
    int start = data->thread_id * (double)data->object_count / data->P;
    int end = MIN((data->thread_id + 1) * (double)data->object_count / data->P, data->object_count);

	for (int i = start; i != end; ++i) {
		data->current_generation[i].fitness = 0;
		data->current_generation[i].chromosomes = (int*) calloc(data->object_count, sizeof(int));
		data->current_generation[i].chromosomes[i] = 1;
		data->current_generation[i].index = i;
		data->current_generation[i].chromosome_length = data->object_count;

		data->next_generation[i].fitness = 0;
		data->next_generation[i].chromosomes = (int*) calloc(data->object_count, sizeof(int));
		data->next_generation[i].index = i;
		data->next_generation[i].chromosome_length = data->object_count;

    }

    // we make sure no thread starts the iteration unless all the other threads have finished
    // the container initialization step
    pthread_barrier_wait(data->barrier);

	// iterate for each generation
	for (int k = 0; k < data->generations_count; ++k) {
        // compute fitness and sort by it
		compute_fitness_function(start, end, data->objects, data->current_generation, data->sack_capacity);
        // we make sure we don't start the sort the array unless all the threads finished computing the fitness
        // of their individuals
         pthread_barrier_wait(data->barrier);
        // only one thread is allowed to sort the array with qsort to avoid race condition
        if (data->thread_id == 0) {
            qsort(data->current_generation, data->object_count, sizeof(individual), cmpfunc);
        }
        // we make sure the sorting phase finished before going to the next step, so we don't have
        // erroneous results
        pthread_barrier_wait(data->barrier);

		// keep first 30% children (elite children selection)
		count = data->object_count * 3/ 10; //

        start = data->thread_id * (double)count / data->P;
        end = MIN((data->thread_id + 1) * (double)count / data->P, count);

        for (int i = start; i < end; ++i) {
			copy_individual(data->current_generation + i, data->next_generation + i);
		}
		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = data->object_count * 2 / 10;

        start = data->thread_id * (double)count / data->P;
        end = MIN((data->thread_id + 1) * (double)count / data->P, count);

		for (int i = start; i < end; ++i) {
			copy_individual(data->current_generation + i, data->next_generation + cursor + i);
			mutate_bit_string_1(data->next_generation + cursor + i, k);
		}
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = data->object_count * 2 / 10;
        start = data->thread_id * (double)count / data->P;
        end = MIN((data->thread_id + 1) * (double)count / data->P, count);

		for (int i = start; i < end; ++i) {
			copy_individual(data->current_generation + i + count, data->next_generation + cursor + i);
			mutate_bit_string_2(data->next_generation + cursor + i, k);
		}
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
		count = data->object_count * 3 / 10;

		if (count % 2 == 1) {
            if (data->thread_id == 0) {
                copy_individual(data->current_generation + data->object_count - 1, data->next_generation + cursor + count - 1);
            }
			count--;
		}

        start = data->thread_id * (double)count / data->P;
        end = MIN((data->thread_id + 1) * (double)count / data->P, count);

        // we need to adjust start and end to make sure that
        // 1) we have no overlapping between threads (because i+=2)
        // 2) the last parent is kept unmodified
        if (start % 2 == 1) {
            start--;
        }
        if (end % 2 == 1) {
            end--;
        }

		for (int i = start; i < end; i += 2) {
			crossover(data->current_generation + i, data->next_generation + cursor + i, k);
		}

        // Generation inter-switch
        tmp = data->current_generation;
        data->current_generation = data->next_generation;
        data->next_generation = tmp;

        // We recalculate the start and end for this thread's array portion
        start = data->thread_id * (double)data->object_count / data->P;
        end = MIN((data->thread_id + 1) * (double)data->object_count / data->P, data->object_count);

		for (int i = start; i < end; ++i) {
			data->current_generation[i].index = i;
		}

        // Thread resynchronization before going to the next generation/before exiting the program
        // We need to make sure that all threads have finished their work before the final
        // fitness computation/sorting (if thread 0 is faster than all the other threads, it may
        // sort the final array with outdated data)
        pthread_barrier_wait(data->barrier);

        // Only one thread is allowed to print (otherwise we may get wrong results)
        if (data->thread_id == 0) {
            if (k % 5 == 0) {
                print_best_fitness(data->current_generation);
            }
        }
    }
    // Last thread can handle the final fitness computation and sorting of all the other threads, as well as
    // the console output
    if (data->thread_id == 0) {
        compute_fitness_function(0, data->object_count, data->objects, data->current_generation, data->sack_capacity);
        qsort(data->current_generation, data->object_count, sizeof(individual), cmpfunc);
        print_best_fitness(data->current_generation);
    }
    // Closing the thread
    return NULL;
}