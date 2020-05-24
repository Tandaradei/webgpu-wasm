#ifndef SPIDER_COLOR_H_
#define SPIDER_COLOR_H_

#include <stdint.h>

typedef struct SPColorRGB8 {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t v[3];
    };
} SPColorRGB8;


/*
Converts a 8-byte uint color component to an 32-bit float color component
*/ 
inline float _spColorComponent8ToFloat(uint8_t component) {
    return (float)component / 255.0f;
}

#endif // SPIDER_COLOR_H