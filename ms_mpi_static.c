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

#define MINIMUM_NUMBER 4
#define MAX(x, y) ((x)>(y)?(x):(y))


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


int test(double *x, double *y, double *y2){
    double xp = *x-0.25;
    double cp = *x+1;
    double theta = atan2(*y, xp);
    double r = 0.5*(1-cos(theta));
    if(r*r >= xp*xp + *y2 || 0.0625 >= cp*cp + *y2)
        return 1;
    else
        return 0;    
}


int main(int argc, char **argv){

    // check argument count
    assert(argc == 9);


    // Initializing
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

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
    int valid_size = world_size;

    if (total_pixel < MINIMUM_NUMBER * world_size){
        valid_size = total_pixel/MINIMUM_NUMBER;
        valid_size = MAX(1, valid_size);
    }


#ifdef __DEBUG__
    printf("Rank %d: valid_size %d\n", world_rank, valid_size);
#endif

    int remain = total_pixel % valid_size;
    int divide = total_pixel / valid_size;
    int bonus = (world_rank < remain) ? 1:0;
    
    start_pixel = divide * world_rank + (bonus?world_rank:remain);
    pixel_range = bonus + divide;

    
#ifdef __DEBUG__
    printf("Rank %d: start_pixel %d, pixel_range %d\n", world_rank, start_pixel, pixel_range);
#endif

    // create image
    int *array = (int*)malloc(pixel_range * sizeof(int));
    int *array_end = array + pixel_range;
    int *iter = array;
    assert(array);

    // mandelbrot set
    int col = start_pixel / width;  
    int row = start_pixel % width;


    double x_step = ((right - left) / width);
    double y_step = ((upper - lower) / height);
    double cr;  //real part
    double ci;  //imag part

#ifdef __DEBUG__
    printf("Rank %d: col %d, row %d\n", world_rank, col, row);
#endif

    int repeat;
    double x, y;
    double x2, y2, xy;
    double len;

    while(iter<array_end){
        
        repeat=1;
        cr = row * x_step + left;
        ci = col * y_step + lower;
        x = cr;
        y = ci;
        x2 = x*x;
        y2 = y*y;
        xy = x*y;
        len = x2 + y2;

        if(test(&x, &y, &y2)){
            repeat = 100000;
        }else{

            while(repeat < 100000 && len < 4){
                x = x2 - y2 + cr;
                y = xy + xy + ci;
                x2 = x*x;
                y2 = y*y;
                xy = x*y;
                len = x2+y2;
            
                ++repeat;
            }
        }

        *iter = repeat;
        ++iter;
        ++row;
        if(row >= width){
            row = 0;
            ++col;
        }

    }


    int *recvcount = (int*)malloc(world_size * sizeof(int));
    assert(recvcount);

    int *displs = (int*)malloc(world_size * sizeof(int));
    assert(displs);

    int *image = (int*)malloc(total_pixel * sizeof(int));
    assert(image);

    for(int i=0;i<world_size;++i){
        if (i < valid_size){
            displs[i] = divide * i + ((i<remain)?i:remain);
            recvcount[i] = ((i < remain) ? 1:0) + divide;
        }else{
            displs[i] = 0;
            recvcount[i] = 0;
        }

#ifdef __DEBUG__
            printf("Rank %d: [%d] displs %d, recvcount %d\n", world_rank, i, displs[i], recvcount[i]);
#endif

    }

    MPI_Gatherv(array, pixel_range, MPI_INT, image, recvcount, displs, MPI_INT, 0, MPI_COMM_WORLD);


    if(world_rank == 0){
#ifdef __DEBUG__
        printf("Rank %d: write to image %s\n", world_rank, filename);
#endif
        write_png(filename, width, height, image);
    }

    free(image);
    free(recvcount);
    free(displs);
    free(array);

    //Final
    MPI_Finalize();


    return 0;
}
