#include "shears_gpsStorage.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"

#define GPS_BUF_SIZE 512

static const char* TAG = "gps_storage";

static void writeCsvHeader(FILE* f)
{
	fprintf(f,
	        "utc_date, utc_time,latitude,longitude,fix_quality,"
	        "num_satellites,hdop,altitude,geoid_height\n");
}

static double nmeaToDecimal(const char* nmeaVal, char hemisphere)
{
	double val = atof(nmeaVal);
	int degrees = (int)(val / 100);
	double minutes = val - (degrees * 100);
	double decimal = degrees + minutes / 60.0;

	if (hemisphere == 'S' || hemisphere == 'W') {
		decimal *= -1;
	}

	return decimal;
}

static void formatUtcTime(const char* nmeaUtc, char* out, size_t outLen)
{
	if (!nmeaUtc || strlen(nmeaUtc) < 6) {
		snprintf(out, outLen, "--:--:--");
		return;
	}

	char hh[3] = { nmeaUtc[0], nmeaUtc[1], '\0' };
	char mm[3] = { nmeaUtc[2], nmeaUtc[3], '\0' };
	char ss[16];

	snprintf(ss, sizeof(ss), "%s", nmeaUtc + 4);
	snprintf(out, outLen, "%s:%s:%s", hh, mm, ss);
}

bool shearsGpsStorageEnsureCsvExists(const char* csvPath)
{
	FILE* f = fopen(csvPath, "r");
	if (f) {
		fclose(f);
		return true;
	}

	f = fopen(csvPath, "w");
	if (!f) {
		ESP_LOGE(TAG, "Failed to create %s", csvPath);
		return false;
	}

	writeCsvHeader(f);
	fclose(f);

	ESP_LOGI(TAG, "Created %s", csvPath);
	return true;
}

bool shearsGpsStorageClearCsv(const char* csvPath)
{
	FILE* f = fopen(csvPath, "w");
	if (!f) {
		ESP_LOGE(TAG, "Could not clear %s", csvPath);
		return false;
	}

	writeCsvHeader(f);
	fclose(f);

	ESP_LOGW(TAG, "Cleared %s", csvPath);
	return true;
}

bool shearsGpsStorageAppendGngga(const char* csvPath, const char* nmea, const char* utcDate)
{
	if (!nmea || strncmp(nmea, "$GNGGA,", 7) != 0) {
		return false;
	}

	char copy[GPS_BUF_SIZE];
	strncpy(copy, nmea, sizeof(copy));
	copy[sizeof(copy) - 1] = '\0';

	char* tokens[20];
	int i = 0;
	char* tok = strtok(copy, ",");

	while (tok && i < 20) {
		tokens[i++] = tok;
		tok = strtok(NULL, ",");
	}

	if (i < 12) {
		ESP_LOGW(TAG, "GNGGA too short (i=%d)", i);
		return false;
	}

	const char* utcTime = tokens[1];
	double lat = nmeaToDecimal(tokens[2], tokens[3][0]);
	double lon = nmeaToDecimal(tokens[4], tokens[5][0]);
	int fix = atoi(tokens[6]);
	int sats = atoi(tokens[7]);
	double hdop = atof(tokens[8]);
	double alt = atof(tokens[9]);
	double geoid = atof(tokens[11]);

	FILE* f = fopen(csvPath, "a");
	if (!f) {
		ESP_LOGE(TAG, "Append open failed: %s", csvPath);
		return false;
	}

	char dateFMT[11] = "MM-DD-YYYY";
	if (utcDate && strlen(utcDate) == 6) {
		snprintf(dateFMT, sizeof(dateFMT), "20%c%c-%c%c-%c%c",
				utcDate[4], utcDate[5], 
				utcDate[2], utcDate[3], 
				utcDate[0], utcDate[1]);
	}

	fprintf(f, "%s,%s,%.7f,%.7f,%d,%d,%.1f,%.3f,%.3f\n",
	        dateFMT, utcTime, lat, lon, fix, sats, hdop, alt, geoid);
	fclose(f);

	ESP_LOGI(TAG, "Saved: date=%s time=%s lat=%.7f lon=%.7f", dateFMT, utcTime, lat, lon);
	return true;
}

void shearsGpsStoragePrintNewest(const char* csvPath, int maxLines)
{
	FILE* f = fopen(csvPath, "r");
	if (!f) {
		ESP_LOGE(TAG, "Could not open %s", csvPath);
		return;
	}

	if (maxLines <= 0) {
		maxLines = 5;
	}

	enum { LINE_BUF = 256 };
	char header[LINE_BUF];

	if (!fgets(header, sizeof(header), f)) {
		fclose(f);
		ESP_LOGW(TAG, "CSV empty");
		return;
	}

	char (*lines)[LINE_BUF] = calloc((size_t)maxLines, LINE_BUF);
	int* lineNums = calloc((size_t)maxLines, sizeof(int));
	if (!lines || !lineNums) {
		fclose(f);
		free(lines);
		free(lineNums);
		ESP_LOGE(TAG, "OOM");
		return;
	}

	int dataLinesSeen = 0;
	char buffer[LINE_BUF];

	while (fgets(buffer, sizeof(buffer), f)) {
		int idx = dataLinesSeen % maxLines;

		strncpy(lines[idx], buffer, LINE_BUF - 1);
		lines[idx][LINE_BUF - 1] = '\0';

		lineNums[idx] = dataLinesSeen + 1;
		dataLinesSeen++;
	}

	fclose(f);

	ESP_LOGI(TAG, "---- Newest GPS Data Points ----");

	if (dataLinesSeen == 0) {
		ESP_LOGI(TAG, "(no data rows yet)");
		free(lines);
		free(lineNums);
		return;
	}

	printf("\n");
	printf("line | %-10s | %-10s | %-11s | %-12s | %-3s | %-4s | %-4s | %-8s | %-11s\n",
		"utc_date", "utc_time", "latitude", "longitude", "fix", "sats", "hdop", "alt(m)", "geoid(m)");
	printf("-----+------------+------------+-------------+--------------+-----+------+------+-"
		"----------+------------\n");
	int linesToPrint = (dataLinesSeen < maxLines) ? dataLinesSeen : maxLines;
	int start = (dataLinesSeen >= maxLines) ? (dataLinesSeen % maxLines) : 0;

	for (int i = 0; i < linesToPrint; i++) {
		int idx = (start + i) % maxLines;

		char row[LINE_BUF];
		strncpy(row, lines[idx], sizeof(row) - 1);
		row[sizeof(row) - 1] = '\0';

		size_t len = strlen(row);
		if (len > 0 && row[len - 1] == '\n') {
			row[len - 1] = '\0';
		}

		char* tokens[9] = {0};
		int t = 0;

		char* tok = strtok(row, ",");
		while (tok && t < 9) {
			tokens[t++] = tok;
			tok = strtok(NULL, ",");
		}

		if (t < 9) {
			printf("%4d | (malformed) %s\n", lineNums[idx], lines[idx]);
			continue;
		}

		char timeFmt[16];
		formatUtcTime(tokens[1], timeFmt, sizeof(timeFmt));

		printf("%4d | %-10s | %-10s | %11s | %12s | %3s | %4s | %4s | %8s | %11s\n",
		       lineNums[idx],
			   tokens[0],
		       timeFmt,
		       tokens[1],
		       tokens[2],
		       tokens[3],
		       tokens[4],
		       tokens[5],
		       tokens[6],
		       tokens[7]);
	}

	printf("\n");

	free(lines);
	free(lineNums);
}