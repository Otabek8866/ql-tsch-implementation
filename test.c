#include <stdio.h>

void ch(float *arr)
{
    arr[0] = 5;
}

int main()
{
    float a[3] = {1, 2, 3};
    ch(&a);
    printf("%f\n", a[0]);
}