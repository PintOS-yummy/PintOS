// #ifndef THREADS_FIXED_POINT_H
// #define THREADS_FIXED_POINT_H // 어떤 의미?

#include <debug.h>
#ifdef VM
#include "vm/vm.h"
#endif

static int LOAD_AVG; // load_avg 전역변수 선언 // thread_create에서 초기화 함
static int READY_THREADS; // READY_THREADS 전역변수 선언 

#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/* 고정 소수점 변환 및 계산을 위한 메크로
	n : integer, 
	x,y : fixed point numbers

  고정소수점 수의 기본적인 연산 방법:

1. 합과 차: `x + y`, `x - y`
2. 정수와의 연산: `x + n * f`, `x - n * f`
3. 곱셈과 나눗셈: `x * n`, `x / n` */
#define FC 16384  // 2^14, 고정소수점에서의 1
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

#define convert_n_to_fp(n) ((n) * (FC)) // 정수 n을 고정소수점으로 변환
#define convert_x_to_int_round_to_zero(x) ((x) / (FC)) // 고정소수점 수 x를 정수로 변환 (내림)
#define convert_x_to_int_round_to_nearest(x) ((x) >= 0 ? (((x) + ((FC) / 2)) / FC) : ((((x) - ((FC) / 2)) / FC))) // 고정소수점 수 x를 정수로 변환 (반올림)
#define add_x_and_y(x,y) ((x) + (y)) // 두 고정소수점 수 x와 y의 합
#define sub_y_from_x(x,y) ((x) - (y)) // 두 고정소수점 수 x와 y의 차 (x - y)
#define add_x_and_n(x,n) ((x)+((n) * (FC))) // 고정소수점 수 x와 정수 n의 합
#define sub_n_from_x(x,n) ((x) - ((n) * (FC))) // 고정소수점 수 x에서 정수 n을 뺀 값 (x - n)
#define mul_x_by_y(x,y) (((int64_t) (x)) * (y) / (FC)) // 두 고정소수점 수 x와 y의 곱
#define mul_x_by_n(x,n) ((x) * (n)) // 고정소수점 수 x와 정수 n의 곱
#define div_x_by_y(x,y) (((int64_t) (x)) * (FC) / (y)) // 고정소수점 수 x를 y로 나눈 값
#define div_x_by_n(x,n) ((x) / (n)) // 고정소수점 수 x를 정수 n으로 나눈 값