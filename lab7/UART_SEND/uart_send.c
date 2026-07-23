#include "uart_send.h"
#include <stdio.h>
#include <string.h>
extern UART_HandleTypeDef huart1;
void UART_SendData(uint8_t dat)
{
    char buf[20];
    sprintf(buf, "VAL:%02d\r\n", dat);
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
}
void Send_Add(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"ADD\r\n", 5, 100);
}
void Send_Sub(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"SUB\r\n", 5, 100);
}
// ???,????ZERO\r\n
void Send_Zero(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"ZERO\r\n", 6, 100);
}
// ????,????ESTOP\r\n
void Send_EStop(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"ESTOP\r\n", 7, 100);
}
// ????,????RESET\r\n
void Send_Reset(void)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)"RESET\r\n", 7, 100);
}
