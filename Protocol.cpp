#include "Protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <endian.h>
#include <new>
#include <iostream>

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

uint16_t extract_short(char *data, int offset)
{
    uint16_t x;
    memcpy(&x, &data[offset], 2);
    return x;
}

void insert_short(char *data, int offset, uint16_t x)
{
    memcpy(&data[offset], &x, 2);
}

uint32_t extract_int(char *data, int offset)
{
    uint32_t x;
    memcpy(&x, &data[offset], 4);
    return x;
}

void insert_int(char *data, int offset, uint32_t x)
{
    memcpy(&data[offset], &x, 4);
}

uint64_t extract_long(char *data, int offset)
{
    uint64_t x;
    memcpy(&x, &data[offset], 8);
    return x;
}

void insert_long(char *data, int offset, uint64_t x)
{
    memcpy(&data[offset], &x, 8);
}

void packet_set_type(char *packet, uint16_t type)
{
    insert_short(packet, OFFSET_OF_TYPE, htobe16(type));
}

uint16_t packet_get_type(char *packet)
{
    return be16toh(extract_short(packet, OFFSET_OF_TYPE));
}

void packet_set_code(char *packet, uint16_t code)
{
    insert_short(packet, OFFSET_OF_CODE, htobe16(code));
}

uint16_t packet_get_code(char *packet)
{
    return be16toh(extract_short(packet, OFFSET_OF_CODE));
}

void packet_set_length(char *packet, uint32_t length)
{
    insert_int(packet, OFFSET_OF_LENGTH, htobe32(length));
}

uint32_t packet_get_length(char *packet)
{
    return be32toh(extract_int(packet, OFFSET_OF_LENGTH));
}

void packet_set_crc(char *packet, uint32_t crc)
{
    insert_int(packet, OFFSET_OF_CRC, htobe32(crc));
}

uint32_t packet_get_crc(char *packet)
{
    return be32toh(extract_int(packet, OFFSET_OF_CRC));
}

void packet_seal(char *packet)
{
    uint32_t crc = crc32c(0, (const unsigned char*)packet, SIZEOF_PACKET_CRC_DATA);
    packet_set_crc(packet, crc);
}

int packet_ok(char *packet)
{
    uint32_t crc = crc32c(0, (const unsigned char*)packet, SIZEOF_PACKET_CRC_DATA);
    return crc == packet_get_crc(packet)?1:0;
}

char *packet_status_new(uint16_t code)
{
    char *ps = new(std::nothrow) char[SIZEOF_PACKET_COMMON];
    if(!ps){
        std::cout <<
        "packet_status_new: memory allocation error" << std::endl;
        return NULL;
    }
    packet_set_type(ps, P_STATUS);
    packet_set_code(ps, code);
    packet_set_length(ps, SIZEOF_PACKET_COMMON);
    packet_seal(ps);
    return ps;
}

std::shared_ptr<char[]> packet_data_new(char *data, int nbytes, uint16_t code)
{
    size_t total_bytes = SIZEOF_PACKET_COMMON + nbytes;
    std::shared_ptr<char[]> sp(new char[total_bytes]);
    char *pd = sp.get();
    packet_set_type(pd, P_DATA);
    packet_set_code(pd, code);
    packet_set_length(pd, total_bytes);
    packet_seal(pd);
    memcpy(&pd[OFFSET_OF_DATA], data, nbytes);
    return sp;
}

