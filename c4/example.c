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

void test()
{
	int i;
	i = 0;
	while (i <= 10) {
		printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
		i = i + 1;
	}
}

struct Struct_test s_a;
struct Struct_test2 s_b;

int fibonacci(int i) {
    if (i <= 1) {
        return 1;
    }
    return fibonacci(i-1) + fibonacci(i-2);
}

int main()
{
    int i, j[10], *p;
	struct Struct_test s_c;

	p = j;
    i = 0;

	s_a.d = 3192432;
	s_b.a = 532;
	s_c.a = 4523;
	printf("s_a.d: %d, s_c.a: %d\n", s_a.d, s_c.a);

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