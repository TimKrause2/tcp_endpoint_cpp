#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <glm/glm.hpp>

#define WATCHDOG_TIMEOUT_S 10
#define CONFIRM_TIMEOUT_S  5

enum {
    P_STATUS,
    P_DATA,
};

enum {
    P_ST_CODE_READY,
    P_ST_CODE_BUSY,
    P_ST_CODE_CONFIRM
};

enum {
    P_DATA_CODE_RAW_DATA,
    P_DATA_CODE_NEW_CONN,
    P_DATA_CODE_DEL_CONN,
    P_DATA_CODE_TELEMETRY
};

#define SIZEOF_PACKET_COMMON  16
#define SIZEOF_PACKET_CRC_DATA 8
#define OFFSET_OF_TYPE   0
#define OFFSET_OF_CODE   2
#define OFFSET_OF_LENGTH 4
#define OFFSET_OF_CRC    8
#define OFFSET_OF_DATA   SIZEOF_PACKET_COMMON

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len);

void packet_set_type(char *packet, uint16_t type);
uint16_t packet_get_type(char *packet);
void packet_set_code(char *packet, uint16_t code);
uint16_t packet_get_code(char *packet);
void packet_set_length(char *packet, uint32_t length);
uint32_t packet_get_length(char *packet);
void packet_set_crc(char *packet, uint32_t crc);
uint32_t packet_get_crc(char *packet);
void packet_seal(char *packet);
int packet_ok(char *packet);

char *packet_status_new(uint16_t code);
std::shared_ptr<char[]> packet_data_new(char *data, int nbytes, uint16_t code);

#define OFFSET_OF_SRC_INDEX OFFSET_OF_DATA
#define SIZEOF_DATA_COMMON (SIZEOF_PACKET_COMMON+4)

std::shared_ptr<char[]> packet_data_new_conn(unsigned int src_index);
std::shared_ptr<char[]> packet_data_del_conn(unsigned int src_index);

struct Telemetry
{
    unsigned int src_index;
    glm::vec3 position;
    glm::vec4 orientation;
};

std::shared_ptr<char[]> packet_data_telemetry(Telemetry *t);
void packet_data_telemetry_set_src(
        std::shared_ptr<char[]> sp,
        unsigned int src_index);
