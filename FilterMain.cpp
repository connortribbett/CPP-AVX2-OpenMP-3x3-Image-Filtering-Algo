#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include "Filter.h"
#include <omp.h>

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

void printVec(__m256 in) {
	float *ptr = (float*)&in;
	for (int i = 0; i < 8; i++) {
		cout << ptr[i] << " | ";
	}
	cout << endl;
}
inline float fClamp255(float x){
    if(x < 0){
        x = 0;
    }
    if(x > 255){
        x = 255;
    }
    return x;
}

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

struct Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(struct Filter *filter, cs1300bmp *input, cs1300bmp *output)
{
    long long cycStart, cycStop;
    cycStart = rdtscll();

    float divisor = filter->getDivisor();
    int w = input->width - 1;
    int h = input->height - 1;
    output->width = w + 1;
    output->height = h + 1;
    
    __2in3Filter filt = filter->getFilter();
    /*
    Get two copies of the filter in 3 256bit vectrs(upper, middle, lower) in the format below
    0 1 2 0 1 2 0 0
    3 4 5 3 4 5 0 0
    6 7 8 6 7 8 0 0
    */
    __2in3Filter128 filt128;
    filt128.upper = _mm256_extractf128_ps(filt.upper, 0);
    filt128.middle = _mm256_extractf128_ps(filt.middle, 0);
    filt128.lower = _mm256_extractf128_ps(filt.lower, 0);
    /*
    Get one copy of the filter in 3 128bit vectors by extracting the lower half of the 256bit filter vector
    0 1 2 0
    3 4 5 3
    6 7 8 6
    Last value is always multiplied to 0 in 128bit calulations
    */
    
    __m256i mask = _mm256_setr_epi32(-1, -1, -1, -1, -1, -1, 0, 0);
    __m128i mask128 = _mm_setr_epi32(-1, -1, -1, 0);
    /*
    Create a 256bit and 128bit mask, taking the data of 2 and 1 pixels respectively that are 3 pixels apart
    Need 3 values per pixel, so the last 2 and 1 values are masked out respectively to not affect the calculation
    */
    
    /*
    OpenMP runs each plane loop iteration in parallel as they are dependent
    SIMD allows for further optimization due to the use of avx2 in the loop
    */
    #pragma omp parallel for simd
    for(int plane = 0; plane < 3; plane++){
        for(int row = 1; row < h; row++) {
            int col = 1;
            for(; col < w - 5; col = col + 6) { //Calculate 6 pixels at once for 1 plane
                __m256 upper1 = _mm256_maskload_ps(&input->color[plane][row-1][col - 1], mask);
                __m256 upper2 = _mm256_maskload_ps(&input->color[plane][row-1][col], mask);
                __m256 upper3 = _mm256_maskload_ps(&input->color[plane][row-1][col + 1], mask);
                
                __m256 middle1 = _mm256_maskload_ps(&input->color[plane][row][col - 1], mask);
                __m256 middle2 = _mm256_maskload_ps(&input->color[plane][row][col], mask);
                __m256 middle3 = _mm256_maskload_ps(&input->color[plane][row][col + 1], mask);
                
                __m256 lower1 = _mm256_maskload_ps(&input->color[plane][row+1][col - 1], mask);
                __m256 lower2 = _mm256_maskload_ps(&input->color[plane][row+1][col], mask);
                __m256 lower3 = _mm256_maskload_ps(&input->color[plane][row+1][col + 1], mask);
                /*
                Load 6 pixels of information into 9 vectors
		ex of data
		p = pixel e = external info for calculation
		0  1  2  3  4  5  6  7
		1
		e1 e1 e1 e2 e2 e2 0 0
		e1 p1 e1 e2 p2 e2 0 0
		e1 e1 e1 e2 e2 e2 0 0
                2
		   e1 e1 e1 e2 e2 e2 0 0
		   e1 p1 e1 e2 p2 e2 0 0
		   e1 e1 e1 e2 e2 e2 0 0
		3
		      e1 e1 e1 e2 e2 e2 0 0
		      e1 p1 e1 e2 p2 e2 0 0
		      e1 e1 e1 e2 e2 e2 0 0
		*/
                
                
                middle1 = _mm256_mul_ps(middle1, filt.middle);
                middle2 = _mm256_mul_ps(middle2, filt.middle);
                middle3 = _mm256_mul_ps(middle3, filt.middle);
                
                lower1 = _mm256_fmadd_ps(lower1, filt.lower, middle1);
                lower2 = _mm256_fmadd_ps(lower2, filt.lower, middle2);
                lower3 = _mm256_fmadd_ps(lower3, filt.lower, middle3);
                
                upper1 = _mm256_fmadd_ps(upper1, filt.upper, lower1);
                upper2 = _mm256_fmadd_ps(upper2, filt.upper, lower2);
                upper3 = _mm256_fmadd_ps(upper3, filt.upper, lower3);
                /*
                Multiplication and fused multiply + add are used to multiply each pixel by its respective
                filter value and then added into upper
                The sum of each pixel are now in the first 3 and next 3 values respectively
                */
                
               
                /*
                Below the values of each pixel is added from their respective 3 vector elements and divided
                The output pixel is then set after clamping the value to 255 with an inline function
                */
                float counter1 = (upper1[0] + upper1[1] + upper1[2])/divisor;
                float counter2 = (upper2[0] + upper2[1] + upper2[2])/divisor;
                float counter3 = (upper3[0] + upper3[1] + upper3[2])/divisor;
            
                output->color[plane][row][col] = fClamp255(counter1);
                output->color[plane][row][col + 1] = fClamp255(counter2);
                output->color[plane][row][col + 2] = fClamp255(counter3);
                
                float counter12 = (upper1[3] + upper1[4] + upper1[5])/divisor;
                float counter22 = (upper2[3] + upper2[4] + upper2[5])/divisor;
                float counter32 = (upper3[3] + upper3[4] + upper3[5])/divisor;
                
                output->color[plane][row][col + 3] = fClamp255(counter12);
                output->color[plane][row][col + 4] = fClamp255(counter22);
                output->color[plane][row][col + 5] = fClamp255(counter32);
            }
            for(; col < w; col++){ //Cleanup last pixels if not divisible by 6
                __m128 upper = _mm_maskload_ps(&(input->color[plane][row - 1][col - 1]), mask128);
                __m128 middle = _mm_maskload_ps(&(input->color[plane][row][col - 1]), mask128);
                __m128 lower = _mm_maskload_ps(&(input->color[plane][row + 1][col - 1]), mask128);
                //Load one pixels information into 3 128bit vectors
                
                
                middle = _mm_mul_ps(middle, filt128.middle);
                lower = _mm_fmadd_ps(lower, filt128.lower, middle);
                upper = _mm_fmadd_ps(upper, filt128.upper, lower);
                //Mul + fused mul + add to calculate values for the one pixel
                
                upper = _mm_hadd_ps(upper, upper);
                upper = _mm_hadd_ps(upper, upper);
                //Two horizontal adds leads the sum of the vector in each element
                //The first sum is taken and divided, setting the output
                output->color[plane][row][col] = fClamp255(upper[0]/divisor);
            }
        }
    }
    
    cycStop = rdtscll();
    double diff = cycStop - cycStart;
    double diffPerPixel = diff / (output -> width * output -> height);
    fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
    return diffPerPixel;
}
