//
//  compute_metrics.hpp
//  metric_playground
//
//  Created by Bill Dirks
//

#ifndef compute_metrics_hpp
#define compute_metrics_hpp

#include <stdio.h>
#include <unordered_map>
#include "metric.hpp"
#include "ddsketch_bounded.hpp"
#include <memory>
#include "min.hpp"
#include "max.hpp"
#include "mean.hpp"

struct ComputeMetricOutput {
    std::vector<std::unique_ptr<Metric<double, double>>> metrics;
    DDSketchBounded ddsketch;

    ComputeMetricOutput(double ddsketch_accuracy, double ddsketch_upper_bound):
    metrics {},
    ddsketch {ddsketch_accuracy, ddsketch_upper_bound} {
        metrics.push_back(std::make_unique<Min>());
        metrics.push_back(std::make_unique<Max>());
        metrics.push_back(std::make_unique<Mean>());
    };
};

void compute_metrics(char* filename, ComputeMetricOutput* computed_metrics);
double duration_in_seconds(std::chrono::steady_clock::time_point start_time);

#endif /* compute_metrics_hpp */
