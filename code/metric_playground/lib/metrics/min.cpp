//
//  min.cpp
//  metric_playground
//
//  Created by Bill Dirks
//

#include <stdio.h>
#include <stdexcept>
#include "min.hpp"
#include <memory>
#include <vector>

Min::Min() noexcept: _min {0}, isInf {true}, infSign{1} {};

std::string Min::name() const {
    return "simple min";
}

void Min::process(std::vector<double> const &batch) {
    for (auto datum: batch) {
        if ((isInf and infSign == 1) || (!isInf and datum < _min)) {
            _min = datum;
            isInf = false;
            infSign = 0;
        }
    }
}

void Min::process(double datum) {
    if ((isInf and infSign == 1) || (!isInf and datum < _min)) {
        _min = datum;
        isInf = false;
        infSign = 0;
    }
}

double Min::value() const {
    return _min;
}
