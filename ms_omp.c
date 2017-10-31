#define PNG_NO_SETJMP

#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


void write_png(const char* filename, const int width, const int height, const int* buffer) {
    FILE* fp = fopen(filename, "wb");
    assert(fp);
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);
    size_t row_size = 3 * width * sizeof(png_byte);
    png_bytep row = (png_bytep)malloc(row_size);
    for (int y = 0; y < height; ++y) {
        memset(row, 0, row_size);
        for (int x = 0; x < width; ++x) {
            int p = buffer[(height - 1 - y) * width + x];
            row[x * 3] = ((p & 0xf) << 4);
        }
        png_write_row(png_ptr, row);
    }
    free(row);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}


inline int test(double x, double y, double y2){
    double xp = x-0.25;
    double cp = x+1;
    double theta = atan2(y, xp);
    double r = 0.5*(1-cos(theta));
    return (r*r >= xp*xp + y2) || (0.0625 >= cp*cp + y2);
}


int main(int argc, char** argv) {
    /* argument parsing */
    assert(argc == 9);
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

    /* allocate memory for image */
    int* image = (int*)malloc(width * height * sizeof(int));
    assert(image);


    double y0, x0;
    int repeats;
    double x, y, x2, y2, xy, len;
    int i, j;
    /* mandelbrot set */
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

            if(test(x, y, y2)){
                repeats = 100000;
            }else{
                while (repeats < 100000 && len < 4) {
                    x = x2 - y2 + x0;
                    y = xy + xy + y0;
                    x2 = x*x;
                    y2 = y*y;
                    xy = x*y;
                    len = x2+y2;
                    ++repeats;
                }
            }
            image[j * width + i] = repeats;
        }
    }


    for (int y = 0; y < height; ++y) {
        memset(png_row, 0, row_size);

#pragma omp parallel for num_threads(num_threads) schedule(dynamic)
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

    /* draw and cleanup */
    //write_png(filename, width, height, image);
    free(image);
    return 0;
}
