#include <stdio.h>

void ch(float *arr)
{
    arr[0] = 5;
}

int main()
{
    float a[10];
    for (int i = 0; i < 10; i++)
        printf("%f ", a[i]);
}