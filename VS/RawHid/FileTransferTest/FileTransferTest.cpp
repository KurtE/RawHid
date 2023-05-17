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

//=============================================================================
// main
//=============================================================================
int main()
{
    // quick and dirty to work like Arduino
    setup();

    for (;;) {
        loop();
    }
}


//=============================================================================
// ClearRAWHidMsgs
//=============================================================================
//DWORD WINAPI clearRAWHidMsgs(__in LPVOID lpParameter) {
void clearRAWHidMsgs() {
    while (g_clear_rawhid_messages) {
        uint8_t status_buf[512];
        RawHID_packet_t* packet = (RawHID_packet_t*)status_buf;
        RawHID_status_packet_data_t* status_packet = (RawHID_status_packet_data_t*)packet->data;
        int cb = rawhid_recv(0, status_buf, 512, 50);
        if (cb > 0) {
            printf("<<< %d, %u >>> ", packet->type, packet->size);
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
    }
    printf(">>> Thread Exit <<<\n");
}


//=============================================================================
// Setup
//=============================================================================

void setup() {
    int r;

    // C-based example is 16C0:0480:FFAB:0200
    r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
    if (r <= 0) {
        // Arduino-based example is 16C0:0486:FFAB:0200
        r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
        if (r <= 0) {
            printf("no rawhid device found\n");
        }
    }
    printf("found rawhid device\n");
    rawhid_rx_tx_size = rawhid_txSize(0);
    printf("packet size:%u\n", rawhid_rx_tx_size);

    show_command_help();
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
    for (auto i : cmd_line_parts) cout << i << endl;

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
        int r;

        // C-based example is 16C0:0480:FFAB:0200
        r = rawhid_open(1, 0x16C0, 0x0480, 0xFFAB, 0x0200);
        if (r <= 0) {
            // Arduino-based example is 16C0:0486:FFAB:0200
            r = rawhid_open(1, 0x16C0, 0x0486, 0xFFAB, 0x0200);
            if (r <= 0) {
                printf("no rawhid device found\n");
            }
        }
        printf("found rawhid device\n");
        rawhid_rx_tx_size = rawhid_txSize(0);
        printf("packet size:%u\n", rawhid_rx_tx_size);
    }

    else {
        show_command_help();
    }
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


