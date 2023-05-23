//
//  ddsketch_bounded.hpp
//  metric_playground
//
//  Created by Bill Dirks
//

#ifndef ddsketch_bounded_hpp
#define ddsketch_bounded_hpp

#include <stdio.h>
#include <memory>
#include <vector>
#include "metric.hpp"


// A fixed length histogram where the 0th element is lower_bound and the
// last element is upper_bound. We set the size of histogram at the beginning
// and never resize.
class BoundedHistogram {
public:
    BoundedHistogram(int lower_bound, int upper_bound);
    int overflow;
    int underflow;
    std::unique_ptr<std::vector<int>> histogram;
    void increment(int i);
private:
    int upper_bound;
    int lower_bound;
};

class DDSketchBounded: public Metric<double, const BoundedHistogram &> {
public:
    DDSketchBounded(double accuracy, double upper_bound);
    std::string name() const;
    void process(std::vector<double> const &batch);
    void process(double datum);
    // The lifetime of the returned value is tied to the lifetime
    // of this object. So if you want it for longer one must copy it.
    const BoundedHistogram & value() const;
    // returns the histogram canonical value for a bucket index.
    double canonical_value(int bucket) const;
    // returns the histogram canonical value for a data point x
    double canonical_value(double x) const;
private:
    std::unique_ptr<BoundedHistogram> histogram;
    double accuracy;
    double gamma;
    double log_gamma;
    int max_bucket;
    // returns the bucket index for a data point x
    int bucket(double x) const;
};

#endif /* ddsketch_bounded_hpp */
