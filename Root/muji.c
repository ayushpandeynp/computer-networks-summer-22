#include <stdio.h>
#include <unistd.h>

int main() {
	int a = fork();

	printf("Muji");
	printf("%d", a);
	return 0;
}