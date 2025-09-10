#ifndef CONFIG_H
#define CONFIG_H

const int BASE_PORT = 4000;
const int SERVER_COUNT = 5;

static constexpr size_t QUEUE_SIZE = 8192; // must be power of 2
static constexpr size_t BATCH_SIZE = 100;

#endif