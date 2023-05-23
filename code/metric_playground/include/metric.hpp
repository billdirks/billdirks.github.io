//
//  metric.hpp
//  Interface for metrics
//
//  Created by Bill Dirks
//

#ifndef metric_hpp
#define metric_hpp

#include <memory>
#include <string>
#include <vector>

template <typename Input, typename Output>
class Metric {
public:
    virtual ~Metric() = default;
    virtual std::string name() const = 0;
    virtual void process(std::vector<Input> const &batch) = 0;
    virtual void process(Input datum) = 0;
    virtual Output value() const = 0;
};


#endif /* metric_hpp */
