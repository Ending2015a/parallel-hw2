#define PNG_NO_SETJMP

#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#define __MEASURE_TIME__

//#define __DEBUG__

#ifdef __MEASURE_TIME__
    double __temp_time=0;
    #define TIC     __temp_time = omp_get_wtime()
    #define TOC(X)  X += (omp_get_wtime() - __temp_time)
    #define TOC_P(X) X += (omp_get_wtime() - __temp_time)
    #define TIME(X) X = omp_get_wtime()
#else
    #define TIC
    #define TOC(X)
    #define TOC_P(X)
    #define TIME(X)
#endif


double init_time=0;
double final_time=0;
double total_exetime=0;
double total_runtime=0;
double total_iotime=0;
double total_commtime=0;


int main(int argc, char** argv) {
    /* argument parsing */
    assert(argc == 9);

    TIME(init_time);

    int num_threads = strtol(argv[1], 0, 10);
    double left = strtod(argv[2], 0);
    double right = strtod(argv[3], 0);
    double lower = strtod(argv[4], 0);
    double upper = strtod(argv[5], 0);
    int width = strtol(argv[6], 0, 10);
    int height = strtol(argv[7], 0, 10);
    const char* filename = argv[8];

    FILE *fp = fopen(filename, "wb");
    assert(fp);

    //create write struct
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);

    //create info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);

    //init io
    png_init_io(png_ptr, fp);

    //set IHDR
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    size_t row_size = 3 * width * sizeof(png_byte);
    png_bytep png_row = (png_bytep)malloc(row_size);

    int world_size = num_threads;

    int total_pixel = width * height;
    /* allocate memory for image */
    int* image = (int*)malloc(total_pixel * sizeof(int));
    int* image_end = image + total_pixel;
    assert(image);

    TIC;

    double col, row;
    double cr, ci;
    double x_step = ((right-left)/width);
    double y_step = ((upper-lower)/height);
    double y0, x0;
    int repeat;
    double x, y, x2, y2, xy, len;
    /* mandelbrot set */

    

#pragma omp parallel num_threads(num_threads) private(x0, y0, repeat, x, y, x2, y2, xy, len, cr, ci, col, row) shared(image, world_size, image_end, x_step, y_step)
#ifdef __MEASURE_TIME__
    {
    double start = omp_get_wtime();
#endif
#pragma omp for  schedule(dynamic)
    for(int *iter=image; iter<image_end; ++iter){
        col = (iter-image) / width;
        row = (iter-image) % width;

        repeat=1;
        cr = row * x_step + left;
        ci = col * y_step + lower;
        x = cr;
        y = ci;
        x2 = x*x;
        y2 = y*y;
        xy = x*y;
        len = x2 + y2;
 
        while(repeat < 100000 && len < 4){
            x = x2 - y2 + cr;
            y = xy + xy + ci;
            x2 = x*x;
            y2 = y*y;
            xy = x*y;
            len = x2+y2;

            ++repeat;
        }

        *iter = repeat;
    }
#ifdef __MEASURE_TIME__
    printf("%d, %lf\n", omp_get_thread_num(), omp_get_wtime()-start);
    }
#endif

/*    
#pragma omp parallel num_threads(num_threads) private(x0, y0, repeats, x, y, x2, y2, xy, len, i, j) shared(image)
#pragma omp for schedule(dynamic) collapse(2)
    for (j = 0; j < height; ++j) {
        for (i = 0; i < width; ++i) {
            y0 = j * ((upper - lower) / height) + lower;
            x0 = i * ((right - left) / width) + left;

            repeats = 1;
            x = x0;
            y = y0;
            x2 = x*x;
            y2 = y*y;
            xy = x*y;
            len = x2 + y2;

//            if(test(x, y, y2)){
//                repeats = 100000;
//            }else{
                while (repeats < 100000 && len < 4) {
                    x = x2 - y2 + x0;
                    y = xy + xy + y0;
                    x2 = x*x;
                    y2 = y*y;
                    xy = x*y;
                    len = x2+y2;
                    ++repeats;
                }
//            }
            image[j * width + i] = repeats;
        }
    }*/

    TOC_P(total_runtime);


#ifdef __DEBUG__
    printf("task done\n");
#endif


    TIC;

    for (int y = 0; y < height; ++y) {
        memset(png_row, 0, row_size);

//#pragma omp parallel for num_threads(num_threads) schedule(dynamic)
        for (int x = 0; x < width; ++x) {
            int p = image[(height - 1 - y) * width + x];
            png_row[x * 3] = ((p & 0xf) << 4);
        }

        png_write_row(png_ptr, png_row);
    }
    free(png_row);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    TOC_P(total_iotime);
    TIME(final_time);

    #ifdef __MEASURE_TIME__
    total_exetime = final_time - init_time;

    //rank, total_exetime, total_runtime, total_iotime, total_commtime;
    printf("%d, %.16lf, %.16lf, %.16lf, %.16f\n", 0, total_exetime, total_runtime, total_iotime, total_commtime);
    #endif


    /* draw and cleanup */
    //write_png(filename, width, height, image);
    free(image);
    return 0;
}
