#define PNG_NO_SETJMP

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <png.h>
#include <string.h>


//#define __MEASURE_TIME__


#define MINIMUM_NUMBER 4
#define MAX(x, y) ((x)>(y)?(x):(y))


#ifdef __MEASURE_TIME__
    double __temp_time=0;
    #define TIC     __temp_time = MPI_Wtime()
    #define TOC(X)  X += (MPI_Wtime() - __temp_time)
    #define TIME(X) X = MPI_Wtime()
#else
    #define TIC
    #define TOC(X)
    #define TIME(X)
#endif

// MPI argument
int world_size, world_rank;
int start_pixel, pixel_range; // from start_pixel to start_pixel+pixel_range (not include)
int num_thread;


// Mandelbrot set argument
double left, right;
double lower, upper;
double width, height;
char* filename;


int main(int argc, char **argv){

    // check argument count
    assert(argc == 9);


    // Initializing
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_size(MPI_COMM_WORLD, &world_rank);

    // Argument parsing
    num_threads = strtol(argv[1], 0, 10);
    left = strtod(argv[2], 0);
    right = strtod(argv[3], 0);
    lower = strtod(argv[4], 0);
    upper = strtod(argv[5], 0);
    width = strtol(argv[6], 0, 10);
    height = strtol(argv[7], 0, 10);
    filename = argv[8];


    // counting task range
    int total_pixel = height * width;
    int valid_size = world_size;

    if (total_pixel < MINIMUM_NUMBER * world_size){
        valid_size = total_num/MINIMUM_NUMBER;
        valid_size = MAX(1, valid_size);
    }


#ifdef __DEBUG__
    printf("Rank %d, valid_size %d\n", world_rank, valid_size);
#endif

    int remain = total_pixel % valid_size;
    int divide = total_pixel / valid_size;
    int bonus = (world_rank < remain) ? 1:0;
    
    start_pixel = divide * world_rank + (bonus?world_rank:remain);
    pixel_range = bonus + divide;

    
    // create image
    int *array = (int*)malloc(pixel_range * sizeof(int));
    int *array_end = array + pixel_range;
    assert(array);

    // mandelbrot set
    int i = start_pixel / width;  
    int j = start_pixel % width;
    int *iter=image;


    double x_step = ((right - left) / width);
    double y_step = ((upper - lower) / height);
    double cr = j * x_step + left;  //real part
    double ci = i * y_step + lower;  //imag part

    while(iter<array_end){
        int repeat=1;   
        
        double zr = cr;  //real
        double zi = ci;  //imag
        double zr2 = zr * zr, zi2 = zi * zi, zrzi = zr * zi;
        double len2 = zr2 + zi2;
        //iterate mandelbrot set
        while(repeat < 100000 && len2 < 4){
            zi = zrzi;
            zi += zi + ci;
            zr = zr2 - zi2 + cr;
            zr2 = zr * zr;
            zi2 = zi * zi;
            len2 = zr2 + zi2;
            ++repeat;
        }
        *iter = repeat;


        //iterate point
        ++j, ++iter;
        cr += x_step;
        if( j>=width ){
            ++i, j=0;
            cr = left;
            ci += y_step;
        }
    }

    if (world_rank == 0){
        
    }else{
        
    }
    
    free()


    //Final
    MPI_Finalize();
    return 0;
}
