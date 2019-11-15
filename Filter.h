//-*-c++-*-
#ifndef _Filter_h_
#define _Filter_h_

#include <immintrin.h>

using namespace std;

struct __2in3Filter{
  __m256 upper;
  __m256 middle;
  __m256 lower;
};
struct __2in3Filter128{
  __m128 upper;
  __m128 middle;
  __m128 lower;
};

class Filter {
  float divisor;
  int dim;
  float *data;

public:
  Filter(int _dim);
inline float get(int r, int c)
{
  return data[ r * dim + c ];
};
  void set(int r, int c, int value);

  float getDivisor();
  void setDivisor(int value);

  int getSize();
  void info();
    
__2in3Filter getFilter();
__2in3Filter128 getFilter128();
};

#endif
