#include "platform.h"
#include <stddef.h>
#include <stdarg.h>


static void putstr(const char *s)
{
  while (*s)
  {
    uart_putc(*s);
    s++;
  }
}

void panic(const char *s)
{
    printf("panic: %s\n",s);
    while(1);
}

void panic_on(int cond, const char *s)
{
  if (cond)
  {
    panic(s);
  }
}

int printf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[1024];
  vsprintf(buf, fmt, ap);
  va_end(ap);
  putstr(buf);
  return 0;
}

int vsprintf(char *out, const char *fmt, va_list ap)
{
  
  // char buf[1024];
  char *pout = out;
  for (const char *p = fmt; *p;)
  {
    char c = *p++;
    switch (c)
    {
    case '%':
      /* code */
      switch (*p++)
      {
      case 'd':
      {
        itoa(pout, va_arg(ap, int));
        for (; (*pout) != '\0'; (pout++));
      
        break;
      }

        
      case 's':
      {
        strcpy(pout, va_arg(ap, char *));
        for (; (*pout) != '\0'; (pout++));
        break;
      }
        
      case 'p':
      {
        *pout++ = '0';
        *pout++ = 'x';
        unsigned long i = va_arg(ap, unsigned long);
        char buf1[64];
        char *p = buf1;
        int n = 0;
        while (i > 0)
        {
          n = i % 16;
          if (n < 10)
          {
            *p++ = n + '0';
          }
          else
          {
            *p++ = n + 'a' - 10;
          }

          i /= 16;
        }
        for (; p-- > buf1;)
        {
          *pout++ = *p;
        }
      }
      break;
      default:
        break;
      }
      break;

    default:
      *pout++ = c;
      break;
    }
    
  }
  *pout = '\0';
  return 0;
}

int sprintf(char *out, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsprintf(out, fmt, ap);
  va_end(ap);
  return 0;
}

int snprintf(char *out, size_t n, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(out,n,  fmt, ap);
  va_end(ap);
  return 0;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap)
{
  char buf[1024];
  char *pout = buf;

  for (const char *p = fmt; *p &&  pout-buf<n;)
  {
    char c = *p++;
    switch (c)
    {
    case '%':
      /* code */
      switch (*p++)
      {
      case 'd':{
        itoa(pout, va_arg(ap, int));
        for (; (*pout) != '\0'; (pout++));
        break;}
        
      case 's':{
        strcpy(pout, va_arg(ap, char *));
        for (; (*pout) != '\0'; (pout++));
        break;
      }
      case 'p':
      {
        *pout++ = '0';
        *pout++ = 'x';
        unsigned long i = va_arg(ap, unsigned long);
        char buf1[64];
        char *p = buf1;
        int n = 0;
        while (i > 0)
        {
          n = i % 16;
          if (n < 10)
          {
            *p++ = n + '0';
          }
          else
          {
            *p++ = n + 'a' - 10;
          }

          i /= 16;
        }
        for (; p-- > buf1;)
        {
          *pout++ = *p;
        }
      }

      break;
      default:
        break;
      }
      break;

    default:
      *pout++ = c;
      break;
    }
    
  }
  
  
  strncpy(out,buf, n);
  return 0;
}

static unsigned long int next = 1;

int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed) {
  next = seed;
}

int abs(int x) {
  return (x < 0 ? -x : x);
}


char* itoa(char *a, int i){// return strlen
  char *ret= a;
  char buf[1024];
  char *p = buf;
  int count = 0;
  int n=0;
  if(i==0){
    *a++='0';
  }else if(i>0){
    while(i>0){
    n=i%10;
    *p++ = n+'0';
    i/=10;
    count++;
    }
    for(;p-->buf;){
      *a++ = *p;
    }
  }else{
    i=-i;
    if(i<0){ // INT_MIN
      for(p="-214748364";*p;){
        *a++ = *p++;
      }
    }else{
      while(i>0){
        n=i%10;
        *p++ = n+'0';
        i/=10;
        count++;
      }
      *p++ = '-';
      for(;p-->buf;){
        *a++ = *p;
      }
    }
  }
  
  *a = '\0';
  return ret;
}


int atoi(const char* nptr) {
  int x = 0;
  while (*nptr == ' ') { nptr ++; }
  while (*nptr >= '0' && *nptr <= '9') {
    x = x * 10 + *nptr - '0';
    nptr ++;
  }
  return x;
}


size_t strlen(const char *s) {
  size_t len=0;
  const char *p = s;
  for(;*p++;){len++;}
  return len;
}

char *strcpy(char *dst, const char *src) {
  char *p=dst;
  for(;*src;src++){
    *dst++ = *src;
  }
  *dst = '\0';
  return p;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *p=dst;
  int i=0;
  for(;*src && i<n;){
    *p++ = *src++;
    i++;
  }
  if(i==n) dst[n-1] = '\0';
  else *p = '\0';
  return dst;
}

char *strcat(char *dst, const char *src) {
  char *old = dst;
  for(;*dst;dst++);
  for(;*src;){
    *dst++=*src++;
  }
  *dst = '\0';
  return old;
}

int strcmp(const char *s1, const char *s2) {
  
  for(;*s2 && *s1 && *s1==*s2;s1++,s2++);
  if(!*s1 && !*s2){
    return 0;
  }else{
    return *s1-*s2;
  }
}

int strncmp(const char *s1, const char *s2, size_t n) {
  
  for(;*s2 && *s1 && *s1++==*s2++ && n-->0;);
  if(!*s1 && !*s2){
    return 0;
  }else{
    return *s1-*s2;
  }
}

void *memset(void *s, int c, size_t n) {
  char *p = s;
  char cc = (char)(c%256);
  for(size_t i=0;i<n;i++){
    *p++ = cc;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  panic_on(n>1024, "太大了");
  
  uint8_t buf[n];
  memcpy(buf, src, n);
  memcpy(dst, buf, n);
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  uint8_t *pout=out;
  const uint8_t* pin = in;
  for(size_t i=0;i<n;i++){
    *pout++ = *pin++;
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
  return -1;
}