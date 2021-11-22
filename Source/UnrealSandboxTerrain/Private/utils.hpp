
#pragma once

#include <functional>
#include <list>
#include <atomic>


// memory statistics

std::atomic<int> md_counter{ 0 };
std::atomic<int> cd_counter{ 0 };


//  https://www.geeksforgeeks.org/print-given-matrix-reverse-spiral-form/
//	m - ending row index
//	n - ending column index
std::list<TChunkIndex> ReverseSpiralWalkthrough(const unsigned int r) {

    auto r2 = r * 2 + 1;
    int m = r2;
    int n = r2;
    
	std::list<TChunkIndex> list;

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
			//Function(k, i);
			list.push_front(TChunkIndex(k - r, i - r));
			++z;
		}
		k++;

		/* Print the last column from the remaining columns */
		for (i = k; i < m; ++i) {
			// printf("%d ", a[i][n-1]); 
			//Function(i, n - 1);
			list.push_front(TChunkIndex(i - r, n - 1 - r));
			++z;
		}
		n--;

		/* Print the last row from the remaining rows */
		if (k < m) {
			for (i = n - 1; i >= l; --i) {
				// printf("%d ", a[m-1][i]); 
				//Function(m - 1, i);
				list.push_front(TChunkIndex(m - 1 - r, i - r));
				++z;
			}
			m--;
		}

		/* Print the first column from the remaining columns */
		if (l < n) {
			for (i = m - 1; i >= k; --i) {
				// printf("%d ", a[i][l]); 
				//Function(i, l);
				list.push_front(TChunkIndex(i - r, l - r));
				++z;
			}
			l++;
		}
	}

	return list;
}

extern FVector sandboxSnapToGrid(FVector vec, float grid_range) {
    FVector tmp(vec);
    tmp /= grid_range;
    //FVector tmp2(std::round(tmp.X), std::round(tmp.Y), std::round(tmp.Z));
    FVector tmp2((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
    tmp2 *= grid_range;
    return FVector((int)tmp2.X, (int)tmp2.Y, (int)tmp2.Z);
}


extern FVector sandboxConvertVectorToCubeIndex(FVector vec) {
    return sandboxSnapToGrid(vec, 200);
}


FVector sandboxGridIndex(const FVector& v, int range) {
    FVector tmp(v);

    const int r = range / 2;

    tmp.X = (tmp.X > 0) ? tmp.X + r : tmp.X - r;
    tmp.Y = (tmp.Y > 0) ? tmp.Y + r : tmp.Y - r;
    tmp.Z = (tmp.Z > 0) ? tmp.Z + r : tmp.Z - r;

    tmp /= range;

    return FVector((int)tmp.X, (int)tmp.Y, (int)tmp.Z);
}


inline unsigned int CRC32(unsigned char* buf, unsigned long len) {
	unsigned long crc_table[256];
	unsigned long crc;
	for (int i = 0; i < 256; i++) {
		crc = i;
		for (int j = 0; j < 8; j++)
			crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
		crc_table[i] = crc;
	};

	crc = 0xFFFFFFFFUL;
	while (len--)
		crc = crc_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
	return crc ^ 0xFFFFFFFFUL;
}
