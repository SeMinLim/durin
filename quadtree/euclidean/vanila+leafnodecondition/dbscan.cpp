#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


// DBSCAN
#define CORE_POINT 1
#define BORDER_POINT 2

#define SUCCESS 0
#define UNCLASSIFIED -1
#define NOISE -2
#define FAILURE -3

#define MINIMUM_POINTS 2
#define PCIECONDITION 4


uint64_t numDataPoints = 0;
uint64_t numQuadrants = 0;
uint64_t numEuclidean = 0;
uint64_t numPointsCondition = 0;
uint64_t numNormal = 0;


typedef struct Point {
	float lat, lon;
}Point;

typedef struct PointQuadTree {
	Point point;
	int datasetID;
}PointQuadTree;

typedef struct PointDBSCAN {
	Point point;
	Point northEastern;
	Point northWestern;
	Point southEastern;
	Point southWestern;
	int clusterID;
}PointDBSCAN;

typedef struct Quadrant {
	std::vector<Quadrant*> child;
	std::vector<PointQuadTree> cities;
	Point northEastern;
	Point northWestern;
	Point southEastern;
	Point southWestern;
	Point center;
	float diagonal;
	bool done;
}Quadrant;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
// Quadrant
void readBenchmarkDataQuadTree(Quadrant *root, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		PointQuadTree temp;
		int datasetID = i;
		float lat = 0.00;
		float lon = 0.00;
		fread(&lat, sizeof(float), 1, f_data);
		fread(&lon, sizeof(float), 1, f_data);
		temp.point.lat = lat;
		temp.point.lon = lon;
		temp.datasetID = datasetID;
		root->cities.push_back(temp);
	}

	root->done = 0;
	
	fclose(f_data);
}
// Data points
void readBenchmarkDataDBSCAN(std::vector<PointDBSCAN> &dataset, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		PointDBSCAN temp;
		int clusterID = UNCLASSIFIED;
		float lat = 0.00;
		float lon = 0.00;
		fread(&lat, sizeof(float), 1, f_data);
		fread(&lon, sizeof(float), 1, f_data);
		temp.point.lat = lat;
		temp.point.lon = lon;
		temp.clusterID = clusterID;
		dataset.push_back(temp);
	}
	
	fclose(f_data);
}

// Function for writing benchmark file
void writeBenchmarkDataDBSCAN(std::vector<PointDBSCAN> &dataset, char* filename, int length) {
	FILE* f_data = fopen(filename, "wb");

	for ( int i = 0; i < length; i ++ ) {
		fwrite(&dataset[i].point.lat, sizeof(float), 1, f_data);
		fwrite(&dataset[i].point.lon, sizeof(float), 1, f_data);
		fwrite(&dataset[i].clusterID, sizeof(int), 1, f_data);
	}

	fclose(f_data);
}

// Euclidean
float euclidean(const Point pointCore, const Point pointTarget) {
	numEuclidean++;
	float sub_lat = pointCore.lat - pointTarget.lat;
	float sub_lon = pointCore.lon - pointTarget.lon;

	float before_sqrt = pow(sub_lat, 2) + pow(sub_lon, 2);

	return sqrt(before_sqrt);
}

// Function for four edge points of square
void findEdgePointsEpsilonBox(std::vector<PointDBSCAN> &dataset, int epsilon) {
	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		dataset[i].northEastern.lat = dataset[i].point.lat + epsilon;
		dataset[i].northEastern.lon = dataset[i].point.lon + epsilon;
		dataset[i].northWestern.lat = dataset[i].point.lat - epsilon;
		dataset[i].northWestern.lon = dataset[i].point.lon + epsilon;
		dataset[i].southEastern.lat = dataset[i].point.lat + epsilon;
		dataset[i].southEastern.lon = dataset[i].point.lon - epsilon;
		dataset[i].southWestern.lat = dataset[i].point.lat - epsilon;
		dataset[i].southWestern.lon = dataset[i].point.lon - epsilon;
	}
}

// Function for four edge points of quadrant
void findEdgePointsQuadrant(Quadrant *root) {
	// First child quad
	root->child[0]->northEastern.lat = root->center.lat;
	root->child[0]->northEastern.lon = root->center.lon;
	root->child[0]->northWestern.lat = root->southWestern.lat;
	root->child[0]->northWestern.lon = root->center.lon;
	root->child[0]->southEastern.lat = root->center.lat;
	root->child[0]->southEastern.lon = root->southWestern.lon;
	root->child[0]->southWestern.lat = root->southWestern.lat;
	root->child[0]->southWestern.lon = root->southWestern.lon;

	// Second child quad
	root->child[1]->northEastern.lat = root->northEastern.lat;
	root->child[1]->northEastern.lon = root->center.lon;
	root->child[1]->northWestern.lat = root->center.lat;
	root->child[1]->northWestern.lon = root->center.lon;
	root->child[1]->southEastern.lat = root->northEastern.lat;
	root->child[1]->southEastern.lon = root->southWestern.lon;
	root->child[1]->southWestern.lat = root->center.lat;
	root->child[1]->southWestern.lon = root->southWestern.lon;

	// Third child quad
	root->child[2]->northEastern.lat = root->center.lat;
	root->child[2]->northEastern.lon = root->northEastern.lon;
	root->child[2]->northWestern.lat = root->southWestern.lat;
	root->child[2]->northWestern.lon = root->northEastern.lon;
	root->child[2]->southEastern.lat = root->center.lat;
	root->child[2]->southEastern.lon = root->center.lon;
	root->child[2]->southWestern.lat = root->southWestern.lat;
	root->child[2]->southWestern.lon = root->center.lon;

	// Fourth child quad
	root->child[3]->northEastern.lat = root->northEastern.lat;
	root->child[3]->northEastern.lon = root->northEastern.lon;
	root->child[3]->northWestern.lat = root->center.lat;
	root->child[3]->northWestern.lon = root->northEastern.lon;
	root->child[3]->southEastern.lat = root->northEastern.lat;
	root->child[3]->southEastern.lon = root->center.lon;
	root->child[3]->southWestern.lat = root->center.lat;
	root->child[3]->southWestern.lon = root->center.lon;
}

// Function for finding center mass value
void findCenterMass(Quadrant *root) {
	float totalX = 0.00;
	float totalY = 0.00;
	for ( int i = 0; i < (int)root->cities.size(); i ++ ) {
		totalX = totalX + root->cities[i].point.lat;
		totalY = totalY + root->cities[i].point.lon;
	}
	root->center.lat = totalX / (float)root->cities.size();
	root->center.lon = totalY / (float)root->cities.size();
}

// Function for finding a length of diagonal euclidean distance
void findDiagonal(Quadrant *root) {
	root->diagonal = euclidean(root->northEastern, root->southWestern);
}

// Function for initialization
void initialize(Quadrant *root, int epsilon) {
	// Highest and lowest
	for ( int i = 0; i < (int)root->cities.size(); i ++ ) {
		if ( i == 0 ) {
			root->southWestern.lat = root->cities[i].point.lat;
			root->southWestern.lon = root->cities[i].point.lon;
			root->northEastern.lat = root->cities[i].point.lat;
			root->northEastern.lon = root->cities[i].point.lon;
		} else {
			if ( root->southWestern.lat > root->cities[i].point.lat ) {
				root->southWestern.lat = root->cities[i].point.lat;
			}
			if ( root->southWestern.lon > root->cities[i].point.lon ) {
				root->southWestern.lon = root->cities[i].point.lon;
			}
			if ( root->northEastern.lat < root->cities[i].point.lat ) {
				root->northEastern.lat = root->cities[i].point.lat;
			}
			if ( root->northEastern.lon < root->cities[i].point.lon ) {
				root->northEastern.lon = root->cities[i].point.lon;
			}
		}
	}

	// Other edge points
	root->northWestern.lat = root->southWestern.lat;
	root->northWestern.lon = root->northEastern.lon;
	root->southEastern.lat = root->northEastern.lat;
	root->southEastern.lon = root->southWestern.lon;

	// Center mass of dataset
	findCenterMass(root);

	// Diagonal euclidean distance
	findDiagonal(root);

	if ( root->diagonal <= epsilon ) root->done = 1;
}

// Quadtree (Divide parent quadrant to 4 childrent quadrant)
void divideQuad(Quadrant *root) {
	for ( int i = 0; i < (int)root->cities.size(); i ++ ) {
		// First child quadrant
		if ( (root->cities[i].point.lat < root->center.lat) && 
		     (root->cities[i].point.lon < root->center.lon) ) {
			PointQuadTree temp;
			temp.point.lat = root->cities[i].point.lat;
			temp.point.lon = root->cities[i].point.lon;
			temp.datasetID = root->cities[i].datasetID;
			root->child[0]->cities.push_back(temp);
		// Second child quadrant
		} else if ( (root->cities[i].point.lat >= root->center.lat) && 
			    (root->cities[i].point.lon < root->center.lon) ) {
			PointQuadTree temp;
			temp.point.lat = root->cities[i].point.lat;
			temp.point.lon = root->cities[i].point.lon;
			temp.datasetID = root->cities[i].datasetID;
			root->child[1]->cities.push_back(temp);
		// Third child quadrant
		} else if ( (root->cities[i].point.lat < root->center.lat) && 
			    (root->cities[i].point.lon >= root->center.lon) ) {
			PointQuadTree temp;
			temp.point.lat = root->cities[i].point.lat;
			temp.point.lon = root->cities[i].point.lon;
			temp.datasetID = root->cities[i].datasetID;
			root->child[2]->cities.push_back(temp);
		// Fourth child quadrant
		} else if ( (root->cities[i].point.lat >= root->center.lat) && 
			    (root->cities[i].point.lon >= root->center.lon) ) {
			PointQuadTree temp;
			temp.point.lat = root->cities[i].point.lat;
			temp.point.lon = root->cities[i].point.lon;
			temp.datasetID = root->cities[i].datasetID;
			root->child[3]->cities.push_back(temp);
		}
	}
}

// Quadtree (Get the needed information for each quadrant)
void getInfoQuad(Quadrant *root, int epsilon, int pointsCondition) {
	for ( int i = 0; i < (int)root->child.size(); ) {
		if ( (int)root->child[i]->cities.size() > pointsCondition ) {
			findCenterMass(root->child[i]);
			findDiagonal(root->child[i]);
			if ( root->child[i]->diagonal <= epsilon ) {
				root->child[i]->done = 1;
				numDataPoints = numDataPoints + (int)root->child[i]->cities.size();
				int quotient = (int)root->child[i]->cities.size() / PCIECONDITION;
				int remainder = (int)root->child[i]->cities.size() % PCIECONDITION;
				numPointsCondition = numPointsCondition + (quotient * PCIECONDITION);
				numNormal = numNormal + remainder;
			} else root->child[i]->done = 0;
			i++;
		} else if ( (int)root->child[i]->cities.size() == pointsCondition ) {
			findCenterMass(root->child[i]);
			findDiagonal(root->child[i]);
			root->child[i]->done = 1;
			numDataPoints = numDataPoints + (int)root->child[i]->cities.size();
			int quotient = (int)root->child[i]->cities.size() / PCIECONDITION;
			int remainder = (int)root->child[i]->cities.size() % PCIECONDITION;
			numPointsCondition = numPointsCondition + (quotient * PCIECONDITION);
			numNormal = numNormal + remainder;
			i++;
		} else if ( (int)root->child[i]->cities.size() < pointsCondition ) {
			if ( root->child[i]->cities.size() == 0 ) {
				delete root->child[i];
				root->child.erase(root->child.begin() + i);
			} else {
				findCenterMass(root->child[i]);
				findDiagonal(root->child[i]);
				root->child[i]->done = 1;
				numDataPoints = numDataPoints + (int)root->child[i]->cities.size();
				int quotient = (int)root->child[i]->cities.size() / PCIECONDITION;
				int remainder = (int)root->child[i]->cities.size() % PCIECONDITION;
				numPointsCondition = numPointsCondition + (quotient * PCIECONDITION);
				numNormal = numNormal + remainder;
				i++;
			}
		} 	
	}
	
	// Count the total number of quadrants & delete parent's data
	if ( (int)root->child.size() > 0 ) {
		numQuadrants = numQuadrants + (int)root->child.size();
		std::vector<PointQuadTree>().swap(root->cities);
	}
}

// Quadtree (Insert new child quadrant to parent quadrant)
void insertQuad(Quadrant *root, int epsilon, int pointsCondition) {
	if ( root->done == 0 ) {
		// Generate child quadrants first
		root->child.resize(4);
		for ( int i = 0; i < 4; i ++ ) {
			root->child[i] = new Quadrant;
			root->child[i]->diagonal = 0.00; 
			root->child[i]->done = 0;
		}

		// Divide
		divideQuad(root);

		// Highest and lowest values of each quadrant
		findEdgePointsQuadrant(root);

		// Center mass value and diagonal distance of each quadrant
		getInfoQuad(root, epsilon, pointsCondition);
	} else return;
}

// Quadtree (Main)
int quadtree(Quadrant *root, int epsilon, int pointsCondition) {
	std::vector<Quadrant*> parentsCurr;
	std::vector<Quadrant*> parentsNext;
	parentsCurr.clear();
	parentsCurr.shrink_to_fit();
	parentsNext.clear();
	parentsNext.shrink_to_fit();
	int level = 0;

	// Root quadrant
	insertQuad(root, epsilon, pointsCondition);
	if ( (int)root->child.size() > 0 ) {
		for ( int i = 0; i < (int)root->child.size(); i ++ ) {
			if ( root->child[i]->done == 0 ) parentsCurr.push_back(root->child[i]);
		}
	}
	level++;

	// Iteration until meeting the terminate conditions
	while (1) {
		parentsNext.clear();
		parentsNext.shrink_to_fit();
		for ( int i = 0; i < (int)parentsCurr.size(); i ++ ) {
			insertQuad(parentsCurr[i], epsilon, pointsCondition);
			if ( (int)parentsCurr[i]->child.size() > 0 ) {
				for ( int j = 0; j < (int)parentsCurr[i]->child.size(); j ++ ) {
					if ( parentsCurr[i]->child[j]->done == 0 ) parentsNext.push_back(parentsCurr[i]->child[j]);
				}
			}
		}
		parentsCurr.clear();
		parentsCurr.shrink_to_fit();
		if ( (int)parentsNext.size() > 0 ) {
			for ( int i = 0; i < (int)parentsNext.size(); i ++ ) {
				parentsCurr.push_back(parentsNext[i]);
			}
		} else break;
		level++;
	}
	
	return level;
}

// DBSCAN (Comparer between epsilon box and quadrant)
// Check the epsilon box is in a quadrant first
int compareEBinQ(std::vector<PointDBSCAN> &dataset, int index, Quadrant *root) {
	int numPoints = 0;
	if ( dataset[index].northEastern.lat >= root->southWestern.lat && 
	     dataset[index].northEastern.lat <= root->northEastern.lat &&
	     dataset[index].northEastern.lon >= root->southWestern.lon &&
	     dataset[index].northEastern.lon <= root->northEastern.lon ) numPoints++;
	if ( dataset[index].northWestern.lat >= root->southWestern.lat && 
	     dataset[index].northWestern.lat <= root->northEastern.lat &&
	     dataset[index].northWestern.lon >= root->southWestern.lon &&
	     dataset[index].northWestern.lon <= root->northEastern.lon ) numPoints++;
	if ( dataset[index].southEastern.lat >= root->southWestern.lat && 
	     dataset[index].southEastern.lat <= root->northEastern.lat &&
	     dataset[index].southEastern.lon >= root->southWestern.lon &&
	     dataset[index].southEastern.lon <= root->northEastern.lon ) numPoints++;
	if ( dataset[index].southWestern.lat >= root->southWestern.lat && 
	     dataset[index].southWestern.lat <= root->northEastern.lat &&
	     dataset[index].southWestern.lon >= root->southWestern.lon &&
	     dataset[index].southWestern.lon <= root->northEastern.lon ) numPoints++;
	return numPoints;
}
// Check the quadrant is in epsilon box
int compareQinEB(std::vector<PointDBSCAN> &dataset, int index, Quadrant *root) {
	int numPoints = 0;
	if ( root->northEastern.lat >= dataset[index].southWestern.lat && 
	     root->northEastern.lat <= dataset[index].northEastern.lat &&
	     root->northEastern.lon >= dataset[index].southWestern.lon &&
	     root->northEastern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root->northWestern.lat >= dataset[index].southWestern.lat && 
	     root->northWestern.lat <= dataset[index].northEastern.lat &&
	     root->northWestern.lon >= dataset[index].southWestern.lon &&
	     root->northWestern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root->southEastern.lat >= dataset[index].southWestern.lat && 
	     root->southEastern.lat <= dataset[index].northEastern.lat &&
	     root->southEastern.lon >= dataset[index].southWestern.lon &&
	     root->southEastern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root->southWestern.lat >= dataset[index].southWestern.lat && 
	     root->southWestern.lat <= dataset[index].northEastern.lat &&
	     root->southWestern.lon >= dataset[index].southWestern.lon &&
	     root->southWestern.lon <= dataset[index].northEastern.lon ) numPoints++;
	return numPoints;
}
// Check epsilon box and quadrant are overlapped each other
int compareOverlap(std::vector<PointDBSCAN> &dataset, int index, Quadrant *root) {
	int numPoints = 0;
	if ( dataset[index].northEastern.lat >= root->southWestern.lat &&
	     dataset[index].northEastern.lat <= root->northEastern.lat &&
	     root->northEastern.lon >= dataset[index].southWestern.lon &&
	     root->northEastern.lon <= dataset[index].northEastern.lon) numPoints++;
	if ( root->northEastern.lat >= dataset[index].southWestern.lat &&
	     root->northEastern.lat <= dataset[index].northEastern.lat &&
	     dataset[index].northEastern.lon >= root->southWestern.lon &&
	     dataset[index].northEastern.lon <= root->northEastern.lon) numPoints++;
	return numPoints;
}

// DBSCAN (Do euclidean calculation based on candidate list)
void candidateListCalculator(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant *root, int epsilon) {
	if ( root->done == 0 ) {
		for ( int i = 0; i < (int)root->child.size(); i ++ ) {
			candidateListCalculator(dataset, index, borders, root->child[i], epsilon);
		}
	} else {
		if ( root->diagonal <= epsilon ) {
			for ( int i = 0; i < (int)root->cities.size(); i ++ ) {
				if ( euclidean(dataset[index].point, root->cities[i].point) <= epsilon ) {
					for ( int j = 0; j < (int)root->cities.size(); j ++ ) {
						borders.push_back(root->cities[j].datasetID);
					}
					break;
				}
			}
		} else {
			for ( int i = 0; i < (int)root->cities.size(); i ++ ) {
				if ( euclidean(dataset[index].point, root->cities[i].point) <= epsilon ) {
					borders.push_back(root->cities[i].datasetID);
				}
			}
		}
	}
}

// DBSCAN (Do make candidate list in case of quadrant is in epsilon box)
void findQuadrantsQinEB(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant *root, int epsilon) {
	int resultQinEB = compareQinEB(dataset, index, root);
	if ( resultQinEB == 1 || resultQinEB == 2 || resultQinEB == 3 ) {
		if ( root->done == 0 ) {
			for ( int i = 0; i < (int)root->child.size(); i ++ ) {
				findQuadrantsQinEB(dataset, index, borders, root->child[i], epsilon);
			}
		} else {
			candidateListCalculator(dataset, index, borders, root, epsilon);
		}
	} else if ( resultQinEB == 4 ) {
		candidateListCalculator(dataset, index, borders, root, epsilon);
	}
}

// DBSCAN (Do make candidate list in case of epsilon box is in quadrant)
void findQuadrantsEBinQ(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant *root, int epsilon) {
	int resultEBinQ = compareEBinQ(dataset, index, root);
	if ( resultEBinQ != 0 ) {
		if ( root->done == 1 ) {
			candidateListCalculator(dataset, index, borders, root, epsilon);
		} else {
			int resultQinEB = compareQinEB(dataset, index, root);
			if ( resultQinEB == 0 ) {
				for ( int i = 0; i < (int)root->child.size(); i ++ ) {
					findQuadrantsEBinQ(dataset, index, borders, root->child[i], epsilon);
				}
			} else findQuadrantsQinEB(dataset, index, borders, root, epsilon);
		}
	} else {
		int resultQinEB = compareQinEB(dataset, index, root);
		if ( resultQinEB != 0 ) {
			findQuadrantsQinEB(dataset, index, borders, root, epsilon);
		} else {
			int resultPart3 = compareOverlap(dataset, index, root);
			if ( resultPart3 != 0 ) {
				if ( root->done == 1 ) candidateListCalculator(dataset, index, borders, root, epsilon);
				else {
					for ( int i = 0; i < (int)root->child.size(); i ++ ) {
						findQuadrantsEBinQ(dataset, index, borders, root->child[i], epsilon);
					}
				}
			}
		}
	}
}

// DBSCAN (Border Point Finder of Core Point)
void borderFinderCore(std::vector<PointDBSCAN> &dataset, int corePoint, std::vector<int> &bordersCore, Quadrant *root, int epsilon) {
	bordersCore.clear();
	bordersCore.shrink_to_fit();
	findQuadrantsEBinQ(dataset, corePoint, bordersCore, root, epsilon);
}

// DBSCAN (Border Point Finder of Border Point)
void borderFinderBorder(std::vector<PointDBSCAN> &dataset, int borderPoint, std::vector<int> &bordersBorder, Quadrant *root, int epsilon) {
	bordersBorder.clear();
	bordersBorder.shrink_to_fit();
	findQuadrantsEBinQ(dataset, borderPoint, bordersBorder, root, epsilon);
}

// DBSCAN (Cluster Expander)
int clusterExpander(std::vector<PointDBSCAN> &dataset, int index, int clusterID, Quadrant *root, int epsilon) {
	std::vector<int> bordersCore;
	std::vector<int> bordersBorder;
	borderFinderCore(dataset, index, bordersCore, root, epsilon);

	if ( bordersCore.size() < MINIMUM_POINTS ) {
		dataset[index].clusterID = NOISE;
		std::vector<int>().swap(bordersCore);
		return FAILURE;
	} else {
		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			dataset[borderPoint].clusterID = clusterID;
		}

		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			if ( (dataset[borderPoint].point.lat == dataset[index].point.lat) && 
			     (dataset[borderPoint].point.lon == dataset[index].point.lon) ) {
				continue;
			} else {
				borderFinderBorder(dataset, borderPoint, bordersBorder, root, epsilon);

				if ( bordersBorder.size() >= MINIMUM_POINTS ) {
					for ( int j = 0; j < (int)bordersBorder.size(); j ++ ) {
						int neighbourPoint = bordersBorder[j];
						if ( dataset[neighbourPoint].clusterID == UNCLASSIFIED ||
						     dataset[neighbourPoint].clusterID == NOISE ) {
							if ( dataset[neighbourPoint].clusterID == UNCLASSIFIED ) {
								bordersCore.push_back(neighbourPoint);
							}
							dataset[neighbourPoint].clusterID = clusterID;
						}
					}
				}
			}
		}
		std::vector<int>().swap(bordersCore);
		std::vector<int>().swap(bordersBorder);
		return SUCCESS;
	}
}

// DBSCAN (Main)
int dbscan(std::vector<PointDBSCAN> &dataset, Quadrant *root, int epsilon) {
	int clusterID = 1;
	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		if ( dataset[i].clusterID == UNCLASSIFIED ) {
			if ( clusterExpander(dataset, i, clusterID, root, epsilon) != FAILURE ) {
				clusterID += 1;
				printf( "Generating cluster %d done!\n", clusterID-1 );
			}
		}
	}

	return clusterID-1;
}

// Function for printing results
void printResults(std::vector<PointDBSCAN> &dataset) {
	printf(" x       y       cluster_id\n"
	       "---------------------------\n");

	for ( int i = 0; i < (int)dataset.size() - 1; i ++ ) {
		for ( int j = 0; j < (int)dataset.size() - 1 - i; j ++ ) {
			if ( dataset[j].clusterID > dataset[j+1].clusterID ) {
				PointDBSCAN temp = dataset[j];
				dataset[j] = dataset[j+1];
				dataset[j+1] = temp;
			}
		}
	}

	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		printf("%f, %f: %d\n", dataset[i].point.lat, dataset[i].point.lon, dataset[i].clusterID);
	}

	printf( "--------------------------\n" );
}

// Main
int main(int argc, char **argv) {
	int numCities = 700968*160;
	int epsilon = atoi(argv[1]);
	int pointsCondition = atoi(argv[2]);

	std::vector<PointDBSCAN> dataset;
	Quadrant *root = new Quadrant;

	// Read point data
	char benchmark_filename[] = "../../../worldcities_augmented.bin";
	readBenchmarkDataDBSCAN(dataset, benchmark_filename, numCities);
	readBenchmarkDataQuadTree(root, benchmark_filename, numCities);

	// Initialize
	initialize(root, epsilon);

	// Get four edge points of epsilon box of each data point
	printf( "Finding Four Edge Points of Epsilon Box of The World Cities Start!\n" );
	double processStartStep1 = timeCheckerCPU();
	findEdgePointsEpsilonBox(dataset, epsilon);
	double processFinishStep1 = timeCheckerCPU();
	double processTimeStep1 = processFinishStep1 - processStartStep1;
	printf( "Finding Four Edge Points of Epsilon Box of The World Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// Quadtree
	printf( "Quadtree for The World Cities Start!\n" );
	double processStartStep2 = timeCheckerCPU();
	int level = quadtree(root, epsilon, pointsCondition);
	double processFinishStep2 = timeCheckerCPU();
	double processTimeStep2 = processFinishStep2 - processStartStep2;
	printf( "Quadtree for The World Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// DBSCAN
	printf( "Quadtree-based DBSCAN Clustering for The World Cities Start!\n" );
	double processStartStep3 = timeCheckerCPU();
	int maxClusterID = dbscan(dataset, root, epsilon);
	double processFinishStep3 = timeCheckerCPU();
	double processTimeStep3 = processFinishStep3 - processStartStep3;
	printf( "Quadtree-based DBSCAN Clustering for The World Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );
/*
	// Save the result dataset as bin file
	char resultDataset_filename[] = "../worldcities_result.bin";
	printf( "Saving The Result Dataset Start!\n" );
	writeBenchmarkDataDBSCAN(dataset, resultDataset_filename, numCities);
	printf( "Saving The Result Dataset Done!\n" );
	printf( "\n" );
	fflush( stdout );
*/	
	// Result of Quadtree-based DBSCAN algorithm
	//printResults(dataset);
	double euclideanPercent_1 = (double)numEuclidean / (double)7677439641;
	double euclideanPercent_2 = euclideanPercent_1 * (double)21.37763600;
	double euclideanPercent_3 = euclideanPercent_2 / (double)processTimeStep3;
	double euclideanPercent_4 = euclideanPercent_3 * (double)100.00;
	float notCompressSize = (float)numNormal * 64.00;
	float compressSize16b = (float)numPointsCondition * 32.00;
	float compressSize12b = (float)numPointsCondition * 24.00;
	float compressSize08b = (float)numPointsCondition * 16.00;
	float dataSize16b = (((((notCompressSize + compressSize16b) / 8) / 1024) / 1024) / 1024);
	float dataSize12b = (((((notCompressSize + compressSize12b) / 8) / 1024) / 1024) / 1024);
	float dataSize08b = (((((notCompressSize + compressSize08b) / 8) / 1024) / 1024) / 1024);

	printf( "Elapsed Time [Step1] [Epsilon Box] (CPU)   : %.8f\n", processTimeStep1 );
	printf( "Elapsed Time [Step2] [Quadtree] (CPU)      : %.8f\n", processTimeStep2 );
	printf( "Elapsed Time [Step3] [DBSCAN] (CPU)        : %.8f\n", processTimeStep3 );
	printf( "The Number of Data Points [Total]          : %ld\n", numDataPoints );
	printf( "The Number of Data Points [Compress]       : %ld\n", numPointsCondition );
	printf( "The Number of Data Points [Not Compress]   : %ld\n", numNormal );
	printf( "Data Structure Size [16 bits]              : %.8f\n", dataSize16b );
	printf( "Data Structure Size [12 bits]              : %.8f\n", dataSize12b );
	printf( "Data Structure Size [08 bits]              : %.8f\n", dataSize08b );
	printf( "The Maximum of Tree Level                  : %d\n", level );
	printf( "The Number of Quadrants                    : %ld\n", numQuadrants );
	printf( "The Number of Euclidean                    : %ld\n", numEuclidean );
	printf( "Euclidean Percentage of Whole Elased Time  : %.8f\n", euclideanPercent_4 );
	printf( "Max Cluster ID                             : %d\n", maxClusterID );
	printf( "Epsilon Value                              : %d\n", epsilon );
	printf( "Elements in a Leaf Node                    : %d\n", pointsCondition );

	delete root;	
	return 0;
}
