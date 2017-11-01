#define PNG_NO_SETJMP

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <png.h>
#include <string.h>
#include <omp.h>

//#define __MEASURE_TIME__

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
    #define TIME(X) X = MPI_Wtime()
#else
    #define TIC
    #define TOC(X)
    #define TIME(X)
#endif

// MPI argument
int world_size, world_rank;
int total_pixel;
int num_threads;

int task_size;


// Mandelbrot set argument
double left, right;
double lower, upper;
int width, height;
char* filename;
unsigned char* line_png;

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
    int *array = (int*)malloc((task_size) * sizeof(int));

#ifdef __DEBUG__
    printf("Rank %d: array size %d\n", world_rank, task_size);
#endif

    assert(array);
    int task[2];
    int off;
    int size;

    double x_step = ((right - left) / width);
    double y_step = ((upper - lower) / height);

    MPI_Status status;
    MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    while(status.MPI_TAG != 0){

        off = task[0];
        size = task[1];

        if(size <= 0)
            break;

#ifdef __DEBUG__
        printf("Rank %d: recv new task(%d +%d)\n", world_rank, off, size);
#endif


        double cr;  //real part
        double ci;  //imag part

        int idx;
        int repeat;
        double x, y;
        double x2, y2, xy;
        double len;
#pragma omp parallel num_threads(num_threads) private(idx, cr, ci, repeat, x, y, x2, y2, xy, len) shared(x_step, y_step, size, off, array)
#pragma omp for schedule(dynamic)
        for(idx=0 ; idx<size ; ++idx){
            
            repeat=1;
            cr = ((idx+off)%width) * x_step + left;
            ci = ((idx+off)/width) * y_step + lower;
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

            array[idx] = repeat;
        }


        MPI_Send(array, size, MPI_INT, 0, 1, MPI_COMM_WORLD);

#ifdef __DEBUG__
        printf("Rank %d: task(%d +%d) done, send back to root\n", world_rank, off, size);
#endif


        MPI_Recv(task, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }


    free(array);
    return;
}


// tag = 1: task signal
// tag = -1: end signal
inline void manager(){
    int *image = (int*)malloc(width * height * sizeof(int));
    int *worker_list = (int*)malloc(world_size*2 * sizeof(int));

    assert(image);
    assert(worker_list);

    int current_pixel=0;
    int remain_pixel = total_pixel;
    int done_pixel = 0;

    int i;
    //First Task
#pragma omp parallel num_threads(num_threads) shared(world_size, current_pixel, remain_pixel, done_pixel, worker_list) private(i)
#pragma omp for schedule(dynamic)
    for(i=1;i<world_size;++i){
        int off=i*2;
        int size=i*2+1;

        if( remain_pixel > 0){
            #pragma omp critical
            {
                worker_list[off] = current_pixel;
                worker_list[size] = MIN(remain_pixel, task_size);
                current_pixel += worker_list[size];
                remain_pixel -= worker_list[size];
            }
#ifdef __DEBUG__
            printf("Rank %d-%d: send task(%d +%d) to rank %d\n", world_rank, omp_get_thread_num(), worker_list[off], worker_list[size], i);
#endif
            MPI_Send(&worker_list[off], 2, MPI_INT, i, 1, MPI_COMM_WORLD);

        }else{
            worker_list[off] = 0;
            worker_list[size] = 0;
#ifdef __DEBUG__
            printf("Rank %d-%d: send end signal to rank %d\n", world_rank, omp_get_thread_num(), i);
#endif

            MPI_Send(&worker_list[off], 2, MPI_INT, i, 0, MPI_COMM_WORLD);

        }
    }

    int tag;
    MPI_Status status;
    int wrank;
    //int parallel_threads = MIN(num_threads, (world_size-1) >> 1);
    int parallel_threads = MAX(1, num_threads/2);

#ifdef __DEBUG__
    printf("Rank %d: parallel threads %d\n", world_rank, parallel_threads);
#endif

    int off_c;
    int size_c;
    int off;
    int size;

    //divide remain task
    #pragma omp parallel num_threads(parallel_threads) private(tag, status, wrank, off_c, size_c, off, size) shared(remain_pixel, done_pixel, current_pixel, worker_list, total_pixel)
    {
        do{
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tag, &status);
            if (tag){
                wrank = status.MPI_SOURCE;
                off_c = wrank*2;
                size_c = wrank*2+1;
                off = worker_list[off_c];
                size = worker_list[size_c];

                MPI_Recv(image+off, size, MPI_INT, wrank, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                #pragma omp atomic
                done_pixel += size;

#ifdef __DEBUG__
                printf("Rank %d-%d: receive rank %d task(%d +%d)\n", world_rank, omp_get_thread_num(), wrank, off, size);
#endif

                #pragma omp parallel sections num_threads(2) shared(remain_pixel, done_pixel, current_pixel, worker_list, total_pixel)
                {
                    #pragma omp section
                    {
                        if (remain_pixel > 0){
                            #pragma omp critical
                            {
                                worker_list[off_c] = current_pixel;
                                worker_list[size_c] = MIN(remain_pixel, task_size);
                                current_pixel += worker_list[size_c];
                                remain_pixel -= worker_list[size_c];
                            }

#ifdef __DEBUG__
                            printf("Rank %d-%d: send task(%d +%d) to rank %d\n", world_rank, omp_get_thread_num(), worker_list[off_c], worker_list[size_c], wrank);
#endif

                            MPI_Send(&worker_list[off_c], 2, MPI_INT, wrank, 1, MPI_COMM_WORLD);
            
                        }else{
                            worker_list[off_c] = 0;
                            worker_list[size_c] = 0;
                            MPI_Send(&worker_list[off_c], 2, MPI_INT, wrank, 0, MPI_COMM_WORLD);
#ifdef __DEBUG__
                            printf("Rank %d-%d: send end signal to rank %d\n", world_rank, omp_get_thread_num(), wrank);
#endif
                        }
                    }
                    #pragma omp section
                    {
#ifdef __DEBUG__
                        printf("Rank %d-%d: transfer to image(%d +%d)\n", world_rank, omp_get_thread_num(), off, size);
#endif
                        for(int n=off;n<off+size;++n){
                            line_png[n*3] = ((image[n] & 0xf) << 4); 
                        }
                    }
                }
            }//if tag
        }while(done_pixel < total_pixel);
        
    }

    free(worker_list);
    free(image);
#ifdef __DEBUG__
    printf("Rank %d: all tasks done\n", world_rank);
#endif
}



inline void set_param(int argc, char **argv){
    // Argument parsing
    num_threads = strtol(argv[1], 0, 10);
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

    int provided;

    // Initializing
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (provided < MPI_THREAD_MULTIPLE){
        printf("ERROR: The MPI library does not have full thread support\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }


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
        line_png = (unsigned char*)malloc(total_pixel * 3);
#ifdef __DEBUG__
        printf("Rank %d: line png size %d\n", world_rank, total_pixel * 3);
#endif
        assert(line_png);
        manager();

#ifdef __DEBUG__
        printf("Rank %d: write to image %s\n", world_rank, filename);
#endif

        //write_png(filename, width, height, image);

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

        png_bytepp rf = (png_bytepp)malloc(height * sizeof(png_bytep));

        for(int y=0;y<height;++y){
            rf[y] = (png_bytep)(line_png + (height - 1 - y) * width*3);
        }
        
        png_write_image(png_ptr, rf);
        png_write_end(png_ptr, NULL);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        free(line_png);
        free(rf);

    }

    //Final
    MPI_Finalize();

    return 0;
}
