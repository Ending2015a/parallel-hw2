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


#define MINIMUM_NUMBER 1024
#define MINIMUM_NUMBER_POW_2 10

#define MAXIMUM_TASKS_PER_WORKER_POW_2 3
#define MAXIMUM_TASKS_PER_WORKER 8
#define MAX(x, y) ((x)>(y)?(x):(y))
#define MIN(x, y) ((x)<(y)?(x):(y))

#ifdef __MEASURE_TIME__
    double __temp_time=0;
    #define TIC     __temp_time = MPI_Wtime()
    #define TOC(X)  X += (MPI_Wtime() - __temp_time)
    #define TOC_P(X) X += (MPI_Wtime() - __temp_time)
    #define TIME(X) X = MPI_Wtime()
#else
    #define TIC
    #define TOC(X)
    #define TOC_P(X)
    #define TIME(X)
#endif

double total_exetime=0;
double total_runtime=0;
double total_iotime=0;
double total_commtime=0;

double init_time=0;
double final_time=0;

// MPI argument
int world_size, world_rank;
int total_pixel;
int num_thread;

int task_size;


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



inline void worker(){
    int *array = (int*)malloc((task_size+10) * sizeof(int));

#ifdef __DEBUG__
    printf("Rank %d: array size %d\n", world_rank, task_size);
#endif

    assert(array);
    int task[2];
    int off;
    int size;

    int col, row;
    double x_step = ((right - left) / width);
    double y_step = ((upper - lower) / height);

    MPI_Status status;
    TIC;
    MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    TOC_P(total_commtime);

    TIC;
    while(status.MPI_TAG != 0){

        off = task[0];
        size = task[1];

#ifdef __DEBUG__
        printf("Rank %d: recv new task(%d +%d)\n", world_rank, off, size);
#endif


        int *iter = array;
        int *array_end = array+size;
        col = off / width;
        row = off % width;
        double cr = row * x_step + left;  //real part
        double ci = col * y_step + lower;  //imag part

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

      //      if(test(&x, &y, &y2)){
    //            repeat = 100000;
  //          }else{

                while(repeat < 100000 && len < 4){
                    x = x2 - y2 + cr;
                    y = xy + xy + ci;
                    x2 = x*x;
                    y2 = y*y;
                    xy = x*y;
                    len = x2+y2;

                    ++repeat;
                }
//            }

            *iter = repeat;
            ++iter;
            ++row;
            if(row >= width){
                row = 0;
                ++col;
            }

        }

        TOC_P(total_runtime);
        TIC;
        MPI_Send(array, size, MPI_INT, 0, 1, MPI_COMM_WORLD);
        
#ifdef __DEBUG__
        printf("Rank %d: task(%d +%d) done, send back to root\n", world_rank, off, size);
#endif


        MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        TOC_P(total_commtime);
        TIC;
    }


    free(array);
    return;
}


// tag = 1: task signal
// tag = -1: end signal
inline void manager(int *image){
    int *worker_list = (int*)malloc(world_size*2 * sizeof(int));

    int current_pixel=0;
    int remain_pixel = total_pixel;
    int done_pixel = 0;

    TIC;

    //First Task
    for(int i=1;i<world_size;++i){
        int off=i*2;
        int size=i*2+1;

        if( remain_pixel > 0){
            worker_list[off] = current_pixel;
            worker_list[size] = MIN(remain_pixel, task_size);
#ifdef __DEBUG__
            printf("Rank %d: send task(%d +%d) to rank %d\n", world_rank, worker_list[off], worker_list[size], i);
#endif      

            TOC_P(total_runtime);
            TIC;

            MPI_Send(&worker_list[off], 2, MPI_INT, i, 1, MPI_COMM_WORLD);


            TOC_P(total_commtime);
            TIC;

            current_pixel += worker_list[size];
            remain_pixel -= worker_list[size];
        }else{
            worker_list[off] = 0;
            worker_list[size] = 0;
#ifdef __DEBUG__
            printf("Rank %d: send end signal to rank %d\n", world_rank, i);
#endif


            TOC_P(total_runtime);
            TIC;

            MPI_Send(&worker_list[off], 2, MPI_INT, i, 0, MPI_COMM_WORLD);   //TODO

            TOC_P(total_commtime);
            TIC;

        }
    }
    

    int tag;
    MPI_Status status;
    int wrank;

    
    //divide remain task
    do{
        
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tag, &status);
        if (tag){
            wrank = status.MPI_SOURCE;
            int off = wrank*2;
            int size = wrank*2+1;
        

            TOC_P(total_runtime);
            TIC;
            
            MPI_Recv(image+worker_list[off], worker_list[size], MPI_INT, wrank, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            TOC_P(total_commtime);    
            TIC;

            done_pixel += worker_list[size];

            if (remain_pixel > 0){
                worker_list[off] = current_pixel;
                worker_list[size] = MIN(remain_pixel, task_size);

                TOC_P(total_runtime);
                TIC;
                MPI_Send(&worker_list[off], 2, MPI_INT, wrank, 1, MPI_COMM_WORLD);
                TOC_P(total_commtime);
                TIC;

#ifdef __DEBUG__
                printf("Rank %d: send task(%d +%d) to rank %d\n", world_rank, worker_list[off], worker_list[size], wrank);
#endif
                current_pixel += worker_list[size];
                remain_pixel -= worker_list[size];
            
            }else{
                worker_list[off] = 0;
                worker_list[size] = 0;

                TOC_P(total_runtime);
                TIC;
                MPI_Send(&worker_list[off], 2, MPI_INT, wrank, 0, MPI_COMM_WORLD);
                TOC_P(total_commtime);
                TIC;
#ifdef __DEBUG__
                printf("Rank %d: send end signal to rank %d\n", world_rank, wrank);
#endif
            }
        }
        
    }while(done_pixel < total_pixel);

    free(worker_list);
#ifdef __DEBUG__
    printf("Rank %d: all tasks done\n", world_rank);
#endif
}



inline void set_param(int argc, char **argv){
    // Argument parsing
    num_thread = strtol(argv[1], 0, 10);
    left = strtod(argv[2], 0);
    right = strtod(argv[3], 0);
    lower = strtod(argv[4], 0);
    upper = strtod(argv[5], 0);
    width = strtol(argv[6], 0, 10);
    height = strtol(argv[7], 0, 10);
    filename = argv[8];
}

int main(int argc, char **argv){

    // check argument count
    assert(argc == 9);


    // Initializing
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    TIME(init_time);

    // Argument parsing
    set_param(argc, argv);

    // counting total pixels
    total_pixel = height * width;
    task_size = (total_pixel / (world_size-1)) >> MAXIMUM_TASKS_PER_WORKER_POW_2;
    task_size = MAX(task_size, MINIMUM_NUMBER);


#ifdef __DEBUG__
    printf("Rank %d: total_pixel %d, task_size %d\n", world_rank, total_pixel, task_size);
#endif

    
    if(world_rank != 0){
        worker();
    }else{
        // create image
        int *image = (int*)malloc(total_pixel*sizeof(int));
        manager(image);

#ifdef __DEBUG__
        printf("Rank %d: write to image %s\n", world_rank, filename);
#endif
        TIC;
        write_png(filename, width, height, image);
        TOC_P(total_iotime);
        free(image);
    }

    TIME(final_time);

#ifdef __MEASURE_TIME__
    total_exetime = final_time - init_time;

    //rank, total_exetime, total_runtime, total_iotime, total_commtime;
    printf("%d, %.16lf, %.16lf, %.16lf, %.16f\n", world_rank, total_exetime, total_runtime, total_iotime, total_commtime);

#endif

    //Final
    MPI_Finalize();

    return 0;
}
