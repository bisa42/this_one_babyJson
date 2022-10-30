#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leptjson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format)\
    do{\
        test_count++;\
        if (equality)\
            test_pass++;\
        else{\
            fprintf(stderr, "%s:%d: expect: " format "; actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    }while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect == (actual)), expect, actual, "%.17g")
#define EXPECT_EQ_STR(expect, actual, len) \
    EXPECT_EQ_BASE(sizeof(expect) - 1 == (len) && memcmp(expect, actual, len) == 0, expect, actual, "%s")

// 测试bool和null
#define TEST_ERROR(error, json)\
    do{\
        lept_value v;\
        v.type = MY_FALSE;\
        EXPECT_EQ_INT(error, lept_parse(&v, json));\
        EXPECT_EQ_INT(MY_NULL, lept_get_type(&v));\
    }while(0)

#define TEST_PARSE_NTF(test_type, flag)\
    do{\
        lept_value v;\
        v.type = test_type;\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, flag));\
        EXPECT_EQ_INT(test_type, lept_get_type(&v));\
    }while(0)

#define TEST_PARSE_INVALID_VALUE(flag)\
    do{\
        v.type = MY_FALSE;\
        EXPECT_EQ_INT(LEPT_PARSE_INVALID_VALUE, lept_parse(&v, flag));\
        EXPECT_EQ_INT(MY_NULL, lept_get_type(&v));\
    }while(0)

// 测试number
#define TEST_NUMBER(expect, json)\
    do{\
        lept_value v;\
        lept_init(&v);\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));\
        EXPECT_EQ_INT(MY_NUMBER, lept_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, lept_get_number(&v));\
        lept_free(&v);\
    }while(0)

// 测试str
#define TEST_STR(expect, json)\
    do{\
        lept_value v;\
        lept_init (&v);\
        EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, json));\
        EXPECT_EQ_INT(MY_STRING, lept_get_type(&v));\
        EXPECT_EQ_STR(expect, lept_get_str(&v), lept_get_str_len(&v));\
        lept_free(&v);\
    }while(0)

#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

static void test_parse_expect_value(){
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(LEPT_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value(){
    lept_value v;
    
    TEST_PARSE_INVALID_VALUE("nul");
    TEST_PARSE_INVALID_VALUE("?");
#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(LEPT_PARSE_INVALID_VALUE, "nan");
#endif
}

static void test_parse_root_not_singular(){
    lept_value v;
    v.type = MY_FALSE;
    EXPECT_EQ_INT(LEPT_PARSE_ROOT_NOT_SINGULAR, lept_parse(&v, "null x"));
    EXPECT_EQ_INT(MY_NULL, lept_get_type(&v));

#if 1
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0123");
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0X0");
    TEST_ERROR(LEPT_PARSE_ROOT_NOT_SINGULAR, "0X123");
#endif
}

static void test_parse_number(){
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000");
    #if 1
    // 这里用不到宏;计算机中科学计算法的表达 -> 其中的 en 相当于十进制中的 10^n
    /* the smallest number > 1 */
    TEST_NUMBER(1.0000000000000002, "1.0000000000000002");
    /* minimum denormal */
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324");
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    /* Max subnormal double */
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    /* Min normal positive double */
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    /* Max double */
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
    #endif
}

static void test_parse_number_too_big(){
#if 1
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(LEPT_PARSE_NUMBER_TOO_BIG, "-1e309");
#endif
}

static void test_parse_str(){
    TEST_STR("", "\"\"");
    TEST_STR("Hello", "\"Hello\"");
#if 1
    TEST_STR("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STR("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
#endif
}

static void test_parse_missing_quotation_mark() {
    TEST_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK, "\"abc");
}

static void test_parse_invalid_string_escape() {
#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
#endif
}

static void test_parse_invalid_string_char() {
// 注意判定反义无效的条件 -> 阅读理解
#if 1
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
#endif
}

static void test_parse_invalid_unicode_hex() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u00/0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
}

static void test_parse_invalid_unicode_surrogate() {
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_arr(){
    size_t i, j;
    lept_value v;
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(MY_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(5, lept_get_array_size(&v));
    EXPECT_EQ_INT(MY_NULL,   lept_get_type(lept_get_array_element(&v, 0)));
    EXPECT_EQ_INT(MY_FALSE,  lept_get_type(lept_get_array_element(&v, 1)));
    EXPECT_EQ_INT(MY_TRUE,   lept_get_type(lept_get_array_element(&v, 2)));
    EXPECT_EQ_INT(MY_NUMBER, lept_get_type(lept_get_array_element(&v, 3)));
    EXPECT_EQ_INT(MY_STRING, lept_get_type(lept_get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(lept_get_array_element(&v, 3)));
    EXPECT_EQ_STR("abc", lept_get_str(lept_get_array_element(&v, 4)), lept_get_str_len(lept_get_array_element(&v, 4)));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(MY_ARRAY, lept_get_type(&v));
    EXPECT_EQ_SIZE_T(4, lept_get_array_size(&v));
    for (i = 0; i < 4; i++) {
        lept_value* a = lept_get_array_element(&v, i);
        EXPECT_EQ_INT(MY_ARRAY, lept_get_type(a));
        EXPECT_EQ_SIZE_T(i, lept_get_array_size(a));
        for (j = 0; j < i; j++) {
            lept_value* e = lept_get_array_element(a, j);
            EXPECT_EQ_INT(MY_NUMBER, lept_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, lept_get_number(e));
        }
    }
    lept_free(&v);

}

static void test_parse_miss_comma_or_square_bracket() {
#if 1
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1}");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1 2");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[[]");
#endif
}

static void test_access_bool(){
    lept_value v;
    lept_init(&v);
    lept_set_bool(&v, false);
    EXPECT_EQ_INT(false, lept_get_bool(&v));
    lept_set_bool(&v, true);
    EXPECT_EQ_INT(true, lept_get_bool(&v));
    lept_free(&v);
}

static void test_access_num(){
    lept_value v;
    lept_init(&v);
    lept_set_number(&v, 123);
    EXPECT_EQ_INT(123, lept_get_number(&v));
    lept_set_number(&v, -456);  // 这里要不要用那种错误的数来test？（可是上面不是test过了吗）
    EXPECT_EQ_DOUBLE(-456, lept_get_number(&v));
    lept_free(&v);
}

static void test_access_string(){
    lept_value v;
    lept_init(&v);
    lept_set_str(&v, "", 0);
    EXPECT_EQ_STR("", lept_get_str(&v), lept_get_str_len(&v));
    lept_set_str(&v, "Hello", 5);
    EXPECT_EQ_STR("Hello", lept_get_str(&v), lept_get_str_len(&v));
    lept_free(&v);
}

static void test_parse_miss_key() {
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{1:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(LEPT_PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon() {
    TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(LEPT_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket() {
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse(){
    TEST_PARSE_NTF(MY_NULL, "null");
    TEST_PARSE_NTF(MY_TRUE, "true");
    TEST_PARSE_NTF(MY_FALSE, "false");
    test_parse_number();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
    test_parse_str();
    test_parse_missing_quotation_mark();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();

    test_access_string();
    test_access_bool();
    test_access_num();
    test_parse_arr();
    test_parse_miss_comma_or_square_bracket();
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();
}

int main(){
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass*100.0 / test_count);
    return main_ret;
}
