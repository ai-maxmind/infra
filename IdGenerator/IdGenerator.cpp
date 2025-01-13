#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;
using namespace std;

class SnowflakeIdGenerator {
public:
    static const int64_t EPOCH = 1622505600000; 
    static const int64_t WORKER_ID_BITS = 5;
    static const int64_t DATACENTER_ID_BITS = 5;
    static const int64_t SEQUENCE_BITS = 12;

    static const int64_t MAX_WORKER_ID = (1 << WORKER_ID_BITS) - 1;
    static const int64_t MAX_DATACENTER_ID = (1 << DATACENTER_ID_BITS) - 1;
    static const int64_t SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;

    static const int64_t WORKER_ID_SHIFT = SEQUENCE_BITS;
    static const int64_t DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
    static const int64_t TIMESTAMP_LEFT_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;

    SnowflakeIdGenerator(int64_t workerId, int64_t datacenterId) : workerId(workerId), datacenterId(datacenterId) {
        if (workerId > MAX_WORKER_ID || workerId < 0 || datacenterId > MAX_DATACENTER_ID || datacenterId < 0) {
            throw std::invalid_argument("Worker ID or Datacenter ID out of range");
        }
        lastTimestamp = 0;
        sequence = 0;
    }

    int64_t nextId() {
        lock_guard<mutex> lock(mtx);
        int64_t timestamp = currentMillis();
        if (timestamp < lastTimestamp) {
            throw std::runtime_error("Clock moved backwards. Refusing to generate ID");
        }

        if (timestamp == lastTimestamp) {
            sequence = (sequence + 1) & SEQUENCE_MASK;
            if (sequence == 0) {
                timestamp = waitForNextMillis(lastTimestamp);
            }
        } else {
            sequence = 0;
        }

        lastTimestamp = timestamp;

        return ((timestamp - EPOCH) << TIMESTAMP_LEFT_SHIFT) |
               (datacenterId << DATACENTER_ID_SHIFT) |
               (workerId << WORKER_ID_SHIFT) |
               sequence;
    }

    vector<int64_t> generateIds(int count) {
        vector<int64_t> ids;
        for (int i = 0; i < count; ++i) {
            ids.push_back(nextId());
        }
        return ids;
    }

private:
    int64_t workerId;
    int64_t datacenterId;
    int64_t lastTimestamp;
    int64_t sequence;
    mutex mtx;

    int64_t currentMillis() {
        return chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    }

    int64_t waitForNextMillis(int64_t lastTimestamp) {
        int64_t timestamp = currentMillis();
        while (timestamp <= lastTimestamp) {
            timestamp = currentMillis();
        }
        return timestamp;
    }
};

void handleGenerateId(http_request request) {
    static SnowflakeIdGenerator generator(1, 1); 

    int64_t id = generator.nextId();
    json::value response_data;
    response_data[U("id")] = json::value::number(id);

    request.reply(status_codes::OK, response_data);
}

void handleGenerateIds(http_request request) {
    static SnowflakeIdGenerator generator(1, 1); 

    vector<int64_t> ids = generator.generateIds(10); 
    json::value response_data;
    response_data[U("ids")] = json::value::array(ids.begin(), ids.end());

    request.reply(status_codes::OK, response_data);
}

int main() {
    uri_builder uri(U("http://localhost:8080"));
    auto addr = uri.to_uri().to_string();

    http_listener listener(addr);

    listener.support(methods::GET, handleGenerateId);     
    listener.support(methods::GET, handleGenerateIds);     

    try {
        listener
            .open()
            .then([&addr](){ std::wcout << L"Starting to listen at: " << addr << std::endl; })
            .wait();
    } catch (const exception& e) {
        std::cerr << "Error occurred: " << e.what() << std::endl;
    }

    return 0;
}
