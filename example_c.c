#include "stdio.h"
#include "gpmc_driver_c.h"
#include <string.h>
#include <time.h>
#include <fcntl.h>     // open()
#include <unistd.h>    // close()
#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>

//registers
int tilt_position_reg = 6;
int pan_position_reg = 7;
int tilt_pwm_reg = 8;
int pan_pwm_reg = 9;

//control loop parameters
double freq = 2.0;

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

//LOOP VARIABLES
int fd; // File descriptor.
char gpmc[] = "/dev/gpmc_fpga";
char webcam[] = "/dev/video0";
	//time
double sampletime; //in seconds
unsigned long long time_current, time_prev; // nano seconds
struct timespec tp;
	//control
double tilt_setpoint = 0.0;
double pan_setpoint = 0.0;
	//pwm
double tilt_position = 0.0;
double pan_position = 0.0;
int tilt_pwm;
int pan_pwm;

//prototypes
static void new_sample (GstAppSink *sink, GstElement *element);
static gboolean link_elements_with_filter (GstElement *element1, GstElement *element2);
static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data);
double tilt(double sampletime, double input, double position);
double pan(double sampletime, double input, double position);

int main (int   argc, char *argv[]){
	GMainLoop *loop;
	GstElement *pipeline, *source, *converter, *sink;
	GstBus *bus;
	guint bus_watch_id;

	/* Initialisation */

	gst_init (&argc, &argv);
	loop = g_main_loop_new (NULL, FALSE);

	/* Create gstreamer elements */
	pipeline 	= gst_pipeline_new ("video-player");
	source   	= gst_element_factory_make ("v4l2src",      	"video-source");
	converter	= gst_element_factory_make ("videoconvert",	"video-converter");
	sink     	= gst_element_factory_make ("appsink", 	"video-output");

	if (!pipeline || !source || !converter || !sink) {
		g_printerr ("One element could not be created. Exiting.\n");
		return -1;
	}

	/* Set up the pipeline */
	/* we set the device to the source element */
	g_object_set (G_OBJECT (source), "device", "/dev/video0", NULL);

	/* we add a message handler */
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
	gst_object_unref (bus);

	/* we add all elements into the pipeline */
	/* source | converter | sink */
	gst_bin_add_many (GST_BIN (pipeline),
					source, converter, sink, NULL);

	/* link the elements together */
	/* file-source (caps)-> converter -> sink */
	if (link_elements_with_filter (source, converter)){
		gst_element_link_many (converter, sink, NULL);
	}
	else{
		g_warning ("Shit!");
	}

	/* configure appsink*/
	GstCaps *caps;
	caps = gst_caps_new_simple ("video/x-raw",
		"format", G_TYPE_STRING, "YUY2",     
		"width", G_TYPE_INT, 160,
		"height", G_TYPE_INT, 120,
		"framerate", GST_TYPE_FRACTION, 30, 1,
	NULL);

	// g_object_set (sink, "emit-signals", TRUE, "caps", caps, NULL);//done!
	g_object_set (sink, "emit-signals", TRUE, "caps", caps, NULL);//done!
	g_signal_connect (sink, "new-sample", G_CALLBACK (new_sample), sink);
	gst_caps_unref (caps);
	//g_free (audio_caps_text);

	/* Set the pipeline to "playing" state*/
	g_print ("WEBCAM STARTING\n");
	gst_element_set_state (pipeline, GST_STATE_PLAYING);

	/* GPMC & DETECTION */
	fd = open(gpmc, 0);
	if (0 > fd){
		printf("Error, could not open device: %s.\n", gpmc);
		return 1;
	}
	clock_gettime(CLOCK_REALTIME, &tp);
	time_prev = 1000000000.0*tp.tv_sec + tp.tv_nsec;
	/* Iterate */
	g_print ("Running...\n");
	g_main_loop_run (loop);

	/* Out of the main loop, clean up nicely */
	close(fd);
	g_print ("Returned, stopping playback\n");
	gst_element_set_state (pipeline, GST_STATE_NULL);
	g_print ("Deleting pipeline\n");
	gst_object_unref (GST_OBJECT (pipeline));
	g_source_remove (bus_watch_id);
	g_main_loop_unref (loop);
	return 0;
}

static void new_sample (GstAppSink *sink, GstElement *element){
	/* Retrieve the buffer */
	GstSample *sample = NULL;
	sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
	if (sample) {
		/* UPDATE SETPOINT*/
		//VARIABLES
		g_print ("*\n"); 
		struct yuyv{
			unsigned char y1;
			unsigned char u;
			unsigned char y2;
			unsigned char v;
		}yuyv;
		int w = 160;
		int h = 120;
		int pixels_per_yuyv = 2;
		int bytes_per_pixel = 2;
		int bytes_per_yuyv = sizeof(yuyv);
		int yuyvs_per_image = (h*w*bytes_per_pixel)/bytes_per_yuyv;
		struct yuyv yuyv_image[yuyvs_per_image];
		//GET DATA
		GstBuffer *buffer = gst_sample_get_buffer(sample);
		GstMapInfo map;
		gst_buffer_map(buffer, &map, GST_MAP_READ);
		memcpy(yuyv_image, map.data, sizeof(yuyv)*yuyvs_per_image);
		//DO MEASUREMENT
		double fov = 3.1415/2.0;// 90 degrees// in radians!! field of view
		double x_avg = 0;
		double y_avg = 0;
		int number_of_accepted_pixels = 0;
		int i = 0;
		for(i=0;i<yuyvs_per_image;i++){
			if(yuyv_image[i].u < 120 && yuyv_image[i].v <120){
				//CALCULATE XY POSITION BLOB
				x_avg += (int)(i % (w/pixels_per_yuyv));
				y_avg += (int)(i / (w/pixels_per_yuyv));
				number_of_accepted_pixels++;
				//debug printf("x: %i\n", (i) % (w/pixels_per_yuyv));
			}
		}
		x_avg = x_avg/number_of_accepted_pixels;
		y_avg = y_avg/number_of_accepted_pixels;
		pan_setpoint = (x_avg-w/2.0)*(fov/w);
		tilt_setpoint = (y_avg-h/2.0)*(fov/h);
		//unmap
		gst_buffer_unmap(buffer, &map);
	}else{
		g_print ("No sample\n");
	}
	gst_sample_unref(sample);
	g_print("get emit signals %d",gst_app_sink_get_emit_signals(GST_APP_SINK(sink)));

	/* UPDATE CONTROL SYSTEM*/
	//update sampletime
	clock_gettime(CLOCK_REALTIME, &tp);
	time_current = 1000000000.0*tp.tv_sec + tp.tv_nsec;
	sampletime = ((double)(time_current-time_prev))/1000000000.0;
	//update pwm
	//tilt_position = (double)getGPMCValue(fd, tilt_position_reg)/318.41;
	//pan_position = (double)getGPMCValue(fd, pan_position_reg)/318.41;
	tilt_pwm = (int)(100*tilt(sampletime, tilt_setpoint,tilt_position));
	pan_pwm = (int)(100*pan(sampletime, pan_setpoint,pan_position));
	setGPMCValue(fd, tilt_pwm, tilt_pwm_reg);
	setGPMCValue(fd, pan_pwm, pan_pwm_reg);
	//debug:printf("sampletime: %f tilt_setpoint: %f tilt_position: %f tilt_pwm: %i\n", 		sampletime, tilt_setpoint, tilt_position, tilt_pwm);
	//update time variables
	time_prev = time_current;
}

static gboolean link_elements_with_filter (GstElement *element1, GstElement *element2){
   gboolean link_ok;
   GstCaps *caps;
   caps = gst_caps_new_simple ("video/x-raw",
	"format", G_TYPE_STRING, "YUY2",     
	"width", G_TYPE_INT, 160,
	"height", G_TYPE_INT, 120,
	"framerate", GST_TYPE_FRACTION, 30, 1,
	NULL);

   link_ok = gst_element_link_filtered (element1, element2, caps);
   gst_caps_unref (caps);

   if (!link_ok){
     g_warning ("Failed to link element1 and element2!");
   }

   return link_ok;
}

static gboolean bus_call (GstBus     *bus,
						GstMessage *msg,
						gpointer    data){
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

/*
FUCNTION: tilt
DESC: outputs speed and direction from input data.
input and position are angles in radians
*/
double tilt(double sampletime, double input, double position){
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
double pan(double sampletime, double input, double position){
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
