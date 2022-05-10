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
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "mandel-lib.h"

#define MANDEL_MAX_ITERATION 100000

#define perror_pthread(ret, msg) \
	do { errno = ret; perror(msg); } while (0)

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

//pthread_t *thread;
//sem_t *sema;

/*struct sema_info_struct {
	sem_t sid; // POSIX semaphore id, as returned by the library
}; */

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

void *compute_and_output_mandel_line(int fd, int threadnum,int NTHREADS, sem_t *wait_sema, sem_t *signal_sema)
{
	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	int color_val[x_chars];
	
	for(int i=0; (threadnum+(NTHREADS*i))<y_chars; i++){
		compute_mandel_line(threadnum+(NTHREADS*i), color_val);
		sem_wait(wait_sema);
		output_mandel_line(fd, color_val);
		sem_post(signal_sema);
	}
	return NULL;
}

int safe_atoi(char *s, int *val)
{
	long l;
	char *endp;

	l = strtol(s, &endp, 10);
	if (s != endp && *endp == '\0') {
		*val = l;
		return 0;
	} else return -1;
}

int main(int argc, char *argv[])
{
	int NTHREADS;

    if (argc < 2){
        fprintf(stderr, "Usage: %s <NTHREADS>\n", argv[0]);
        exit(1);
    }

	if (safe_atoi(argv[1], &NTHREADS) < 0 || NTHREADS <= 0) {
		fprintf(stderr, "`%s' is not valid for `thread_count'\n", argv[1]);
		exit(1);
	}	

	//int NTHREADS=*argv[1]-'0';//cast argv[1] to int

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	/*
	 * draw the Mandelbrot Set, one line at a time.
	 * Output is sent to file descriptor '1', i.e., standard output.
	 */

	/*for(int i=0; i<NTHREADS; i++){
		thread[i];
		//sema[i];
	}*/

	pthread_t thread[NTHREADS];
	sem_t sema[NTHREADS];
	//struct sema_info_struct *sema;
	
	sem_init(&sema[0], 0, 1);//output initiation thread (first)
	for(int i=1; i<NTHREADS; i++){
		sem_init(&sema[i], 0, 0); //output threads wait at start
	}
	
	int ret;

	for(int i=0; i<NTHREADS; i++){
		ret = pthread_create(&thread[i], NULL, compute_and_output_mandel_line(1, i, NTHREADS, &sema[i], &sema[(i+1)%NTHREADS]),0);
		if (ret) {
			perror_pthread(ret, "pthread_create");
			exit(1);
		}
	}

	for(int i=0; i<NTHREADS; i++){
		pthread_join(thread[i],NULL);
		sem_destroy(&sema[i]);
	}

	reset_xterm_color(1);
	return 0;
}
