// Probably included too many things
// https://en.cppreference.com/w/cpp/numeric/random
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>

int main()
{
    // Seed with a real random value, if available
    std::random_device r;

    // Choose a random mean between 10000 and 1000000
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(10000, 1000000);
    int mean = uniform_dist(e1);
    // std::cout << "Randomly-chosen mean: " << mean << '\n';

    std::uniform_int_distribution<int> uniform_dist2(50, 1000);
    int std = uniform_dist2(e1);

    // Generate a normal distribution around that mean
    std::seed_seq seed2{r(), r(), r(), r(), r(), r(), r(), r()};
    std::mt19937 e2(seed2);
    std::normal_distribution<> normal_dist(mean, std);

    // Makes 100 million random numbers
    for (int n = 0; n != 100000000; ++n) {
        // ensure x is postive and >= 1
        auto x = std::abs(normal_dist(e2)) + 1;
        std::cout << x << "\n";
    }
}
