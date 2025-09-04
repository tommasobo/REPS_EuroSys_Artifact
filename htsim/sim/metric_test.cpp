#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "metric.h"

namespace fs = std::filesystem;

namespace htsim {

// A dummy event source that does nothing and is used only for testing to advance the time.
class TestDummyEventSource : public EventSource {
public:
    TestDummyEventSource() : EventSource("dummy_event_source") {}

    void doNextEvent() override {}
};

class MetricTest : public ::testing::Test {
protected:
    std::string tempDirPath_;
    std::string metricName_ = "test_metric";
    std::vector<std::string> columns_ = {"column1", "column2"};

    MetricTest() {}

    void SetUp() override {
        // Create a unique temporary directory for each test.
        char tempDirTemplate[] = "/tmp/metric_test_output_XXXXXX";
        char* result = mkdtemp(tempDirTemplate);
        if (result == nullptr) {
            std::cerr << "Failed to create temporary directory\n";
            exit(1);
        }
        tempDirPath_ = std::string(result);
    }

    void TearDown() override {
        // Reset the event list after each test.
        EventList::resetTheEventListForTesting();
        // Remove the temporary directory and its contents after the test.
        try {
            if (fs::exists(tempDirPath_) && fs::is_directory(tempDirPath_)) {
                fs::remove_all(tempDirPath_);
            } else {
                std::cerr << "Temporary directory does not exist or is not a directory.\n";
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << '\n';
        }
    }

    // Advance the time by the specified number of nanoseconds.
    void AdvanceTimeNs(uint64_t timeNs) {
        EventList::getTheEventList().sourceIsPendingRel(dummyEventSource_, timeFromNs(timeNs));
        EventList::getTheEventList().doNextEvent();
    }

private:
    TestDummyEventSource dummyEventSource_;
};

TEST_F(MetricTest, EnabledCsvMetricTest) {
    bool enabled = true;
    CsvMetric metric(metricName_, enabled, columns_);

    metric.LogData({"data1_1", "data1_2"});
    AdvanceTimeNs(1000);
    metric.LogData({"data2_1", "data2_2"});
    metric.ExportData(tempDirPath_);

    std::string file_path = tempDirPath_ + "/" + metric.name() + ".csv";
    std::ifstream file_stream(file_path);
    EXPECT_TRUE(file_stream.is_open());
    std::string line;
    std::getline(file_stream, line);
    EXPECT_EQ(line, "column1,column2");
    std::getline(file_stream, line);
    EXPECT_EQ(line, "data1_1,data1_2");
    std::getline(file_stream, line);
    EXPECT_EQ(line, "data2_1,data2_2");
    file_stream.close();
}

TEST_F(MetricTest, DisabledCsvMetricTest) {
    bool enabled = false;
    CsvMetric metric(metricName_, enabled, columns_);

    metric.LogData({"data1_1", "data1_2"});
    AdvanceTimeNs(1000);
    metric.LogData({"data2_1", "data2_2"});
    metric.ExportData(tempDirPath_);

    std::string file_path = tempDirPath_ + "/" + metric.name() + ".csv";
    EXPECT_FALSE(fs::exists(file_path));
}

TEST_F(MetricTest, EmptyCsvMetricTest) {
    bool enabled = true;
    CsvMetric metric(metricName_, enabled, columns_);

    metric.ExportData(tempDirPath_);

    std::string file_path = tempDirPath_ + "/" + metric.name() + ".csv";
    EXPECT_FALSE(fs::exists(file_path));
}

TEST_F(MetricTest, EnabledTimeSeriesMetricTest) {
    bool enabled = true;
    TimeSeriesMetric metric(metricName_, enabled, columns_);

    metric.LogData({"data1_1", "data1_2"});
    AdvanceTimeNs(1000);
    metric.LogData({"data2_1", "data2_2"});
    metric.ExportData(tempDirPath_);

    std::string file_path = tempDirPath_ + "/" + metric.name() + ".csv";
    std::ifstream file_stream(file_path);
    EXPECT_TRUE(file_stream.is_open());
    std::string line;
    std::getline(file_stream, line);
    EXPECT_EQ(line, "timeNs,column1,column2");
    std::getline(file_stream, line);
    EXPECT_EQ(line, "0,data1_1,data1_2");
    std::getline(file_stream, line);
    EXPECT_EQ(line, "1000,data2_1,data2_2");
    file_stream.close();
}

TEST_F(MetricTest, DisabledTimeSeriesMetricTest) {
    bool enabled = false;
    TimeSeriesMetric metric(metricName_, enabled, columns_);

    metric.LogData({"data1_1", "data1_2"});
    AdvanceTimeNs(1000);
    metric.LogData({"data2_1", "data2_2"});
    metric.ExportData(tempDirPath_);

    std::string file_path = tempDirPath_ + "/" + metric.name() + ".csv";
    EXPECT_FALSE(fs::exists(file_path));
}

TEST_F(MetricTest, TimeSeriesMetricDownsampleLogDataTest) {
    bool enabled = true;
    std::vector<std::string> columns = {"column1", "column2"};
    uint32_t downsampleFactor = 2;  // log once every 2 log calls
    uint32_t logEveryNs = 1000;     // log once in a 1000 ns interval [0, 1000), [1000, 2000), ...
    TimeSeriesMetric metric(metricName_, enabled, columns, downsampleFactor, logEveryNs);

    // Log data at time 0
    metric.LogData({"data1_1", "data1_2"});
    EXPECT_EQ(metric.data().size(), 1);
    EXPECT_EQ(metric.data()[0], std::vector<std::string>({"0", "data1_1", "data1_2"}));

    // Log data at time 1000 (logEveryNs allows it but downsampleFactor skips it).
    AdvanceTimeNs(1000);
    metric.LogData({"data2_1", "data2_2"});
    EXPECT_EQ(metric.data().size(), 1);

    // Log data at time 1500 (should be logged because logEveryNs allows it and downsampleFactor
    // allows it).
    AdvanceTimeNs(500);
    metric.LogData({"data3_1", "data3_2"});
    EXPECT_EQ(metric.data().size(), 2);
    EXPECT_EQ(metric.data()[1], std::vector<std::string>({"1500", "data3_1", "data3_2"}));

    // Log data at time 1600 (should be skipped due to downsampleFactor)
    AdvanceTimeNs(100);
    metric.LogData({"data4_1", "data4_2"});
    EXPECT_EQ(metric.data().size(), 2);

    // Log data at time 1700 (should be skipped due to logEveryNs)
    AdvanceTimeNs(100);
    metric.LogData({"data4_1", "data4_2"});
    EXPECT_EQ(metric.data().size(), 2);
}

}  // namespace htsim

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
