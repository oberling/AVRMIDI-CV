#include <stdio.h>
#include <portmidi.h>
#include <stdbool.h>
#include <stdint.h>

PortMidiStream* midiOutputStream = 0x00;
int numDevices = 0;
PmError err = pmNoError;
char readLineBuffer[100];

void initVariables(void);
void listAllDevices(void);
void listOutputDevices(void);
int getMappedOutputDeviceID(int);
void testit(void);
void testNote(uint8_t, uint8_t);
void testOneNote(void);
void printTestMenu(void);

void initVariables(void) {
	numDevices = Pm_CountDevices();
}

void listAllDevices(void) {
	int i=0;
	for(;i<numDevices; i++) {
		const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
		printf("%s-Device: %s (id %d of %d devices) \n", (info->input == true) ? "input" : "output", info->name, i, numDevices);
	}
}

void listOutputDevices(void) {
	int numOutputDevices = 0;
	int i=0;
	for(;i<numDevices; i++) {
		const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
		if(info->output) {
			printf(" [%d] output-Device: %s\n", numOutputDevices++, info->name);
		}
	}
}

int getMappedOutputDeviceID(int selectedDevice) {
	int numOutputDevices = 0;
	int i=0;
	if(selectedDevice > numDevices || selectedDevice < 0) return -1;
	for(; i<numDevices; i++) {
		const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
		if(info->output) {
			if(numOutputDevices++ == selectedDevice) {
				break;
			}
		}
	}
	return i;
}

void testit(void) {
	testOneNote();
}

void testNote(uint8_t note, uint8_t velocity) {
	PmEvent event;
	event.timestamp = 0;
	event.message = Pm_Message(0x94, note, velocity);
	Pm_Write(midiOutputStream, &event, 1);
	printf("check if it playes note %d (enter to continue)", note);
	fgets(readLineBuffer, sizeof(readLineBuffer), stdin);
	event.message = Pm_Message(0x94, note, 0x00);
	Pm_Write(midiOutputStream, &event, 1);
	printf("now the note should be off\n");
}

void testOneNote(void) {
	testNote(120, 77);
}

void printTestMenu(void) {
	char line[100];
	int answer = -1;
	while(answer != 0) {
		printf("\nMIDI-CV-TEST Main Menu\n");
		printf(" [0] Quit\n");
		printf(" [1] Test simple Note\n");
		printf(" [9] Test all\n");
		printf("\nplease enter a number from the above ones: ");
		fgets(line, sizeof(line), stdin);
		int sscanf_result = sscanf(line, "%d", &answer); 
		if((sscanf_result == 0) || (sscanf_result) == EOF) {
			printf("\nnot a valid input - please chose a proper number (integer) insted\n");
			answer = -1;
		}
		switch(answer) {
			case 0:
				break;
			case 1:
				testOneNote();
				break;
			case 9:
				testit();
				break;
			default:
				break;
		}
	}
}

int main(int argc, char** argv) {
	initVariables();
	err = Pm_Initialize();
//	PmDeviceID pID = Pm_GetDefaultOutputDeviceID();
//	listAllDevices();
	listOutputDevices();
	int selectedDevice=-100;
	char readLineBuffer[100];
	do {
		printf("please select an output device to test with (-1 for quit): ");
		fgets(readLineBuffer, sizeof(readLineBuffer), stdin);
		sscanf(readLineBuffer, "%d", &selectedDevice);
	} while(selectedDevice < -1 || selectedDevice > numDevices);
	if(selectedDevice == -1) {
		printf("-1 selected - exiting program\n");
		err = Pm_Terminate();
		return 0;
	}
	printf("opening device: %d\n", selectedDevice);

	err = Pm_OpenOutput(
			&midiOutputStream,
			getMappedOutputDeviceID(selectedDevice),
			NULL,
			1,
			NULL,
			NULL,
			-1
	);
	if(err != pmNoError) {
		printf("something went wrong - applying ostrich algorithm\n");
		return 0;
	}
	printTestMenu();
	err = Pm_Close(midiOutputStream);
	err = Pm_Terminate();
	return 0;
}

