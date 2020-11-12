
#ifndef PACIF_H
#define PACIF_H

#include <stdint.h>

int init_pac(void);
int frame_pac(const void** src, int frame_bytes, void (*callback)(void*,uint8_t*,int,int), void *pkt);
int close_pac(void);

#endif //#ifndef PACIF_H
