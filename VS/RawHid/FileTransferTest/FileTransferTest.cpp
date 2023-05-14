#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include "TransferFunctions.h"
#include <iostream>
#include <sstream>
#include <vector>
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

//=============================================================================
// forward references
//=============================================================================
extern int send_status_packet(int32_t status, uint32_t size, uint32_t modifyDateTime = 0,
    uint32_t timeout = 1000);
extern bool OnReceiveHIDData(uint32_t usage, const uint8_t* data, uint32_t len);
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
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}


void loop() {
    string command_line;
    std::vector<std::string> cmd_line_parts;

    printf(": ");
    getline(cin, command_line);
    cmd_line_parts = split(command_line, " ");
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
    printf("\tdownload(or d) <remote file> <localfile spec> - download(Receive) file from From remote\n");
    printf("\tupload(or u) <local file spec> [remote file spec] - Upload file to remote\n");
    printf("\tld [optional pattern] - show directory files on Local FS\n");
    printf("\tlc <file pattern> - Change directory on local FS\n");
    printf("\t$ - Reboot remote\n");
}


