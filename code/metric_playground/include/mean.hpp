//
//  mean.hpp
//  metric_playground
//
//  Created by Bill Dirks
//

#ifndef mean_hpp
#define mean_hpp

#include "metric.hpp"
#include <string>
#include <vector>

class Mean: public Metric<double, double> {
public:
    Mean() noexcept;
    ~Mean();
    std::string name() const;
    void process(std::vector<double> const &batch);
    void process(double datum);
    double value() const;
private:
    double total;
    unsigned int length;
};

#endif /* mean_hpp */
