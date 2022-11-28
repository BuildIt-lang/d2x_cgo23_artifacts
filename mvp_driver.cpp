#include <stdio.h>

void matrix_vector(int* C, int* A, int* B);

int main(int argc, char* argv[]) {
	int M = 1024;	
	int N = 512;
	int *C = new int[M];
	int *B = new int[N];
	int *A = new int[M * N];
	
	matrix_vector(C, A, B);

	delete[] A;
	delete[] B;
	delete[] C;

	return 0;
}
