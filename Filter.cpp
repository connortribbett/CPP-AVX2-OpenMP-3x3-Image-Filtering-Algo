#include "Filter.h"
#include <iostream>

Filter::Filter(int _dim)
{
  divisor = 1;
  dim = _dim;
  data = new float[dim * dim];
}
void Filter::set(int r, int c, int value)
{
  data[ r * dim + c ] = value;
}

float Filter::getDivisor()
{
  return divisor;
}

void Filter::setDivisor(int value)
{
  divisor = (float)value;
}

int Filter::getSize()
{
  return dim;
}

void Filter::info()
{
  cout << "Filter is.." << endl;
  for (int col = 0; col < dim; col++) {
    for (int row = 0; row < dim; row++) {
      int v = get(row, col);
      cout << v << " ";
    }
    cout << endl;
  }
}
__2in3Filter Filter::getFilter(){
    __2in3Filter filt;
	filt.upper	= _mm256_setr_ps(get(0, 0), get(0, 1), get(0, 2), get(0, 0), get(0, 1), get(0, 2), 0.0, 0.0);
	filt.middle = _mm256_setr_ps(get(1, 0), get(1, 1), get(1, 2), get(1, 0), get(1, 1), get(1, 2), 0.0, 0.0);
	filt.lower	= _mm256_setr_ps(get(2, 0), get(2, 1), get(2, 2), get(2, 0), get(2, 1), get(2, 2), 0.0, 0.0);
    return filt;
}
__2in3Filter128 Filter::getFilter128() {
	__2in3Filter128 filt;
	filt.upper	= _mm_setr_ps(get(0, 0), get(0, 1), get(0, 2), 0.0);
	filt.middle = _mm_setr_ps(get(1, 0), get(1, 1), get(1, 2), 0.0);
	filt.lower	= _mm_setr_ps(get(2, 0), get(2, 1), get(2, 2), 0.0);
	return filt;
}