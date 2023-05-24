//
//  min.hpp
//  metric_playground
//
//  Created by Bill Dirks
//

#ifndef min_hpp
#define min_hpp

#include <stdio.h>
#include "metric.hpp"
#include <string>
#include <vector>

class Min: public Metric<double, double> {
public:
    Min() noexcept;
    std::string name() const;
    void process(std::vector<double> const &batch);
    void process(double datum);
    double value() const;
private:
    double _min;
    bool isInf;
    short infSign;  // Can be +1 or -1
};

#endif /* min_hpp */
