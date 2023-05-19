#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include "TransferFunctions.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>

#ifndef OS_LINUX
#include <Windows.h>
#endif
using namespace std;
#include <string>
#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <sys/ioctl.h>
#include <termios.h>
#elif defined(OS_WINDOWS)
#include <conio.h>
#endif

#include "hid.h"
#include "seremu.h"

#define MAX_PACKET_SIZE 512

#define FILE_SIZE 22400000ul
int packet_size;
uint32_t count_packets;

volatile bool g_clear_rawhid_messages = false;

//=============================================================================
// forward references
//=============================================================================
extern int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime = 0,
    uint32_t timeout = 1000);
extern void setup();
extern void loop();
extern void show_command_help();
extern bool scan_for_rawhid_device();

//=============================================================================
// main
//=============================================================================
int main()
{
    // quick and dirty to work like Arduino
    scan_for_rawhid_device();
    show_command_help();

    for (;;) {
        loop();
    }
}


//=============================================================================
// ClearRAWHidMsgs
//=============================================================================
//DWORD WINAPI clearRAWHidMsgs(__in LPVOID lpParameter) {
void clearRAWHidMsgs() {
    bool seremu_ouputs_active = false;
    while (g_clear_rawhid_messages) {
        uint8_t status_buf[512];
        RawHID_packet_t* packet = (RawHID_packet_t*)status_buf;
        RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
        int cb = rawhid_recv(g_rawhid_index, status_buf, 512, 50);
        if (cb > 0) {
            if (seremu_ouputs_active) {
                puts("]<<<< <SEREMU >>>>\n");
                seremu_ouputs_active = false;
            }
            printf("(%d, %u) ", packet->type, packet->size);
            switch (packet->type) {
            case CMD_DATA: printf("DATA\n"); break;
            case CMD_DIR: printf("DIR\n"); break;
            case CMD_CD: printf("CD\n"); break;
            case CMD_DOWNLOAD: printf("DOWNLOAD\n"); break;
            case CMD_UPLOAD: printf("UPLOAD\n"); break;
            case CMD_LOCAL_DIR: printf("LOCAL_DIR\n"); break;
            case CMD_LOCAL_CD: printf("LOCAL_CD\n"); break;
            case CMD_RESPONSE: printf("RESPONSE\n"); break;
            case CMD_PROGRESS: printf("PROGRESS\n"); break;
            case CMD_RESET: printf("RESET\n"); break;
            case CMD_FILELIST: printf("FILELIST\n"); break;
            case CMD_FILEINFO: printf("FILEINFO\n"); break;
            }
            printf(": ");
        }
        else if (cb < 0) break;

        // Check for Seremu messages as well
        if (seremu_available()) {
            if (!seremu_ouputs_active) {
                puts("<<<SEREMU >>>> [\n");
                seremu_ouputs_active = true;
            }
            seremu_print_pending_data();
        }
    }
    if (seremu_ouputs_active) puts("]<<<< <SEREMU >>>>\n");
    printf(">>> Thread Exit <<<\n");
}



//=============================================================================
// loop
//=============================================================================
// for string delimiter
std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, pos_quote, delim_len = delimiter.length();
    string token;
    std::vector<std::string> res;

    for (;;) {
        if (s.substr(pos_start, 1) == "\"") {
            // next thing was a quote, try to find ending one. 
            // we will skip processing the "
            pos_start++;
            pos_quote = s.find("\"", pos_start);
            if (pos_quote == std::string::npos) break; // sort of error but...
            token = s.substr(pos_start, (pos_quote - pos_start)); // don't keep the "
            pos_start = pos_quote + 1;
            if (s.substr(pos_start, delim_len) == delimiter) pos_start += delim_len;
            res.push_back(token);

        }
        else {
            // get the next end point.
            if ((pos_end = s.find(delimiter, pos_start)) == std::string::npos) break;
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }
    }
    //
    res.push_back(s.substr(pos_start));
    return res;
}



void loop() {
    string command_line;
    std::vector<std::string> cmd_line_parts;

    printf(": ");

    // Quick and dirty thread to clear out messages from Teensy.
    g_clear_rawhid_messages = true;
    std::thread thread_cleanup_msgs(clearRAWHidMsgs);
    getline(cin, command_line);
    cmd_line_parts = split(command_line, " ");

    // signal the other thread to exit.
    g_clear_rawhid_messages = false;
    if (thread_cleanup_msgs.joinable()) {
        thread_cleanup_msgs.join(); 
    }

    if ((cmd_line_parts[0] == "dir") || (cmd_line_parts[0] == "ls")) {
        remote_dir(cmd_line_parts);
    }
    else if ((cmd_line_parts[0] == "upload") || (cmd_line_parts[0] == "u")) {
        upload(cmd_line_parts);
    }
    else if ((cmd_line_parts[0] == "download") || (cmd_line_parts[0] == "d")) {
        download(cmd_line_parts);
    }
    else if ((cmd_line_parts[0] == "cd") || (cmd_line_parts[0] == "c")) {
        change_directory(cmd_line_parts);
    }
    else if (cmd_line_parts[0] == "pwd") {
        print_remote_current_directory();
    }
    else if ((cmd_line_parts[0] == "mkdir") || (cmd_line_parts[0] == "m")) {
        create_directory(cmd_line_parts);
    }
    else if (cmd_line_parts[0] == "rmdir") {
        remove_directory(cmd_line_parts);
    }  
    else if ((cmd_line_parts[0] == "del") || (cmd_line_parts[0] == "rm")) {
        delete_file(cmd_line_parts);
    }
    else if ((cmd_line_parts[0] == "quit") || (cmd_line_parts[0] == "exit")) {
        exit(0);
    }

    else if (cmd_line_parts[0] == "reset") {
        resetRAWHID();
    }
    else if (cmd_line_parts[0] == "scan") {
        scan_for_rawhid_device();
    }

    else {
        show_command_help();
    }
}

//=============================================================================
// Scan for rawhid device - check also for SEREMU
//=============================================================================
bool scan_for_rawhid_device() {
    int r;

    // C-based example is 16C0:0480:FFAB:0200
    //r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
    // This one is setup for Teensy based one built as rawhid
    //if (r <= 0) {
    // Arduino-based example is 16C0:0486:FFAB:0200
    //r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
    r = rawhid_open(2, 0x16C0, 0x0486, -1, -1);
    if (r <= 0) {
        printf("no rawhid device found\n");
        return false;
    }
    //}
    printf("found rawhid %d devices\n", r);
    g_rawhid_index = -1;
    seremu_end(); // make sure we are not running one now. 
    for (int i = 0; i < r; i++) {
        printf("\t(%x, %x):", rawhid_usage_page(i), rawhid_usage(i));
        if ((rawhid_usage_page(i) == 0xFFAB) && (rawhid_usage(i) == 0x0200)) {
            // found the rawhid one
            g_rawhid_index = i;
                g_rawhid_rx_tx_size = rawhid_txSize(i);
                printf("Rawhid index %d packet size:%u\n", i, g_rawhid_rx_tx_size);
        }
        else if ((rawhid_usage_page(i) == 0xffc9) && (rawhid_usage(i) == 0x4)) {
            // SEREMU
            g_rawhid_sereum_index = i;  //which rawhid index to use
            uint16_t g_seremu_rx_size = rawhid_rxSize(i);
            printf("SEREMU index % d packet size : % u\n", i, g_seremu_rx_size);
            seremu_begin(i);
        }
        else {
            printf("???\n");
        }
    }

    return true;

}


//=============================================================================
// Show the user some information about commands
//=============================================================================
void show_command_help() {
    printf("Command list\n");
    printf("\tdir(or ls) [optional pattern] - show directory files on remote FS\n");
    printf("\tcd <file pattern> - Change directory on remote FS\n");
    printf("\tpwd - Print remote working directory\n");
    printf("\tmkdir <name> - Create a directory on remote FS\n");
    printf("\trmdir <name> - Remove a directory on remote FS\n");
    printf("\tdel(rm) <name> - delete a file on FS\n");
    printf("\tdownload(or d) <remote file> <localfile spec> - download(Receive) file from From remote\n");
    printf("\tupload(or u) <local file spec> [remote file spec] - Upload file to remote\n");
    //printf("\tld [optional pattern] - show directory files on Local FS\n");
    //printf("\tlc <file pattern> - Change directory on local FS\n");
    printf("\treset - reboots the remote teensy\n");
    printf("\tscan - run the rawhid scan again\n");
    printf("\texit - end the program\n");
}


