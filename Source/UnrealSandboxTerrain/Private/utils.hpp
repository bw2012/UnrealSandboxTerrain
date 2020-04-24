
#pragma once

#include <functional>
#include <list>
  
//  https://www.geeksforgeeks.org/print-given-matrix-reverse-spiral-form/
//	m - ending row index
//	n - ending column index
void ReverseSpiralWalkthrough(const unsigned int r, std::function<bool(int x, int y)> Function) {
	struct XY {
		int x, y;
		XY(int x_, int y_) : x(x_), y(y_) { };
	};

    auto r2 = r * 2 + 1;
    int m = r2;
    int n = r2;
    
	std::list<XY> list;

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
			list.push_front(XY(k, i));
			++z;
		}
		k++;

		/* Print the last column from the remaining columns */
		for (i = k; i < m; ++i) {
			// printf("%d ", a[i][n-1]); 
			//Function(i, n - 1);
			list.push_front(XY(i, n - 1));
			++z;
		}
		n--;

		/* Print the last row from the remaining rows */
		if (k < m) {
			for (i = n - 1; i >= l; --i) {
				// printf("%d ", a[m-1][i]); 
				//Function(m - 1, i);
				list.push_front(XY(m - 1, i));
				++z;
			}
			m--;
		}

		/* Print the first column from the remaining columns */
		if (l < n) {
			for (i = m - 1; i >= k; --i) {
				// printf("%d ", a[i][l]); 
				//Function(i, l);
				list.push_front(XY(i, l));
				++z;
			}
			l++;
		}
	}

	for (auto& itm : list) {
        if(Function(itm.x - r, itm.y - r)){
            break;
        }
	}
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
