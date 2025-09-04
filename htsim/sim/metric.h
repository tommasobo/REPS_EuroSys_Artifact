#ifndef HTSIM_SIM_METRIC_H_
#define HTSIM_SIM_METRIC_H_

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "eventlist.h"

namespace htsim {

// Defines a metric that can be logged and exported to a file.
// A Metric can be enabled or disabled. When disabled, they do not log data.
// Also, a Metric can be exported to a file. The format of the file depends on the type of Metric.
class Metric {
public:
    // Ensure that the default constructor is not used to enforce the use of the
    // constructor with arguments.
    // Metric() = delete;
    Metric(std::string name, bool enable) : name_(name), enable_(enable) {}

    // Define a virtual destructor to ensure that the destructor of the derived class is called.
    virtual ~Metric() = default;

    // Exports the data to a file in the specified directory.
    virtual void ExportData(std::string data_dir) = 0;

    // Getters.
    virtual std::string name() const { return name_; }
    virtual bool enable() const { return enable_; }

protected:
    // Name of the metric. It is used as the file name when exporting data. It should be unique
    // among all metrics. It is defined as const to ensure that it is not modified after creation.
    const std::string name_;
    // Whether the metric is enabled. If disabled, the metric does not log data or export data.
    bool enable_;
};

// Defines a metric that logs data in memory and exports it to a CSV file. It has a list of columns
// that define the data format of the CSV file.
class CsvMetric : public Metric {
public:
    CsvMetric(std::string name, bool enable, std::vector<std::string> columns);

    // Exports the data to a CSV file in the specified directory.
    void ExportData(std::string data_dir) override;

    // Logs the data sample. The data should have the same number of elements as the columns.
    virtual void LogData(std::vector<std::string> data);

    // Getters.
    virtual std::vector<std::string> columns() const { return columns_; }
    const std::vector<std::vector<std::string>>& data() const { return data_; }


protected:
    // Columns of the CSV file. Each column should have a name that describes the data.
    std::vector<std::string> columns_;
    // Data logged in memory. Each element of the vector is a row of data corresponding to the
    // columns.
    std::vector<std::vector<std::string>> data_;
};

class TimeSeriesMetric : public CsvMetric {
public:
    TimeSeriesMetric(std::string name,
                     bool enable,
                     std::vector<std::string> columns,
                     uint32_t downsampleFactor = 1,
                     uint32_t logEveryNs = 0);

    void LogData(std::vector<std::string> data) override;

    // Only return the data columns that are not the timestamp.
    std::vector<std::string> columns() const override {
        return std::vector<string>(columns_.begin() + 1, columns_.end());
    }

private:
    // Downsample factor for the metric. The metric logs data every downsampleFactor_ calls to
    // LogData (>= 1, 1 means log every event).
    uint32_t downsampleFactor_;
    // Skip next log calls to downsample the data.
    uint64_t skipNextLogCalls_;
    // Have at most one log entry per logEveryNs_ nanoseconds interval (>= 0, 0 means log every
    // event).
    uint32_t logEveryNs_;
    // Time of the last sample in nanoseconds.
    uint64_t lastSampleTimeNs_;
};

}  // namespace htsim

#endif  // HTSIM_SIM_METRIC_H_