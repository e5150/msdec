#ifndef _MS_CRC_H
#define _MS_CRC_H

uint32_t checksum(const uint8_t *msg, size_t len);
uint32_t crc_syndrome(const uint8_t *msg, size_t len);
int errorbit(size_t len, uint32_t syn_M);

#endif
