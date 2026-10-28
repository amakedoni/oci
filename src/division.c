#include "division.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

enum calculation divide_line(const char *line, float *result)
{
    const char *position = line;
    int first = 1;
    while (*position != '\0') {
        while (isspace((unsigned char)*position)) ++position;
        if (*position == '\0') break;
        char *end;
        errno = 0;
        float value = strtof(position, &end);
        if (end == position || (*end != '\0' && !isspace((unsigned char)*end)))
            return CALC_INVALID;
        if (errno == ERANGE || !isfinite(value)) return CALC_RANGE;
        if (first) { *result = value; first = 0; }
        else {
            if (value == 0.0f) return CALC_ZERO;
            *result /= value;
            if (!isfinite(*result)) return CALC_RANGE;
        }
        position = end;
    }
    return first ? CALC_EMPTY : CALC_OK;
}
