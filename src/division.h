#ifndef LAB2_DIVISION_H
#define LAB2_DIVISION_H

enum calculation { CALC_OK, CALC_EMPTY, CALC_INVALID, CALC_ZERO, CALC_RANGE };
enum calculation divide_line(const char *line, float *result);

#endif
