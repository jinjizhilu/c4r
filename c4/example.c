#include <stdio.h>

int array[10];

int fibonacci(int i);

void test()
{
	int i;
	i = 0;
	while (i <= 10) {
		printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
		i = i + 1;
	}
}

int fibonacci(int i) {
    if (i <= 1) {
        return 1;
    }
    return fibonacci(i-1) + fibonacci(i-2);
}

int main()
{
    int i, j[10], *p;
	p = j;
    i = 0;
    while (i <= 10) {
        printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
        i = i + 1;
    }
	test();

	//array = malloc(10 * sizeof(int));
	i = 0;
	while (i < 10)
	{
		array[i] = i;
		p[i] = i;
		printf("test: %d\n", array[i]);
		printf("test: %d\n", j[i]);
		i++;
	}

    return 0;
}