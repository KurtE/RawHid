#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <vector>
#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
#include <conio.h>
#endif

#include "hid.h"

enum {
    CMD_NONE = -1,
    CMD_DATA = 0,
    CMD_DIR = 1,
    CMD_CD,
    CMD_DOWNLOAD,
    CMD_UPLOAD,
    CMD_LOCAL_DIR,
    CMD_LOCAL_CD,
    CMD_RESPONSE,
    CMD_PROGRESS,
    CMD_RESET,
    CMD_FILELIST,
    CMD_FILEINFO
};


//uint8_t buffer[512];  // most of the time will be 64 bytes, but if we support 512...
extern uint16_t rawhid_rx_tx_size;       // later will change ... hopefully

#define FILE_IO_SIZE 4096
#define FILE_BUFFER_SIZE (FILE_IO_SIZE * 8)
//uint8_t g_transfer_buffer[FILE_BUFFER_SIZE];
//uint16_t g_transfer_buffer_head = 0;
//uint16_t g_transfer_buffer_tail = 0;

typedef struct {
	uint16_t type;    // type of data in buffer
	uint16_t size;    // size of data in buffer
	uint8_t data[1];  // packet data.
} RawHID_packet_t;

typedef struct {
	uint16_t type;  // type of data in buffer
	uint16_t size;  // size of data in buffer
} RawHID_packet_header_t;

typedef struct {
	int32_t status;           // status information
	uint32_t size;            // optional size
	uint32_t modifyDateTime;  // optional date/time fields
} RawHID_status_packet_data_t;

typedef struct {
	uint32_t count;  // progress information
} RawHID_progress_packet_data_t;

typedef struct {
    uint8_t sec;   // 0-59
    uint8_t min;   // 0-59
    uint8_t hour;  // 0-23
    uint8_t wday;  // 0-6, 0=sunday
    uint8_t mday;  // 1-31
    uint8_t mon;   // 0-11
    uint8_t year;  // 70-206, 70=1970, 206=2106
} DateTimeFields;

typedef struct {
    uint32_t size;
    uint32_t createTime;
    uint32_t modifyTime;
    uint8_t isDirectory;
    char name[1];

} RawHid_file_info_t;


extern uint16_t rawhid_rx_tx_size;

extern void remote_dir(std::vector<std::string> cmd_line_parts);
extern void change_directory(std::vector<std::string> cmd_line_parts);
extern void upload(std::vector<std::string> cmd_line_parts);
extern void download(std::vector<std::string> cmd_line_parts);
extern void breakTime(uint32_t time, DateTimeFields& tm);


