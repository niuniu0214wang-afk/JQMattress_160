#ifndef __MY_FLASH_H
#define __MY_FLASH_H
#include "main.h"

#define STM32_FLASH_BASE 0x08000000 //STM32 FLASH的起始地址
#define FLASH_WAITETIME 50000       //FLASH等待超时时间

//FLASH 扇区的起始地址
#define ADDR_FLASH_SECTOR_0 ((uint32_t)0x08000000)  //扇区0起始地址, 16 Kbytes
#define ADDR_FLASH_SECTOR_1 ((uint32_t)0x08004000)  //扇区1起始地址, 16 Kbytes
#define ADDR_FLASH_SECTOR_2 ((uint32_t)0x08008000)  //扇区2起始地址, 16 Kbytes
#define ADDR_FLASH_SECTOR_3 ((uint32_t)0x0800C000)  //扇区3起始地址, 16 Kbytes
#define ADDR_FLASH_SECTOR_4 ((uint32_t)0x08010000)  //扇区4起始地址, 64 Kbytes
#define ADDR_FLASH_SECTOR_5 ((uint32_t)0x08020000)  //扇区5起始地址, 128 Kbytes
#define ADDR_FLASH_SECTOR_6 ((uint32_t)0x08040000)  //扇区6起始地址, 128 Kbytes
#define ADDR_FLASH_SECTOR_7 ((uint32_t)0x08060000)  //扇区7起始地址, 128 Kbytes
#define ADDR_FLASH_SECTOR_8 ((uint32_t)0x08080000)  //扇区8起始地址, 128 Kbytes
#define ADDR_FLASH_SECTOR_9 ((uint32_t)0x080A0000)  //扇区9起始地址, 128 Kbytes
#define ADDR_FLASH_SECTOR_10 ((uint32_t)0x080C0000) //扇区10起始地址,128 Kbytes
#define ADDR_FLASH_SECTOR_11 ((uint32_t)0x080E0000) //扇区11起始地址,128 Kbytes

#define BOOT_ADDR      ADDR_FLASH_SECTOR_0          /* BOOT区起始地址 */
#define BOOT_SIZE      (32 * 1024)
#define DATA_ADDR      ADDR_FLASH_SECTOR_2          /* 数据区起始地址 */
#define DATA_SIZE      (16 * 1024)
#define APP_ADDR       ADDR_FLASH_SECTOR_6          /* app区起始地址 */
#define APP_SIZE       (3 * 128 * 1024)                     
#define BIN_ADDR       ADDR_FLASH_SECTOR_9          /* bin文件存放起始地址 */
#define BIN_SIZE       APP_SIZE

#define INTO_BOOT_FLASH      0x55AAAA55             /* 进入BOOT区魔数 */
#define IAP_PRE_OK           0x77888877             /* 升级成功，待确认 */

typedef struct
{
	  char version[64];                               /* 程序版本，保留，暂时未使用 */
    unsigned  int flag;                             /* 升级标志 */
	  unsigned  int count;                            /* 升级次数 */
}t_flash_data;

int app_flash_write(unsigned int addr, unsigned int *data, unsigned int size);
int app_flash_erase(void);
int data_flash_write(unsigned int addr, unsigned char *data, unsigned int size);
uint32_t STM_Flash_ReadWord(uint32_t faddr);                                      //读出字
void STM_Flash_Read(uint32_t ReadAddr, uint32_t *pBuffer, uint32_t NumToRead);    //从指定地址开始读出指定长度的数据
uint8_t STMFLASH_GetFlashSector(uint32_t addr);                                   //获取Flash扇区号
#endif /*__USER_FLASH_H*/ 
