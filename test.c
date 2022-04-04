#include <stdio.h>

int main()
{
    unsigned char ch[2];
    unsigned int i = 1;
    ch[0] = i & 0xff;
    ch[1] = (i >> 8) & 0xff;
    printf("%d %d\n", ch[0], ch[1]);
    printf("%d\n", i);

    unsigned int n;
    n = ch[1] & 0xff;
    n = n << 8;
    n += ch[0];
    printf("%d\n", n);
}

// func to convert int to char array
void int_to_char(int i, char *ch)
{
    ch[0] = i & 0xff;
    ch[1] = (i >> 8) & 0xff;
}

// convert char array to int
int char_to_int(char *ch)
{
    int n;
    n = ch[1] & 0xff;
    n += ch[0] << 8;
    return n;
}