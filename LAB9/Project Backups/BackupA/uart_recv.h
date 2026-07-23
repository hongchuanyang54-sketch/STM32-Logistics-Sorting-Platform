#ifndef __UART_RECV_H
#define __UART_RECV_H

#include "main.h"
#include <stdint.h>

#define RX_BUF_SIZE  128
#define JSON_BUF_SIZE 512

// ??????
typedef enum {
    MODE_TERMINAL = 0,   // ???:????(VAL:XX??)
    MODE_APRILTAG        // ???:AprilTag(JSON??)
} UART_Mode_t;

// ??????
extern uint8_t recv_buf;
extern volatile uint8_t uart_frame_ready;
extern char uart_rx_buffer[RX_BUF_SIZE];
extern char json_buffer[JSON_BUF_SIZE];
extern UART_Mode_t uart_mode;

// ????
void UART_Init(void);
void UART_SwitchMode(UART_Mode_t mode);
void UART_ProcessByte(uint8_t byte);
uint8_t UART_GetGoodsIdFromTerminal(void);
uint8_t UART_GetJSONData(char **out_buf);

#endif
