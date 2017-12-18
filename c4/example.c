#include <stdio.h>

int array[10];

int fibonacci(int i);

struct Struct_test
{
	int a, *b;
	char c, *d;
};

struct Struct_test2
{
	int *a;
	char *b;
};

struct Struct_test s_a;
struct Struct_test2 s_b;

void test()
{
	struct Struct_test s_a;
	int i;
	i = 0;
	s_a.a = 53253252;
	s_a.c = 643090;
	while (i <= 10) {
		printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
		i = i + 1;
	}
	printf("s_a.d: %d, s_a.a: %d\n", s_a.c, s_a.a);
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

	//s_a.d = 127;
	s_a.b = 532;
	s_a.a = 4523;
	//s_a.c = 5;
	printf("s_a.d: %d, s_a.a: %d\n", s_a.b, s_a.a);

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