
#pragma once

#include <functional>
  
//  https://www.geeksforgeeks.org/print-given-matrix-reverse-spiral-form/
//	m - ending row index
//	n - ending column index
void ReverseSpiralWalkthrough(int m, int n, std::function<void(int x, int y)> Function) {
	/* k - starting row index
	l - starting column index*/
	int i, k = 0, l = 0;

	// Counter for single dimension array 
	//in which elements will be stored 
	int z = 0;

	// Total elements in matrix 
	int size = m * n;

	while (k < m && l < n) {
		/* Print the first row from the remaining rows */
		for (i = l; i < n; ++i) {
			// printf("%d ", a[k][i]); 
			Function(k, i);
			++z;
		}
		k++;

		/* Print the last column from the remaining columns */
		for (i = k; i < m; ++i) {
			// printf("%d ", a[i][n-1]); 
			Function(i, n - 1);
			++z;
		}
		n--;

		/* Print the last row from the remaining rows */
		if (k < m) {
			for (i = n - 1; i >= l; --i) {
				// printf("%d ", a[m-1][i]); 
				Function(m - 1, i);
				++z;
			}
			m--;
		}

		/* Print the first column from the remaining columns */
		if (l < n) {
			for (i = m - 1; i >= k; --i) {
				// printf("%d ", a[i][l]); 
				Function(i, l);
				++z;
			}
			l++;
		}
	}
}