#include <stdio.h>

char a[10];

// func to convert int to char array
void *foo()
{
    a[0] = 'a';
    a[1] = 'b';
    return a;
}

int main()
{
    char *s = foo();
    printf("%c\n", s[0]);
    printf("%c\n", s[1]);
    printf("%d\n", s[0] + s[1]);
    return 0;
}