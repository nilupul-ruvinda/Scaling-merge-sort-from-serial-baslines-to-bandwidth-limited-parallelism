#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <omp.h>

#include "data_generators.hpp"
#include "verifier.hpp"
#include "sort_variants.hpp"

int main(int argc, char* argv[]) {
    int variant = 1;
    size_t n = 50000000;
    int threads = 1;
    std::string dist_str = "uniform";
    bool silent = false;

    // Minimal CLI Parser
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--variant=", 0) == 0) {
            variant = std::stoi(arg.substr(10));
        } else if (arg.rfind("--size=", 0) == 0) {
            n = std::stoull(arg.substr(7));
        } else if (arg.rfind("--threads=", 0) == 0) {
            threads = std::stoi(arg.substr(10));
        } else if (arg.rfind("--dist=", 0) == 0) {
            dist_str = arg.substr(7);
        } else if (arg == "--silent") {
            silent = true;
        }
    }

    DistributionType dist_type = parse_distribution_type(dist_str);
    omp_set_num_threads(threads);

    // 1. Upfront arena allocation (O(n) primary + O(n) aux scratchpad)
    std::vector<int64_t> arr(n);
    std::vector<int64_t> aux(n);

    // 2. Synthesize dataset
    generate_data(arr, dist_type, 42);

    // Keep an untimed reference copy to verify correctness
    std::vector<int64_t> ref_arr = arr;

    // 3. First-touch write pass to resolve demand-paging faults before timing
    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < n; ++i) {
        aux[i] = 0;
    }

    // 4. Timed sorting execution
    auto start_time = std::chrono::steady_clock::now();

    switch (variant) {
        case 1:
            serial_merge_sort(arr.data(), aux.data(), 0, n - 1);
            break;
        case 2:
            #pragma omp parallel
            {
                #pragma omp single
                omp_divide_merge_sort(arr.data(), aux.data(), 0, n - 1, 0);
            }
            break;
        case 3:
            #pragma omp parallel
            {
                #pragma omp single
                omp_blelloch_merge_sort(arr.data(), aux.data(), 0, n - 1, 0);
            }
            break;
        default:
            std::cerr << "Invalid variant: " << variant << std::endl;
            return 1;
    }

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    // 5. Verification
    ValidationResult val = verify_results(arr, ref_arr);
    if (!val.is_sorted || !val.checksum_match) {
        std::cerr << "[-] Error: Sort validation failed!" << std::endl;
        return 1;
    }

    if (!silent) {
        std::cout << "========================================================\n";
        std::cout << "[✓] Variant:      " << variant << "\n";
        std::cout << "[✓] Elements:     " << n << "\n";
        std::cout << "[✓] Threads:      " << threads << "\n";
        std::cout << "[✓] Distribution: " << dist_str << "\n";
        std::cout << "[✓] Sorted:       PASSED\n";
        std::cout << "[✓] Checksum:     MATCHED\n";
        std::cout << "[✓] Wall Time:    " << elapsed.count() << " seconds\n";
        std::cout << "[✓] Throughput:   " << (static_cast<double>(n) / 1e6) / elapsed.count() 
                  << " M elements/sec\n";
        std::cout << "========================================================\n";
    }

    return 0;
}
