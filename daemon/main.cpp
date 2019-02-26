/*
 * rpitx_alsa module
 * Author: Kevin "felixzero" Guilloy, F4VQG
 
 * Very basic daemon that reads /dev/rpitxin as a source of I-Q samples
 * and send it to librpitx.
 * Largely taken from rpitx's sendiq.cpp file.
 *
 * This file is licensed under GNU GPL v3.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <complex>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <librpitx.h>

#define IQBURST 4000
#define INPUT_FILENAME "/dev/rpitxin"
#define SYSFS_PATH "/sys/devices/rpitx"

static bool running = true, requiresReset = false;
static float SetFrequency;
static float SampleRate = 44100;
static int Harmonic;

static bool read_frequency_and_harmonic();
static int read_sys_variable(const char *name);
static void terminate(int num);

int main(int argc, char **argv)
{
    int iqfile = open(INPUT_FILENAME, O_RDONLY);
    if (iqfile < 0) {
        printf("Cannot open input. Are you root?\n");
        exit(-1);
    }
    
	 for (int i = 0; i < 64; i++) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminate;
        sigaction(i, &sa, NULL);
    }

    int FifoSize = IQBURST*4;

    read_frequency_and_harmonic();
    
    std::complex<float> CIQBuffer[IQBURST];
    while (running) {
        iqdmasync iqtest(SetFrequency, SampleRate, 14, FifoSize, MODE_IQ);
        iqtest.SetPLLMasterLoop(3, 4, 0);
        requiresReset = false;        

        while (!requiresReset && running) {
            int CplxSampleNumber = 0;
            static short IQBuffer[IQBURST * 2];
            int nbread = read(iqfile, IQBuffer, sizeof(short) * IQBURST) / sizeof(short);
            
            if ((nbread > 0) && running) {
                for(int i = 0; i < nbread/2; i++) {
                    CIQBuffer[CplxSampleNumber++] =
                        std::complex<float>(IQBuffer[i*2] / 32768.0,
                                            IQBuffer[i*2 + 1] / 32768.0);
                }
                
                iqtest.SetIQSamples(CIQBuffer, CplxSampleNumber, Harmonic);
            } else {
                iqtest.stop();
                iqtest.disableclk(4);
                
                if (read_frequency_and_harmonic())
                    requiresReset = true;
            }
        }
        
        iqtest.stop();
    }
    
    close(iqfile);
    
    return 0;
}

static bool read_frequency_and_harmonic()
{
    float NewFrequency, NewHarmonic;
    
    NewFrequency = (float)read_sys_variable("frequency");
    NewHarmonic = read_sys_variable("harmonic");
    
    if ((NewFrequency != SetFrequency) || (NewHarmonic != Harmonic)) {
        SetFrequency = NewFrequency;
        Harmonic = NewHarmonic;
        return true;
    }
    
    return false;
}

static int read_sys_variable(const char *name)
{
    char filepath[128];
    char buffer[128];
    int output = 0;

    sprintf(filepath, "%s/%s", SYSFS_PATH, name);
    FILE *sysfile = fopen(filepath, "r");
    if (!sysfile) {
        printf("No /sys/devices/rpitx file found.\n");
        exit(-1);
    }
    
    fread(buffer, 1, 128, sysfile);
    output = atoi(buffer);
    fclose(sysfile);
    
    return output;
}

static void terminate(int num)
{
    running = false;
    fprintf(stderr, "Caught signal - Terminating %x\n", num);
}
