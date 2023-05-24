//
//  mean.cpp
//  metric_playground
//
//  Created by Bill Dirks
//

#include <stdio.h>
#include <stdexcept>
#include "mean.hpp"

Mean::Mean() noexcept: total {0}, length {0} {};

Mean::~Mean() {
    // printf("Destructor on Mean called for %d.\n", count);
}

std::string Mean::name() const {
    return "simple mean";
}

void Mean::process(std::vector<double> const &batch) {
    size_t batch_size = batch.size();
    if (UINT_MAX - length < batch_size) {
        throw std::overflow_error("Too many values to compute mean");
    }
    length += batch_size;
    for (auto datum: batch) {
        total += datum;
    }
}

void Mean::process(double datum) {
    if (length == UID_MAX) {
        throw std::overflow_error("Too many values to compute mean");
    }
    ++length;
    total += datum;
}

double Mean::value() const {
    return total/length;
}
