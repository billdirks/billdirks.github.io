//
//  compute_metrics.cpp
//  metric_playground
//
//  Created by Bill Dirks
//

#include "compute_metrics.hpp"
#include <memory>
#include <string>
#include "metric.hpp"
#include "ddsketch_bounded.hpp"
#include "min.hpp"
#include "max.hpp"
#include "mean.hpp"
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>

// These are for ::open
#include <sys/stat.h>
#include <fcntl.h>

// For mmap
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <string>

#include <stdlib.h>     /* strtod */
#include <unistd.h>     /* close */


class MmapFile {
public:
    MmapFile(char* filename);
    ~MmapFile();
    char* start() const; // Returns a pointer to the first char.
    char* end() const;  // Returns a pointer to the position after the last char
private:
    char* addr = nullptr;
    std::size_t mmap_size;
};


MmapFile::MmapFile(char* filename) {
        mmap_size = std::filesystem::file_size(filename);
        int fd = ::open(filename, O_RDONLY);
        // We use MAP_SHARED instead of MAP_PRIVATE since it is faster and read-only
        auto mmap_flags = MAP_FILE | MAP_SHARED;
#ifdef __linux__
        mmap_flags |= MAP_POPULATE;
#endif
        addr = reinterpret_cast<char *>(mmap(NULL, mmap_size, PROT_READ, mmap_flags , fd, 0));
        // We might want to check for errors closing.
        ::close(fd);
        if (addr == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap failed for this reason: "));
            // Deal with ::strerror_r(errno) for error message later
        }

        if(madvise(addr, mmap_size, MADV_SEQUENTIAL|MADV_WILLNEED) != 0) {
            // deal with errno later
            std::cerr << "madvise failed in MmapWholeFile constructor" << std::endl;
        }
    }

MmapFile::~MmapFile() {
        if (addr != nullptr) {
            ::munmap(addr, mmap_size);
        }
    }

char* MmapFile::start() const {
    return addr;
}

char* MmapFile::end() const {
    return &(addr[mmap_size-1]);
}


void compute_metrics(const MmapFile mmap_file, ComputeMetricOutput* computed_metrics) {
    char* cur_ptr = mmap_file.start();
    char* end_ptr = mmap_file.end();
    while (cur_ptr != end_ptr) {
        strtod(cur_ptr, &cur_ptr);
        double d = strtod(cur_ptr, &cur_ptr);
        computed_metrics->ddsketch.process(d);
        for (int i = 0; i < computed_metrics->metrics.size(); ++i) {
            computed_metrics->metrics[i]->process(d);
        }
    }
}


void compute_metrics(char* filename, ComputeMetricOutput* computed_metrics) {
    MmapFile mmap_file = MmapFile(filename);
    compute_metrics(mmap_file, computed_metrics);
}


double duration_in_seconds(std::chrono::steady_clock::time_point start_time) {
    return (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time)).count() / 1000000.0;
}
