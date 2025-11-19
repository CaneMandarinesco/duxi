
duxi: duxi.c
	$(CC) -g -fsanitize=address  duxi.c -o duxi -Wall -Wextra -pedantic -std=c99

kilo: kilo.c
	$(CC) -g kilo.c -o kilo -Wall -Wextra -pedantic -std=c99