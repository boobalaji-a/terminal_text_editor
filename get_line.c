#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	if(argc <2) return 0;
	FILE *f = fopen(argv[1], "r");
	char *buf = NULL;
	size_t cap;
	ssize_t len = 0;
	while((len = getline(&buf,&cap,f)) != -1){
		printf("%s\n",buf);
	}
	
}
