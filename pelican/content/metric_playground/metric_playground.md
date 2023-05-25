Title: Building a fast metric pipeline
Date: 2023-05-23
Category: metrics
Tags: c++, performance, metrics
Slug: building-a-fast-metric-pipeline
Summary: Building a proof of concept for a fast metric pipeline


# Motivation

Lately, I've been frustrated with slow software.  Poor startup times, poor response times, it feels like a slog to use a lot of tools.

My path to writing software started with analyzing biological data and a lot of my interest in computing revolves around processing and getting insights from data.

Lately I've been writing a lot of Python. While it's great to build functional things quickly, a downside is, it is slow.

# Goals

For this project I wanted to build a metric pipeline meeting the following criteria:

* I want to compute metrics over numeric data.
* I want the computation to be fast. That is, how long would it take to compute metrics on 1 billion data points on my laptop?
* The pipeline is allowed to see each data point exactly once.
    * This is so we can use the pipeline for streaming data or reading files.
    * It encourages writing O(N) metrics
* Metrics are defined outside of the core pipeline for easy extension.

Since metrics are defined outside of the main pipeline, I have some metric specific criteria:

* For metrics, I'm willing to sacrifice exactness if I can bound the error.
* The metrics should have bounded memory, independent of the data size. That is, the metric can't just store all the data to get around the seeing data once criteria.
* Metrics should be summable. That is, if the same metric is computed over 2 data sets, we can combine the results to get an aggregate result.

# Metrics

I find estimating a distribution will answer most questions about one dimensional numeric data. In addition I'd like to compute some descriptive statistics. The metrics I am interested in are:

* min
* max
* mean
* a distribution estimate

To estimate the distribution I've chosen to use a very slight variation of [DDSketch](https://arxiv.org/pdf/1908.10693.pdf). DDSketch is basically an algorithm for generating a histogram. It is a bit more complicated than a traditional histogram with fixed size bins because it scales the bin size to bound the relative error. That means we will likely have less bins overall and we can still make meaningful statements about percentiles. For example, we could use this distribution and infer that our estimate of the median is within 0.01% of the true empirical median.

# Pipeline architecture

I'd like to describe the architecture and some high level implementation details. The code is all in C++ and is organized into 2 logical components: the pipeline and the metrics. All the code is available [here](https://github.com/billdirks/billdirks.github.io/tree/publish/code/metric_playground).

The metric interface lives in `metric.hpp` which defines a `Metric` template class. The important parts of the interface are:

* A `process` method which takes a batch of input data and is responsible for updating the metric's state using this data.
* A `value` method which returns the current value of the metric. It should be callable and return an accurate value after any arbitrary number of calls to `process`.

I have not yet implemented an `add` method to allow combining of metrics though all the current metrics could support this type of operation. An `add` method would allow one to distribute computations over many nodes and then combine them afterwards.

While one could envision different uses for a metrics pipeline, such as processing streaming data or batch processing, for this project I have written a pipeline that reads a data file from disk, looping over the data and calling the metrics `process` method on each data point. The pipeline is implemented in `compute_metrics.cpp` and we setup and run it in `main.cpp`.

# Learnings

It took me a few passes before I settled on my final code optimizations. In this section I enumerate the lessons learned and the things that mattered to make progress. There were 2 classes of optimization: IO (reading the data) and compute.

## Compute

For optimizing compute, I focus on DDSketch. The other metrics were more straightforward, and while they likely can be more optimized, most of the compute time was spent in DDSketch. Since DDSketch basically produces a histogram, difficulties arise from storing and sorting the histogram as new data arrives.

There are 3 main lessons about compute:

1. Choosing the correct data structure is important.
2. Asymptotics may matter but constants also matter.
3. Measuring where time is spent is important and can be surprising.

Here are the high level steps we do when creating a histogram. The `process` method looks at each point in a batch, determines the bucket it falls into, and increments the counter on that bucket by one. The edges of the bucket are determined by DDSketch which is parameterized by the relative error we allow, in our case 0.01%. A call to `value` will produce the current histogram.

There are a number of data structures that can store histogram data. I tried 3: `map`, `unordered_map`, and `vector` which are all available in the C++ standard library.

`map`: This is a dictionary with an ordering on the keys. So every [insert is `log(n)`](https://en.cppreference.com/w/cpp/container/map) where `n` is the current size of the `map`. If we insert `N` data points the number of operations is `log(1) + log(2) + ... + log(N)` which is `O(N log(N))`. For my initial implementation, I computed the bucket for each data point which I used as a key into a `map` and incremented the key's value by 1. This was disappointingly slow.

`unordered_map`: An `unordered_map` is like a `map`, however, the keys are unordered. Since a histogram is ordered data, I end up ordering the `keys` on a call to `value`. In my code, I only call `value` once, after I see all the data from the input file. Inserts into an `unordered_map` are [O(1) on average](https://en.cppreference.com/w/cpp/container/unordered_map). However, we need to sort the `unordered_map` before outputting a histogram so the asymptotic time is once again `O(N log(N))`. The reason I chose to try `map` first was because the asymptotic times are the same but `map` was easier to implement since I didn't have to think about sorting. `unordered_map` proved to be much faster, around 4X for my data.

This still proved to be slow and I hoped that I could do better so I profiled the code. I compiled my code using `gperftools`, ran it on a test file with 100,000,000 points, and looked at the output with `gprof`. The results were surprising. The time wasn't spent computing the bucket for a data point (my code) but instead was spent inserting numbers into the `unordered_map` (stdlib code). That means, computing the hash for the bucket, was the most expensive operation I was doing.

I hadn't thought too hard about implementing a bound on memory usage at this point. However, now that I understood how expensive hashing was, I wanted to predetermine the amount of memory needed and store the histogram in a list. The bucket produced by DDSketch is an integer representing the first bucket, second bucket, etc, so can be used as indexes into a list.

`vector`: A `vector` in C++ is a dynamically resizable list that is stored in a contiguous section of memory. In order to use it, I wanted to determine the amount of memory necessary up front so I could do 1 allocation and avoid copying and moving data around. DDSketch's strategy to bound memory usage is to predetermine the number of buckets allowed and to combine the smallest index buckets when the maximum number of buckets is reached. I'm guessing this is because the authors are interested in accurately estimating the highest value percentiles (eg the p99 response time for web requests for an app). My use case is more inspired by data quality so I follow [Great Expectations](https://greatexpectations.io/) lead and presume the user has some idea of the range the data should lie in. So this implementation requires the user to provide a lower and upper bound for the data. I then use these bounds to determine the number of buckets. I also add 2 additional buckets that are used when the data falls below or above this range. Using this strategy, I can preallocate the needed memory. The `vector` data structure also lets me avoid sorting and, since inserts are O(1), this results in an O(N) algorithm. So, while the major speedup is due to eliminating hashing, rethinking the algorithm also resulted in a faster asymptotic runtime!

## IO

I like computing things and I thought the interesting part of this project would be computing metrics.  However, that wasn't right. The actual performance bottleneck was reading the data from disk, ie IO.

Since I didn't initially think about IO at all, I implemented reading data in the most naive way possible.  I opened a file and read in the data one point at a time like this:

```
# variables
input = ifstream {filename, ios::in};
double value;

# read the data in a loop using
input >> value;
```

I initially picked a batch size, read in at most batch size points, and updated the metrics before starting the next cycle. This was horrendously slow primarily because I was doing a separate read call for each data point.

After some searching I came across this [fantastic blog post](https://lemire.me/blog/2012/06/26/which-is-fastest-read-fread-ifstream-or-mmap/) about reading data which had some [sample benchmarking code](https://github.com/lemire/Code-used-on-Daniel-Lemire-s-blog/blob/master/2012/06/26/ioaccess.cpp). Running the benchmarks on my machine indicated that `mmap` was going to be the fastest. I did try using `ifstream` to read in the whole file at once but `mmap` was significantly faster.

# Setup for Timings

All the timings were done on my laptop which is a 1st gen M1 powerbook with 16 GB of RAM. They  were done by modifying the `compute_metrics` function in `compute_metrics.cpp`. The function is defined as:
```
void compute_metrics(const MmapFile mmap_file, ComputeMetricOutput* computed_metrics) {
    char* cur_ptr = mmap_file.start();
    char* end_ptr = mmap_file.end();
    while (cur_ptr != end_ptr) {
      <CODE>
    }
}
```
Where `<CODE>` is one of three code snippets:

**Reading Data Only**
```
strtod(cur_ptr, &cur_ptr);
```

**Compute Histogram Only**
```
double d = strtod(cur_ptr, &cur_ptr);
computed_metrics->ddsketch.process(d);
```

**Compute Histogram and Descriptive Metrics**
```
double d = strtod(cur_ptr, &cur_ptr);
computed_metrics->ddsketch.process(d);
for (int i = 0; i < computed_metrics->metrics.size(); ++i) {
    computed_metrics->metrics[i]->process(d);
}
```

The code was built and run using the command:
```
make clean
make compute_opt
rm o
gtime -v build/compute data/<datafile> > o
```

I use the compiler flag `-O3`. This optimizes for speed (as opposed to binary size) while keeping the computations accurate. That is, one can get faster binaries if one is willing to do inaccurate math, but this is antithetical to what I am trying to do.

The data was generated using the code in `random_number.cpp` which can be compiled as run as follows:
```
g++ -Wall --std=c++20 -Iinclude -O3 utils/random_number.cpp -o random_number
./random_number
```

Running it produces a 100 million normally distributed ascii encoded numbers.

### Timings

`gtime -v` prints out some nice diagnostic information about a pipeline run. To determine speed I report the `Elapsed (wall clock) time`. I did timings with 2 data sets, one with 100 million data points and one with 1 billion data points. If I run the pipeline more than once in succession subsequent runs are significantly faster since the OS caches data. To normalize for this I reboot between timings. I report timing both the "cold" and "hot" runs.

| Name        | Number of data points | Cold Speed | Hot Speed   |
| ----------- | --------------------- | --------- | ------------ |
| Reading Data Only | 100 million     | 3.69 s    | 1.89 s      |
| Compute Histogram Only | 100 million | 4.21 s   | 2.45 s    |
| Compute Histogram and Descriptive Metrics | 100 million | 4.59 s | 2.81 s  |
| Reading Data Only |   1 billion    | 32.40 s | 17.31 s |
| Compute Histogram Only |   1 billion | 39.14 s | 23.24 s |
| Compute Histogram and Descriptive Metrics |   1 billion | 41.84 s | 26.19 s |

From these measurements we see that, despite focusing on what I thought was a computational problem, reading in the data dominates the timings. While I would need to do more timings, the cold runs suggest a possible linear scaling. If I don't reboot between runs and have other apps running (eg browser, editor) the 100 million data point runs are very similar to what I report here while the 1 billion data point runs are seconds slower and the "hot" runs take the same amount of time as the "cold" runs.

I also examined the memory output of `gtime -v`.  Both data sets fit completely into memory and, for a particular data set, the same amount of memory is used across runs.

| Number of data points | File size | Max resident memory |
| --------------------- | --------- | ------------------- |
| 100 million           |  744 MB   |  745 MB             |
| 1 billion             | 6753 MB   | 6754 MB             |

# Comparison to Python

Since I spend a lot of time looking at Python code, I was curious how fast a similar computation would be there. For this quick comparison, I only looked at computing a histogram using numpy. This would be comparable to the "Compute Histogram Only" numbers, however, the Python histogram is a little simpler to compute than DDSketch. I also don't write the histogram data to a file. This gives the Python code a slight advantage. The script I used was:

```
import pandas
import numpy

data = pandas.read_csv("/path/to/data.csv", header=None)
hist = numpy.histogram(data, 80591, range=(0, 10000000))
```

| Language    | Number of data points | Speed       | Max resident memory |
| ----------- | --------------------- | ----------- | ------ |
| Python      | 100 million    | 5.84 s   | 2262 MB |
| C++         | 100 million    | 4.21 s   |  745 MB |
| Python      | 1 billion      | 48.20 s  | 8773 MB |
| C++         | 1 billion      | 39.14 s  | 6754 MB |

The Python code is significantly slower and consumes more memory but is much, much simpler. Most of the time in Python was also spent in IO (~4.8 s and ~40.3 s).

# Final Thoughts

This project was a lot of fun! The two things I found surprising, but are "obvious" in retrospect:

1. I thought compute would dominate the timings since this project was about summarizing data but IO dominates. The reason is, I'm not doing very many operations on any data point and the most complicated operation I'm doing is taking a log.
2. Using a map was slow since computing hashes is more expensive than computing the index (bucket id) into a vector. Having written mostly Python recently where dictionaries are a very common data structure it was good to be reminded of their cost.

I also wanted to highlight 2 lessons that I took away:

1. Experimenting by benchmarking is very helpful. I was lucky that I came across Daniel Lemire's blog and could use his code to do timings of different IO strategies.
2. Profilings code is helpful and often surprising. I was very focused on the map data structures and it took the nudge from profiling to make me think of a different strategy altogether.

Even though my code has a lot of shortcomings (eg I assume the data is well formed and only contains 1 column), it did teach me about data processing bottlenecks. I see why people develop new data formats and why so much attention is spent on IO.

Thanks for reading!
