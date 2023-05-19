#pragma once
extern bool seremu_begin(int rawhid_index);
extern bool seremu_end();
extern bool seremu_available();
extern void seremu_print_pending_data();

