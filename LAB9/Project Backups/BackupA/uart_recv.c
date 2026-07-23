#include "uart_recv.h"
#include "usart.h"
#include <string.h>
#include <stdlib.h>

// ===== ???? =====
uint8_t recv_buf = 0;
volatile uint8_t uart_frame_ready = 0;
char uart_rx_buffer[RX_BUF_SIZE] = {0};
char json_buffer[JSON_BUF_SIZE] = {0};
UART_Mode_t uart_mode = MODE_TERMINAL;

// ===== ???? =====
static uint16_t rx_index = 0;
static uint16_t json_index = 0;
static uint8_t in_json = 0;

// ===== ???? =====

void UART_Init(void) {
    uart_frame_ready = 0;
    rx_index = 0;
    json_index = 0;
    in_json = 0;
    uart_mode = MODE_TERMINAL;
    HAL_UART_Receive_IT(&huart1, &recv_buf, 1);
}

void UART_SwitchMode(UART_Mode_t mode) {
    uart_mode = mode;
    rx_index = 0;
    json_index = 0;
    in_json = 0;
    uart_frame_ready = 0;
    // ?????
    memset(uart_rx_buffer, 0, RX_BUF_SIZE);
    memset(json_buffer, 0, JSON_BUF_SIZE);
}

void UART_ProcessByte(uint8_t byte) {
    if (uart_mode == MODE_TERMINAL) {
        // ???:??????????
        if (byte == '\n' || byte == '\r') {
            uart_rx_buffer[rx_index] = '\0';
            uart_frame_ready = 1;
            rx_index = 0;
        } else if (rx_index < RX_BUF_SIZE - 1) {
            uart_rx_buffer[rx_index++] = byte;
        }
    }
    else if (uart_mode == MODE_APRILTAG) {
        // ???:??JSON?? { ... }
        if (byte == '{') {
            json_index = 0;
            in_json = 1;
            json_buffer[json_index++] = byte;
        } else if (in_json && json_index < JSON_BUF_SIZE - 1) {
            json_buffer[json_index++] = byte;
            if (byte == '}') {
                json_buffer[json_index] = '\0';
                uart_frame_ready = 1;
                in_json = 0;
                json_index = 0;
            }
        }
    }
}

// ??????????ID(?? "VAL:XX" ??)
uint8_t UART_GetGoodsIdFromTerminal(void) {
    char *p = strstr(uart_rx_buffer, "VAL:");
    if (p) {
        return atoi(p + 4);
    }
    return 0;
}

// ??JSON????
uint8_t UART_GetJSONData(char **out_buf) {
    if (out_buf) {
        *out_buf = json_buffer;
        return 1;
    }
    return 0;
}
