#include "leptjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <math.h>

// ifndef让使用者可以自定义初始栈大小
#ifndef LEPT_PARSE_STACK_INIT_SIZE 
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch) do{ assert(*c->json == (ch)); c->json++; }while(0)
// 这里的赋值是为了让放入栈中的数据达到内存连续; 或者说把分配给栈的空间拿来放置c
#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

typedef struct{
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

// 压栈 : 其实就是申请了一块地方给PUTC、然后在PUTC中让申请的空间被赋值了ch
static void* lept_context_push(lept_context* c, size_t size){
    void* ret;  // 它是指针
    assert(size > 0);
    if (c->top + size >= c->size){
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;
        c->stack = (char*)realloc(c->stack, c->size);   // 每一次realloc都会更换栈的地址
    }
    ret = c->stack + c->top;    // top的位置（相当于栈的地址+偏移量）、链表栈的意思
    c->top += size;             // 这个top是顺序栈的意思
    return ret;
}

// 弹栈
static void* lept_context_pop(lept_context* c, size_t size){
    // assert(c->top >= size && c->size >= 0);
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

// ws = *(%x20 / %x09 / %x0A / %x0D )
static void lept_parse_whitespace(lept_context* c){
    const char* p = c->json;
    while( *p == ' ' || *p == '\t'|| *p == '\n'|| *p == '\r')
        p++;
    c->json = p;
}

// JSON-ntf = null, true, false
static int lept_parse_ntf(lept_context* c, lept_value* v, const char* flag, lept_type flag_type){
    int32_t i = 0;
    EXPECT(c, flag[0]);
    for(; flag[i+1]; ++i){
    // 不能用i-1来迭代-->这里如果直接从1开始，万一被恶意传进来了只有一个字符的、那程序就崩了
        if (c->json[i] != flag[i+1])
            return LEPT_PARSE_INVALID_VALUE;
    }
    c->json += i;
    v->type = flag_type;
    return LEPT_PARSE_OK;
}

// JSON-number = number, int, frac, exp
static int lept_parse_number(lept_context* c, lept_value* v){
    // 这里只是起检查作用->strtod可以应付转换
    auto check_fir_num = [](const char* one){ return *one>='1' && *one<='9' ? 0 : 1; };
    auto check_mid_num = [](const char* one){ return *one>='0' && *one<='9' ? 0 : 1; };
    const char* p = c->json;
    // 不需要每个return前面都要有type和json的设置->当返回invalid_value时就相当于报错
    if (*p == '-') p++;
    // TODO 这里的条件其实是：单个0(只有一个数字且为0)或开头为1-9的数字！
    if (*p == '0') p++;
    else { 
        if(check_fir_num(p)) return LEPT_PARSE_INVALID_VALUE;   // 排除0123这种格式
        for(p++; check_mid_num(p) != 1; p++);   // 把小数点前的数都略过
    }
    if (*p == '.') {
        p++;
        if (check_mid_num(p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; check_mid_num(p) != 1; p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (check_mid_num(p)) return LEPT_PARSE_INVALID_VALUE;  // 如果后面e后面没有幂则报错无效
        for (p++; check_mid_num(p) != 1; p++);
    }   
    v->n = strtod(c->json, NULL);   // 这里借用标准库中的函数，使十进制转二进制
    if (v->n == HUGE_VAL || v->n == -HUGE_VAL) return LEPT_PARSE_NUMBER_TOO_BIG;
    // 要清空c的json: 不然要么出现野指针、要么不满足v!=NULL && v->type == MY_NUMBER 
    c->json = p;
    v->type = MY_NUMBER;
    return LEPT_PARSE_OK;
}

// JSON-string = 常规字符+反义
void lept_free(lept_value* v){
    assert(v != NULL);
    size_t i;
    switch (v->type)
    {
    case MY_STRING:
        free(v->s);
        break;
    case MY_NUMBER:
        v->n = 0;
        break;
    case MY_ARRAY:
        for (i=0; i<v->arrSize; ++i){
            lept_free(&(v->e[i]));
        }
        // if (v->arrSize != 0) v->arrSize--;
        free(v->e);
        break;
    case MY_OBJECT:
        for (i=0; i<v->objSize; ++i){
            lept_free(&v->m[i].v);
            free(v->m->k);
            v->m->klen = 0;
            // lept_free(&(v->m[i]));
        }
        free(v->m);
    default: break;
    }
    // v->type = MY_NULL;
}

// 读取16进制的四位；return null来说明格式（范围->不能有G、字符长度）不合法
static const char* lept_parse_hex4(const char* p, unsigned* u) {
    if (!((*p>='0' && *p<='9') || (*p>='A' && *p<='F') || (*p>='a' && *p<='f'))) return NULL;
    char* end;
    *u = (unsigned)strtol(p, &end, 16);
    return end == p + 4 ? end : NULL;
}

// 检查编码是否正确：计算后查看范围
static const void lept_encode_utf8(lept_context* c, unsigned u){
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

//             // if (ch != 'b' || ch != 'f' || ch != 'n' || ch != 'r' || ch != 't' || ch != '\\' || ch != '/'){
//             //     c->top = head;
//             //     return LEPT_PARSE_INVALID_STRING_ESCAPE;
//             // }
//             // else {
//             //     if ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x10FFFF){
//             //         c->top = head;
//             //         return LEPT_PARSE_INVALID_STRING_CHAR;
//             //     }
//             // }

// 负责解析字符、更改str和len
static int lept_parse_str_raw(lept_context* c, char** str, size_t* len){
    // 这里的len计数时不包括双引号、遇到就直接跳过
    size_t head = c->top;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;   // 这里不要传c—>json的地址，；因为后面迭代的是p++，最好还是操作p
    unsigned u, u2;
    for(;;){
        char ch = *p++;
        switch (ch)
        {
        case '\\': 
        // TODO 优化性能
            switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        // 先检查后压栈
                        if (!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default: 
                        c->top = head;
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                } break;
        case '\"':
            // c->stack = c->top-c->size;
            *len = c->top - head;   // 每一次PUTC都会c->top++
            c->json = p;
            *str = (char*)lept_context_pop(c, *len);
            return LEPT_PARSE_OK;
        case '\0':
            c->top = head;
            return LEPT_PARSE_MISS_QUOTATION_MARK;
        default:
            if ((unsigned char)ch < 0x20){
                return LEPT_PARSE_INVALID_STRING_CHAR;
            }
            PUTC(c, ch);
        }
    }
}

static int lept_parse_str(lept_context* c, lept_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = lept_parse_str_raw(c, &s, &len)) == LEPT_PARSE_OK)
        lept_set_str(v, s, len);
    return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v);
static int lept_parse_array(lept_context *c, lept_value *v){
    size_t size = 0;
    int ret;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        v->type = MY_ARRAY;
        v->arrSize = 0;
        v->e = NULL;
        return LEPT_PARSE_OK;
    }
    for (;;){
        lept_value e;
        lept_init(&e);
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK) {
            break; // 这里是break，跳出循环后统一弹栈并返回错误码
        }
        // TODO 这里memcpy不能省略：
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
        size++;
        lept_parse_whitespace(c);   // 每个元素后且'，'前可以有空格
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == ']'){
            c->json++;
            v->type = MY_ARRAY;
            v->arrSize = size;
            size *= sizeof(lept_value);
            memcpy(v->e = (lept_value*)malloc(size), lept_context_pop(c, size), size);  // 把栈回复到解析当前元素之前，同时给v->e赋值
            return LEPT_PARSE_OK;
        }else {
            // 要是在这里弹栈会漏掉成功解析的数据
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    // 这里弹的是单个元素，因为在parse_array和parse_value相互调用过程中，每一组parse_value+parse_array都对应一个数组元素
    for(size_t i=0; i<size; ++i){
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
    }
    return ret;
}

static int lept_parse_object(lept_context* c, lept_value* v){
    size_t size;
    lept_member m;
    int ret;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = MY_OBJECT;
        v->m = 0;
        v->objSize = 0;
        return LEPT_PARSE_OK;
    }
    m.k = NULL;
    size = 0;
    for(;;){
        char* str;
        lept_init(&m.v);
        // TODO 所以这里不应该是先把第一个引号跳过去然后再处理右边的冒号吗？
        if(*c->json != '\"'){
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        // 这里只借用了解析str的一部分, 解析键
        if (ret = lept_parse_str_raw(c, &str, &m.klen) != LEPT_PARSE_OK){
            break;
        }
        // 解析下冒号
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        else *c->json++;
        lept_parse_whitespace(c);
        // 解析值
        if((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK){
            break;
        }
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
        size++;
        m.k = NULL; // 已经把m压入栈了，m要赋值下一个元素的键了

        // 解析 ws [comma | right-curly-brace] ws
        lept_parse_whitespace(c);
        if(*c->json == ','){
            c->json++;
            lept_parse_whitespace(c);
        } else if (*c->json == '}'){
            size_t s = sizeof(lept_member) * size;
            c->json++;
            v->arrSize += size;
            v->type = MY_OBJECT;
            size *= sizeof(lept_member);
            memcpy(v->m = (lept_member *)malloc(s), lept_context_pop(c, s), s);
            return LEPT_PARSE_OK;
        } else{
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
        lept_parse_whitespace(c); 
    }
    free(m.k);
    for(size_t i=0; i<size; ++i){
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_member))); // are U sure?
    }
    return ret;
}

// value = null / false / true / number ：json数据解析
static int lept_parse_value(lept_context* c, lept_value* v){
    switch (*c->json){
        case '{': return lept_parse_object(c, v);
        case '[': return lept_parse_array(c, v);
        case 'n': return lept_parse_ntf(c, v, "null", MY_NULL);
        case 't': return lept_parse_ntf(c, v, "true", MY_TRUE);
        case 'f': return lept_parse_ntf(c, v, "false", MY_FALSE);
        case '"': return lept_parse_str(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default: return lept_parse_number(c, v);
    }
}

/* 封装可以类比接口、放到手机充电器上就是手机要有个插口、充电器也要有个type-C插头（封装会有两部分，一个是对内、一个对外）*/
// 对外的接口：解析器！
int lept_parse(lept_value* v, const char* json){
    lept_context c;
    int ret = 0;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    v->type = MY_NULL;
    lept_parse_whitespace(&c);
    // 终止是用'\0'来判断的，也就是解释器从前往后读到换行
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK){
        lept_parse_whitespace(&c);
        // 解析完的反馈是ret，如果c此时未读完，就说明用户传了多个值
        if (*c.json != '\0'){
            v->type = MY_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

// 对外的接口:先得到类型
lept_type lept_get_type(const lept_value* v){
    assert(v != NULL);
    return v->type;
}

// 先按照b=1时v为true、b=0时v为false算
void lept_set_bool(lept_value* v, int b){
    lept_free(v);
    v->type = b==1 ? MY_TRUE : MY_FALSE;
}

int lept_get_bool(const lept_value* v){
    assert(v->type == MY_TRUE || v->type == MY_FALSE);
    assert(v != NULL);
    return v->type == MY_TRUE ? 1 : 0;
}

// 写入数字
void lept_set_number(lept_value* v, double n){
    lept_free(v);
    // 这里要不要加一个lept_parse_num？不然n不是正常要传进来的东西呢？-< 不需要，因为是在内部的解析器里调用这几个接口的
    v->n = n;
    v->type = MY_NUMBER;
}

// 得到数字
double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == MY_NUMBER);
    return v->n;
}

// TODO 这里要先cp一份str，既然是动态分配，cpp可以智能指针什么的把？
void lept_set_str(lept_value* v, const char* s, size_t len){
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->s = (char*)malloc(len + 1);
    memcpy(v->s, s, len);
    v->s[len] = '\0';
    v->len = len;
    v->type = MY_STRING;
}

const char* lept_get_str(const lept_value* v) {
    assert(v != NULL && v->type == MY_STRING);
    return v->s;
}

size_t lept_get_str_len(const lept_value* v){
    assert(v != NULL && v->type == MY_STRING);
    return v->len;
}

size_t lept_get_array_size(const lept_value* v) {
    assert(v != NULL && v->type == MY_ARRAY);
    return v->arrSize;
}

lept_value* lept_get_array_element(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == MY_ARRAY);
    assert(index < v->arrSize);
    return &v->e[index];
}

size_t lept_get_object_size(const lept_value* v){
    assert(v != NULL && v->type == MY_OBJECT);
    return v->objSize;
}

const char* lept_get_object_key(const lept_value* v, size_t index){
    assert(v != NULL && index>=0 && v->type == MY_OBJECT);
    return v->m[index].k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index){
    assert(v != NULL && v->type == MY_OBJECT && index>=0);
    return v->m[index].klen;
}
lept_value* lept_get_object_value(const lept_value* v, size_t index){
    assert(v != NULL && index>=0 && v->type == MY_OBJECT);
    return &v->m->v;
}