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

/*
 * A (distinct) instance of this structure
 * is passed to each thread
 */
struct thread_info_struct {
	pthread_t tid; /* POSIX thread id, as returned by the library */

    int id;
    int nThreads;
    sem_t* semaphores;

	//double *arr; /* Pointer to array to manipulate */
	//int len; 
	//double mul;
	
	//int thrid; /* Application-defined thread id */
	//int thrcnt;
};

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

//void *compute_and_output_mandel_line(int fd, int threadnum,int NTHREADS, sem_t *wait_sema, sem_t *signal_sema)
//void *compute_and_output_mandel_line(int fd, struct thread_info_struct **thr, int id)
void *compute_and_output_mandel_line(int fd, void *arg1, void *arg2)
{
	/*
	 * A temporary array, used to hold color values for the line being drawn
	 */
	int color_val[x_chars];
	int i;
	//int fd=1;

	/* We know arg points to an instance of thread_info_struct */
	struct thread_info_struct *thr1 = arg1;
	struct thread_info_struct *thr2 = arg2;

	//sem_t *sem = arg2;

	/*
	for(i=0; thr1->id+thr1->nThreads*i<y_chars; i++){
		compute_mandel_line(thr1->id+thr1->nThreads*i, color_val);
		sem_wait(&thr1->semaphores[thr1->id]);
		output_mandel_line(fd, color_val);
		sem_post(&thr1->semaphores[(thr1->id+1)%thr1[thr1->id].nThreads]thr1[(i+1)%thr1[i].nThreads]->semaphores);
	}*/

	/*for(i=0; id+thr[i]->nThreads*i<y_chars; i++){
		compute_mandel_line(id+thr[id]->nThreads*i, color_val);
		sem_wait(thr[id]->semaphores);
		output_mandel_line(fd, color_val);
		sem_post(thr[(i)%thr[id]->nThreads]->semaphores);
	}*/

	for(i=thr1->id; i<y_chars; i+=thr1->nThreads){
		compute_mandel_line(i, color_val);
		sem_wait(thr1->semaphores);
		output_mandel_line(fd, color_val);
		sem_post(thr2->semaphores);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, ret, nThreads;
	sem_t* semaphores;
	struct thread_info_struct *thr;

	/*
	 * Parse the command line
	 */
    if (argc < 2){
        fprintf(stderr, "Usage: %s <NTHREADS>\n", argv[0]);
        exit(1);
    }

	if (safe_atoi(argv[1], &nThreads) < 0 || nThreads <= 0) {
		fprintf(stderr, "`%s' is not valid for `thread_count'\n", argv[1]);
		exit(1);
	}	

	//int NTHREADS=*argv[1]-'0';//cast argv[1] to int

	xstep = (xmax - xmin) / x_chars;
	ystep = (ymax - ymin) / y_chars;

	//pthread_t thread[NTHREADS];
	//sem_t sema[NTHREADS];
	
	/*
	 * Allocate and initialize semaphores
	 */

	thr = safe_malloc(nThreads * sizeof(*thr));
	//thr->semaphores = safe_malloc(nThreads * sizeof(sem_t));
	semaphores = safe_malloc(nThreads *sizeof(*semaphores));
	
	sem_init(&semaphores[0], 0, 1);//output initiation thread (first)
	for(i=1; i<nThreads; i++){
		sem_init(&semaphores[i], 0, 0); //output threads wait at start
	}
	
	/*
	sem_init(thr[0].semaphores, 0, 1);//output initiation thread (first)
	for(i=1; i<nThreads; i++){
		sem_init(thr[i].semaphores, 0, 0); //output threads wait at start
	}
	*/

	/*
	 * draw the Mandelbrot Set, one line at a time.
	 * Output is sent to file descriptor '1', i.e., standard output.
	 */
	for(i=0; i<nThreads; i++){
		/* Initialize per-thread structure */
		thr[i].nThreads=nThreads;
		thr[i].id = i;
		thr[i].semaphores=semaphores;
		//(i=0) ? sem_init(thr[i].semaphores, 0, 1) : sem_init(thr[i].semaphores, 0, 0);
		//ret = pthread_create(&thread[i], NULL, compute_and_output_mandel_line(1, i, NTHREADS, &sema[i], &sema[(i+1)%NTHREADS]),0);
		//ret = pthread_create(&thr[i].tid, NULL, compute_and_output_mandel_line(1, &thr[i], &thr[(i+1)%nThreads]), 0);
		ret = pthread_create(&thr[i].tid, NULL, compute_and_output_mandel_line(1, &thr[i] , &thr[(i+1)%nThreads]), 0);
		//ret = pthread_create(&thr[i].tid, NULL, compute_and_output_mandel_line(1, &thr, i), 0);
		if (ret) {
			perror_pthread(ret, "pthread_create");
			exit(1);
		}
	}

	for(i=0; i<nThreads; i++){
		pthread_join(thr[i].tid,NULL);
		sem_destroy(thr[i].semaphores);
	}

	reset_xterm_color(1);
	return 0;
}