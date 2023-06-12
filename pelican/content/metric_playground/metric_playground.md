Title: Computing metrics fast
Date: 2023-05-23
Category: metrics
Tags: c++, performance, metrics
Slug: computing-metrics-fast
Summary: A proof of concept for computing metrics quickly


# Motivation

My path to writing software started with analyzing biological data and a lot of my interest in computing revolves around processing and getting insights from data.

Lately, I've been frustrated with slow software.  Poor startup times, poor response times, too many tools are just not fun to use.

I've been writing a lot of Python. While it's great to quickly build something functional, the result is often not particularly fast.

# Goals

There are a lot of large, complex libraries for computing metrics over large datasets. I was curious how fast a naive implementation could be. To keep this simple I decided on the following constraints and goals:

* I want to compute metrics over numeric data.
* I want the computation to be fast. That is, how long would it take to compute metrics on 1 billion data points on my laptop?
* The program is allowed to see each data point exactly once. This is so we can use this code for streaming data in addition to reading data from files.
* Metrics are defined outside of the compute engine for easy extension (e.g. someone could write new metrics as a plugin).

Since metrics are defined outside of the compute engine, I have some metric specific criteria:

* I'm willing to sacrifice exactness if I can bound the error.
* The metrics should have bounded memory, independent of the data size. The metrics can't just store all the data in some internal buffer to get around the "seeing data only once" constraint.
* Metrics should be mergeable. That is, if the same metric is computed over 2 data sets, we can combine the results to get an aggregate result that is equivalent to computing the metric over the concatenation of the data sets.

# Metrics

I find estimating a distribution will answer most questions about one dimensional numeric data. In addition I'd like to compute some descriptive statistics. The metrics I am interested in are:

* min
* max
* mean
* a distribution estimate

To estimate the distribution I've chosen to use a variation of [DDSketch](https://arxiv.org/pdf/1908.10693.pdf). DDSketch is basically an algorithm for generating a histogram. It is a bit more complicated than a traditional histogram with fixed size bins because it scales the bin size to bound the relative error, which is a configurable parameter. For example, if we pick a relative error of 1%, the bin containing 100 will be approximately 99 to 101, while the bin containing 1000 will be approximately 990 to 1010. The bins won't exactly be this because they won't be centered exactly at 100 and 1000. The implication is we will likely have fewer bins than a histogram with fixed sized bins. We will still be able to make meaningful statements about percentiles. For example, we could use this distribution and infer that our estimate of the median is within 1% of the true empirical median.

The DDSketch paper describes 2 versions of the algorithm. The simple version has storage cost that is linear in the data size. For every data point, it determines the bin based on the relative error set by the user and increments the counter for the bin. If all the data points are spread apart, we may get a new bin for each point. For example, if the error rate is 1% and the data follows the pattern {1, 10, 100, 1000, ...}, we will get a unique bin for each data point. The paper also describes a second version where the number of bins is a fixed integer, say N. When processing data points, if an N+1 bin is needed, the algorithm collapses the 2 smallest bins into 1. This loses resolution for the lowest percentile bins. Presumably the authors chose this strategy because they care more about the high percentile bins for their use cases, e.g. measuring slow response times for a web app. I created a third version of this algorithm based on my use case. I assume the user has some expectation for the range of their data and create an algorithm with 3 parameters: the relative error, a min value, and a max value. I then fix the number of bins based on the [min, max] range and the relative error. I also create 2 extra bins for all the values smaller than min and greater than max. This idea that the user has some prior knowlege and expectations about the range of their data was inspired by the data validation library [Great Expectations](https://greatexpectations.io/).

For this project, I initially implemented the simple version of DDSketch. After observing performance, I ended up creating and implementing my third version described above. For more details see the section about the `vector` data structure below.

# Architecture

I'd like to describe the architecture and some high level implementation details. The code is all in C++ and is organized into 2 logical components: the compute engine and the metrics. All the code is available [here](https://github.com/billdirks/billdirks.github.io/tree/publish/code/metric_playground).

The metric interface lives in `metric.hpp` which defines a `Metric` template class. The important parts of the interface are:

* A `process` method which takes a batch of input data and is responsible for updating the metric's state using this data.
* A `value` method which returns the current value of the metric. It should return an accurate value after any arbitrary number of calls to `process`.

I have not yet implemented a `merge` method that would aggregate a metric after computing it over two independent data sets though all the metrics I have implemented could be extended in this fashion.

While one could envision different uses for a metric compute engine, such as processing streaming data or batch processing, for this project I have written a program that reads a data file from disk, loops over the data and calls the `process` method on each data point. At then end of processing the file, a call to `value` is made to get the final metric value. The metric engine is implemented in `compute_metrics.cpp` and we set it up and run it in `main.cpp`.

# Learnings

It took me a few passes before I settled on my final code optimizations. In this section I enumerate the lessons learned as I worked on making the compute engine faster. There were 2 classes of optimization: IO (reading the data) and compute.

## Compute

For optimizing compute, I focused on DDSketch. The other metrics were more straightforward, and while they can be more optimized, most of the compute time was spent in DDSketch. Since DDSketch basically produces a histogram, difficulties arise from updating and sorting the histogram bins as new data arrives.

There are 3 main lessons about compute:

1. Choosing the correct data structure is important.
2. Asymptotics may matter but constants also matter.
3. Measuring where time is spent is important and can be surprising.

Here are the high-level steps we do when creating a histogram: the `process` method looks at each data point, determines the bucket it falls into, and increments the counter on that bucket by one. The edges of the bucket are determined by DDSketch which is parameterized by the relative error. For this project, we've chosen a relative error of 0.01%.

There are a number of data structures that can store histogram data. I tried 3: [`map`]((https://en.cppreference.com/w/cpp/container/map)), [`unordered_map`]((https://en.cppreference.com/w/cpp/container/unordered_map)), and [`vector`](https://en.cppreference.com/w/cpp/container/vector) which are all available in the C++ standard library.

For my initial implementations using `map` and `unordered_map`, I used the simple version of DDSketch which does not bound memory.

`map`: This is a dictionary with ordered keys. Every [insert is `log(n)`](https://en.cppreference.com/w/cpp/container/map) where `n` is the current size of the `map`. If we insert `N` data points the number of operations is `log(1) + log(2) + ... + log(N)` which is `O(N log(N))`. For my initial implementation, I computed the canonical bucket value, the floating point number that is the center of a histogram bin, for each data point which I then used as a key into a `map` and incremented the key's value by 1. This was disappointingly slow.

`unordered_map`: An `unordered_map` is like a `map`, but the keys are unordered. Since a histogram is ordered data, I need to sort the `keys` on a call to `value`. In my code, I only call `value` once, after I see all the data from the input file. Inserts into an `unordered_map` are [O(1) on average](https://en.cppreference.com/w/cpp/container/unordered_map). However, we need to sort the `unordered_map` before outputting a histogram so the asymptotic time is once again `O(N log(N))`. The reason I chose to try `map` first was because the asymptotic times are the same but `map` was easier to implement since I didn't have to think about sorting. `unordered_map` proved to be much faster, around 4X for my data.

This still proved to be slow and I hoped that I could do better so I profiled the code. I compiled my code using [`gperftools`](https://gperftools.github.io/gperftools/cpuprofile.html), ran it on a test file with a 100,000,000 points, and looked at the output with `gprof`. The results were surprising. The time wasn't spent computing the bucket for a data point (my code) but instead was spent inserting numbers into the `unordered_map` (stdlib code). That is, computing the hash for the bucket was the most expensive operation I was doing.

Up to this point, I had been using the simple version of DDSketch that does not bound memory usage. However, since I was using a fixed data set of known values, the memory usage was known for these runs. I wanted to bound memory usage for an arbitrary data set. Understanding that hashing was a bottleneck provided extra motivation to bound the memory because then I could preallocate the memory and store the histogram in a list.

`vector`: A `vector` in C++ is a dynamically resizable list that is stored in a contiguous section of memory. In order to use it, I wanted to determine the amount of memory necessary up front so I could do 1 allocation and avoid copying and moving data around. I could also avoid resizing the list. To do this, I created my variant of DDSketch, described above, where I parameterize the algorithm by the relative error, the min value, and the max value. Using these values, I preallocate the appropriately sized vector to cover this range and initialize it to 0. Using this datastructure, processing new points becomes:

1. Determine the correct index, `i`,  into the vector.
2. Increment the `vector[i]` counter by 1.

If the data point is larger or smaller than the max/min, I increment the overflow or underflow counters.

This data structure allows me to avoid sorting and, since processing a new data point is O(1), this results in an O(N) algorithm. So, while the major speedup is due to eliminating hashing, rethinking the algorithm also resulted in a faster asymptotic runtime!

## IO

For this project, I wanted to use the most common and easy to use data format. My data file contains ASCII encoded floats seperated by newlines. This is a 1 column CSV file with no header.

I like computing things and I thought the interesting part of this project would be computing metrics.  However, that wasn't right. The actual performance bottleneck was reading the data from disk, i.e. IO.

Since I didn't initially think about IO at all, I implemented reading data in the most naive way possible.  I opened a file and read in the data one point at a time like this:

```
# variables
input = ifstream {filename, ios::in};
double value;

# read the data in a loop using
input >> value;
```

I basically read in a point, update the metrics, and repeated. This was horrendously slow because I was doing a separate read call for each data point.

After some searching I came across this [fantastic blog post](https://lemire.me/blog/2012/06/26/which-is-fastest-read-fread-ifstream-or-mmap/) about reading data which had some [sample benchmarking code](https://github.com/lemire/Code-used-on-Daniel-Lemire-s-blog/blob/master/2012/06/26/ioaccess.cpp). Running the benchmarks on my machine indicated that `mmap` was going to be the fastest. I also tried using `ifstream` to read in the whole file at once but `mmap` was significantly faster. I believe this is because using `mmap`, and setting the appropriate `advise` parameters, parallelized IO and compute by allowing the OS to load in the next chunks of data while computing over the current one.

# Setup for Timings

All the timings were done on my laptop which is a 1st gen M1 powerbook with 16 GB of RAM. They were done by modifying the `compute_metrics` function in `compute_metrics.cpp`. The function is defined as:
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

I use the compiler flag `-O3`. This optimizes for speed (as opposed to binary size) while keeping the computations accurate. That is, one can get faster binaries by using `-Ofast`, but one has to make assumptions about the data, and it was unclear to me if I could formally bound the error. This is something I need to investigate further.

The data was generated using the code in `random_number.cpp` which can be compiled and run as follows:
```
g++ -Wall --std=c++20 -Iinclude -O3 utils/random_number.cpp -o random_number
./random_number
```

Running it produces a 100 million normally distributed ASCII encoded numbers.

### Timings

`gtime -v <executable>` prints out some nice diagnostic information for the executable run. To determine latency I report the `Elapsed (wall clock) time`. I did timings with 2 data sets, one with 100 million data points and one with 1 billion data points. If I run the pipeline more than once in succession subsequent runs are significantly faster since the OS caches data. To control for this I reboot between timings. I report timing both the "cold" and "hot" runs.

| Name        | Number of data points | Cold Latency | Hot Latency   |
| ----------- | --------------------- | --------- | ------------ |
| Reading Data Only | 100 million     | 3.69 s    | 1.89 s      |
| Compute Histogram Only | 100 million | 4.21 s   | 2.45 s    |
| Compute Histogram and Descriptive Metrics | 100 million | 4.59 s | 2.81 s  |
| Reading Data Only |   1 billion    | 32.40 s | 17.31 s |
| Compute Histogram Only |   1 billion | 39.14 s | 23.24 s |
| Compute Histogram and Descriptive Metrics |   1 billion | 41.84 s | 26.19 s |

From these measurements we see that, despite focusing on what I thought was a computational problem, reading in the data dominates the timings. While I would need to do more timings, the cold runs suggest a possible linear scaling. If I don't reboot between runs and have other apps running (e.g. browser, editor) the 100 million data point runs are very similar to what I report here while the 1 billion data point runs are seconds slower and the "hot" runs take roughly the same amount of time as the "cold" runs.

I also examined the memory output of `gtime -v`.  The same amount of memory is being used across all runs on a particular file. While, in these cases, we are able to load all the data into memory, being able to completely load the data into memory isn't necessary for this implementation.

| Number of data points | File size | Max resident memory |
| --------------------- | --------- | ------------------- |
| 100 million           |  744 MB   |  745 MB             |
| 1 billion             | 6753 MB   | 6754 MB             |

# Comparison to Python

Since I spend a lot of time looking at Python code, I was curious how fast a similar computation in Python would be. For this quick comparison, I looked at only computing a histogram using NumPy, a common numerics package, which is a thin python wrapper around C code (pure Python is very slow). This would be comparable to the "Compute Histogram Only" numbers above, however, the Python histogram is a little simpler to compute than DDSketch. I also don't write the histogram data to a file. This gives the Python code a slight advantage. The script I used was:

```
import pandas
import numpy

data = pandas.read_csv("/path/to/data.csv", header=None)
hist = numpy.histogram(data, 80591, range=(0, 10000000))
```

| Language     | Number of data points | Latency    | Max resident memory |
| ------------ | --------------------- | ----------- | ------ |
| Python/NumPy | 100 million    | 5.84 s   | 2262 MB |
| C++          | 100 million    | 4.21 s   |  745 MB |
| Python/Numpy | 1 billion      | 48.20 s  | 8773 MB |
| C++          | 1 billion      | 39.14 s  | 6754 MB |

The Python code is significantly slower and consumes more memory but is much, much simpler to write. Most of the time in Python was also spent in IO (~4.8 s and ~40.3 s).

# Final Thoughts

This project was a lot of fun! The two things I found surprising, but are "obvious" in retrospect:

1. I thought compute would dominate the timings since this project was about summarizing data but IO dominates. The reason is, I'm not doing very many operations on any data point and the most complicated operation I'm doing is taking a log.
2. Using a map was slow since computing hashes is more expensive than computing the index (bucket id) into a vector. Having written mostly Python recently where dictionaries are a very common data structure it was good to be reminded of their cost.

I also wanted to highlight 2 lessons that I took away:

1. Experimenting by benchmarking is very helpful. I was lucky that I came across Daniel Lemire's blog and could use his code to do timings of different IO strategies.
2. Profiling code is helpful and often surprising. I was very focused on the map data structures and it took the nudge from the profiling results to make me think of a different strategy altogether.

Even though my code has a lot of shortcomings (e.g. I assume the data is well-formed and only contains 1 column), it did teach me about data processing bottlenecks. I see why people develop new data formats and why so much attention is spent on IO.

Thanks for reading!
