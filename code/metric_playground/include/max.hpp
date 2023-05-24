//
//  max.hpp
//  metric_playground
//
//  Created by Bill Dirks
//

#ifndef max_hpp
#define max_hpp

#include <stdio.h>
#include "metric.hpp"
#include <string>
#include <vector>

class Max: public Metric<double, double> {
public:
    Max() noexcept;
    std::string name() const;
    void process(std::vector<double> const &batch);
    void process(double datum);
    double value() const;
private:
    double _max;
    bool isInf;
    short infSign;  // Can be +1 or -1
};

#endif /* max_hpp */
