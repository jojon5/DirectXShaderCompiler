// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify %s

// To test with the classic compiler, run
// %sdxroot%\tools\x86\fxc.exe /T ps_5_1 literals.hlsl

// we use -Wno-unused-value because we generate some no-op expressions to yield errors
// without also putting them in a static assertion

// __decltype is the GCC way of saying 'decltype', but doesn't require C++11
// _Static_assert is the C11 way of saying 'static_assert', but doesn't require C++11
#ifdef VERIFY_FXC
#define _Static_assert(a,b,c) ;
#define VERIFY_TYPES(typ, exp) {typ _tmp_var_ = exp;}
#else
#define VERIFY_TYPES(typ, exp) _Static_assert(std::is_same<typ, __decltype(exp)>::value, #typ " == __decltype(" #exp ") failed")
#endif

#define VERIFY_TYPE_CONVERSION(typ, exp) {typ _tmp_var_ = exp;}

float overload1(float v) { return (float)100; }             /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} fxc-pass {{}} */
int overload1(int v) { return (int)200; }                   /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} fxc-pass {{}} */
uint overload1(uint v) { return (uint)300; }                /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  fxc-pass {{}} */
bool overload1(bool v) { return (bool)400; }                /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}} fxc-pass {{}} */
double overload1(double v) { return (double)500; }          /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}} fxc-pass {{}} */
int64_t overload1(int64_t v) { return (int64_t)600; }                   /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}} fxc-pass {{}} */
uint64_t overload1(uint64_t v) { return (uint64_t)700; }                /* expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}} expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}}  expected-note {{candidate function}} fxc-pass {{}} */

float overload2(float v1, float v2) { return (float)100; }
int overload2(int v1, int v2) { return (int)200; }
uint overload2(uint v1, uint v2) { return (uint)300; }
bool overload2(bool v1, bool v2) { return (bool)400; }
double overload2(double v1, double v2) { return (double)500; }
int64_t overload2(int64_t v1, int64_t v2) { return (int64_t)600; }
uint64_t overload2(uint64_t v1, uint64_t v2) { return (uint64_t)700; }

min16float m16f;
min16float4x4 m16f4x4;

float test() {
  // Ambiguous due to literal int and literal float:
  VERIFY_TYPES(float, overload1(1.5));                      /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int, overload1(20));                         /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int, overload1(-2));                         /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload1(20));                         /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload1(-2));                         /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  
  // Single-digit literals take a special path:
  VERIFY_TYPES(int, overload1(2));                          /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload1(2));                          /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload1(2));                          /* expected-error {{call to 'overload1' is ambiguous}} fxc-error {{X3067: 'overload1': ambiguous function call}} */

  // ambiguous to fxc, since it ignores 'L' on literal int
  VERIFY_TYPES(int, overload1(2l));                         /* fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int, overload1(2L));                         /* fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int, overload1(-2L));                        /* fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload1(2l));                     /* expected-error {{static_assert failed "int64_t == __decltype(overload1(2l)) failed"}}  fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload1(2L));                     /* expected-error {{static_assert failed "int64_t == __decltype(overload1(2L)) failed"}}  fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload1(-2L));                    /* expected-error {{static_assert failed "int64_t == __decltype(overload1(-2L)) failed"}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload1(2l));                    /* expected-error {{static_assert failed "uint64_t == __decltype(overload1(2l)) failed"}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload1(2L));                    /* expected-error {{static_assert failed "uint64_t == __decltype(overload1(2L)) failed"}} fxc-error {{X3067: 'overload1': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload1(-2L));                   /* expected-error {{static_assert failed "uint64_t == __decltype(overload1(-2L)) failed"}} fxc-error {{X3067: 'overload1': ambiguous function call}} */

  
  // Not ambiguous due to literal suffix:
  VERIFY_TYPES(float, overload1(1.5f));
  VERIFY_TYPES(float, overload1(1.5F));
  VERIFY_TYPES(double, overload1(1.5l));
  VERIFY_TYPES(double, overload1(1.5L));
  VERIFY_TYPES(uint, overload1(2u));
  VERIFY_TYPES(uint, overload1(2U));
  VERIFY_TYPES(uint, overload1(2UL));
  VERIFY_TYPES(uint64_t, overload1(2ull));
  VERIFY_TYPES(uint64_t, overload1(2ULL));
  VERIFY_TYPES(int64_t, overload1(2ll));
  VERIFY_TYPES(int64_t, overload1(2LL));
  
  // Not ambiguous due to one literal and one specific:
  VERIFY_TYPES(float, overload2(1.5, 1.5f));
  VERIFY_TYPES(float, overload2(1.5f, 1.5));
  VERIFY_TYPES(double, overload2(1.5, 1.5l));
  VERIFY_TYPES(double, overload2(1.5l, 1.5));
  VERIFY_TYPES(uint, overload2(2, 2u));
  VERIFY_TYPES(uint, overload2(2u, 2));
  VERIFY_TYPES(uint, overload2(2, 2ul));
  VERIFY_TYPES(uint, overload2(2ul, 2));
  
  VERIFY_TYPES(uint64_t, overload2(2, 2ull));
  VERIFY_TYPES(uint64_t, overload2(2ull, 2));
  VERIFY_TYPES(uint64_t, overload2(2, 2ULL));
  VERIFY_TYPES(uint64_t, overload2(2ULL, 2));  
  
  VERIFY_TYPES(int64_t, overload2(2, 2ll));
  VERIFY_TYPES(int64_t, overload2(2ll, 2));
  VERIFY_TYPES(int64_t, overload2(2, 2LL));
  VERIFY_TYPES(int64_t, overload2(2LL, 2));

  // Verify that intrinsics will accept 64-bit overloads properly.
  VERIFY_TYPES(uint64_t, abs(2ULL));
  VERIFY_TYPES(uint, countbits(2ULL));
  VERIFY_TYPES(uint2, countbits(uint64_t2(2ULL, 3ULL)));
  VERIFY_TYPES(uint, firstbithigh(2ULL));
  VERIFY_TYPES(uint2, firstbithigh(uint64_t2(2ULL, 3ULL)));
  VERIFY_TYPES(uint, firstbitlow(2ULL));
  VERIFY_TYPES(uint2, firstbitlow(uint64_t2(2ULL, 3ULL)));
  VERIFY_TYPES(uint, reversebits(2u));
  VERIFY_TYPES(uint64_t, reversebits(2ULL));
  
  // fxc thinks these are ambiguous since it ignores the 'l' suffix:
  VERIFY_TYPES(int, overload2(2, 2l));                      /* fxc-error {{X3067: 'overload2': ambiguous function call}} */
  VERIFY_TYPES(int, overload2(2l, 2));                      /* fxc-error {{X3067: 'overload2': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload2(2, 2l));                  /* expected-error {{static_assert failed "int64_t == __decltype(overload2(2, 2l)) failed"}} fxc-error {{X3067: 'overload2': ambiguous function call}} */
  VERIFY_TYPES(int64_t, overload2(2l, 2));                  /* expected-error {{static_assert failed "int64_t == __decltype(overload2(2l, 2)) failed"}} fxc-error {{X3067: 'overload2': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload2(2, 2l));                 /* expected-error {{static_assert failed "uint64_t == __decltype(overload2(2, 2l)) failed"}} fxc-error {{X3067: 'overload2': ambiguous function call}} */
  VERIFY_TYPES(uint64_t, overload2(2l, 2));                 /* expected-error {{static_assert failed "uint64_t == __decltype(overload2(2l, 2)) failed"}} fxc-error {{X3067: 'overload2': ambiguous function call}} */

  // long long not supported:
  long long l;                                              /* expected-error {{'long' is a reserved keyword in HLSL}} expected-error {{'long' is a reserved keyword in HLSL}} expected-error {{HLSL requires a type specifier for all declarations}} fxc-error {{X3000: unrecognized identifier 'long'}} */
  VERIFY_TYPES(int, 2LL);                                   /* expected-error {{static_assert failed "int == __decltype(2LL) failed"}} fxc-error {{X3000: syntax error: unexpected token 'L'}} */
  VERIFY_TYPES(int, 2ULL);                                  /* expected-error {{static_assert failed "int == __decltype(2ULL) failed"}} fxc-error {{X3000: syntax error: unexpected token 'L'}} */
  VERIFY_TYPES(double, overload1(2ULL * 1.5));              /* expected-error {{static_assert failed "double == __decltype(overload1(2ULL * 1.5)) failed"}} fxc-error {{X3000: syntax error: unexpected token 'L'}} */
  VERIFY_TYPES(double, overload1(2ULL * 1.5L));
  
  
  // ensure operator combinations produce the right literal types and concrete types
  VERIFY_TYPES(float, 1.5 * 2 * 1.5f);
  VERIFY_TYPES(float, 1.5 * 2 * 1.5F);
  VERIFY_TYPES(double, 1.5 * 2 * 1.5l);
  VERIFY_TYPES(double, 1.5 * 2 * 1.5L);
  VERIFY_TYPES(int, 2 * 2L);
  VERIFY_TYPES(uint, 2 * 2U);
  VERIFY_TYPES(uint, 2 * 2UL);
  VERIFY_TYPES(int64_t, 2 * 2LL);
  VERIFY_TYPES(uint64_t, 2 * 2ULL);
  
  // Specific width int forces float to be specific-width
  VERIFY_TYPES(float, 1.5 * 2 * 2L);
  VERIFY_TYPES(float, 1.5 * 2 * 2U);
  VERIFY_TYPES(float, 1.5 * 2 * 2UL);
  VERIFY_TYPES(float, 1.5 * 2 * 2L);
  VERIFY_TYPES(float, m16f * (1.5 * 2 * 2L));

  // Combination of literal float + literal int to literal float, then combined with
  // min-precision float matrix to produce min-precision float matrix - ensures that
  // literal combination results in literal that doesn't override the min-precision type.
  VERIFY_TYPES(min16float4x4, m16f4x4 * (0.5 + 1));

  // Ensure Conversion works.
  VERIFY_TYPE_CONVERSION(min10float, 1.5f);  /* expected-warning {{min10float is promoted to min16float}} */
  VERIFY_TYPE_CONVERSION(min10float, 0.25l); /* expected-warning {{min10float is promoted to min16float}} */

  VERIFY_TYPE_CONVERSION(min16float, 1.5f);
  VERIFY_TYPE_CONVERSION(min16float, 2.5l);

  VERIFY_TYPE_CONVERSION(float, 1.5h);
  VERIFY_TYPE_CONVERSION(float, 1.6l);

  VERIFY_TYPE_CONVERSION(double, 1.65h);
  VERIFY_TYPE_CONVERSION(double, 0.25f);

  VERIFY_TYPE_CONVERSION(int, 1L);
  VERIFY_TYPE_CONVERSION(int, 1U);
  VERIFY_TYPE_CONVERSION(int, 1UL);
  VERIFY_TYPE_CONVERSION(int, 1LL);

  VERIFY_TYPE_CONVERSION(uint, 1L);
  VERIFY_TYPE_CONVERSION(uint, 1UL);
  VERIFY_TYPE_CONVERSION(uint, 1LL);

  VERIFY_TYPE_CONVERSION(min12int, 1L);  /* expected-warning {{min12int is promoted to min16int}} */
  VERIFY_TYPE_CONVERSION(min12int, 1UL); /* expected-warning {{min12int is promoted to min16int}} */
  VERIFY_TYPE_CONVERSION(min12int, 1LL); /* expected-warning {{min12int is promoted to min16int}} */

  VERIFY_TYPE_CONVERSION(min16int, 1L);
  VERIFY_TYPE_CONVERSION(min16int, 1UL);
  VERIFY_TYPE_CONVERSION(min16int, 1LL);

  return 0.0f;
}