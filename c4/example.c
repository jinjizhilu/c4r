#include <stdio.h>

int array[10];

int fibonacci(int i);

struct Struct_test
{
	int a, b[5];
	char c[10], *d;
};

struct Struct_test2
{
	char *b;
	struct Struct_test a;
};

struct Struct_test *s_a;
struct Struct_test s_a2[10];
struct Struct_test2 s_c;

void test()
{
	struct Struct_test s_a;
	int i;
	i = 0;
	//s_a.a = 53253252;
	//s_a.c = 643090;
	//printf("s_a.d: %d, s_a.a: %d\n", s_a.d, s_a.a);
	while (i <= 10) {
		printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
		i = i + 1;
	}
	//printf("s_a.d: %d, s_a.a: %d\n", s_a.c, s_a.a);
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

	s_a = &s_a2[0];
	s_c.a.b[0] = 234023;
	s_a2[0].b[2] = 63242;
	s_a2[1].c[1] = 6;
	//s_a.d = 127;
	//s_a.d = 532;
	//s_a.a = 4523;
	//s_a.c = 5;
	printf("v1: %d, v2: %d, v3: %d\n", s_c.a.b[0], s_a2[0].b[2], s_a2[1].c[1]);

    while (i <= 10) {
        printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
        i = i + 1;
    }
	test();

	//array = malloc(10 * sizeof(int));
	i = 0;
	while (i < 10)
	{
		s_a = &s_a2[i];
		(*s_a).a = i * i;
		(*s_a).a = (*s_a).a * 2;
		printf("test: %d\n", (*s_a).a);
		printf("test: %d\n", j[i]);
		i++;
	}

    return 0;
}