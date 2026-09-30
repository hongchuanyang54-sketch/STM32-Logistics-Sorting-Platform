#ifndef __JSON_PARSER_H
#define __JSON_PARSER_H

#include <stdint.h>

/*
 * 极简 JSON 字段提取。
 * 上游（网关/上位机）下发的报文格式固定且简单，例如：
 *   {"Down":"GoodC,GoodD","Up":"GoodA","Left":"","Right":""}
 * 因此这里不做通用 JSON 解析，只做「找键 → 取引号内字符串」。
 */

/**
  * @brief  取出某个键对应的字符串值
  * @param  json     源字符串
  * @param  key      键名（按原始子串查找，调用方自行决定是否带引号）
  * @param  out      输出缓冲
  * @param  out_size 输出缓冲大小（含结尾 '\0'）
  * @note   找不到键、格式不符或参数非法时，out 一律置为空字符串。
  *         取值超过 out_size-1 会被截断后补 '\0'。
  */
void Json_GetValue(const char *json, const char *key, char *out, uint16_t out_size);

/**
  * @brief  去掉货物名前面的 "Good" 前缀（不区分大小写），原地修改
  * @param  src      待处理字符串，形如 "GoodA,GoodB"
  * @param  src_size 缓冲大小（含结尾 '\0'）
  * @note   处理结果形如 "A,B"。逐段（以 ',' 分隔）判断，
  *         只有确实以 good 开头的段才去掉前 4 个字符。
  */
void Json_StripGoodPrefix(char *src, uint16_t src_size);

#endif
