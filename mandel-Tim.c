/*
 * mandel.c
 *
 * A program to draw the Mandelbrot Set on a 256-color xterm.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

/***************************
 * Compile-time parameters *
 ***************************/

/*
 * Output at the terminal is is x_chars wide by y_chars long
*/
int y_chars = 50;
int x_chars = 90;

/*
 * The part of the complex plane to be drawn:
 * upper left corner is (xmin, ymax), lower right corner is (xmax, ymin)
*/
double xmin = -1.8, xmax = 1.0;
double ymin = -1.0, ymax = 1.0;

/*
 * Every character in the final output is
 * xstep x ystep units wide on the complex plane.
 */
double xstep;
double ystep;

sem_t* semaphores;

/*
 * This function computes a line of output
 * as an array of x_char color values.
 */

void compute_mandel_line(int line, int color_val[])
{
        /*
         * x and y traverse the complex plane.
         */
        double x, y;

        int n;
        int val;

        /* Find out the y value corresponding to this line */
        y = ymax - ystep * line;

        /* and iterate for all points on this line */
        for (x = xmin, n = 0; n < x_chars; x+= xstep, n++) {

                /* Compute the point's color value */
                val = mandel_iterations_at_point(x, y, MANDEL_MAX_ITERATION);
                if (val > 255)
                        val = 255;

                /* And store it in the color_val[] array */
                val = xterm_color(val);
                color_val[n] = val;
        }
}

/*
 * This function outputs an array of x_char color values
 * to a 256-color xterm.
 */

void output_mandel_line(int fd, int color_val[])
{
        int i;

        char point ='@';
        char newline='\n';

        for (i = 0; i < x_chars; i++) {
                /* Set the current color, then output the point */
                set_xterm_color(fd, color_val[i]);
                if (write(fd, &point, 1) != 1) {
                        perror("compute_and_output_mandel_line: write point");
                        exit(1);
                }
        }

        /* Now that the line is done, output a newline character */
        if (write(fd, &newline, 1) != 1) {
                perror("compute_and_output_mandel_line: write newline");
                exit(1);
        }
}

void compute_and_output_mandel_line(void* arg)
{
        int i;

        /* We know arg points to an instance of thread_info_struct */
        struct thread_info_struct *thr = arg;
        /*
         * A temporary array, used to hold color values for the line being drawn
         */
        for (i = thr->thrid; i < y_chars; i += thr->nThreads){
            int color_val[x_chars];
            compute_mandel_line(i, color_val);
            sem_wait(&semaphores[i % (thr->nThreads)]);
            /* Critical section */
            output_mandel_line(thr->fd, color_val);
             /* Critical section */
            sem_post(&semaphores[(i+1) % (thr->nThreads)])
        }
}

int safe_atoi(char *s, int *val)
{
        long l;
        char *endp;

        l = strtol(s, &endp, 10);
        if (s != endp && *endp == '\0') {
                *val = l;
                return 0;
        } else
                return -1;
}

void *safe_malloc(size_t size)
{
        void *p;

        if ((p = malloc(size)) == NULL) {
                fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
                        size);
                exit(1);
        }

        return p;
}

void usage(char *argv0)
{
        fprintf(stderr, "Usage: %s thread_count\n\n"
                "Exactly one argument required:\n"
                "    thread_count: The number of threads to create.\n", argv0);
        exit(1);
}

struct thread_info_struct {
    pthread_t tid;

    sem_t* semaphores; /* For thread sync */
    int fd;

    int thrid; /* Application-defined thread id */
    int nThreads;
    
};

int main(int argc, char *argv[])
{
        int i, ret, nThreads;
	    struct thread_info_struct *thr;

        if (argc != 2) {
            usage(argv[0]);
        }
        if (safe_atoi(argv[1], &nThreads) < 0 || nThreads <= 0) {
                fprintf(stderr, "`%s' is not valid for `thread_count'\n", argv[1]);
                exit(1);
        }

        semaphores = safe_malloc(nThreads *sizeof(*semaphores));

        sem_init(&semaphores[0], 0, 1); // output initiation thread (first)
	    for(i=1; i<nThreads; i++){
		    sem_init(&semaphores[i], 0, 0); // output threads wait at start
	    }

        thr = safe_malloc(nThreads * sizeof(*thr));

        xstep = (xmax - xmin) / x_chars;
        ystep = (ymax - ymin) / y_chars;

        /*
         * draw the Mandelbrot Set, one line at a time.
         * Output is sent to file descriptor '1', i.e., standard output.
         */

        for(i=0; i<nThreads; i++){
		    /* Initialize per-thread structure */
		    thr[i].nThreads = nThreads;
		    thr[i].thrid = i;
		    thr[i].semaphores = semaphores;
            thr[i].fd = 1;

            /* Spawn new thread */
            ret = pthread_create(&thr[i].tid, NULL, compute_and_output_mandel_line, &thr[i]);
		    if (ret) {
			    perror_pthread(ret, "pthread_create");
			    exit(1);
            }

        }

        /*
         * Wait for all threads to terminate
         */

        for (i = 0; i < nThreads; i++) {
                sem_destroy(&semaphores[0]);
                ret = pthread_join(thr[i].tid, NULL);
                if (ret) {
                        perror_pthread(ret, "pthread_join");
                        exit(1);
                }
        }

        reset_xterm_color(1);
        return 0;
}

