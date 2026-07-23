#include "json_parser.h"
#include <string.h>
#include <stdlib.h>

uint8_t ParseAprilTagJSON(const char *json_str, AprilTag_Result_t *result) {
    char *p_tags, *p;
    if (!json_str || !result) return 0;

    // ????
    result->tag_id = 0;
    result->center_x = 0;
    result->center_y = 0;
    result->valid = 0;

    // ??tags????,??tags??????,??timestamp??
    p_tags = strstr(json_str, "\"tags\":");
    if (!p_tags) return 0;

    // ??tag id
    p = strstr(p_tags, "\"id\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    result->tag_id = atoi(p);

    // ??center_x
    p = strstr(p_tags, "\"center_x\"");
    if (p) {
        p = strchr(p, ':');
        if (p) result->center_x = atoi(p + 1);
    }

    // ??center_y
    p = strstr(p_tags, "\"center_y\"");
    if (p) {
        p = strchr(p, ':');
        if (p) result->center_y = atoi(p + 1);
    }

    result->valid = 1;
    return 1;
}
