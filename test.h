#ifndef TEST_H__
#define TEST_H__

#define button_down        5
#define button_right       7
#define button_left        6
#define button_up          4
#define button_a           8
#define button_x           9
#define button_y           1
#define button_b           0
#define button_select      2
#define button_start       3
#define button_l           10
#define button_r           11

void test_update_input(int keyid, int playerNumber,int state);
void compute_all_states();
void update_vga(uint32_t *buf, unsigned stride);
void show_message(char * show_message);
size_t retro_serialize_size(void);
bool retro_serialize(void *data_, size_t size);
bool retro_unserialize(const void *data_, size_t size);

#endif
