// Includes
#include "bitmap.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

// Thread arguments struct
struct thread_arguements{
    int thread_id, num_threads;
    struct bitmap *bm;
    double xmin, xmax, ymin, ymax;
    int itermax;
};

// Function Prototypes
int iteration_to_color (int i, int max_interations);
int iterations_at_point (double x, double y, int max_interations);
void *compute_image (void *thread_arguements);

void show_help(){
    printf("Use: mandel [options]\n");
    printf("Where options are:\n");
    printf("-n <threads>    Set the number of threads to be used. (default=1)\n");
    printf("-m <max_interations>        The max_interationsimum number of iterations per point. (default=1000)\n");
    printf("-x <coord>      X coordinate of image center point. (default=0)\n");
    printf("-y <coord>      Y coordinate of image center point. (default=0)\n");
    printf("-s <scale>      Scale of the image in Mandlebrot coordinates. (default=4)\n");
    printf("-W <pixels>     Width of the image in pixels. (default=500)\n");
    printf("-H <pixels>     Height of the image in pixels. (default=500)\n");
    printf("-o <file>       Set output file. (default=mandel.bmp)\n");
    printf("-h              Show this help text.\n");
    printf("\nSome examples are:\n");
    printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
    printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
    printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

int main( int argc, char *argv[] ){
    char c;

    // These are the default configuration values used
    // if no command line arguments are given.
    const char *outfile = "mandel.bmp";
    double xcenter = 0;
    double ycenter = 0;
    double scale = 4;
    int    image_width = 500;
    int    image_height = 500;
    int    max_interations = 1000;
    int    num_threads = 1;
    struct timeval begin_time;
    struct timeval end_time;

    // For each command line argument given,
    // override the appropriate configuration value.
    while((c = getopt( argc, argv, "x:y:s:W:H:m:n:o:h" )) != -1 ){
        switch(c){
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                scale = atof(optarg);
                break;
            case 'W':
                image_width = atoi(optarg);
                break;
            case 'H':
                image_height = atoi(optarg);
                break;
            case 'm':
                max_interations = atoi(optarg);
                break;
            case 'n':
                num_threads = atoi(optarg);
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'h':
                show_help();
                exit(1);
                break;
        }
    }

    // Start recording time
    gettimeofday( &begin_time, NULL );

    // Display the configuration of the image.
    printf("mandel: x=%lf y=%lf scale=%lf max_interations=%d outfile=%s\n", xcenter, ycenter, scale, max_interations, outfile);

    // Create a bitmap of the appropriate size.
    struct bitmap *bm = bitmap_create(image_width, image_height);

    // Fill it with a dark blue, for debugging
    bitmap_reset(bm, MAKE_RGBA(0, 0, 255, 0));

    // Set up pthreads and related argument structs
    struct thread_arguements *thread_arguements_array = malloc(sizeof(struct thread_arguements) * num_threads);
    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);

    int t, ret;
    for(t = 0; t < num_threads; t++){
        thread_arguements_array[t].thread_id = t;
        thread_arguements_array[t].num_threads = num_threads;
        thread_arguements_array[t].bm = bm;
        thread_arguements_array[t].xmin = xcenter - scale;
        thread_arguements_array[t].xmax = xcenter + scale;
        thread_arguements_array[t].ymin = ycenter - scale;
        thread_arguements_array[t].ymax = ycenter + scale;
        thread_arguements_array[t].itermax = max_interations;

        // Compute the Mandelbrot image portions
        ret = pthread_create(&threads[t], NULL, (void *) compute_image, (void *) &thread_arguements_array[t]);
        if(ret){
            fprintf(stderr, "Unable to create thread; return code: %d\n", ret);
            exit(1);
        }
    }
    // Wait for the threads and join
    for(t = 0; t < num_threads; t++){
       ret = pthread_join(threads[t], NULL);
       if(ret){
          fprintf(stderr, "Return code from pthread_join() is %d\n", ret);
          exit(1);
        }
    }

    // Save the image in the stated file.
    if(!bitmap_save(bm, outfile)){
        fprintf(stderr, "mandel: couldn't write to %s: %s\n", outfile, strerror(errno));
        return 1;
    }

    //pthread_exit(NULL);
    free(threads);
    free(thread_arguements_array);
    bitmap_delete(bm);

    gettimeofday( &end_time, NULL );

    long time_to_execute = ( end_time.tv_sec * 1000000 + end_time.tv_usec ) -
                            ( begin_time.tv_sec * 1000000 + begin_time.tv_usec );

    printf("This code took %ld microseconds to execute\n", time_to_execute);
    return 0;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax_interations), limiting iterations to "max_interations"
*/
void *compute_image(void *thread_arguements){
    struct thread_arguements *args;
    args = (struct thread_arguements *) thread_arguements;

    int i, j, start, end;

    int width = bitmap_width(args->bm);
    int height = bitmap_height(args->bm);

    // Threaded portion only has to do part of the image
    start = (height / args->num_threads) * args->thread_id;

    if(args->thread_id + 1 == args->num_threads)
    {
        end = height;
    }
    else
    {
        end = start + (height / args->num_threads);
    }

    // For every pixel that the thread is assigned...
    for(j = start; j < end; j++) {
        for(i = 0; i < width; i++) {

            // Determine the point in x,y space for that pixel.
            double x = args->xmin + i*(args->xmax - args->xmin) / width;
            double y = args->ymin + j*(args->ymax - args->ymin) / height;

            // Compute the iterations at that point.
            int iters = iterations_at_point(x, y, args->itermax);

            // Set the pixel in the bitmap.
            bitmap_set(args->bm,i,j,iters);
        }
    }
    return 0;
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a max_interationsimum of max_interations.
*/
int iterations_at_point(double x, double y, int max_interations){
    double x0 = x;
    double y0 = y;

    int iter = 0;

    while((x*x + y*y <= 4) && iter < max_interations){
        double xt = x*x - y*y + x0;
        double yt = 2*x*y + y0;
        x = xt;
        y = yt;
        iter++;
    }
    return iteration_to_color(iter, max_interations);
}
/*
Convert a iteration number to an RGBA color.
*/
int iteration_to_color(int i, int max_interations){
    int r = 55 * i / max_interations;
    int g = 155 * i / max_interations;
    int b = 255 * i / max_interations;
    return MAKE_RGBA(r, g, b, 0) / 5;
}