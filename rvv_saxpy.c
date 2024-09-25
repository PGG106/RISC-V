#include <riscv_vector.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>

#define N 31

float input[N] = {-0.4325648115282207, -1.6655843782380970, 0.1253323064748307,
                  0.2876764203585489, -1.1464713506814637, 1.1909154656429988,
                  1.1891642016521031, -0.0376332765933176, 0.3272923614086541,
                  0.1746391428209245, -0.1867085776814394, 0.7257905482933027,
                  -0.5883165430141887, 2.1831858181971011, -0.1363958830865957,
                  0.1139313135208096, 1.0667682113591888, 0.0592814605236053,
                  -0.0956484054836690, -0.8323494636500225, 0.2944108163926404,
                  -1.3361818579378040, 0.7143245518189522, 1.6235620644462707,
                  -0.6917757017022868, 0.8579966728282626, 1.2540014216025324,
                  -1.5937295764474768, -1.4409644319010200, 0.5711476236581780,
                  -0.3998855777153632};

float output_golden[N] = {
    1.7491401329284098, 0.1325982188803279, 0.3252281811989881,
    -0.7938091410349637, 0.3149236145048914, -0.5272704888029532,
    0.9322666565031119, 1.1646643544607362, -2.0456694357357357,
    -0.6443728590041911, 1.7410657940825480, 0.4867684246821860,
    1.0488288293660140, 1.4885752747099299, 1.2705014969484090,
    -1.8561241921210170, 2.1343209047321410, 1.4358467535865909,
    -0.9173023332875400, -1.1060770780029008, 0.8105708062681296,
    0.6985430696369063, -0.4015827425012831, 1.2687512030669628,
    -0.7836083053674872, 0.2132664971465569, 0.7878984786088954,
    0.8966819356782295, -0.1869172943544062, 1.0131816724341454,
    0.2484350696132857};

float output[N] = {
    1.7491401329284098, 0.1325982188803279, 0.3252281811989881,
    -0.7938091410349637, 0.3149236145048914, -0.5272704888029532,
    0.9322666565031119, 1.1646643544607362, -2.0456694357357357,
    -0.6443728590041911, 1.7410657940825480, 0.4867684246821860,
    1.0488288293660140, 1.4885752747099299, 1.2705014969484090,
    -1.8561241921210170, 2.1343209047321410, 1.4358467535865909,
    -0.9173023332875400, -1.1060770780029008, 0.8105708062681296,
    0.6985430696369063, -0.4015827425012831, 1.2687512030669628,
    -0.7836083053674872, 0.2132664971465569, 0.7878984786088954,
    0.8966819356782295, -0.1869172943544062, 1.0131816724341454,
    0.2484350696132857};

void saxpy_golden(size_t n, const float a, const float *x, float *y)
{
  for (size_t i = 0; i < n; ++i)
  {
    y[i] = a * x[i] + y[i];
  }
}

// reference https://github.com/riscv/riscv-v-spec/blob/master/example/saxpy.s
void saxpy_vec(size_t n, const float a, const float *x, float *y)
{
  for (size_t vl; n > 0; n -= vl, x += vl, y += vl)
  {
    // vsetvli allows for stripmining, The application specifies the total number of elements to be processed (the application vector length or AVL) as a candidate value for vl,
    // and the hardware responds via a general-purpose register with the (frequently smaller) number of elements that the hardware will handle per iteration

    // e32 -> element width 32 (float)
    // m1 -> LMUL size 8
    // expected VL -> VLEN/32-> 4 locally, 8 on bananapi
    vl = __riscv_vsetvl_e32m1(n);
    printf("%ld\n", vl);
    // e32 -> element width 32 (float)
    // m8 -> LMUL size 8
    // expected VL -> VLEN/32 * 8, 32 (31) locally, still 31 on bananapi because we are capped at n
    vl = __riscv_vsetvl_e32m8(n);
    printf("%ld\n", vl);
    // vfloat32m8_t is the datatype for a vector of float32 with EMUL = 8
    // __riscv_vle32_v_f32m8 -> __riscv is the prefix that is always present, vle -> vector strided load of SEW 32 LMUL 8, base addr + number of elements to read
    vfloat32m8_t vx = __riscv_vle32_v_f32m8(x, vl);
    vfloat32m8_t vy = __riscv_vle32_v_f32m8(y, vl);
    // print the vector content
    for (size_t i = 0; i < vl; ++i)
      printf("%f ", __riscv_vfmv_f_s_f32m8_f32(__riscv_vslidedown_vx_f32m8(vx, i, vl)));
    printf("%f ", __riscv_vfmv_f_s_f32m8_f32(vx));
    // __riscv_vfmacc_vf_f32m8 -> multiply + accumulate aka fmadd, as always e32 for floats32 and m8 aka lmul 8
    // https://dzaima.github.io/intrinsics-viewer/#0q1YqVbJSKsosTtYtU9JRSoVzFMvSchOTk@PL0uLTjI1yLYCSiUpW0UplSrE6SskgFohRDFQfHw/SURaPqSETKGsIAkq1AA

    // store into y the result of a fmadd with a * vx + vy
    __riscv_vse32_v_f32m8(y, __riscv_vfmacc_vf_f32m8(vy, a, vx, vl), vl);
  }
}

int fp_eq(float reference, float actual, float relErr)
{
  // if near zero, do absolute error instead.
  float absErr = relErr * ((fabsf(reference) > relErr) ? fabsf(reference) : relErr);
  return fabsf(actual - reference) < absErr;
}

int main()
{
  saxpy_golden(N, 55.66, input, output_golden);
  saxpy_vec(N, 55.66, input, output);
  int pass = 1;
  for (int i = 0; i < N; i++)
  {
    if (!fp_eq(output_golden[i], output[i], 1e-6))
    {
      printf("fail, %f=!%f\n", output_golden[i], output[i]);
      pass = 0;
    }
  }
  if (pass)
    printf("pass\n");
  return (pass == 0);
}
