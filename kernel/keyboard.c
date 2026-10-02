#include "keyboard.h"
#include "serial.h"

#define KBD_DATA_PORT   0x60

/* US QWERTY scan code set 1, lower-case (key-down). 0 = no printable. */
static const char kbd_us[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0, ' ',
};

/* Shifted variants for the same indices. */
static const char kbd_us_shift[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0, ' ',
};

static volatile char    ring[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t ring_head = 0;
static volatile uint32_t ring_tail = 0;
static volatile int      shift_down = 0;

static void ring_push(char c)
{
    uint32_t next = (ring_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == ring_tail) return;   /* full, drop */
    ring[ring_head] = c;
    ring_head = next;
}

void keyboard_init(void)
{
    /* Drain any pending bytes. */
    for (int i = 0; i < 16; i++) {
        if (!(inb(0x64) & 1)) break;
        (void)inb(KBD_DATA_PORT);
    }
    ring_head = ring_tail = 0;
    shift_down = 0;
    serial_print("kbd: initialised\n");
}

void keyboard_irq(void)
{
    uint8_t sc = inb(KBD_DATA_PORT);

    /* Bit 7 set means key release. */
    if (sc & 0x80) {
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) shift_down = 0;
        return;
    }

    /* Shift presses */
    if (sc == 0x2A || sc == 0x36) {
        shift_down = 1;
        return;
    }

    /* Only handle set-1 scancodes we recognise */
    if (sc >= sizeof(kbd_us)) return;

    char c = shift_down ? kbd_us_shift[sc] : kbd_us[sc];
    if (c == 0) return;

    ring_push(c);
}

int keyboard_has_key(void)
{
    return ring_head != ring_tail;
}

char keyboard_poll(void)
{
    if (ring_head == ring_tail) return 0;
    char c = ring[ring_tail];
    ring_tail = (ring_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}


char keyboard_getchar_blocking(void)
{
    for (;;) {
        char c = keyboard_poll();
        if (c) return c;
        __asm__ volatile("hlt");
    }
}
