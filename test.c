#include <stdio.h>

void ch(float *arr)
{
    arr[0] = 5;
}

int main()
{
    int a[3];
    // ch(&a);
    printf("%u\n", a[0]);
    printf("%u\n", a[1]);
    printf("%u\n", a[2]);
}