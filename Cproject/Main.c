/*
From 501bb2378cc95b7ebd5941763779cca882f6bec5 Mon Sep 17 00:00:00 2001
From: Pavol Sakac <pavsa@users.noreply.github.com>
Date: Sun, 5 Mar 2023 17:52:06 +0100
Subject: [PATCH] hackrf_sweep to library conversion

---
 host/hackrf-tools/src/hackrf_sweep.c | 247 ++++++++++++++++++++++++++-
 1 file changed, 240 insertions(+), 7 deletions(-)

diff --git a/host/hackrf-tools/src/hackrf_sweep.c b/host/hackrf-tools/src/hackrf_sweep.c
index a38675a1..326387d3 100644
--- a/host/hackrf-tools/src/hackrf_sweep.c
+++ b/host/hackrf-tools/src/hackrf_sweep.c
@@ -20,7 +20,7 @@
  * the Free Software Foundation, Inc., 51 Franklin Street,
  * Boston, MA 02110-1301, USA.
  */

#define HACKRF_SWEEP_AS_LIBRARY
 #include <hackrf.h>
 
 #include <stdio.h>
@@ -36,11 +36,16 @@
 #include <fftw3.h>
 #include <inttypes.h>
 
#ifdef HACKRF_SWEEP_AS_LIBRARY
	#include <libusb.h>
	#include "../../../../../src-c/hackrf_sweep.h"
#endif

 #define _FILE_OFFSET_BITS 64
 
 #ifndef bool
 typedef int bool;
-	#define true 1
+	#define true  1
 	#define false 0
 #endif
 
@@ -90,8 +95,8 @@ int gettimeofday(struct timeval* tv, void* ignored)
 
 #define FREQ_ONE_MHZ (1000000ull)
 
-#define FREQ_MIN_MHZ (0)    /*    0 MHz */
-#define FREQ_MAX_MHZ (7250) /* 7250 MHz */
+#define FREQ_MIN_MHZ (0)                             /*    0 MHz */
+#define FREQ_MAX_MHZ (7250)                          /* 7250 MHz */
 
 #define DEFAULT_SAMPLE_RATE_HZ            (20000000) /* 20MHz default sample rate */
 #define DEFAULT_BASEBAND_FILTER_BANDWIDTH (15000000) /* 15MHz default */
@@ -202,6 +207,18 @@ uint32_t ifft_idx = 0;
 float* pwr;
 float* window;
 
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+static void (*fft_power_callback)(
+	char /*full_sweep_done*/,
+	int /*bins*/,
+	double* /*freqStart*/,
+	float /*fft_bin_Hz*/,
+	float* /*powerdBm*/);
+static double* binsFreqStart = NULL;
+static float* binsPowerdBm = NULL;
+static int binsMaxEntries = 0;
+#endif
+
 float logPower(fftwf_complex in, float scale)
 {
 	float re = in[0] * scale;
@@ -222,6 +239,12 @@ int rx_callback(hackrf_transfer* transfer)
 	char time_str[50];
 	struct timeval usb_transfer_time;
 
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+	int binsLength = 0;
+	bool fullSweepDone = false;
+	bool stopProcessing = false;
+#endif
+
 	if (NULL == outfile) {
 		return -1;
 	}
@@ -248,6 +271,19 @@ int rx_callback(hackrf_transfer* transfer)
 			continue;
 		}
 		if (frequency == (uint64_t) (FREQ_ONE_MHZ * frequencies[0])) {
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+			fullSweepDone = true;
+			//flush previous data so that it can be displayed
+			fft_power_callback(
+				fullSweepDone,
+				binsLength,
+				binsFreqStart,
+				fft_bin_width,
+				binsPowerdBm);
+			binsLength = 0;
+			fullSweepDone = false;
+#endif
+
 			if (sweep_started) {
 				if (ifft_output) {
 					fftwf_execute(ifftwPlan);
@@ -296,6 +332,44 @@ int rx_callback(hackrf_transfer* transfer)
 			pwr[i] = logPower(fftwOut[i], 1.0f / fftSize);
 		}
 		if (binary_output) {
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+			if (!stopProcessing) {
+				for (i = 0; i < fftSize / 4; ++i) {
+					if (binsLength >= binsMaxEntries) {
+						fprintf(stderr,
+							"binsLength %d > binsMaxEntries %d\n",
+							binsLength,
+							binsMaxEntries);
+						stopProcessing = true;
+						break;
+					}
+					binsFreqStart[binsLength] =
+						frequency + i * (double) fft_bin_width;
+					binsPowerdBm[binsLength] =
+						pwr[1 + (fftSize * 5) / 8 + i];
+					binsLength++;
+				}
+			}
+			if (!stopProcessing) {
+				for (i = 0; i < fftSize / 4; ++i) {
+					if (binsLength >= binsMaxEntries) {
+						fprintf(stderr,
+							"binsLength %d > binsMaxEntries %d\n",
+							binsLength,
+							binsMaxEntries);
+						stopProcessing = true;
+						break;
+					}
+					binsFreqStart[binsLength] = frequency +
+						DEFAULT_SAMPLE_RATE_HZ / 2 +
+						i * (double) fft_bin_width;
+					binsPowerdBm[binsLength] =
+						pwr[1 + fftSize / 8 + i];
+					binsLength++;
+				}
+			}
+#else
+
 			record_length =
 				2 * sizeof(band_edge) + (fftSize / 4) * sizeof(float);
 
@@ -315,6 +389,7 @@ int rx_callback(hackrf_transfer* transfer)
 			band_edge = frequency + (DEFAULT_SAMPLE_RATE_HZ * 3) / 4;
 			fwrite(&band_edge, sizeof(band_edge), 1, outfile);
 			fwrite(&pwr[1 + fftSize / 8], sizeof(float), fftSize / 4, outfile);
+#endif
 		} else if (ifft_output) {
 			ifft_idx = (uint32_t) round(
 				(frequency - (uint64_t) (FREQ_ONE_MHZ * frequencies[0])) /
@@ -357,7 +432,8 @@ int rx_callback(hackrf_transfer* transfer)
 				time_str,
 				(long int) usb_transfer_time.tv_usec,
 				(uint64_t) (frequency + (DEFAULT_SAMPLE_RATE_HZ / 2)),
-				(uint64_t) (frequency + ((DEFAULT_SAMPLE_RATE_HZ * 3) / 4)),
+				(uint64_t) (frequency +
+					    ((DEFAULT_SAMPLE_RATE_HZ * 3) / 4)),
 				fft_bin_width,
 				fftSize);
 			for (i = 0; (fftSize / 4) > i; i++) {
@@ -366,6 +442,18 @@ int rx_callback(hackrf_transfer* transfer)
 			fprintf(outfile, "\n");
 		}
 	}
+
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+	if (binsLength > 0) {
+		fft_power_callback(
+			fullSweepDone,
+			binsLength,
+			binsFreqStart,
+			fft_bin_width,
+			binsPowerdBm);
+	}
+#endif
+
 	return 0;
 }
 
@@ -411,7 +499,34 @@ void sigint_callback_handler(int signum)
 }
 #endif
 
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+
+void hackrf_sweep_lib_stop()
+{
+	do_exit = true;
+}
+
+/**
+ * for parameters, enter 0 for default values
+ */
+int hackrf_sweep_lib_start(
+	void (*_fft_power_callback)(
+		char /*full_sweep_done*/,
+		int /*bins*/,
+		double* /*freqStart*/,
+		float /*fft_bin_Hz*/,
+		float* /*powerdBm*/),
+	uint32_t freq_min,
+	uint32_t freq_max,
+	uint32_t _fft_bin_width,
+	uint32_t num_samples,
+	unsigned int lna_gain,
+	unsigned int vga_gain,
+	unsigned int _antennaPowerEnable,
+	unsigned int _enableAntennaLNA)
+#else
 int main(int argc, char** argv)
+#endif
 {
 	int opt, i, result = 0;
 	const char* path = NULL;
@@ -421,11 +536,57 @@ int main(int argc, char** argv)
 	struct timeval time_prev;
 	float time_diff;
 	float sweep_rate = 0;
+#ifndef HACKRF_SWEEP_AS_LIBRARY
 	unsigned int lna_gain = 16, vga_gain = 20;
 	uint32_t freq_min = 0;
 	uint32_t freq_max = 6000;
+#endif
 	uint32_t requested_fft_bin_width;
 
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+	do_exit = false;
+	setbuf(stderr, NULL);
+	int step_count;
+	binary_output = true;
+	fft_bin_width = _fft_bin_width;
+	num_ranges = 0;
+
+	if (freq_min >= freq_max) {
+		fprintf(stderr,
+			"argument error: freq_max must be greater than freq_min.\n");
+		return EXIT_FAILURE;
+	}
+	if (FREQ_MAX_MHZ < freq_max) {
+		fprintf(stderr,
+			"argument error: freq_max may not be higher than %u.\n",
+			FREQ_MAX_MHZ);
+		return EXIT_FAILURE;
+	}
+	if (MAX_SWEEP_RANGES <= num_ranges) {
+		fprintf(stderr,
+			"argument error: specify a maximum of %u frequency ranges.\n",
+			MAX_SWEEP_RANGES);
+		return EXIT_FAILURE;
+	}
+	frequencies[2 * num_ranges] = (uint16_t) freq_min;
+	frequencies[2 * num_ranges + 1] = (uint16_t) freq_max;
+	num_ranges++;
+
+	num_samples = num_samples == 0 ? SAMPLES_PER_BLOCK : num_samples;
+	fftSize = DEFAULT_SAMPLE_RATE_HZ / fft_bin_width;
+
+	if (_fft_power_callback == NULL) {
+		fprintf(stderr, "argument error: callback function pointer NULL\n");
+		return EXIT_FAILURE;
+	}
+	fft_power_callback = _fft_power_callback;
+
+	antenna = true;
+	antenna_enable = !!_antennaPowerEnable;
+	amp = true;
+	amp_enable = !!_enableAntennaLNA;
+
+#else
 	while ((opt = getopt(argc, argv, "a:f:p:l:g:d:n:N:w:1BIr:h?")) != EOF) {
 		result = HACKRF_SUCCESS;
 		switch (opt) {
@@ -526,6 +687,7 @@ int main(int argc, char** argv)
 			return EXIT_FAILURE;
 		}
 	}
+#endif
 
 	if (lna_gain % 8) {
 		fprintf(stderr, "warning: lna_gain (-l) must be a multiple of 8\n");
@@ -620,6 +782,52 @@ int main(int argc, char** argv)
 	}
 #endif
 
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+	binsMaxEntries = fftSize / 4 * 2 * BLOCKS_PER_TRANSFER;
+	binsFreqStart = malloc(sizeof(*binsFreqStart) * binsMaxEntries);
+	binsPowerdBm = malloc(sizeof(*binsPowerdBm) * binsMaxEntries);
+	if (!binsFreqStart || !binsPowerdBm) {
+		if (binsFreqStart)
+			free(binsFreqStart);
+		if (binsPowerdBm)
+			free(binsPowerdBm);
+
+		fprintf(stderr, "cannot allocate memory\n");
+		return EXIT_FAILURE;
+	}
+
+	if (1) {
+		//Reset device before using it to prevent stuck hardware
+		if (libusb_init(NULL) != 0) {
+			fprintf(stderr, "cannot init libusb\n");
+			return EXIT_FAILURE;
+		}
+		uint16_t hackrf_usb_vid = 0x1d50;
+		uint16_t hackrf_one_usb_pid = 0x6089;
+		struct libusb_device_handle* usb_device = libusb_open_device_with_vid_pid(
+			NULL,
+			hackrf_usb_vid,
+			hackrf_one_usb_pid);
+		if (usb_device) {
+			libusb_reset_device(usb_device);
+			libusb_close(usb_device);
+
+			int timeout = 100;
+			while (timeout-- > 0) {
+				usb_device = libusb_open_device_with_vid_pid(
+					NULL,
+					hackrf_usb_vid,
+					hackrf_one_usb_pid);
+				if (usb_device != NULL) {
+					libusb_close(usb_device);
+					break;
+				}
+				usleep(10000);
+			}
+		}
+	}
+#endif
+
 	result = hackrf_init();
 	if (result != HACKRF_SUCCESS) {
 		fprintf(stderr,
@@ -658,16 +866,20 @@ int main(int argc, char** argv)
 		return EXIT_FAILURE;
 	}
 
-#ifdef _MSC_VER
+#ifndef HACKRF_SWEEP_AS_LIBRARY
+
+	#ifdef _MSC_VER
 	SetConsoleCtrlHandler((PHANDLER_ROUTINE) sighandler, TRUE);
-#else
+	#else
 	signal(SIGINT, &sigint_callback_handler);
 	signal(SIGILL, &sigint_callback_handler);
 	signal(SIGFPE, &sigint_callback_handler);
 	signal(SIGSEGV, &sigint_callback_handler);
 	signal(SIGTERM, &sigint_callback_handler);
 	signal(SIGABRT, &sigint_callback_handler);
+	#endif
 #endif
+
 	fprintf(stderr,
 		"call hackrf_sample_rate_set(%.03f MHz)\n",
 		((float) DEFAULT_SAMPLE_RATE_HZ / (float) FREQ_ONE_MHZ));
@@ -786,7 +998,18 @@ int main(int argc, char** argv)
 	fprintf(stderr, "Stop with Ctrl-C\n");
 	while ((hackrf_is_streaming(device) == HACKRF_TRUE) && (do_exit == false)) {
 		float time_difference;
+
+#ifndef HACKRF_SWEEP_AS_LIBRARY
 		m_sleep(50);
+#else
+		//allows fast shutdown
+		int limit = 20 * 10;
+		while (do_exit == false && limit-- > 0) {
+			usleep(10000);
+		}
+		if (do_exit)
+			break;
+#endif
 
 		gettimeofday(&time_now, NULL);
 		if (TimevalDiff(&time_now, &time_prev) >= 1.0f) {
@@ -848,7 +1071,9 @@ int main(int argc, char** argv)
 
 	fflush(outfile);
 	if ((outfile != NULL) && (outfile != stdout)) {
+#ifndef HACKRF_SWEEP_AS_LIBRARY
 		fclose(outfile);
+#endif
 		outfile = NULL;
 		fprintf(stderr, "fclose() done\n");
 	}
@@ -858,6 +1083,14 @@ int main(int argc, char** argv)
 	fftwf_free(window);
 	fftwf_free(ifftwIn);
 	fftwf_free(ifftwOut);
+
+#ifdef HACKRF_SWEEP_AS_LIBRARY
+	if (binsFreqStart)
+		free(binsFreqStart);
+	if (binsPowerdBm)
+		free(binsPowerdBm);
+#endif
+
 	fprintf(stderr, "exit\n");
 	return exit_code;
 }
-- 
2.25.1

