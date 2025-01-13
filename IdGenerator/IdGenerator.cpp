#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <microhttpd.h>
#include <pthread.h>
#include <time.h>

#define EPOCH 1622505600000 
#define WORKER_ID_BITS 5
#define DATACENTER_ID_BITS 5
#define SEQUENCE_BITS 12

#define MAX_WORKER_ID ((1 << WORKER_ID_BITS) - 1)
#define MAX_DATACENTER_ID ((1 << DATACENTER_ID_BITS) - 1)
#define SEQUENCE_MASK ((1 << SEQUENCE_BITS) - 1)

#define WORKER_ID_SHIFT SEQUENCE_BITS
#define DATACENTER_ID_SHIFT (SEQUENCE_BITS + WORKER_ID_BITS)
#define TIMESTAMP_LEFT_SHIFT (SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS)

static long workerId = 1;  
static long datacenterId = 1; 
static long lastTimestamp = 0;
static long sequence = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


long currentMillis() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}


long waitForNextMillis(long lastTimestamp) {
    long timestamp = currentMillis();
    while (timestamp <= lastTimestamp) {
        timestamp = currentMillis();
    }
    return timestamp;
}

long nextId() {
    pthread_mutex_lock(&mutex);

    long timestamp = currentMillis();

    if (timestamp < lastTimestamp) {
        pthread_mutex_unlock(&mutex);
        return -1;
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

    long id = ((timestamp - EPOCH) << TIMESTAMP_LEFT_SHIFT) |
              (datacenterId << DATACENTER_ID_SHIFT) |
              (workerId << WORKER_ID_SHIFT) |
              sequence;

    pthread_mutex_unlock(&mutex);
    return id;
}

int generateIdHandler(void *cls, struct MHD_Connection *connection, const char *url, const char *method,
                        const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
    long id = nextId();
    if (id == -1) {
        const char *error_msg = "Clock moved backwards. Cannot generate ID.";
        struct MHD_Response *response = MHD_create_response_from_buffer(strlen(error_msg), (void *)error_msg,
                                                                        MHD_RESPMEM_PERSISTENT);
        int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
        MHD_destroy_response(response);
        return ret;
    }

    char response_str[256];
    snprintf(response_str, sizeof(response_str), "{\"id\": %ld}", id);

    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str,
                                                                    MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

int generateIdsHandler(void *cls, struct MHD_Connection *connection, const char *url, const char *method,
                         const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
    int num_ids = 10; 
    char response_str[1024];
    char id_str[64];
    int offset = 0;

    for (int i = 0; i < num_ids; i++) {
        long id = nextId();
        if (id == -1) {
            const char *error_msg = "Clock moved backwards. Cannot generate IDs.";
            struct MHD_Response *response = MHD_create_response_from_buffer(strlen(error_msg), (void *)error_msg,
                                                                            MHD_RESPMEM_PERSISTENT);
            int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
            MHD_destroy_response(response);
            return ret;
        }

        snprintf(id_str, sizeof(id_str), "%ld", id);
        if (i == 0) {
            snprintf(response_str + offset, sizeof(response_str) - offset, "[%s", id_str);
        } else {
            snprintf(response_str + offset, sizeof(response_str) - offset, ", %s", id_str);
        }
        offset += strlen(id_str) + 2;
    }

    snprintf(response_str + offset, sizeof(response_str) - offset, "]");

    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str,
                                                                    MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

void startServer() {
    struct MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, 8080, NULL, NULL,
                                                 &generateIdHandler, NULL, MHD_OPTION_END);
    if (daemon == NULL) {
        fprintf(stderr, "Failed to start HTTP server\n");
        exit(1);
    }

    printf("Server started on http://localhost:8080\n");
    MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, 8080, NULL, NULL, &generateIdsHandler, NULL,
                     MHD_OPTION_URI, "/generate-ids", MHD_OPTION_END);
}

int main() {
    startServer();
    getchar(); 
    return 0;
}
