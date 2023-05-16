#include "TransferFunctions.h"
#include <fstream>
//#include <winnt.h>
//#include "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um/winnt.h"
#ifndef OS_LINUX
#include <Windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef OS_LINUX
#include <sysinfoapi.h>
#endif

#include <chrono>

uint16_t rawhid_rx_tx_size = 64;       // Checks to see when connected.
//uint8_t g_transfer_buffer[FILE_BUFFER_SIZE];
//uint16_t g_transfer_buffer_head = 0;
//uint16_t g_transfer_buffer_tail = 0;
uint8_t buffer[512];  // most of the time will be 64 bytes, but if we support 512...

extern uint32_t makeTime(const DateTimeFields& tm);


//=============================================================================
// Send packet to USBHost Raw hid
//=============================================================================
bool send_rawhid_packet(int cmd, const void* packet_data, uint16_t data_size, uint32_t timeout = 250) {
	//DBGPrintf("lsrhid: %d %p %u %u\n", cmd, packet_data, data_size, timeout);
	memset(buffer, 0, rawhid_rx_tx_size);
	RawHID_packet_t* packet = (RawHID_packet_t*)buffer;
	packet->type = cmd;
	if (packet_data) memcpy(packet->data, packet_data, data_size);
	packet->size = data_size;

	//elapsedMillis emTimeout = 0;
	if (rawhid_send(0, buffer, rawhid_rx_tx_size, timeout) > 0) return true;
	return false;
}

int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime, uint32_t timeout) {
	RawHID_status_packet_data_t status_data = { status, size, modifyDateTime };
	return send_rawhid_packet(CMD_RESPONSE, &status_data, sizeof(status_data), timeout);
}



void remote_reset() {
	printf("Send remote reset command\n");
	send_rawhid_packet(CMD_RESET, nullptr, 0, 5000);
}

void printSpaces(int count) {
	while (count > 0) {
		printf(" ");
		count--;
	}
}

void remote_dir(std::vector<std::string> cmd_line_parts) {

	printf("remote_dir called\n");
	const char* filename = nullptr;
	size_t cb = 0;
	if (cmd_line_parts.size() > 1) {
		filename = cmd_line_parts[1].c_str();
		cb = strlen(filename);
	}
	if (!send_rawhid_packet(CMD_FILELIST, filename, (uint16_t)cb)) {
		printf("Remote dir/ls *** failed ***\n");
		return;
	}
	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("Receive *** failed ***\n");
			break;
		}
		if (packet->type == CMD_RESPONSE) {
			printf("*** completed ***\n");
			break;
		}
		else if (packet->type == CMD_FILEINFO) {
			RawHid_file_info_t* file_info = (RawHid_file_info_t *)packet->data;
			printf("%s", file_info->name);
			printSpaces(36 - (int)strlen(file_info->name));
			DateTimeFields dtf;
			if (file_info->createTime) {
				breakTime(file_info->createTime, dtf);
				printf(" C: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, 
					dtf.year + 1900, dtf.hour, dtf.min);
			}

			if (file_info->modifyTime) {
				breakTime(file_info->modifyTime, dtf);
				printf(" M: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday,
					dtf.year + 1900, dtf.hour, dtf.min);
			}
			if (file_info->isDirectory) {
				printf(" <dir>\n");
			}
			else {
				// files have sizes, directories do not
				printf(" %u\n", file_info->size);
			}
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}

}

//-----------------------------------------------------------------------------
// Change directory on the remote
//-----------------------------------------------------------------------------
void change_directory(std::vector<std::string> cmd_line_parts) {
	printf("change_directory called\n");

	//todo: cleanup duplicate junk here.
	const char* filename = nullptr;
	size_t cb = 0;
	if (cmd_line_parts.size() > 1) {
		filename = cmd_line_parts[1].c_str();
		cb = strlen(filename);
	}
	if (!send_rawhid_packet(CMD_CD, filename, (uint16_t)cb)) {
		printf("Change Directory *** failed***\n");
		return;
	}

	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("*** Timeout ***\n");
			break;
		}
		if (packet->type == CMD_RESPONSE) {
			RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
			if (status_packet->status == 0) printf("*** completed ***\n");
			else printf("*** failed ***\n");
			break;
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}

}

//-----------------------------------------------------------------------------
// create  directory on the remote
//-----------------------------------------------------------------------------
void create_directory(std::vector<std::string> cmd_line_parts) {
	printf("Create Directory called\n");

	//todo: cleanup duplicate junk here.
	const char* filename = nullptr;
	size_t cb = 0;
	if (cmd_line_parts.size() > 1) {
		filename = cmd_line_parts[1].c_str();
		cb = strlen(filename);
	}
	if (!send_rawhid_packet(CMD_MKDIR, filename, (uint16_t)cb)) {
		printf("Make Directory *** failed ***\n");
		return;
	}

	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("*** Timeout ***\n");
			break;
		}
		if (packet->type == CMD_RESPONSE) {
			RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
			if (status_packet->status == 0) printf("*** completed ***\n");
			else printf("*** failed ***\n");
			break;
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}

}

//-----------------------------------------------------------------------------
// Remove  directory on the remote
//-----------------------------------------------------------------------------
void remove_directory(std::vector<std::string> cmd_line_parts) {
	printf("Remove Directory called\n");

	//todo: cleanup duplicate junk here.
	const char* filename = nullptr;
	size_t cb = 0;
	if (cmd_line_parts.size() > 1) {
		filename = cmd_line_parts[1].c_str();
		cb = strlen(filename);
	}
	if (!send_rawhid_packet(CMD_RMDIR, filename, (uint16_t)cb)) {
		printf("Remove Directory *** failed ***\n");
		return;
	}

	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("*** Timeout ***\n");
			break;
		}
		if (packet->type == CMD_RESPONSE) {
			RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
			if (status_packet->status == 0) printf("*** completed ***\n");
			else printf("*** failed ***\n");
			break;
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}
}

//-----------------------------------------------------------------------------
// delete a file on the remote
//-----------------------------------------------------------------------------
void delete_file(std::vector<std::string> cmd_line_parts) {
	printf("delete file called\n");

	//todo: cleanup duplicate junk here.
	const char* filename = nullptr;
	size_t cb = 0;
	if (cmd_line_parts.size() > 1) {
		filename = cmd_line_parts[1].c_str();
		cb = strlen(filename);
	}
	if (!send_rawhid_packet(CMD_DEL, filename, (uint16_t)cb)) {
		printf("delete file *** failed ***\n");
		return;
	}

	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("*** Timeout ***\n");
			break;
		}
		if (packet->type == CMD_RESPONSE) {
			RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
			if (status_packet->status == 0) printf("*** completed ***\n");
			else printf("*** failed ***\n");
			break;
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}
}

extern void print_remote_current_directory(std::vector<std::string> cmd_line_parts);
//-----------------------------------------------------------------------------
// delete a file on the remote
//-----------------------------------------------------------------------------
void print_remote_current_directory() {
	printf("retrieve remote current directory\n");

	if (!send_rawhid_packet(CMD_PWD, nullptr, 0)) {
		printf("PWD command *** failed ***\n");
		return;
	}

	uint8_t buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)buf;
	for (;;) {
		int cb = rawhid_recv(0, buf, 512, 1000);
		if (cb <= 0) {
			printf("*** Timeout ***\n");
			break;
		}
		if (packet->type == CMD_DATA) {
			printf("\t'%s'\n", packet->data);
			break;
		}
		else {
			printf("**** unexpected packet type:%u size:%u\n", packet->type, packet->size);
		}
	}
}

//-----------------------------------------------------------------------------
// upload
//-----------------------------------------------------------------------------
void upload(std::vector<std::string> cmd_line_parts) {
	printf("upload called\n");
	if (cmd_line_parts.size() < 2) {
		printf("upload failed must specify file to upload\n");
		return;
	}
	// first parameter is the local path.
	// 
	// first pass quick and dirty.
	FILE* fp;
	const char* pathname = cmd_line_parts[1].c_str();
	
	// fileInfo.CreationTime is when file was created.
#ifdef OS_LINUX
	fp = fopen(pathname, "rb"); 
	if (fp == NULL) {
#else
	errno_t status = fopen_s(&fp, pathname, "rb");
	if (status != 0) {
#endif
		printf("Failed to open %s\n", pathname);
		return;
	}
	printf("Uploading %s", pathname);
	if ((cmd_line_parts.size() > 2)) pathname = cmd_line_parts[2].c_str();
	else {
		// we will extract the last section off of the pathname;
		const char* p_last_slash = nullptr;
		for (const char* p = pathname; *p != 0; p++) {
			if ((*p == '/') || (*p == '\\')) p_last_slash = p;
		}
		if (p_last_slash != nullptr) pathname = p_last_slash + 1;
	}

	printf(" to %s\n", pathname);


	// need to add some error checking.

	if (!send_rawhid_packet(CMD_UPLOAD, pathname, (uint16_t)strlen((char*)pathname))) {
		printf("upload *** failed ***\n");
		return;
	}

	uint8_t status_buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)status_buf;
	RawHID_status_packet_data_t * status_packet = (RawHID_status_packet_data_t*)packet->data;
	int cb = rawhid_recv(0, status_buf, 512, 1000);
	if ((cb <= 0) || (packet->type != CMD_RESPONSE) || (status_packet->status != 0)) {
		printf("upload *** failed ***\n");
		return;
	}

	uint32_t remote_buffer_free = status_packet->size;

	// Send them a status packet that has size and modify time
	fseek(fp, 0, SEEK_END);
	off_t offset = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint32_t file_size = offset;
	printf("File size:%u\n", file_size);

	// Modify date? 
	uint16_t modify_date = 0;
#ifndef OS_LINUX
	FILETIME ftm;
	if (GetFileTime(fp, nullptr, nullptr, &ftm)) {
		SYSTEMTIME stm;
		FileTimeToSystemTime(&ftm, &stm);
		DateTimeFields dtf = { (uint8_t)stm.wSecond, (uint8_t)stm.wMinute, (uint8_t)stm.wHour, (uint8_t)stm.wDayOfWeek, 
			(uint8_t)stm.wDay, (uint8_t)(stm.wMonth - 1),(uint8_t)(stm.wYear - (1900 - 1601)) };
		modify_date = makeTime(dtf);
		printf("Modify Date: %02u / %02u / %04u %02u: %02u", stm.wMonth, stm.wDay, stm.wYear + 1601,
			stm.wHour, stm.wMinute);
	}
#endif
	send_status_packet(0, file_size, modify_date, 5000);  // send a failure code


	// in this case we are doing a quick and dirty
	uint16_t cb_transfer = rawhid_rx_tx_size - sizeof(RawHID_packet_header_t);
	printf("bytes per data packet:%u\n", cb_transfer);
	size_t cbRead = 0;
	uint8_t transfer_buf[512];
	//uint32_t start_time = GetTickCount();
	using namespace std::chrono;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	uint8_t dot_count = 0;
	while (!feof(fp)) {
		do {
			// see if the other side has sent us anything
			int cb = rawhid_recv(0, status_buf, 512, 0);
			if (cb < 0) {
				printf("*** Timeout ***\n");
				break;
			}
			else if (cb > 0) {
				if (packet->type == CMD_PROGRESS) {
					RawHID_progress_packet_data_t* progress = (RawHID_progress_packet_data_t*)packet->data;
					remote_buffer_free += progress->count;
					//printf("Progress: %u %u\n", progress->count, remote_buffer_free);
					printf(".");
					dot_count++;
					if ((dot_count & 0x3f) == 0) printf("\n");
				}
				// maybe check status as well
			}
		} while (remote_buffer_free < cb_transfer);

		cbRead = fread(transfer_buf, 1, cb_transfer, fp);
		//printf("%u\n", cbRead);
		if (!send_rawhid_packet(CMD_DATA, transfer_buf, (uint16_t)cbRead, 5000)) {
			printf("\n*** Upload Failed - Timeout ***\n");
			fclose(fp);
			return;
		}
		remote_buffer_free -= (uint32_t)cbRead;
	}
	if (cbRead == cb_transfer) {
		// send 0 length to let them know we are done
		send_rawhid_packet(CMD_DATA, transfer_buf, 0, 5000);
	}
	fclose(fp);

	high_resolution_clock::time_point end_time = high_resolution_clock::now();
	duration<double, std::milli> delta_time = end_time - start_time;

	printf("\n*** Completed dt:%f ***\n", delta_time.count());
}


//-----------------------------------------------------------------------------
// download
//-----------------------------------------------------------------------------
void download(std::vector<std::string> cmd_line_parts) {
	printf("Download called\n");
	if (cmd_line_parts.size() < 3) {
		printf("faile: must specifiy remote file name as well as local path\n");
		return;
	}


	const char* remote_filename = cmd_line_parts[1].c_str();
	const char* local_filename = cmd_line_parts[2].c_str();
	printf("Downloading from:%s to:%s\n", remote_filename, local_filename);
	FILE* fp;

	if (!send_rawhid_packet(CMD_DOWNLOAD, remote_filename, (uint16_t)strlen((char*)remote_filename))) {
		printf("download of %s failed = failed to send command", remote_filename);
		return;
	}

	uint8_t status_buf[512];
	RawHID_packet_t* packet = (RawHID_packet_t*)status_buf;
	RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
	int cb = rawhid_recv(0, status_buf, 512, 1000);
	if ((cb <= 0) || (packet->type != CMD_RESPONSE) || (status_packet->status != 0)) {
		printf("download failed - no or improper response from remote\n");
		return;
	}

	uint32_t modify_date_time = status_packet->modifyDateTime;
	// need to add some error checking.
	using namespace std::chrono;
	high_resolution_clock::time_point start_time = high_resolution_clock::now();
	uint32_t total_bytes_transfered = 0;
	uint32_t report_count = 0;
	uint16_t cb_transfer = rawhid_rx_tx_size - sizeof(RawHID_packet_header_t);
#ifdef OS_LINUX
	fp = fopen(local_filename, "wb");
	if (fp == NULL) {
#else
	errno_t status = fopen_s(&fp, local_filename, "wb");
	if (status != 0) {
#endif
		printf("Failed to open %s\n", local_filename);
		send_status_packet(2, 0, 0, 5000); // tell them we failed.
		return;
	}

	// Quick and dirty version. 
	// Lets try to write out 4K chunks.
	// now lets receive the data or complete or the other side sent us a status.
	RawHID_progress_packet_data_t progress_data = { FILE_IO_SIZE };
	uint8_t dot_count = 0;
	for (;;) {
		int cb = rawhid_recv(0, status_buf, 512, 5000);
		if (cb <= 0) {
			printf("failed timeout\n");
			fclose(fp);
			send_status_packet(2, 0, 0, 5000); // tell them we failed.
			return;
		}
		if (packet->type == CMD_DATA) {
			uint16_t cb_data = packet->size;
			//printf("%u\n", cb_data);
			if (cb_data == 0) break; // 0 length says EOF
			size_t cbWritten = fwrite(packet->data, 1, cb_data, fp);
			if (cbWritten < (size_t)cb_data) {
				printf("failed incomplete write: %u != %lu\n", cb_data, cbWritten);
				fclose(fp);
				send_status_packet(2, 0, 0, 5000); // tell them we failed.
				return;
			}
			total_bytes_transfered += cb_data;
			report_count += cb_data;
			if (report_count >= progress_data.count) {
				if (!send_rawhid_packet(CMD_PROGRESS, (uint8_t*)&progress_data, sizeof(progress_data), 1000)) {
					printf("failed timeout send progress\n");
					fclose(fp);
					send_status_packet(2, 0, 0, 5000); // tell them we failed.
					return;
				}
				printf(".");
				if ((++dot_count & 0x3f) == 0)printf("\n");
				report_count -= progress_data.count;
			}
			if (cb_data < cb_transfer) break;  // EOF
		}
		else {
			printf("??? message %u %u\n", packet->type, packet->size);
		}
	}

	// We finished :D 
	// Modify date? 
#ifndef OS_LINUX
	if (modify_date_time) {
		DateTimeFields dtf;
		breakTime(modify_date_time, dtf);
		SYSTEMTIME stm = { (WORD)(dtf.year + (WORD)(1900 - 1601)), (WORD)(dtf.mon + 1), dtf.wday, dtf.mday, dtf.hour, dtf.min, dtf.sec, 0 };
		FILETIME ftm;

		SystemTimeToFileTime(&stm, &ftm);
		SetFileTime(fp, nullptr, nullptr, &ftm);
		printf("Modify Date: %02u / %02u / %04u %02u: %02u", stm.wMonth, stm.wDay, stm.wYear + 1601,
			stm.wHour, stm.wMinute);
	}
#endif
	fclose(fp);
	
	high_resolution_clock::time_point end_time = high_resolution_clock::now();
	duration<double, std::milli> delta_time = end_time - start_time;

	printf("\nCompleted, total byte: %u elapsed millis: %f\n", total_bytes_transfered, delta_time.count());
}

//-----------------------------------------------------------------------------
// Reset RAWHID
//-----------------------------------------------------------------------------
void resetRAWHID() {
	printf("Send remote reset command\n");
	send_rawhid_packet(CMD_RESET, nullptr, 0, 5000);
}

//-----------------------------------------------------------------------------
// Remote directory
//-----------------------------------------------------------------------------
// leap year calculator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)     ( ((1970+(Y))>0) && !((1970+(Y))%4) && ( ((1970+(Y))%100) || !((1970+(Y))%400) ) )
static const uint8_t monthDays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
#define SECS_PER_MIN  60ul
#define SECS_PER_HOUR 3600ul
#define SECS_PER_DAY  86400ul


void breakTime(uint32_t time, DateTimeFields& tm)
{
	// break the given time_t into time components
	// this is a more compact version of the C library localtime function
	// note that year is offset from 1970 !!!

	uint8_t year;
	uint8_t month, monthLength;
	unsigned long days;

	tm.sec = time % 60;
	time /= 60; // now it is minutes
	tm.min = time % 60;
	time /= 60; // now it is hours
	tm.hour = time % 24;
	time /= 24; // now it is days
	tm.wday = ((time + 4) % 7);  // Sunday is day 0

	year = 0;
	days = 0;
	while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
		year++;
	}
	tm.year = year + 70; // year is offset from 1970

	days -= LEAP_YEAR(year) ? 366 : 365;
	time -= days; // now it is days in this year, starting at 0

	days = 0;
	month = 0;
	monthLength = 0;
	for (month = 0; month < 12; month++) {
		if (month == 1) { // february
			if (LEAP_YEAR(year)) {
				monthLength = 29;
			}
			else {
				monthLength = 28;
			}
		}
		else {
			monthLength = monthDays[month];
		}

		if (time >= monthLength) {
			time -= monthLength;
		}
		else {
			break;
		}
	}
	tm.mon = month;  // jan is month 0
	tm.mday = time + 1;     // day of month
}

uint32_t makeTime(const DateTimeFields& tm)
{
	// assemble time elements into time_t
	// note year argument is offset from 1970 (see macros in time.h to convert to other formats)
	// previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9

	int i;
	uint32_t seconds;

	// seconds from 1970 till 1 jan 00:00:00 of the given year
	seconds = (tm.year - 70) * (SECS_PER_DAY * 365);
	for (i = 70; i < tm.year; i++) {
		if (LEAP_YEAR(i - 70)) {
			seconds += SECS_PER_DAY;   // add extra days for leap years
		}
	}

	// add days for this year, months start from 1
	for (i = 0; i < tm.mon; i++) {
		if ((i == 1) && LEAP_YEAR(tm.year)) {
			seconds += SECS_PER_DAY * 29;
		}
		else {
			seconds += SECS_PER_DAY * monthDays[i];
		}
	}
	seconds += (tm.mday - 1) * SECS_PER_DAY;
	seconds += tm.hour * SECS_PER_HOUR;
	seconds += tm.min * SECS_PER_MIN;
	seconds += tm.sec;
	return seconds;
}

