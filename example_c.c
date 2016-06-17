#include "stdio.h"
#include "gpmc_driver_c.h"

#include <time.h>
#include <fcntl.h>      // open()
#include <unistd.h>     // close()

//registers
int tilt_position_reg = 0;
int pan_position_reg = 1;
int tilt_pwm_reg = 2;
int pan_pwm_reg = 4;

//control loop parameters
double freq = 50.0;

//tilt parameters
double tilt_corrGain = 0.0;
double tilt_kp = 1.6;
double tilt_tauD = 0.05;
double tilt_beta = 0.001;
double tilt_tauI = 10.5;
double tilt_SLmin = -0.99;
double tilt_SLmax = 0.99;

//tilt states
double tilt_uD_prev = 0.0;
double tilt_error_prev = 0.0;
double tilt_error = 0.0;

//tilt parameters
double pan_corrGain = 0.0;
double pan_kp = 2.6;
double pan_tauD = 0.05;
double pan_beta = 0.017;
double pan_tauI = 9.0;
double pan_SLmin = -0.99;
double pan_SLmax = 0.99;

//tilt states
double pan_uD_prev = 0.0;
double pan_error_prev = 0.0;
double pan_error = 0.0;

int main(int argc, char* argv[]){
	int fd; // File descriptor.
	if (2 != argc){
		printf("Usage: %s <device_name>\n", argv[0]);
		return 1;
	}

	printf("Gerard & Henk yoghurt-demo\n");
	// open connection to device.
	printf("Opening gpmc_fpga...\n");
	fd = open(argv[1], 0);
	if (0 > fd){
		printf("Error, could not open device: %s.\n", argv[1]);
		return 1;
	}
	//time
	double sampletime; //in seconds
	unsigned long long time_current, time_prev; //in micro seconds
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	time_prev = 1000000*tp.tv_sec + tp.tv_usec;
	//control
	int counter = -100;
	double tilt_setpoint = 0;
	double pan_setpoint = 0;
	//pwm
	int tilt_position;
	int pan_position;
	int tilt_pwm;
	int pan_pwm;
	
	
	while(1){
		//update sampletime
		while(sampletime < 1/freq){ //wait until its time to run again as defined by freq
			clock_gettime(CLOCK_REALTIME, &tp);
			time_current = 1000000*tp.tv_sec + tp.tv_usec;
			sampletime = ((double)(time_current-time_prev))/1000000.0;
		}
		time_prev = time_current;
		//UPDATE target position
		if(count <0){
			tilt_setpoint = tilt_setpoint-0.01;
			pan_setpoint = pan_setpoint-0.01;
		}else if(count <100){
			tilt_setpoint = tilt_setpoint+0.01;
			pan_setpoint = pan_setpoint+0.01;
		}else count = -100;
		
		//UPDATE pwm
		tilt_position = getGPMCValue(fd, tilt_position_reg);
		pan_position = getGPMCValue(fd, pan_position_reg);
		tilt_pwm = (int)(100*tilt(sampletime, tilt_setpoint,tilt_position));
		pan_pwm = (int)(100*pan(sampletime, pan_setpoint,pan_position));
		setGPMCValue(fd, tilt_pwm, tilt_pwm_reg)
		setGPMCValue(fd, pan_pwm, pan_pwm_reg)
	}
	
	printf("Exiting...\n");
	// close connection to free resources.
	close(fd);
	
  return 0;
}

/*
FUCNTION: tilt
DESC: outputs speed and direction from input data.
input and position are angles in radians
*/
double tilt(double sampletime, int input, int position){
	double factor = 1.0/(sampletime+tilt_tauD+tilt_beta);
	double error = input - position;
	double uD = factor*(((tilt_tauD*tilt_uD_prev)*tilt_beta + 
	(tilt_tauD*tilt_kp)*(error-tilt_error_prev)) + (sampletime*tilt_kp)*error);
	double uI = tilt_uD_prev+(sampletime*uD)/tilt_tauI;
	double pid_output = uI + uD;
	double SL_output = (pid_output < tilt_SLmin) ? //since corrGain = 0
		tilt_SLmin
		:
		((pid_output > tilt_SLmax) ?
			tilt_SLmax
			:
			pid_output);
	return SL_output;
}

/*
FUCNTION: pan
DESC: outputs speed and direction from input data.
input and position are angles in radians
*/
double pan(double sampletime, int input, int position){
	double factor = 1.0/(sampletime+pan_tauD+pan_beta);
	double error = input - position;
	double uD = factor*(((pan_tauD*pan_uD_prev)*pan_beta + 
	(pan_tauD*pan_kp)*(error-pan_error_prev)) + (sampletime*pan_kp)*error);
	double uI = pan_uD_prev+(sampletime*uD)/pan_tauI;
	double pid_output = uI + uD;
	double SL_output = (pid_output < pan_SLmin) ? //since corrGain = 0
		pan_SLmin
	:
		((pid_output > pan_SLmax) ?
			pan_SLmax
		:
			pid_output); 
	return SL_output;
}