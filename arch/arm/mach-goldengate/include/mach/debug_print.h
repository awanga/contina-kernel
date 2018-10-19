#ifndef __DEBUG_PRINT_H__
#define __DEBUG_PRINT_H__

#include <mach/uncompress.h>

static inline void g2_debug_putx_no_0x(unsigned int val) {
    int i;
    unsigned int v1, v2;
    v1 = val;
    for (i = 0; i < 8; i++) {
        v2 = (v1 & 0xf0000000)>>28;
        v1 = (v1 << 4);
        if (v2 <= 9) {
                putc('0'+v2);
        } else {
                putc('a' + v2 - 10);
        }
    }
};

static inline void g2_debug_putx(unsigned int val) {
    putc('0');
    putc('x');
    g2_debug_putx_no_0x(val);
};



static inline void g2_debug_print(const char *ptr)
{
  char c;

  while ((c = *ptr++) != '\0') {
    if (c == '\n')
      putc('\r');
    putc(c);
  }

  flush();
}

#endif /* __DEBUG_PRINT_H__ */
