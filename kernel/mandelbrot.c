#include "defs.h"


// 定义复数结构体
typedef struct {
    float real;
    float imag;
} Complex;

// 复数加法
Complex add(Complex a, Complex b) {
    return (Complex){a.real + b.real, a.imag + b.imag};
}

// 复数乘法
Complex multiply(Complex a, Complex b) {
    return (Complex){
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real
    };
}

float Q_rsqrt(float number) {
    long i = 0;
    float x2=number * 0.5F, y=number;
    const float threehalfs = 1.5F;
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
    y  = * ( float * ) &i;
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
              // y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}

// 获取复数的模长（绝对值）
float modulus(Complex c) {
    return 1/Q_rsqrt(c.real * c.real + c.imag * c.imag);
}

#define MAX_ITER 100 // 最大迭代次数
#define ESCAPE_RADIUS 2.0 // 如果模长超过此值，则认为已经发散

void mandelbrot(bgra_t *im, int width, int height) {
    float x_min = -2.0, x_max = 1.0;
    float y_min = -1.5, y_max = 1.5;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 将像素坐标转换为复数平面坐标
            float real = x_min + (x / (float)(width - 1)) * (x_max - x_min);
            float imag = y_min + (y / (float)(height - 1)) * (y_max - y_min);
            Complex c = {real, imag};
            Complex z = {0.0, 0.0};

            int iter;
            for (iter = 0; iter < MAX_ITER; ++iter) {
                if (modulus(z) > ESCAPE_RADIUS)
                    break;
                z = add(multiply(z, z), c);
            }

            // 根据迭代次数确定颜色
            unsigned char r, g, b, a;
            if (iter == MAX_ITER) {
                // 属于 Mandelbrot 集合，设置为黑色
                r = g = b = 0;
                a = 255;
            } else {
                // 不属于 Mandelbrot 集合，根据迭代次数上色
                int hue = (float)iter / MAX_ITER * 360.0;
                // 使用简单的HSV到RGB转换，这里省略了复杂的转换函数
                // 可以根据需要添加更详细的色彩映射逻辑
                r = (unsigned char)(hue % 128 * 2);
                g = (unsigned char)((hue / 128) % 2 * 255);
                b = (unsigned char)((hue / 256) % 2 * 255);
                a = 255;
            }

            // 设置像素颜色
            im[y * width + x] = (bgra_t){b, g, r, a};
        }
    }
}
