#include "json_parser.h"
#include <string.h>
#include <ctype.h>

/* 去前缀时使用的本地缓冲，够放调用方传入的短字符串 */
#define STRIP_BUF_SIZE  64

void Json_GetValue(const char *json, const char *key, char *out, uint16_t out_size)
{
    const char *p;
    const char *start;
    const char *end;
    uint16_t    len;

    if (out == NULL || out_size == 0) return;
    out[0] = '\0';
    if (json == NULL || key == NULL) return;

    p = strstr(json, key);                  /* 1. 找键 */
    if (p == NULL) return;

    p = strchr(p, ':');                     /* 2. 找冒号 */
    if (p == NULL) return;

    p = strchr(p, '"');                     /* 3. 找值的起始引号 */
    if (p == NULL) return;
    start = p + 1;

    end = strchr(start, '"');               /* 4. 找值的结束引号 */
    if (end == NULL) return;

    len = (uint16_t)(end - start);
    if (len > out_size - 1) len = (uint16_t)(out_size - 1);   /* 截断保护 */

    memcpy(out, start, len);
    out[len] = '\0';
}

void Json_StripGoodPrefix(char *src, uint16_t src_size)
{
    char     temp[STRIP_BUF_SIZE];
    char     result[STRIP_BUF_SIZE];
    char     low[STRIP_BUF_SIZE];
    char    *tok;
    uint32_t rlen  = 0;
    uint8_t  first = 1;
    uint16_t n;
    uint16_t i;

    if (src == NULL || src_size == 0) return;

    /* 先把源串安全拷进本地缓冲 */
    n = (uint16_t)strlen(src);
    if (n > STRIP_BUF_SIZE - 1) n = STRIP_BUF_SIZE - 1;
    memcpy(temp, src, n);
    temp[n] = '\0';
    result[0] = '\0';

    tok = temp;
    while (*tok != '\0')
    {
        char       *sep;
        const char *piece;
        uint32_t    plen;

        while (*tok == ' ') tok++;                  /* 跳过前导空格 */
        if (*tok == '\0') break;

        sep = strchr(tok, ',');                     /* 本段以 ',' 结尾（最后一段没有） */
        if (sep != NULL) *sep = '\0';

        /* 转小写后判断是否以 "good" 开头（不区分大小写） */
        n = (uint16_t)strlen(tok);
        if (n > STRIP_BUF_SIZE - 1) n = STRIP_BUF_SIZE - 1;
        for (i = 0; i < n; i++)
        {
            low[i] = (char)tolower((unsigned char)tok[i]);
        }
        low[n] = '\0';

        piece = (strncmp(low, "good", 4) == 0) ? (tok + 4) : tok;

        if (!first && rlen < STRIP_BUF_SIZE - 1)    /* 非首段补分隔符 */
        {
            result[rlen++] = ',';
        }

        plen = (uint32_t)strlen(piece);
        if (plen > STRIP_BUF_SIZE - 1 - rlen)       /* 拼接长度保护 */
        {
            plen = STRIP_BUF_SIZE - 1 - rlen;
        }
        memcpy(&result[rlen], piece, plen);
        rlen += plen;
        result[rlen] = '\0';

        first = 0;

        if (sep == NULL) break;                     /* 已是最后一段 */
        tok = sep + 1;
    }

    /* 结果写回 src，长度受 src_size 限制 */
    n = (uint16_t)rlen;
    if (n > src_size - 1) n = (uint16_t)(src_size - 1);
    memcpy(src, result, n);
    src[n] = '\0';
}
