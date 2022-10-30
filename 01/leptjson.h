#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h>
#define lept_init(v) do { (v)->type = MY_NULL; } while(0)

// 定义json的数据类型
typedef enum{
    MY_NULL, MY_FALSE, MY_TRUE, MY_NUMBER, MY_STRING, MY_ARRAY, MY_OBJECT
}lept_type;

// 定义解析json的结果
enum{
    LEPT_PARSE_OK = 0,                      // 无异常
    LEPT_PARSE_EXPECT_VALUE,                // 没传入值
    LEPT_PARSE_INVALID_VALUE,               // 无效的值（无法识别的值）
    LEPT_PARSE_ROOT_NOT_SINGULAR,           // 一个参数传了多个值
    LEPT_PARSE_NUMBER_TOO_BIG,              // 数字太大存不下
    LEPT_PARSE_MISS_QUOTATION_MARK,         // 引号对不上
    LEPT_PARSE_INVALID_STRING_ESCAPE,       // 无效的转义字符串（这个字符无转义功能）
    LEPT_PARSE_INVALID_STRING_CHAR,         // 字符串中有无效字符(转义格式错)
    LEPT_PARSE_INVALID_UNICODE_HEX,         // 编码错误
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,   // 编码代理对检查
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,// 错过 ',' 或 '[]'
    LEPT_PARSE_MISS_KEY,                    // 没有key
    LEPT_PARSE_MISS_COLON,                  // 没有冒号（或者是缺少值的意思）
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET  // 错过 ',' 或 '{}'
};

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

struct lept_value{
    // union比直接随意扔在struct里更省内存->这里要看它的存储结构而不是数据大小
    union {
        struct { lept_member* m ; size_t objSize;}; // object : 包括每个元素的详情
        struct { lept_value* e; size_t arrSize;}; // array : arrSize是元素个数！
        struct { char* s; size_t len;}; //string
        double n;   // number
    };
    lept_type type;
};

struct lept_member
{
    char* k; size_t klen; // 单个对象的键由字符串表示，klen是字符串长度
    lept_value v;
};


// json的解析函数
int lept_parse(lept_value* v, const char* json);

// 获得json的类型（要有返回值）
lept_type lept_get_type(const lept_value* v);

// 这些set都是先清空原有内存-》要free掉
int lept_get_bool(const lept_value* v);
void lept_set_bool(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

size_t lept_get_array_size(const lept_value* v);
lept_value* lept_get_array_element(const lept_value* v, size_t index);

#define lept_set_null(v) lept_free(v)
const char* lept_get_str(const lept_value* v);
size_t lept_get_str_len(const lept_value* v);
void lept_set_str(lept_value* v, const char* s, size_t len);
void lept_set_arr_one(lept_value* v);
void lept_free(lept_value* v);

size_t lept_get_object_size(const lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(const lept_value* v, size_t index);
#endif
