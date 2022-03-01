# APD - Tema 1
# Octombrie 2021

build:
	@echo "Building..."
	@gcc -o tema1_par tema1_par.c genetic_algorithm.c -lm -lpthread -std=gnu99 -Wall -Wextra -Werror
	@echo "Done"

build_debug:
	@echo "Building debug..."
	@gcc -o tema1_par tema1_par.c genetic_algorithm.c -lm -lpthread -std=gnu99 -Wall -Wextra  -O0 -g3 -DDEBUG
	@echo "Done"

clean:
	@echo "Cleaning..."
	@rm -rf tema1_par
	@echo "Done"
