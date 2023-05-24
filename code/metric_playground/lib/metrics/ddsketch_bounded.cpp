//
//  ddsketch_bounded.cpp
//  metric_playground
//

#include <vector>
#include <memory>
#include <cmath>
#include "ddsketch_bounded.hpp"


BoundedHistogram::BoundedHistogram(int lower_bound, int upper_bound) {
    this->lower_bound = lower_bound;
    this->upper_bound = upper_bound;
    histogram = std::make_unique<std::vector<int>>();
    histogram->resize(upper_bound - lower_bound + 1, 0);
}

void BoundedHistogram::increment(int i) {
    if (i > upper_bound) {
        ++overflow;
    } else if (i < lower_bound) {
        ++underflow;
    } else {
        ++(*histogram)[i];
    }
}

// TODO: verify accuracy is between [0,1)
DDSketchBounded::DDSketchBounded(double accuracy, double upper_bound) {
    this->accuracy = accuracy;
    gamma = (1.0 + accuracy)/(1.0 - accuracy);
    log_gamma = log(gamma);
    max_bucket = bucket(upper_bound);
    histogram = std::make_unique<BoundedHistogram>(0, max_bucket);
}

std::string DDSketchBounded::name() const {
    return "ddsketch-bounded";
}

void DDSketchBounded::process(std::vector<double> const &batch) {
    for (const auto datum: batch) {
        histogram->increment(bucket(datum));
    }
}

void DDSketchBounded::process(double datum) {
    histogram->increment(bucket(datum));
}

int DDSketchBounded::bucket(double x) const {
    // TODO: validate x >= 1
    return std::ceil(std::log(x) / log_gamma);
}

double DDSketchBounded::canonical_value(int bucket) const {
    // TODO: validate bucket >= 0
    return (2 * std::pow(gamma, bucket)) / (gamma + 1.0);
}

double DDSketchBounded::canonical_value(double x) const {
    // TODO: validate x >= 1
    return canonical_value(bucket(x));
}

const BoundedHistogram & DDSketchBounded::value() const {
    return *histogram;
}
