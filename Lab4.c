////////////////////////////////////////////////////////////////////////////////
//
// Lab4.c
// Author: Nathan Hartman
// Program to log objects entering/leaving room
// Inputs:  none
// Outputs: Logging of system components as well as periodic statistics output
// Operation: Monitor lasers
//
// Change History:
// December 3rd ---- Added comments

#include "gpiolib_addr.h"
#include "gpiolib_reg.h"
#include "gpiolib_reg.c"

#include <stdint.h> 
#include <stdio.h> //for the printf() function
#include <fcntl.h> 
#include <linux/watchdog.h > //needed for the watchdog specific constants
#include <unistd.h> //needed for sleep
#include <sys/ioctl.h> //needed for the ioctl function
#include <stdlib.h> //for atoi
#include <time.h> //for time_t and the currTime() function
#include <sys/time.h> 
#include <string.h>

#define diode1Pin 4
#define diode2Pin 5
#define laserOne 1
#define laserTwo 2
#define delay 500

#define PRINT_MSG(file, currTime, programName, str)\
do {\
    fprintf(file, "%s : %s : %s \n", currTime, programName, str);\
    fflush(file);\
} while (0) //Macro to write to log file

#define PRINT_STATS(file, currTime, programName, str, stat)\
do {\
    fprintf(file, "%s : %s : %s: %d \n\n", currTime, programName, str, stat);\
    fflush(file);\
} while (0) //Macro to write to log file

//Function accepts a pointer to a character array, and formats the array with the current time
void getTime(char * buffer) {

    //Create a timeval struct named tv
    struct timeval tv;

    //Create a time_t variable named curtime
    time_t curtime;

    //Get the current currTime and store it in the tv struct
    gettimeofday( & tv, NULL);

    //Set curtime to be equal to the number of seconds in tv
    curtime = tv.tv_sec;

    //This will set buffer to be equal to a string that in
    //equivalent to the current date, in a month, day, year and
    //the current currTime in 24 hour notation.
    strftime(buffer, 50, "%m-%d-%Y  %T.", localtime( & curtime)); //Gets currTime of day
}

//Function creates time_t object, and returns it with the current time in seconds
time_t compareTime() {

    struct timeval tv;
    time_t curtime;
    gettimeofday( & tv, NULL);
    curtime = tv.tv_sec;

    return curtime;
}

//Accepts character array of file path, returns 0 or 1 depending on whether or not it could be opened
int pathCheck(const char * filename) {
    FILE * file = fopen(filename, "a");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

//Reads the specified config file, and sets watchdog timer, log path, and stats path values
void readConfig(FILE * configFile, int * timeout, char * logFileName, char * statsFileName) { //Reads config file	//Loop counter

    //A char array to act as a buffer for the file
    char buffer[255];
    * timeout = 0;

    // This is a variable used to track which input we are currently looking
    // for (timeout, logFileName or numBlinks)

    enum readType {
        UNKNOWN,
        TIMEOUT,
        LOGFILE,
        STATSFILE
    };
    enum readType prop = UNKNOWN;

    int gotTimeout = 0;
    int gotLogPath = 0;
    int gotStatsPath = 0;

    while (fgets(buffer, 255, configFile) != NULL) { //Reads until end of file

        if ((buffer[0] >= 'a' && buffer[0] <= 'z') || (buffer[0] >= 'A' && buffer[0] <= 'Z')) { //Reading a property

            int i = 0;
            char compare[50];

            for (int i = 0; i < 50; i++) { //Clears character array
                compare[i] = 0;
            }

            while (buffer[i] != ' ') { //Reads until a space
                compare[i] = buffer[i];
                i++;
            }

            if (!strcmp(compare, "WATCHDOG_TIMEOUT")) { //Compares string to set property names
                prop = TIMEOUT;
                gotTimeout = 1;
            } else if (!strcmp(compare, "LOGFILE")) {
                prop = LOGFILE;
                gotLogPath = 1;
            } else if (!strcmp(compare, "STATSFILE")) {
                prop = STATSFILE;
                gotStatsPath = 1;
            }

            if (prop == TIMEOUT) {
                while (buffer[i] != 0) {
                    if (buffer[i] >= '0' && buffer[i] <= '9') {
                        * timeout = ( * timeout * 10) + (buffer[i] - '0'); //Sets timeout value
                    }
                    i++;
                }
                if ( * timeout < 1 || * timeout >= 15) { //Invalid timeout value
                    * timeout = -1;
                }
                continue;
            }

            int pos = 0;
            if (prop == LOGFILE) { //Records logfile path

                while (buffer[i] != 0) {
                    if ((buffer[i] >= '0' && buffer[i] <= '9') || (buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z') || buffer[i] == '.' || buffer[i] == '/') {
                        logFileName[pos] = buffer[i];
                        pos++;
                    }
                    i++;
                }
                if (pathCheck(logFileName) == 0) { //Checks for valid file path
                    for (int i = 0; i < 50; i++) {
                        logFileName[i] = 0;
                    }
                    strncpy(logFileName, "/home/pi/Lab4/DefaultLogFile.log", 50); //Sets default path
                }

                continue;
            }

            pos = 0;
            if (prop = STATSFILE) { //Records stats file path

                while (buffer[i] != 0) {
                    if ((buffer[i] >= '0' && buffer[i] <= '9') || (buffer[i] >= 'a' && buffer[i] <= 'z') || (buffer[i] >= 'A' && buffer[i] <= 'Z') || buffer[i] == '.' || buffer[i] == '/') {
                        statsFileName[pos] = buffer[i];
                        pos++;
                    }
                    i++;
                }
                if (!pathCheck(statsFileName)) { //Checks for valid file path
                    for (int i = 0; i < 50; i++) {
                        statsFileName[i] = 0;
                    }
                    strncpy(statsFileName, "/home/pi/Lab4/DefaultStatsfile.log", 50); //Sets default path
                }

                continue;

            }
        }

    }

    if (gotTimeout == 0) { //Sets default values
        * timeout = -1;
    }
    if (gotLogPath == 0) {
        strncpy(logFileName, "/home/pi/Lab4/DefaultLogfile.log", 50);
    }
    if (gotStatsPath == 0) {
        strncpy(statsFileName, "/home/pi/Lab4/DefaultStatsfile.log", 50);
    }
}

void outputOn(GPIO_Handle gpio, int pinNumber) //Provides power to specified pin
{
    gpiolib_write_reg(gpio, GPSET(0), 1 << pinNumber);
}

void setToOutput(GPIO_Handle gpio, int pinNumber) //Changes specified pin number to output
{
    //Check that the gpio is functional
    if (gpio == NULL) {
        printf("The GPIO has not been intitialized properly \n");
        return;
    }

    if (pinNumber < 2 || pinNumber > 27) { //Check that we are trying to set a valid pin number

        printf("Not a valid pinNumer \n");
        return;
    }

    int registerNum = pinNumber / 10; //This will create a variable that has the appropriate select register number. 

    //This will create a variable that is the appropriate amount that 
    //the 1 will need to be shifted by to set the pin to be an output
    int bitShift = (pinNumber % 10) * 3;

    //Changes pin to output
    uint32_t sel_reg = gpiolib_read_reg(gpio, GPFSEL(registerNum));
    sel_reg |= 1 << bitShift;
    gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);
}

//Initialises GPIO pins and return GPIO_Handle type variable
GPIO_Handle initializeGPIO() {

    GPIO_Handle gpio;
    gpio = gpiolib_init_gpio();
    if (gpio == NULL) //Returns null if error
    {
        perror("Could not initialize GPIO");
    }
    return gpio;
}

//This function should accept the diode number (1 or 2) and output
//a 0 if the laser beam is not reaching the diode, a 1 if the laser
//beam is reaching the diode or -1 if an error occurs.
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber) {

    usleep(delay); //Sleep to reduce inconsistencies
    if (gpio == NULL) { //Checks for GPIO pin errors
        perror("GPIO pin error");
        return -1;
    }

    if (diodeNumber == laserOne) { //Checks laser1
        uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0)); //reads register
        int pin_state = (level_reg & (1 << diode1Pin)); //Compares specific pin number

        if (pin_state) {
            return 1;
        } else {
            return 0;
        }

    } else if (diodeNumber == laserTwo) { //Checks laser2
        uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0)); //reads register
        int pin_state = level_reg & (1 << diode2Pin); //Compares to specific pin number
        if (pin_state) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return -1; //Returns -1 if invalid laser number specified
    }
}

//Main logic. Monitors laser diode status, records laser breakage, object entries, and logs to stats file
int laserCount(GPIO_Handle gpio, FILE * logFile, char programName[], char statsFileName[], int watchdog) {

    char currTime[50];
    getTime(currTime);

    PRINT_MSG(logFile, currTime, programName, "Program starting");

    enum State {
        READING,
        IN1,
        OUT1,
        IN2,
        OUT2,
        IN3,
        OUT3
    }; //Creates state machine for laser checking
    enum State s = READING; //Initialises state at READING

    int in = 0; //Variables for counting ins, outs, and laser breaks
    int out = 0;
    int l1broken = 0;
    int l2broken = 0;
    int l1 = -1; //Variables for checking diode status
    int l2 = -1;
    int update = 0;

    time_t nowTime = compareTime(); //Time values for periodic logging/kicking the watchdog
    time_t watchDogTime = nowTime + 2;
    time_t statsFileTime = nowTime + 5;

    FILE * statsFile = fopen(statsFileName, "a"); //Opens stats file
    if (!statsFile) {
        perror("The stats file could not be opened at laserCount HERE");
        return -1;
    }

    PRINT_MSG(logFile, currTime, programName, "Opened stats file");

    while (1) { //Laser diode monitor loop

        getTime(currTime); //Updates time value for logging
        nowTime = compareTime(); //Time value for periodic logging/kicking watchdog

        if (nowTime > watchDogTime) { //Watchdog kick

            watchDogTime = nowTime + 2;
            ioctl(watchdog, WDIOC_KEEPALIVE, 0);
            PRINT_MSG(logFile, currTime, programName, "The Watchdog was kicked\n\n");

        }

        if (nowTime > statsFileTime) { //Stats logging

            statsFileTime = nowTime + 5;

            PRINT_STATS(statsFile, currTime, programName, "The first laser was broken", l1broken);
            PRINT_STATS(statsFile, currTime, programName, "The second laser was broken", l2broken);
            PRINT_STATS(statsFile, currTime, programName, "Objects entered room", in );
            PRINT_STATS(statsFile, currTime, programName, "Objects exited room", out);
            update = 0;

            PRINT_MSG(logFile, currTime, programName, "Recorded updated stats \n\n");

        }

        switch (s) { //Checks for state of s

        case READING: //Default monitoring state

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);

            if (l1 == 0 && l2 == 1) { // Going left to right
                usleep(delay);
                s = IN1;
                l1broken++;
                break;
            }
            if (l1 == 1 && l2 == 0) { // Going right to left
                usleep(delay);
                s = OUT1;
                l2broken++;
                break;
            }

            break;

        case IN1: //Currently breaking left diode

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 1 && l2 == 1) { //Item reversed and blocks nothing
                usleep(delay);
                s = READING;
                break;
            }
            if (l1 == 0 && l2 == 0) { //Item advances and blocks both diodes
                usleep(delay);
                s = IN2;
                l2broken++;
                break;
            }
            break;

        case OUT1: //Currently breaking right diode

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 1 && l2 == 1) { //Item reversed and blocks nothing
                usleep(delay);
                s = READING;
                break;
            }
            if (l1 == 0 && l2 == 0) { //Item advances and blocks both diodes 
                usleep(delay);
                s = OUT2;
                l1broken++;
                break;
            }
            break;

        case IN2: //Currently blocking both diodes

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 0 && l2 == 1) { //Item reverses and only blocks left diode
                usleep(delay);
                s = IN1;
                break;
            }
            if (l1 == 1 && l2 == 0) { //Item advances and only blocks right diode
                usleep(delay);
                s = IN3;
                break;
            }
            break;

        case OUT2: // Currently blocking both diodes

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 0 && l2 == 1) { //Item advances and only blocks left diode
                usleep(delay);
                s = OUT3;
                break;
            }
            if (l1 == 1 && l2 == 0) { //Item reverses and only blocks right diode
                usleep(delay);
                s = OUT1;
                break;
            }
            break;

        case IN3: //Blocking only right diode

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 1 && l2 == 1) { //Item has advanced past both diodes
                in ++;
                PRINT_MSG(logFile, currTime, programName, "Item entered room");
                s = READING;
                update = 1;
                usleep(delay);
                break;
            }
            if (l1 == 0 && l2 == 0) { //Item reverses and blocks both diodes
                usleep(delay);
                s = IN2;
                l1broken++;
                break;
            }
            break;

        case OUT3: //Blocking only left diode

            l1 = laserDiodeStatus(gpio, laserOne); //Updates diode status
            l2 = laserDiodeStatus(gpio, laserTwo);
            if (l1 == 1 && l2 == 1) { // Item has advanced past both diodes
                out++;
                s = READING;
                PRINT_MSG(logFile, currTime, programName, "Item departed room");
                usleep(delay);
                update = 1;
                break;
            }
            if (l1 == 0 && l2 == 0) { //Item reverses and blocks both diodes
                usleep(delay);
                s = OUT2;
                break;
            }
            break;
        }

    }

    fclose(statsFile);
    return 0;
}

int main(const int argc,
    const char *
        const argv[]) {

    int timeout;
    char logFileName[50];
    char statsFileName[50];
    char currTime[50];
    getTime(currTime);

    //Creates/Opens cfg file
    FILE * configFile = fopen("/home/pi/Lab4/Config.cfg", "r");
    if (!configFile) {
        perror("The config file could not be opened");
        return -1;
    }

    readConfig(configFile, & timeout, logFileName, statsFileName); //Reads config file
    fclose(configFile);

    FILE * logFile = fopen(logFileName, "a"); //Creates/opens log file
    if (!logFile) {
        perror("The log file could not be opened");
        return -1;
    }

    //Records program name
    const char * argName = argv[0];
    int namelength = 0;
    while (argName[namelength] != 0) {
        namelength++;
    }
    char programName[namelength - 2];
    for (int i = 2; i < namelength; i++) {
        programName[i] = argName[i];
    }

    //Error checking
    if (strncmp(statsFileName, "/home/pi/Lab4/DefaultStatsFile.log", 50)) {
        PRINT_MSG(logFile, currTime, programName, "Invalid log file specified. Using default DefaultLogFile.log\n");
    }
    if (strncmp(logFileName, "/home/pi/Lab4/DefaultLogFile.log", 50)) {
        PRINT_MSG(logFile, currTime, programName, "Invalid stats file specified. Using default DefaultStatsFile.log\n");
    }
    if (timeout == -1) {
        PRINT_MSG(logFile, currTime, programName, "Invalid watchdog time specified, using default of 5\n");
        timeout = 5;
    }

    PRINT_MSG(logFile, currTime, programName, "System startup succesful\n");
    PRINT_MSG(logFile, currTime, programName, "Opened log file successfully \n");
    PRINT_MSG(logFile, currTime, programName, "Read config file\n");

    PRINT_MSG(logFile, currTime, programName, "Recorded program name\n");

    GPIO_Handle gpio = initializeGPIO(); //Initializes GPIO

    if (gpio == NULL) { //Checks for initialization error
        fclose(logFile);
        return -1;
    }

    PRINT_MSG(logFile, currTime, programName, "Initialized GPIO \n");

    int watchdog; //Opens watchdog file
    if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
    	PRINT_MSG(logFile, currTime, programName, "Could not open watchdog\n");
        fclose(logFile);
        return -1;
    }

    PRINT_MSG(logFile, currTime, programName, "Opened watchdog \n");

    ioctl(watchdog, WDIOC_SETTIMEOUT, & timeout); //Sets watchdog timer value

    PRINT_MSG(logFile, currTime, programName, "Set watchdog value \n");

    setToOutput(gpio, 17); //Turns on LEDs
    outputOn(gpio, 17);
    setToOutput(gpio, 18);
    outputOn(gpio, 18);

    PRINT_MSG(logFile, currTime, programName, "intitialized specific GPIO pins \n");

    printf("Stats file path in main%s\n", statsFileName);
    laserCount(gpio, logFile, programName, statsFileName, watchdog); //Runs laser monitoring function

    fclose(logFile);
    return 0;
}