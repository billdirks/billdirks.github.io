#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "metric.hpp"
#include "ddsketch_bounded.hpp"
#include "compute_metrics.hpp"
#include <functional>
#include <chrono>

using namespace std;


int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "This command requires at least 1 argument, a file path.\n";
        exit(126);
    }
    
    // We should allow the user to set this via flags
    double accuracy = 0.0001;
    double hist_upper_bound = 10000000;
    ComputeMetricOutput computed_metrics_output {accuracy, hist_upper_bound};
    
    auto compute_start_time = std::chrono::high_resolution_clock::now();
    compute_metrics(argv[1], &computed_metrics_output);
    double compute_time = duration_in_seconds(compute_start_time);

    auto output_hist_start_time = std::chrono::high_resolution_clock::now();
    const BoundedHistogram & histogram = computed_metrics_output.ddsketch.value();
    cout << "underflow: " << histogram.underflow << "\n";
    for (int i = 0; i < histogram.histogram->size(); ++i) {
        int value = (*histogram.histogram)[i];
        if (value > 0) {
            cout << computed_metrics_output.ddsketch.canonical_value(i) << ": " << value << "\n";
        }
    }
    cout << "overflow: " << histogram.overflow << "\n";
    cout << "\n";
    double output_hist_time = duration_in_seconds(output_hist_start_time);

    auto output_metric_start_time = std::chrono::high_resolution_clock::now();
    for (auto &metric : computed_metrics_output.metrics) {
        cout << metric->name() << ": " << metric->value() << "\n"; 
    }
    double output_metric_time = duration_in_seconds(output_metric_start_time);

    cout << "Compute time: " << compute_time << " s\n";
    cout << "Output hist metric time: " << output_hist_time << " s\n";
    cout << "Output other metric time: " << output_metric_time << " s\n";
    return 0;
}