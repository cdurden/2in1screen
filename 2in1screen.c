/* 2in1screen
 *
 * This script will trigger a udev action on
 * the tablet device with the given name.
 * Running this as a non-root user will require
 * an addition to the sudo security policy.
 *
SUBSYSTEM=="input", ACTION=="change", PROGRAM="/usr/local/bin/2in1screen"
RESULT=="0", ENV{LIBINPUT_CALIBRATION_MATRIX}="1 0 0 0 1 0"
RESULT=="1", ENV{LIBINPUT_CALIBRATION_MATRIX}="-1 0 1 0 -1 1"
RESULT=="2", ENV{LIBINPUT_CALIBRATION_MATRIX}="0 -1 1 1 0 0"
RESULT=="3", ENV{LIBINPUT_CALIBRATION_MATRIX}="0 1 0 -1 0 1"
 *
 * This udev rule calls this program to obtain the state
 * which is 0 (normal), 1 (inverted), 2 (left), or 3 (right)
 *
 * To test the udev rule, run the following command
 * sudo /usr/bin/udevadm trigger --action=change /dev/input/eventX
 *
 * The libinput Calibration Matrix should then be set.
 * Check this with using:
 *
 * xinput list-props ${TABLET_NAME}
 *
 * For the udev trigger in rotate_screen() to work,
 * the user needs permission to run udevadm trigger.
 *
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define N_STATE 4
#define DATA_SIZE 256

const char* tablet_name = "Wacom HID 482E Finger";

char basedir[DATA_SIZE];
char *basedir_end = NULL;
char content[DATA_SIZE];
char command[DATA_SIZE*4];
char tablet_devnode[DATA_SIZE] = "";


char *ROT[]   = {"normal", 				"inverted", 			"left", 				"right"};
//char *COOR[]  = {"1 0 0 0 1 0 0 0 1",	"-1 0 1 0 -1 1 0 0 1", 	"0 -1 1 1 0 0 0 0 1", 	"0 1 0 -1 0 1 0 0 1"};
char *LAYO[]  = {"${HOME}/.local/share/onboard/layouts/Split.onboard",	"${HOME}/.local/share/onboard/layouts/Split.onboard", "/usr/share/onboard/layouts/Small.onboard", "/usr/share/onboard/layouts/Small.onboard"};
//const char* vendor = "056a";
//const char* product = "482e";

double accel_y = 0.0,
#if N_STATE == 4
	   accel_x = 0.0,
#endif
	   accel_g = 7.0;

int current_state = 0;

int orientation(FILE *dev_accel_y, FILE *dev_accel_x, double scale) {
    int state;
	fseek(dev_accel_y, 0, SEEK_SET);
	fgets(content, DATA_SIZE, dev_accel_y);
	accel_y = atof(content) * scale;
#if N_STATE == 4
	fseek(dev_accel_x, 0, SEEK_SET);
	fgets(content, DATA_SIZE, dev_accel_x);
	accel_x = atof(content) * scale;
#endif
	if(accel_y < -accel_g) state = 0;
	else if(accel_y > accel_g) state = 1;
#if N_STATE == 4
	else if(accel_x > accel_g) state = 2;
	else if(accel_x < -accel_g) state = 3;
#endif
    else state = current_state;
    return state;
}
int rotation_changed(FILE *dev_accel_y, FILE *dev_accel_x, double scale) {
	int state = orientation(dev_accel_y, dev_accel_x, scale);

	if(current_state!=state){
		current_state = state;
		return 1;
	}
	else return 0;
}

FILE* bdopen(char const *fname, char leave_open){
	*basedir_end = '/';
	strcpy(basedir_end+1, fname);
	FILE *fin = fopen(basedir, "r");
	setvbuf(fin, NULL, _IONBF, 0);
	fgets(content, DATA_SIZE, fin);
	*basedir_end = '\0';
	if(leave_open==0){
		fclose(fin);
		return NULL;
	}
	else return fin;
}

void get_tablet_devnode() {
  FILE *fp;
  char *pos;

  sprintf(command, "libinput list-devices | grep \"%s\" -A 5 | awk '$1==\"Kernel:\" {print $2}'", tablet_name);
  printf(command);
  /* Open the command for reading. */
  fp = popen(command, "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  if (fgets(tablet_devnode, sizeof(tablet_devnode), fp) != NULL) {
    if ((pos=strchr(tablet_devnode, '\n')) != NULL)
      *pos = '\0';
    printf("%s", tablet_devnode);
    pclose(fp);
    return;
  } else {
    printf("Failed to get devnode path\n" );
    exit(1);
  }
}

void rotate_screen(){
    if (strlen(tablet_devnode) == 0) {
        get_tablet_devnode();
    }
	sprintf(command, "xrandr -o %s", ROT[current_state]);
	system(command);
    /*
     * We might want more specific udev rules and triggers
     * (see below) but I cannot get these to work correctly.
     *
SUBSYSTEM=="input", ATTRS{id/product}=="", ATTRS{id/vendor}=="", ACTION=="change", PROGRAM="/usr/local/bin/2in1screen"
RESULT=="0", ENV{LIBINPUT_CALIBRATION_MATRIX}="1 0 0 0 1 0"
RESULT=="1", ENV{LIBINPUT_CALIBRATION_MATRIX}="-1 0 1 0 -1 1"
RESULT=="2", ENV{LIBINPUT_CALIBRATION_MATRIX}="0 -1 1 1 0 0"
RESULT=="3", ENV{LIBINPUT_CALIBRATION_MATRIX}="0 1 0 -1 0 1"

    printf("Triggering udev change event for tablet with vendor id \"%s\" and product id \"%s\"\n", vendor, product);
    sprintf(command, "sudo /usr/bin/udevadm trigger --action=change --subsystem-match=input --attr-match=\"id/vendor=%s\" --attr-match=\"id/product=%s\"", vendor, product);
    */
    printf("Triggering udev change event for tablet with devnode \"%s\"\n", tablet_devnode);
    sprintf(command, "sudo /usr/bin/udevadm trigger --action=change %s", tablet_devnode);
	system(command);
    sprintf(command, "sudo systemctl restart touchegg");
	system(command);
	sprintf(command, "gsettings set org.onboard layout \"%s\"", LAYO[current_state]);
	system(command);
}
int main(int argc, char const *argv[]) {
	FILE *pf = popen("ls /sys/bus/iio/devices/iio:device*/in_accel*", "r");
	if(!pf){
		fprintf(stderr, "IO Error.\n");
		return 2;
	}

	if(fgets(basedir, DATA_SIZE , pf)!=NULL){
		basedir_end = strrchr(basedir, '/');
		if(basedir_end) *basedir_end = '\0';
		//fprintf(stderr, "Accelerometer: %s\n", basedir);
	}
	else{
		fprintf(stderr, "Unable to find any accelerometer.\n");
		return 1;
	}
	pclose(pf);

	bdopen("in_accel_scale", 0);
	double scale = atof(content);

	FILE *dev_accel_y = bdopen("in_accel_y_raw", 1);
#if N_STATE == 4
	FILE *dev_accel_x = bdopen("in_accel_x_raw", 1);
#else
	FILE *dev_accel_x = NULL;
#endif

    printf("%d", orientation(dev_accel_y, dev_accel_x, scale));

    if (argc>1 && strcmp(argv[1],"-l")==0) {
		while(1){
			fseek(dev_accel_y, 0, SEEK_SET);
			fgets(content, DATA_SIZE, dev_accel_y);
			accel_y = atof(content) * scale;
	#if N_STATE == 4
			fseek(dev_accel_x, 0, SEEK_SET);
			fgets(content, DATA_SIZE, dev_accel_x);
			accel_x = atof(content) * scale;
	#endif
			if(rotation_changed(dev_accel_y, dev_accel_x, scale))
				rotate_screen();
			sleep(2);
		}
    }

	return 0;
}
