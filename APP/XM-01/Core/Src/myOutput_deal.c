#include "myOutput_deal.h"

#define IAP_TRY_CNT 0x03      /* 重发次数 */
#define DEVICE_READY 0x00     /* 准备就绪 */
#define CRC_FAILSE 0xe1       /* CRC校验失败 */
#define DATA_LEN_ERROR 0xe2   /* 数据长度错误 */
#define FW_TYPE_ERROR 0xe3    /* 固件类型错 */
#define FW_SIZE_ERROR 0xe4    /* 固件大小超限 */
#define FW_PACK_ERROR 0xe5    /* 分包大小超限 */
#define IAP_STATUS_ERROR 0xe6 /* 已经处于升级状态或者升级状态异常 */
#define FLASH_ERROR 0xe7      /* flash错误 */
#define TRY_TIME_INTER 5000   /* 通讯重发间隔 */
#define WINDOW_SIZE 5

/* IAP升级状态 */
typedef enum
{
    IAP_IDLE_STATUS = 0,
    IAP_START_STATUS,
    IAP_REC_BIN,
    IAP_CHECK
} t_iap_status;

/* IAP升级数据结构 */
typedef struct
{
    t_iap_status step;             /* iap状态 */
    rt_tick_t req_tick;            /* 通讯请求重发监控 */
    rt_tick_t iap_tick;            /* IAP全局监控 */
    unsigned char try_cnt;         /* 重试次数 */
    unsigned char sw_ver[3];       /* 软件版本 */
    unsigned short total_pack_num; /* 总包数 */
    unsigned short pack_num;       /* 当前包数 */
    unsigned short pack_size;      /* 一包字节数 */
    unsigned int file_size;        /* BIN文件大小 */
    unsigned int file_crc;         /* 文件校验 */
    unsigned int total_time_out;   /* 升级整个过程超时时间 */
} t_iap;

static t_iap s_iap;

/* BIN文件特殊标识，防止错乱的BIN文件 */
const char iap_tag[] __attribute__((section(".iap_tag"))) = "OTA_TAG";
unsigned char UploadSrcMattressData[160] = {0};  /* 上传缓冲区，与160点输出一致 (2026-05-06) */

static ProtocolFrame resp;
static ProtocolFrame rx_frame;
static uint8_t first_flag = 0;
static uint8_t send_buf[512];
static uint8_t recv_buf[OUTPUT_RX_BUFFER_SIZE];
static uint16_t recv_len = 0;
static rt_tick_t start_tick = 0;
static rt_tick_t count_tick = 0;
static rt_tick_t device_upload_tick = 0;
static rt_uint16_t seq_num = 0xffff;
static rt_uint32_t timeStamp = 0xffffffff;
static rt_uint16_t Upload_Time = 500;
static CommState comm_state = STATE_WAIT_RECV;
static unsigned char bin_file[2048];
/* 导入直接输出变量 - 避免struct对齐问题 - 2025-11-06 */

/*------------------/-------------------/-------------------/------------------/
 * @brief   获取软件版本号
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static int get_sw_version(unsigned char *version)
{
    int major, minor, patch;

    if (sscanf(Project_Version, "V%d.%d.%d", &major, &minor, &patch) == 3)
    {
        version[0] = major;
        version[1] = minor;
        version[2] = patch;
        return 1;
    }

    return -1;
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   查找众数
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
unsigned char find_most_value(unsigned char *arr, unsigned int len)
{
    unsigned char value;
    unsigned char num, max_num = 0;

    for (int i = 0; i < len; i++)
    {
        num = 0;
        for (int j = 0; j < len; j++)
        {
            if (arr[i] == arr[j])
            {
                num++;
            }
        }
        if (num > max_num)
        {
            max_num = num;
            value = arr[i];
        }
    }

    return value;
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   计算平均值
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/

/*------------------/-------------------/-------------------/------------------/
 * @brief   30 17 设备信息上报
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void upload_devide_info(void)
{
    uint8_t output_frame_ctrl = FRAME_CTL_TYPE_DATA;
    uint8_t myPayload[27] = {0x30, 0x17, 0x00, 0x17};

    myPayload[4] = 0x06; /* 设备类型，默认为6 */
    myPayload[5] = 0x01;

    get_sw_version(myPayload + 6);

    /* mac地址设置为全0 */
    rt_memset(myPayload + 9, 0, 16);

    /* 数据上报 */
    output_frame_ctrl |= FRAME_CTL_CRC_EN;
    Protocol_BuildFrame(&resp,
                        output_frame_ctrl,
                        seq_num++,
                        timeStamp,
                        myPayload,
                        sizeof(myPayload),
                        true);
    Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
    HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(myPayload) + 4, 50);
    rt_kprintf("\n------------------------------------------------------------------------\n");
    hex_dump_simple(send_buf, 12 + sizeof(myPayload) + 4);
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   状态上传
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void upload_StatusPackage(void)
{
    uint8_t myPayload[10 + 160] = {0x02, 0x11, 0x01, 0x0a};  /* payload改为160字节 (2026-05-06) */
    rt_uint8_t output_frame_ctrl = FRAME_CTL_TYPE_DATA;

    /* 160点版本不计算腰部坐标，waist字段填0xFF (2026-05-06) */
    myPayload[4] = g_posture_0;
    myPayload[5] = 0xFF;
    myPayload[6] = 0xFF;
    myPayload[7] = g_posture_1;
    myPayload[8] = 0xFF;
    myPayload[9] = 0xFF;

    rt_memcpy(myPayload + 10, UploadSrcMattressData, sizeof(UploadSrcMattressData));
    //---------------------------------------------
    output_frame_ctrl |= FRAME_CTL_CRC_EN;
    Protocol_BuildFrame(&resp,
                        output_frame_ctrl,
                        seq_num++,
                        timeStamp,
                        myPayload,
                        sizeof(myPayload),
                        true);
    Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
    HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(myPayload) + 4, 50);
    rt_kprintf("\n------------------------------------------------------------------------\n");
    hex_dump_simple(send_buf, 12 + sizeof(myPayload) + 4);
    rt_kprintf("\n------------------------------------------------------------------------\n");
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   iap结果上报
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void iap_result_report(unsigned char r)
{
    unsigned char output_frame_ctrl = FRAME_CTL_TYPE_ACK;
    unsigned char payload[9] = {0xF0, 0xA3, 0x00, 0x05};

    payload[4] = 6;
    payload[5] = r;

    get_sw_version(payload + 6);
    if (rx_frame.crc_enabled)
    {
        output_frame_ctrl |= FRAME_CTL_CRC_EN;
    }

    Protocol_BuildFrame(&resp, 0x80, rx_frame.seq_num, timeStamp, payload, sizeof(payload), true);
    // 发送 数据
    Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
    HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
    // hex_dump_simple(send_buf, 8 + sizeof(payload) + 2);
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   状态机
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void comm_state_machine_run(void)
{
    rt_uint32_t e;

    switch (comm_state)
    {
    case STATE_WAIT_RECV:
    {
        if (rt_event_recv(&output_uart_rx_event, 0x01, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 100, &e) == RT_EOK)
        {
            static unsigned char s_fifo[2500] = {0};
            static unsigned int s_pos = 0;

            // hex_dump_simple(output_uart_rx_buffer, output_uart_rx_len);
            if(s_pos + output_uart_rx_len > sizeof(s_fifo))
            {
                s_pos = 0;
            }

            rt_memcpy(s_fifo + s_pos, output_uart_rx_buffer, output_uart_rx_len);
            rt_memset(output_uart_rx_buffer, 0, sizeof(output_uart_rx_buffer));

            s_pos += output_uart_rx_len;
            for (int i = 0; i < s_pos - 1; i++)
            {
                int temp_len = Protocol_ParseFrame(s_fifo + i, s_pos - i, &rx_frame);

                if (temp_len < 0)
                {
                    continue;
                }
                else if (temp_len == 0)
                {
                    rt_memmove(s_fifo, s_fifo + i, s_pos - i);
                    s_pos -= i;
                    return;
                }
                else
                {
                    rt_memcpy(recv_buf, s_fifo + i, temp_len);
                    rt_memmove(s_fifo, s_fifo + i + temp_len, s_pos - i - temp_len);
                    rt_memset(&rx_frame, 0, sizeof(rx_frame));
                    s_pos -= (i + temp_len);
					recv_len = temp_len;
                    comm_state = STATE_PARSE_FRAME;
                    return;
                }
            }

            s_fifo[0] = s_fifo[s_pos - 1];
            s_pos = 1;
        }
        break;
    }

    case STATE_PARSE_FRAME:
    {
        if (Protocol_ParseFrame(recv_buf, recv_len, &rx_frame) > 0)
        {
            rt_kprintf("Parse Data Done...\n");
            if (rx_frame.is_ack)
            {
                uint8_t mcmd = rx_frame.payload[0];
                uint8_t scmd = rx_frame.payload[1];

                if ((mcmd == 0xF0) && (scmd == 0x02))
                {
                    unsigned char status = 0;

                    unsigned short cmd_len = rx_frame.payload[2];
                    cmd_len <<= 8;
                    cmd_len |= rx_frame.payload[3];

                    unsigned short pack_no = rx_frame.payload[5];
                    pack_no <<= 8;
                    pack_no |= rx_frame.payload[6];

                    unsigned short pack_size = rx_frame.payload[7];
                    pack_size <<= 8;
                    pack_size |= rx_frame.payload[8];

                    if ((s_iap.step != IAP_START_STATUS) && (s_iap.step != IAP_REC_BIN))
                    {
                        SEGGER_RTT_printf(0, "OTA status error!\r\n");
                        status++;
                    }

                    SEGGER_RTT_printf(0, "pack num:%d, receive pack num:%d\r\n", s_iap.pack_num, pack_no);
                    /* 包号比对 */
                    if (pack_no != s_iap.pack_num)
                    {
                        SEGGER_RTT_printf(0, "pack number error!\r\n");
                        status++;
                    }

                    SEGGER_RTT_printf(0, "pack size:%d, receive pack size:%d\r\n", s_iap.pack_size, pack_size);
                    if (pack_no < s_iap.total_pack_num)
                    {
                        if (pack_size != s_iap.pack_size)
                        {
                            SEGGER_RTT_printf(0, "pack_size != s_iap.pack_size\r\n ");
                            status++;
                        }
                    }
                    else
                    {
                        unsigned short temp_len = (s_iap.file_size - (s_iap.total_pack_num - 1) * s_iap.pack_size);
                        SEGGER_RTT_printf(0, "pack size:%d, receive pack size:%d\r\n", temp_len, pack_size);
                        if (pack_size != temp_len)
                        {
                            // 这里temp_len最好是四的整数倍
                            SEGGER_RTT_printf(0, "pack_size != temp_len\r\n");
                            status++;
                        }
                    }

                    SEGGER_RTT_printf(0, "pack size:%d, cmd len:%d\r\n", pack_size, cmd_len);
                    if (pack_size + 5 != cmd_len)
                    {
                        SEGGER_RTT_printf(0, "pack_size + 5 != cmd_len\r\n");
                        status++;
                    }

                    SEGGER_RTT_printf(0, "status:0x%x\r\n", rx_frame.payload[4]);
                    if (rx_frame.payload[4])
                    {
                        SEGGER_RTT_printf(0, "error: rx_frame.payload[4] is not 0\r\n");
                        status++;
                    }

                    if (status)
                    {
                        SEGGER_RTT_printf(0, "F0 02 ack format error!\r\n");
                        rt_memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
                        // 上报失败
                        iap_result_report(4);
                    }
                    else
                    {
                        /* 防止非四字节对齐 */
                        rt_memcpy(bin_file, rx_frame.payload + 9, pack_size);

                        /* 写FLASH */
                        bin_flash_write(BIN_ADDR + (pack_no - 1) * s_iap.pack_size, (unsigned int *)bin_file, pack_size);

                        /* 最后一包 */
                        if (s_iap.pack_num == s_iap.total_pack_num)
                        {
                            unsigned int cs = 0;

                            for (int i = 0; i < s_iap.file_size; i++)
                            {
                                cs += *(unsigned char *)(BIN_ADDR + i);
                            }

                            /* 校验不通过 */
                            if (cs != s_iap.file_crc)
                            {
                                SEGGER_RTT_printf(0, "crc check error calc crc:0x%x, receive crc:0x%x\r\n", cs, s_iap.file_crc);
                                iap_result_report(4);
                                rt_memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
                            }
                            else
                            {
                                if (memcmp(iap_tag, (unsigned char *)(((unsigned int)iap_tag) - APP_ADDR + BIN_ADDR), strlen(iap_tag)))
                                {
                                    SEGGER_RTT_printf(0, "OTA tag error!\r\n");
                                    iap_result_report(4); // bin文件错乱
                                }
                                else
                                {
                                    t_flash_data flash_data;

                                    STM_Flash_Read(DATA_ADDR, (unsigned int *)&flash_data, sizeof(flash_data));
                                    flash_data.flag = INTO_BOOT_FLASH;
                                    rt_memcpy(flash_data.version, s_iap.sw_ver, 3);
                                    data_flash_write(DATA_ADDR, (unsigned char *)&flash_data, sizeof(flash_data));
                                    NVIC_SystemReset();
                                }
                            }
                        }
                        else
                        {
                            s_iap.try_cnt = IAP_TRY_CNT;
                            s_iap.pack_num++;
                            s_iap.req_tick = rt_tick_get_millisecond() - TRY_TIME_INTER;
                        }
                    }
                }
                // rt_kprintf("[ACK] Frame received. SEQ=%d\n", rx_frame.seq_num);
                comm_state = STATE_BACK_WAIT;
            }
            else
            {
                // 非 ACK，跳转执行命令
                comm_state = STATE_HANDLE_CMD;
            }
        }
        else
        {
            // rt_kprintf("Parse Data Fail...\n");
            comm_state = STATE_BACK_WAIT;
        }
        break;
    }

    case STATE_HANDLE_CMD:
    {
        uint8_t mcmd = rx_frame.payload[0];
        uint8_t scmd = rx_frame.payload[1];

        // 软件版本读取命令
        if (mcmd == 0x02 && scmd == 0x12)
        {
            rt_uint8_t output_frame_ctrl = FRAME_CTL_TYPE_ACK;
            uint8_t payload[1 + 1 + 2 + 1 + 16] = {0x02, 0x12, 0x00, 0x11, 0x00};

            get_sw_version(payload + 5);
            if (rx_frame.need_ack)
            {
                if (rx_frame.crc_enabled)
                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                Protocol_BuildFrame(&resp,
                                    output_frame_ctrl,
                                    rx_frame.seq_num,
                                    timeStamp,
                                    payload,
                                    sizeof(payload),
                                    true);
                // 发送 数据
                Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
                // hex_dump_simple(send_buf, 8 + sizeof(payload) + 2);
            }
        }
        // 设置状态包上报间隔
        else if (mcmd == 0x02 && scmd == 0x10)
        {
            rt_uint8_t output_frame_ctrl = FRAME_CTL_TYPE_ACK;
            uint8_t payload[1 + 1 + 2 + 1 + 2] = {0x02, 0x10, 0x00, 0x03, 0x00};
            Upload_Time = (rx_frame.payload[4] << 8) + (rx_frame.payload[5]);
            // 0.1秒~30秒 都是正常范围
            if (Upload_Time >= 100 && Upload_Time <= 30000)
                payload[4] = 0x00;
            else
            {
                payload[4] = 0xe1;
                Upload_Time = 100;
            }
            payload[5] = rx_frame.payload[4];
            payload[6] = rx_frame.payload[5];

            if (rx_frame.need_ack)
            {
                if (rx_frame.crc_enabled)
                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                Protocol_BuildFrame(&resp,
                                    output_frame_ctrl,
                                    rx_frame.seq_num,
                                    timeStamp,
                                    payload,
                                    sizeof(payload),
                                    true);
                // 发送 数据
                Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
                // hex_dump_simple(send_buf, 8 + sizeof(payload) + 2);
            }
        }
        else if ((mcmd == 0x30) && (scmd == 0x18)) /* 按摩模块设备信息查询 */
        {
            rt_uint8_t output_frame_ctrl = FRAME_CTL_TYPE_ACK;
            uint8_t payload[1 + 1 + 2 + 24] = {0x30, 0x18, 0x00, 0x18, 0x00, 0x06, 0x01};

            get_sw_version(payload + 7);
            rt_memset(payload + 10, 0, sizeof(payload) - 10);

            if (rx_frame.need_ack)
            {
                if (rx_frame.crc_enabled)
                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                Protocol_BuildFrame(&resp,
                                    output_frame_ctrl,
                                    rx_frame.seq_num,
                                    timeStamp,
                                    payload,
                                    sizeof(payload),
                                    true);
                // 发送 数据
                Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
            }
        }
        /*------------------------------------------------------------------------*/
        else if ((mcmd == 0xF0) && (scmd == 0x01)) /* 开始OTA请求 */
        {
            unsigned char status = DEVICE_READY;

            /* 软件版本号 */
            rt_memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
            rt_memcpy(s_iap.sw_ver, &rx_frame.payload[4], 3);

            /* 固件类型 */
            if (rx_frame.payload[7] != 0x06)
            {
                status = FW_TYPE_ERROR;
            }
            SEGGER_RTT_printf(0, "FW_TYPE:%d\r\n", rx_frame.payload[7]);

            /* 固件文件大小 */
            unsigned int size = rx_frame.payload[8];
            size <<= 8;
            size |= rx_frame.payload[9];
            size <<= 8;
            size |= rx_frame.payload[10];
            size <<= 8;
            size |= rx_frame.payload[11];
            SEGGER_RTT_printf(0, "FW_SIZE:%d bytes\r\n", size);
            if ((size > APP_SIZE) || (size <= 20 * 1024))
            {
                status = FW_SIZE_ERROR;
            }
            else
            {
                s_iap.file_size = size;
            }

            /* 数据包的数量 */
            unsigned pack_num = rx_frame.payload[12];
            pack_num <<= 8;
            pack_num |= rx_frame.payload[13];
            SEGGER_RTT_printf(0, "PACK_NUM:%d\r\n", pack_num);
            if ((pack_num < 20 * 1024 / 2048) || (pack_num > APP_SIZE / 128))
            {
                status = DATA_LEN_ERROR;
            }
            else
            {
                s_iap.total_pack_num = pack_num;
            }

            /* 固件文件校验和 */
            unsigned int crc = rx_frame.payload[14];
            crc <<= 8;
            crc |= rx_frame.payload[15];
            crc <<= 8;
            crc |= rx_frame.payload[16];
            crc <<= 8;
            crc |= rx_frame.payload[17];
            SEGGER_RTT_printf(0, "FILE_CRC:0x%x\r\n", size);
            s_iap.file_crc = crc;

            unsigned short pack_size = rx_frame.payload[18];
            pack_size <<= 8;
            pack_size |= rx_frame.payload[19];
            SEGGER_RTT_printf(0, "PACK_SIZE:0x%x\r\n", pack_size);
            if ((pack_size < 128) || (pack_size > 2048) || (pack_size % 128))
            {
                status = FW_PACK_ERROR;
            }
            s_iap.pack_size = pack_size;

            SEGGER_RTT_printf(0, "status:%d\r\n", status);
            /* 启动IAP准备工作 */
            unsigned char erase_flag = 0;
            if (status == DEVICE_READY)
            {
                erase_flag = 1;
                s_iap.step = IAP_START_STATUS;
                s_iap.total_time_out = 1000 * 300; /*升级过程监控 */
                s_iap.iap_tick = rt_tick_get_millisecond();
                s_iap.req_tick = rt_tick_get_millisecond() - TRY_TIME_INTER;
                s_iap.pack_num = 1;
                s_iap.try_cnt = IAP_TRY_CNT;
            }

            /* 开始OTA回复 */
            unsigned char output_frame_ctrl = FRAME_CTL_TYPE_ACK;
            unsigned char payload[5] = {0xF0, 0x01, 0x00, 0x01};
            payload[4] = status;

            if (rx_frame.need_ack)
            {
                if (rx_frame.crc_enabled)
                {
                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                }

                Protocol_BuildFrame(&resp,
                                    output_frame_ctrl,
                                    rx_frame.seq_num,
                                    timeStamp,
                                    payload,
                                    sizeof(payload),
                                    true);
                // 发送 数据
                Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
                // hex_dump_simple(send_buf, 8 + sizeof(payload) + 2);
            }

            if (erase_flag)
            {
                bin_flash_erase();
            }
        }
        else if ((mcmd == 0xF0) && (scmd == 0x03)) /* 结束OTA请求 */
        {
            unsigned char cmd = rx_frame.payload[4];
            if (cmd <= 2)
            {
                rt_memset(&s_iap, 0, sizeof(s_iap));
            }

            /* 开始OTA回复 */
            unsigned char output_frame_ctrl = FRAME_CTL_TYPE_ACK;
            unsigned char payload[5] = {0xF0, 0x03, 0x00, 0x01, 0x00};

            if (rx_frame.need_ack)
            {
                if (rx_frame.crc_enabled)
                {
                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                }

                Protocol_BuildFrame(&resp,
                                    output_frame_ctrl,
                                    rx_frame.seq_num,
                                    timeStamp,
                                    payload,
                                    sizeof(payload),
                                    true);
                // 发送 数据
                Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 100);
                // hex_dump_simple(send_buf, 8 + sizeof(payload) + 2);
            }
        }
        /*------------------------------------------------------------------------*/
        else
        {
            rt_kprintf("[WARN] Unhandled CMD: MCMD=0x%02X SCMD=0x%02X\n", mcmd, scmd);
        }
        comm_state = STATE_BACK_WAIT;
        break;
    }
    case STATE_BACK_WAIT:
    {
        rt_memset(&rx_frame, 0, sizeof(&rx_frame));
        rt_memset(recv_buf, 0, sizeof(recv_buf));
        recv_len = 0;
        comm_state = STATE_WAIT_RECV;
        break;
    }
    default:
        break;
    }

    // 30 17设备信息上报
    static unsigned char upload_cnt = 3;
    if (device_upload_tick == 0)
    {
        device_upload_tick = rt_tick_get_millisecond();
    }
    else if (upload_cnt && (rt_tick_get_millisecond() - device_upload_tick >= 500))
    {
        upload_cnt--;
        device_upload_tick = rt_tick_get_millisecond();
        upload_devide_info();
    }

#if 1
    if (s_iap.iap_tick == 0)
    {
        // 默认上报状态
        if (rt_tick_get_millisecond() - start_tick > Upload_Time)
        {
            upload_StatusPackage();
            start_tick = rt_tick_get_millisecond();
        }

        // 时间戳
        if (rt_tick_get_millisecond() - count_tick > 1000)
        {
            timeStamp++;
            count_tick = rt_tick_get_millisecond();
        }
    }
#endif

    /* IAP请求监控 */
    if (s_iap.req_tick)
    {
        unsigned int t = rt_tick_get_millisecond() - s_iap.req_tick;
        if (t >= TRY_TIME_INTER)
        {
            s_iap.req_tick = rt_tick_get_millisecond();
            if ((s_iap.step == IAP_START_STATUS) || (s_iap.step == IAP_REC_BIN))
            {
                s_iap.step = IAP_REC_BIN;
                if (s_iap.try_cnt)
                {
                    static unsigned short seq;
                    unsigned char payload[8] = {0xF0, 0x02, 0x00, 0x04};

                    if (s_iap.try_cnt == IAP_TRY_CNT)
                    {
                        seq = seq_num;
                        seq_num++;
                    }
                    s_iap.try_cnt--;
                    rt_uint8_t output_frame_ctrl = FRAME_CTL_TYPE_DATA;
                    payload[4] = s_iap.pack_num >> 8;
                    payload[5] = s_iap.pack_num & 0xFF;
                    if (s_iap.pack_num == s_iap.total_pack_num)
                    {
                        unsigned short temp_len;

                        temp_len = s_iap.file_size - (s_iap.pack_num - 1) * s_iap.pack_size;
                        SEGGER_RTT_printf(0, "the last request frame, data len:%d\r\n", temp_len);
                        if ((temp_len == 0) || (temp_len > s_iap.pack_size))
                        {
                            SEGGER_RTT_printf(0, "the last data len error!\r\n");
                            iap_result_report(0);
                            return;
                        }
                        else
                        {
                            payload[6] = temp_len >> 8;
                            payload[7] = temp_len & 0xFF;
                        }
                    }
                    else
                    {
                        payload[6] = s_iap.pack_size >> 8;
                        payload[7] = s_iap.pack_size & 0xFF;
                    }

                    output_frame_ctrl |= FRAME_CTL_CRC_EN;
                    output_frame_ctrl |= FRAME_CTL_ACK_REQ;
                    Protocol_BuildFrame(&resp, output_frame_ctrl, seq, timeStamp, payload, sizeof(payload), true);
                    Protocol_SerializeFrame(send_buf, sizeof(send_buf), &resp);
                    HAL_UART_Transmit(&huart3, send_buf, 12 + sizeof(payload) + 4, 50);
                }
                else
                {
                    /* 重发机会用完 */
                    memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
                }
            }
        }
    }

    /* iap全程监控 */
    unsigned int temp = rt_tick_get_millisecond() - s_iap.iap_tick;
    if ((temp >= 100) && s_iap.iap_tick)
    {
        if (s_iap.total_time_out >= temp)
        {
            s_iap.total_time_out -= temp;
            s_iap.iap_tick = rt_tick_get_millisecond();
        }
        else
        {
            memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
            iap_result_report(5);
        }
    }

    /* 升级成功后第一次上电 */
    if (first_flag == 0)
    {
        t_flash_data flash_data;

        memset((unsigned char *)&s_iap, 0, sizeof(s_iap));
        first_flag = 1;

        STM_Flash_Read(DATA_ADDR, (unsigned int *)&flash_data, sizeof(flash_data) >> 2);
        if (flash_data.flag == IAP_PRE_OK)
        {
            SEGGER_RTT_printf(0, "OTA success!\r\n");
            flash_data.flag = 0;
            data_flash_write(DATA_ADDR, (unsigned char *)&flash_data, sizeof(flash_data));
            iap_result_report(0);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////

/* 定义线程栈与控制块（静态分配） */
#define OUTPUT_THREAD_STACK_SIZE 2048  /* 增大线程栈防止upload_StatusPackage中270字节局部变量导致栈溢出 (2026-04-07) */
struct rt_event output_uart_rx_event; // 静态事件对象;
static struct rt_thread output_thread;
static rt_uint8_t output_thread_stack[OUTPUT_THREAD_STACK_SIZE];

/*------------------/-------------------/-------------------/------------------/
 * @brief   线程入口
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
static void output_thread_entry(void *parameter)
{
    while (1)
    {
        comm_state_machine_run();
        rt_thread_mdelay(10);
    }
}

/*------------------/-------------------/-------------------/------------------/
 * @brief   线程初始化
 * @param   none
 * @return  none.
 * @note    none
/-------------------/-------------------/-------------------/-----------------*/
int output_thread_init(void)
{
    rt_err_t result1 = rt_event_init(&output_uart_rx_event, "uart_rx_evt", RT_IPC_FLAG_FIFO);
    if (result1 != RT_EOK)
        return -RT_ERROR;
    rt_thread_init(&output_thread,              // 线程控制块
                   "output_task",               // 名称
                   output_thread_entry,         // 入口函数
                   RT_NULL,                     // 参数
                   &output_thread_stack[0],     // 栈起始地址
                   sizeof(output_thread_stack), // 栈大小
                   10,                          // 优先级(高)
                   10);                         // 时间片
    rt_thread_startup(&output_thread);          // 启动线程
    return RT_EOK;
}
INIT_APP_EXPORT(output_thread_init);
