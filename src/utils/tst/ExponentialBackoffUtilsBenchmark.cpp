#include "UtilTestFixture.h"
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <iterator>

class ExponentialBackoffUtilsBenchmark : public UtilTestBase {
private:
    std::atomic<int> serverCounter;
    std::vector<long> requestArrivalTimeStamps;
    std::mutex serverLock;
    long currentTime;

    void makeAPICall() {
        serverCounter++;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());

        serverLock.lock();
        requestArrivalTimeStamps.emplace_back((ms.count() - currentTime));
        serverLock.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(RAND() % 10));
    }

    void threadTask() {
        PExponentialBackoffState pExponentialBackoffState = NULL;
        UINT32 maxTestRetryCount = 10;

        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateWithDefaultConfigCreate(&pExponentialBackoffState));
        pExponentialBackoffState->exponentialBackoffConfig.jitterType = FIXED_JITTER;

        EXPECT_TRUE(pExponentialBackoffState != NULL);

        for (int i = 1; i <= maxTestRetryCount; i++) {
            auto status = exponentialBackoffBlockingWait(pExponentialBackoffState);
            makeAPICall();
            if (status != STATUS_SUCCESS) {
            }
        }

        EXPECT_EQ(STATUS_SUCCESS, exponentialBackoffStateFree(&pExponentialBackoffState));
    }

    std::map<long, int> processRequestArrivalTimestamps(std::vector<long>& requestArrivalTimestamps) {
        std::map<long, int> bucketRequests;
        for (int i = 1000; i < 30000; i+=100) {
            bucketRequests[i] = 0;
        }
        long startTimeStamp = 1000;

        for (auto& timestamp : requestArrivalTimestamps) {
            if (timestamp <= startTimeStamp + 100) {
                bucketRequests[startTimeStamp + 100]++;
            } else {
                startTimeStamp += 100;
            }
        }
        return bucketRequests;
    }

    void dumpToFile(std::map<long, int>& groupedRequests) {
        std::ofstream file;
        file.open("/tmp/exponential_backoff_benchmark_details.txt");
        for (auto it = groupedRequests.begin(); it != groupedRequests.end(); it++) {
            file << it->first << "\t" << it->second << "\n";
        }
        file.close();
    }

public:
    void startBenchMarkTest() {
        srand(time(0));
        std::atomic_init(&serverCounter, 0);
        currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        std::vector<std::thread> threadPool;
        for (int i = 1; i <= 4000; i++) {
            threadPool.emplace_back(std::thread(&ExponentialBackoffUtilsBenchmark::threadTask, this));
        }

        for (auto& th : threadPool) {
            th.join();
        }

        std::map<long, int> groupedRequests = processRequestArrivalTimestamps(requestArrivalTimeStamps);
        dumpToFile(requestArrivalTimeStamps);
        dumpToFile(groupedRequests);
        std::cout << "\n Total server calls: " << serverCounter;
    }
};

TEST_F(ExponentialBackoffUtilsBenchmark, exponential_backoff_utils_benchmark_test)
{
    startBenchMarkTest();
}
