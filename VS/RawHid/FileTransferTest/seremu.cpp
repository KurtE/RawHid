
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include "TransferFunctions.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include "seremu.h"

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


static int s_seremu_index = -1;
static bool s_seremu_active = false;
static std::thread* seremu_thread = nullptr;
static std::mutex seremu_mutex;

typedef struct _seremu_data {
    _seremu_data* next;
    uint16_t    cb;
    char        data[64];
} seremu_data_t;

static volatile seremu_data_t* s_first_seremu_buffer = nullptr;
static volatile seremu_data_t* s_last_seremu_buffer = nullptr;


void seremu_thread_proc() {
    while (s_seremu_active) {
        uint8_t packet_buf[64];
        int cb = rawhid_recv(s_seremu_index, packet_buf, sizeof(packet_buf), 50);
        if (cb > 0) {
            seremu_data_t *new_buf = (seremu_data_t * )malloc(sizeof(seremu_data_t));
            if (new_buf) {
                new_buf->cb = cb;
                memcpy(new_buf->data, packet_buf, cb);
                new_buf->next = nullptr;
                seremu_mutex.lock();
                if (s_last_seremu_buffer) s_last_seremu_buffer->next = new_buf;
                else {
                    s_first_seremu_buffer = new_buf;
                }
                s_last_seremu_buffer = new_buf;
                seremu_mutex.unlock();
            }
        }
        else if (cb < 0) break;
    }
    printf(">>> Seremu Thread Exit <<<\n");

}

bool seremu_begin(int rawhid_index) {
	if (rawhid_index < 0) return false;
	s_seremu_index = rawhid_index;
	s_seremu_active = true;
    seremu_thread = new std::thread(seremu_thread_proc);
    return (seremu_thread != nullptr);
}

bool seremu_end() {
    if (seremu_thread == nullptr) return false;
    s_seremu_active = false;
    if (seremu_thread->joinable()) {
        seremu_thread->join();
    }
    delete seremu_thread;
    seremu_thread = nullptr;
    return true;
}
bool seremu_available() {
    return s_first_seremu_buffer != nullptr;
}

void seremu_print_pending_data() {
    while (s_first_seremu_buffer) {
        seremu_mutex.lock();
        volatile seremu_data_t* print_buf = s_first_seremu_buffer;
        s_first_seremu_buffer = print_buf->next;
        if (s_first_seremu_buffer == nullptr) s_last_seremu_buffer = nullptr;
        seremu_mutex.unlock();
        fwrite((void*)print_buf->data, 1, print_buf->cb, stdout);
        free((void*)print_buf);
    }

}
