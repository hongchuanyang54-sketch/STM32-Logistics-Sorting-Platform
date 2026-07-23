#include "flash_save.h"
#include "stm32f1xx_hal.h"

// ???????,???????0
uint8_t Flash_Read_Count(void)
{
    uint16_t flag = *(__IO uint16_t *)FLASH_STORE_ADDR;
    // ????,??count
    if(flag == FLASH_VALID_FLAG)
    {
        return *(__IO uint16_t *)(FLASH_STORE_ADDR + 2);
    }
    return 0;
}

// ????+?????
void Flash_Save_Count(uint8_t num)
{
    FLASH_EraseInitTypeDef erase_cfg;
    uint32_t page_err = 0;

    HAL_FLASH_Unlock(); // ??Flash

    // ??????
    erase_cfg.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_cfg.PageAddress = FLASH_STORE_ADDR;
    erase_cfg.NbPages = 1;
    HAL_FLASHEx_Erase(&erase_cfg, &page_err);

    // ?????? + ???
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, FLASH_STORE_ADDR, FLASH_VALID_FLAG);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, FLASH_STORE_ADDR + 2, num);

    HAL_FLASH_Lock(); // ????
}
