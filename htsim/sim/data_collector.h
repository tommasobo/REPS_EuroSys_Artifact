#ifndef HTSIM_SIM_DATA_COLLECTOR_H_
#define HTSIM_SIM_DATA_COLLECTOR_H_

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "metric.h"
#include "third_party/json.hpp"

namespace htsim {

struct Filter {
    // Regular expression to match the metric name. If the regex matches the metric name, the filter
    // is applied.
    std::string regex = ".*";
    // Whether to enable the metrics that match the regex. If disabled, the metric is not logged or
    // exported.
    bool enabled = true;
    // Only applies to timeseries data. Specifies the downsampling ratio to log data. If 1, all data
    // is logged.
    uint64_t downsampling_ratio = 1;
    // Only applies to timeseries data. Specifies the interval in nanoseconds to log data. The first
    // sample in a log_every_ns interval is logged, and the rest are ignored.
    uint64_t log_every_ns = 0;

    inline std::string str() {
        return "Filter: regex=" + regex + ", enabled=" + std::to_string(enabled) +
               ", downsampling_ratio=" + std::to_string(downsampling_ratio);
    }
};

// DataCollector is a singleton class that manages all metrics in the simulation. It is responsible
// for creating and exporting metrics. It also applies filters to metrics based on their names. The
// filters are defined in a config file. The data is exported to a specified directory when the
// DataCollector is destroyed. The data directory is specified in the config file. If the directory
// is not specified, the default directory kDefaultDataDir is used.
// Config file format:
// {
//     "data_dir": "./data/",
// "filters": [
//     {
//         "regex": "rtt.*",
//         "enabled": true,
//         "downsampling_ratio": "3",
//         "log_every_ns": "1000"
//     },
//     ...
// ]
// }
class DataCollector {
public:
    static inline DataCollector& get_instance() {
        static DataCollector instance;
        return instance;
    }

    // Initialize the DataCollector with a config file. This function should be called only once.
    void InitWithConfig(std::string config_file);

    void setDataDir(std::string data_dir);

    // Register a CsvMetric with the specified name and columns. If the metric already exists, it is
    // returned if return_existing is true, else it fails.
    CsvMetric* RegisterCsvMetric(std::string name,
                                 std::vector<std::string> columns,
                                 bool return_existing = true);

    // Register a TimeseriesMetric with the specified name and columns. If the metric already
    // exists, it is returned if return_existing is true, else it fails.
    TimeSeriesMetric* RegisterTimeseriesMetric(std::string name,
                                               std::vector<std::string> columns,
                                               bool return_existing = true);

private:
    static constexpr const char* kDefaultDataDir = "./output_metrics/";

    // Private constructor to ensure that the DataCollector is a singleton.
    DataCollector() {
        data_dir_ = kDefaultDataDir;
        initialized_with_config_ = false;
    };
    // Destructor that exports all metrics to files. It is called when the program exits. If the
    // data directory does not exist, it is created using the mkdir -p command.
    ~DataCollector();

    // Match the metric name with the filters that are defined in the config file. Return the first
    // filter that matches the metric name. If no filter matches, return the filter with default
    // values.
    Filter MatchRegex(std::string name);
    // Check if the existing metric is a CsvMetric with the same columns. If not, throw an error.
    // The existing metric is the downcasted version of the metric that is already in the registry.
    void AssertExistingCsvMetricAfterDowncast(const CsvMetric* existing,
                                              const std::vector<std::string>& columns);

    // The DataCollector global instance only be initialized once with a config file.
    bool initialized_with_config_;
    nlohmann::json config_;

    std::vector<Filter> filters_;  // If empty, the default filter is applied.
    std::string data_dir_;         // Directory to export the data. Default is kDefaultDataDir.
    std::map<std::string, std::unique_ptr<Metric>>
        metrics_registry_;  // map: metric_name -> Metric unique_ptr
};

}  // namespace htsim

#endif  // HTSIM_SIM_DATA_COLLECTOR_H_
