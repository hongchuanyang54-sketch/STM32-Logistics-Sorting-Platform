#ifndef __JSON_PARSER_H
#define __JSON_PARSER_H

#include <stdint.h>

typedef struct {
    uint8_t tag_id;        // AprilTag ID
    uint16_t center_x;     // ??X??
    uint16_t center_y;     // ??Y??
    uint8_t valid;         // ??????
} AprilTag_Result_t;

// ????
uint8_t ParseAprilTagJSON(const char *json_str, AprilTag_Result_t *result);

#endif
