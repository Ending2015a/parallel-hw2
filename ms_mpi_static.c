#define PNG_NO_SETJMP

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <png.h>
#include <string.h>


#define __MEASURE_TIME__

//#define __DEBUG__

#define MAX(x, y) ((x)>(y)?(x):(y))
#define MIN(x, y) ((x)<(y)?(x):(y))

#ifdef __MEASURE_TIME__
    double __temp_time=0;
    #define TIC     __temp_time = MPI_Wtime()
    #define TOC(X)  X = (MPI_Wtime() - __temp_time)
    #define TOC_P(X) X += (MPI_Wtime() - __temp_time)
    #define TIME(X) X = MPI_Wtime()
#else
    #define TIC
    #define TOC(X)
    #define TOC_P(X)
    #define TIME(X)
#endif

// MPI argument
int world_size, world_rank;
int start_pixel, pixel_range; // from start_pixel to start_pixel+pixel_range (not include)
int num_thread;


// Mandelbrot set argument
double left, right;
double lower, upper;
int width, height;
char* filename;

int task_size;

double init_time=0;
double final_time=0;
double total_exetime=0;
double total_commtime=0;
double total_iotime=0;
double total_runtime=0;


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

/*
int test(double *x, double *y, double *y2){
    double xp = *x-0.25;
    double cp = *x+1;
    double theta = atan2(*y, xp);
    double r = 0.5*(1-cos(theta));
    if(r*r >= xp*xp + *y2 || 0.0625 >= cp*cp + *y2)
        return 1;
    else
        return 0;    
}*/


int main(int argc, char **argv){

    // check argument count
    assert(argc == 9);


    // Initializing
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);


    TIME(init_time);

    // Argument parsing
    num_thread = strtol(argv[1], 0, 10);
    left = strtod(argv[2], 0);
    right = strtod(argv[3], 0);
    lower = strtod(argv[4], 0);
    upper = strtod(argv[5], 0);
    width = strtol(argv[6], 0, 10);
    height = strtol(argv[7], 0, 10);
    filename = argv[8];


    // counting task range
    int total_pixel = height * width;
    int start_pixel = world_rank;

#ifdef __DEBUG__
    printf("Rank %d: total_pixel %d, start_pixel %d\n", world_rank, total_pixel, start_pixel);
#endif

    int total_pixel_byte = total_pixel * sizeof(int);
    // create image
    int *array = (int*)malloc(total_pixel_byte);
    int *array_end = array + total_pixel;
    int *iter = array + start_pixel;
    assert(array);

    memset(array, 0, total_pixel_byte);

    // mandelbrot set
    int col;  
    int row;


    double x_step = ((right - left) / width);
    double y_step = ((upper - lower) / height);
    double cr;  //real part
    double ci;  //imag part

    int repeat;
    double x, y;
    double x2, y2, xy;
    double len;

    TIC;

    while(iter<array_end){
        
        col = (iter-array) / width;
        row = (iter-array) % width;

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
        iter += world_size;
    }

    TOC(total_runtime);
    
    int* image = (int*)malloc(total_pixel_byte);

    TIC;

    //MPI_Gatherv(array, pixel_range, MPI_INT, image, recvcount, displs, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Reduce(array, image, total_pixel, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);


    TOC(total_commtime);



    if(world_rank == 0){
#ifdef __DEBUG__
        printf("Rank %d: write to image %s\n", world_rank, filename);
#endif
        TIC;
        write_png(filename, width, height, image);
        TOC(total_iotime);
    }

    free(image);
    free(array);


    TIME(final_time);

#ifdef __MEASURE_TIME__
    total_exetime = final_time - init_time;
    //rank, total_exetime, total_runtime, total_iotime, total_commtime;
    printf("%d, %.16lf, %.16lf, %.16lf, %.16lf\n", world_rank, total_exetime, total_runtime, total_iotime, total_commtime);
#endif


    //Final
    MPI_Finalize();


    return 0;
}
