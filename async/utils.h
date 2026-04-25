#ifndef UTILS_H_
#define UTILS_H_

#include <vector>

namespace rbruck_alltoallv_utils {

int pow(int x, unsigned int p);

std::vector<int> convert10tob(int w, int N, int b);

int check_errors(int *recvcounts, long long *recv_buffer, int rank, int nprocs);

}

#endif /* UTILS_H_ */