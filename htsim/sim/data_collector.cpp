#include "data_collector.h"

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

void DataCollector::setDataDir(std::string data_dir) {
    data_dir_ = data_dir;
}

// Initialize the DataCollector with a config file. This function should be called only once.
void DataCollector::InitWithConfig(std::string config_file) {
    if (initialized_with_config_) {
        throw std::runtime_error("DataCollector has already been initialized with a config file");
    }
    initialized_with_config_ = true;
    std::ifstream config_stream(config_file);
    if (!config_stream.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_file);
    }
    config_ = nlohmann::json::parse(config_stream);

    data_dir_ = config_["data_dir"].get<std::string>();
    for (auto& filter_json : config_["filters"]) {
        assert(filter_json.find("regex") != filter_json.end());
        // filter will be initialized with default values. Only the fields that are present in the
        // json will be updated.
        Filter filter;
        for (auto& [key, value] : filter_json.items()) {
            if (key == "regex") {
                filter.regex = value.get<std::string>();
            } else if (key == "enabled") {
                filter.enabled = value.get<bool>();
            } else if (key == "downsampling_ratio") {
                filter.downsampling_ratio = value.get<uint64_t>();
                assert(filter.downsampling_ratio > 0);
            } else if (key == "log_every_ns") {
                filter.log_every_ns = value.get<uint64_t>();
            }
        }
        filters_.push_back(filter);
    }
}

// Destructor that exports all metrics to files. It is called when the program exits. If the
// data directory does not exist, it is created using the mkdir -p command.
DataCollector::~DataCollector() {
    // Create data directory if it does not exist.
    std::string command = "mkdir -p " + data_dir_;
    int return_value = system(command.c_str());
    if (return_value != 0) {
        std::cerr << "Failed to create directory: " + data_dir_ << std::endl;
    }

    for (auto& [name, metric] : metrics_registry_) {
        metric->ExportData(data_dir_);
    }
}

// Match the metric name with the filters that are defined in the config file. Return the first
// filter that matches the metric name. If no filter matches, return the filter with default
// values.
Filter DataCollector::MatchRegex(std::string name) {
    for (auto& filter : filters_) {
        if (std::regex_match(name, std::regex(filter.regex))) {
            return filter;
        }
    }
    return Filter();  // default filter
}

// Check if the existing metric is a CsvMetric with the same columns. If not, throw an error. The
// existing metric is the downcasted version of the metric that is already in the registry.
void DataCollector::AssertExistingCsvMetricAfterDowncast(const CsvMetric* existing,
                                                         const std::vector<std::string>& columns) {
    if (existing == nullptr) {
        throw std::runtime_error("Metric " + existing->name() +
                                 " already exists with a different type");
    }
    if (existing->columns() != columns) {
        throw std::runtime_error("Metric " + existing->name() +
                                 " already exists with different columns");
    }
}

// Register a CsvMetric with the specified name and columns. If the metric already exists, it is
CsvMetric* DataCollector::RegisterCsvMetric(std::string name,
                                            std::vector<std::string> columns,
                                            bool return_existing /*= true*/) {
    if (metrics_registry_.find(name) != metrics_registry_.end()) {
        if (return_existing) {
            // Metric already exists. Check if it is a CsvMetric with the same columns. If not,
            // throw an error.
            auto existing_metric = dynamic_cast<CsvMetric*>(metrics_registry_[name].get());
            AssertExistingCsvMetricAfterDowncast(existing_metric, columns);
            return existing_metric;
        } else {
            throw std::runtime_error("Metric " + name + " already exists");
        }
    }
    Filter chosen_filter = MatchRegex(name);
    std::cout << "Chosen filter for metric " << name << ": " << chosen_filter.str() << std::endl;
    metrics_registry_[name] = std::make_unique<CsvMetric>(name, chosen_filter.enabled, columns);
    return dynamic_cast<CsvMetric*>(metrics_registry_[name].get());
}

TimeSeriesMetric* DataCollector::RegisterTimeseriesMetric(std::string name,
                                                          std::vector<std::string> columns,
                                                          bool return_existing /*= true*/) {
    if (metrics_registry_.find(name) != metrics_registry_.end()) {
        if (return_existing) {
            // Metric already exists. Check if it is a TimeSeriesMetric with the same columns. If
            // not, throw an error.
            auto existing_metric = dynamic_cast<TimeSeriesMetric*>(metrics_registry_[name].get());
            AssertExistingCsvMetricAfterDowncast(existing_metric, columns);
            return existing_metric;
        } else {
            throw std::runtime_error("Metric " + name + " already exists");
        }
    }
    Filter chosen_filter = MatchRegex(name);
    std::cout << "Chosen filter for metric " << name << ": " << chosen_filter.str() << std::endl;
    metrics_registry_[name] = std::make_unique<TimeSeriesMetric>(
        name, chosen_filter.enabled, columns, chosen_filter.downsampling_ratio,
        chosen_filter.log_every_ns);
    return dynamic_cast<TimeSeriesMetric*>(metrics_registry_[name].get());
}

}  // namespace htsim
