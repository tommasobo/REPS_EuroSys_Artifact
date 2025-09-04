
#include "metric.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace htsim {

CsvMetric::CsvMetric(std::string name, bool enable, std::vector<std::string> columns)
    : Metric(name, enable) {
    if (columns.size() == 0) {
        throw std::runtime_error("Columns for metric " + name + " should not be empty");
    }
    this->columns_ = columns;
}

// Logs the data sample. The data should have the same number of elements as the columns.
void CsvMetric::LogData(std::vector<std::string> data) {
    if (!enable_) {
        return;
    }
    // Log data in memory for exporting later.
    if (data.size() != columns_.size()) {
        throw std::runtime_error("Data size for metric " + name_ + " does not match column size");
    }
    data_.push_back(data);
}

// Exports the data to a CSV file in the specified directory.
void CsvMetric::ExportData(std::string data_dir) {
    // Do not export if the metric is disabled or if there is no data.
    if (!enable_ || data_.size() == 0) {
        return;
    }
    // Export data to a CSV file.
    std::string file_path = data_dir + "/" + name_ + ".csv";
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    // Write columns.
    for (size_t i = 0; i < columns_.size(); i++) {
        file << columns_[i];
        if (i < columns_.size() - 1) {
            file << ",";
        }
    }
    file << std::endl;
    // Write data.
    for (size_t i = 0; i < data_.size(); i++) {
        for (size_t j = 0; j < data_[i].size(); j++) {
            file << data_[i][j];
            if (j < data_[i].size() - 1) {
                file << ",";
            }
        }
        file << std::endl;
    }
    // Close file.
    file.close();
}

TimeSeriesMetric::TimeSeriesMetric(std::string name,
                                   bool enable,
                                   std::vector<std::string> columns,
                                   uint32_t downsampleFactor,
                                   uint32_t logEveryNs)
    : CsvMetric(name, enable, columns),
      downsampleFactor_(downsampleFactor),
      skipNextLogCalls_(0),
      logEveryNs_(logEveryNs),
      lastSampleTimeNs_(0) {
    if (downsampleFactor_ < 1) {
        throw std::runtime_error("downsampleFactor must be >= 1");
    }
    this->columns_.insert(this->columns_.begin(), "timeNs");
}

void TimeSeriesMetric::LogData(std::vector<std::string> data) {
    if (!enable_) {
        return;
    }

    uint64_t nowNs = timeAsNs(EventList::getTheEventList().now());
    // Skip the next log call if skipNextLogCalls_ is greater than 0.
    if (skipNextLogCalls_ > 0) {
        skipNextLogCalls_--;
        return;
    }
    if (logEveryNs_ > 0) {
        if (data_.size() > 0 && nowNs - lastSampleTimeNs_ < logEveryNs_) {
            return;
        } else {
            // lastSampleTimeNs_ is the nearest multiple of logEveryNs_ that is less than or equal
            // to nowNs.
            lastSampleTimeNs_ = (nowNs / logEveryNs_) * logEveryNs_;
        }
    }
    // Sample can be logged after the above checks.
    // Reset skipNextLogCalls_.
    skipNextLogCalls_ = downsampleFactor_ - 1;
    data.insert(data.begin(), std::to_string(nowNs));
    CsvMetric::LogData(data);
}

}  // namespace htsim
