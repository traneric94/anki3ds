#ifndef ANKI3DS_TESTS_STUBS_3DS_H
#define ANKI3DS_TESTS_STUBS_3DS_H

#define BIT(value) (1u << (value))

typedef unsigned int u32;
typedef unsigned char u8;
typedef int Result;

#define R_FAILED(result) ((result) < 0)

enum
{
	KEY_A = BIT(0),
	KEY_B = BIT(1),
	KEY_SELECT = BIT(2),
	KEY_START = BIT(3),
	KEY_DRIGHT = BIT(4),
	KEY_DLEFT = BIT(5),
	KEY_DUP = BIT(6),
	KEY_DDOWN = BIT(7),
	KEY_R = BIT(8),
	KEY_L = BIT(9),
	KEY_X = BIT(10),
	KEY_Y = BIT(11),
	KEY_CPAD_RIGHT = BIT(28),
	KEY_CPAD_LEFT = BIT(29),
	KEY_CPAD_UP = BIT(30),
	KEY_CPAD_DOWN = BIT(31),
	KEY_UP = KEY_DUP | KEY_CPAD_UP,
	KEY_DOWN = KEY_DDOWN | KEY_CPAD_DOWN,
	KEY_LEFT = KEY_DLEFT | KEY_CPAD_LEFT,
	KEY_RIGHT = KEY_DRIGHT | KEY_CPAD_RIGHT,
};

Result ptmuInit(void);
void ptmuExit(void);
Result PTMU_GetShellState(u8 *state);
Result PTMU_GetBatteryLevel(u8 *level);
Result PTMU_GetBatteryChargeState(u8 *state);

#endif
