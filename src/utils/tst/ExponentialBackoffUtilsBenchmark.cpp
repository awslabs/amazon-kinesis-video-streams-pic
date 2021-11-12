#include "UtilTestFixture.h"
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <iterator>

class Server {
private:
    std::atomic<int> requestCounter;
    std::vector<long> requestArrivalTimeStamps;
    std::mutex serverLock;
    long baseTime;

    std::string metricsFile;
    int metricsPeriodMilliseconds;

public:
    Server() {}
    explicit Server(const std::string& metricsFile, int metricsPeriodMilliseconds) {
        std::atomic_init(&requestCounter, 0);
        this->baseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        this->metricsFile = metricsFile;
        this->metricsPeriodMilliseconds = metricsPeriodMilliseconds;
    }

    ~Server() {
        std::cout << "\n Total requests received on server: " << requestCounter;
        flushRequestTimestampsToFile();
    }

    void fakeAPI() {
        requestCounter++;
        auto requestArrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());

        serverLock.lock();
        requestArrivalTimeStamps.emplace_back((requestArrivalTime.count() - baseTime));
        serverLock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(RAND() % 10));
    }

private:
    void flushRequestTimestampsToFile() {
        std::cout << "\n requestArrivalTimeStamps: " << requestArrivalTimeStamps.size();
        if (requestArrivalTimeStamps.empty()) {
            return;
        }

        std::map<long, int> requestsMap;
        long largestTimeStamp = requestArrivalTimeStamps.back();
        for (int i = 0; i < largestTimeStamp; i += metricsPeriodMilliseconds) {
            requestsMap[i] = 0;
        }

        long startTimeStamp = 0;
        for (auto& timestamp : requestArrivalTimeStamps) {
            if (timestamp <= startTimeStamp + metricsPeriodMilliseconds) {
                requestsMap[startTimeStamp + metricsPeriodMilliseconds]++;
            } else {
                startTimeStamp += metricsPeriodMilliseconds;
            }
        }

        // trim leading and trailing 0 entries
        auto startIter = requestsMap.begin();
        for (; startIter->second != 0 && startIter != requestsMap.end(); startIter++);

        if (startIter == requestsMap.end()) {
            std::cout << "\n No metrics found on server";
            return;
        }

        auto endIter = --requestsMap.end();
        for (; endIter->second != 0 && endIter != requestsMap.begin(); endIter--);

        std::ofstream file;
        file.open(metricsFile);
        std::cout << "\n Writing server metrics to file: " << metricsFile << "   requestsMap: " << requestsMap.size();

        while(startIter <= endIter) {
            file << startIter->first << "\t" << startIter->second << "\n";
            startIter++;
        }

        file.close();
    }
};

class ExponentialBackoffUtilsBenchmark : public UtilTestBase {
    void threadTask(Server* server, int maxRetryCount, ExponentialBackoffJitterType jitterType) {
        PExponentialBackoffState pExponentialBackoffState = NULL;
        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateWithDefaultConfigCreate(&pExponentialBackoffState));
        pExponentialBackoffState->exponentialBackoffConfig.jitterType = jitterType;

        EXPECT_TRUE(pExponentialBackoffState != NULL);

        for (int i = 1; i <= maxRetryCount; i++) {
            auto status = exponentialBackoffBlockingWait(pExponentialBackoffState);
            server->fakeAPI();
        }

        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    }

public:

    void startBenchMarkTest(
            int concurrentThreads,
            int maxRetryCount,
            int metricsPeriodMilliseconds,
            const std::string& testResultsFilePath,
            ExponentialBackoffJitterType jitterType) {

        srand(time(0));

        std::vector<std::thread> threadPool;
        Server server(testResultsFilePath, metricsPeriodMilliseconds);

        for (int i = 1; i <= concurrentThreads; i++) {
            threadPool.emplace_back(std::thread(&ExponentialBackoffUtilsBenchmark::threadTask, this, &server, maxRetryCount, jitterType));
        }

        for (auto& th : threadPool) {
            std::cout << "\n Joining thread";
            th.join();
        }
    }
};

TEST_F(ExponentialBackoffUtilsBenchmark, exponential_backoff_utils_benchmark_test)
{
    int concurrentThreads = 20;
    int maxRetryCount = 3;
    int metricsPeriodMilliseconds = 10;
    std::string testResultsFilePath = "/tmp/exponential_backoff_benchmark_details_no_jitter.txt";
    startBenchMarkTest(concurrentThreads, maxRetryCount, metricsPeriodMilliseconds, testResultsFilePath, NO_JITTER);
}
