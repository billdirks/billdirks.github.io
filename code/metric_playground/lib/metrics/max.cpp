//
//  max.cpp
//  metric_playground
//
//  Created by Bill Dirks
//

#include <stdio.h>
#include <stdexcept>
#include "max.hpp"

Max::Max() noexcept: _max {0}, isInf {true}, infSign{-1} {};

std::string Max::name() const {
    return "simple max";
}

void Max::process(std::vector<double> const &batch) {
    for (const auto datum: batch) {
        if ((isInf and infSign == -1) || (!isInf and datum > _max)) {
            _max = datum;
            isInf = false;
            infSign = 0;
        }
    }
}

void Max::process(double datum) {
    if ((isInf and infSign == -1) || (!isInf and datum > _max)) {
        _max = datum;
        isInf = false;
        infSign = 0;
    }
}

double Max::value() const {
    return _max;
}
