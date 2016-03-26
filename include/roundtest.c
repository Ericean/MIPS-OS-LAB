#include<stdio.h>
#include "mytype.h"
int main()
{
    int num;
    scanf("%d",&num);
    printf("%d\n",ROUND(num,4096));
    return 0;
}
