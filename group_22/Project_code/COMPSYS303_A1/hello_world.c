#include <system.h>
#include <sys/alt_alarm.h>
#include <sys/alt_irq.h>
#include <altera_avalon_pio_regs.h>
#include <alt_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// UART & LCD
FILE* Storeuart;
FILE* lcd;
void lcd_set_mode(unsigned int mode);

// Timer ISRs & Declarations
alt_u32 tlc_timer_isr(void* context);
alt_u32 camera_timer_isr(void* context);
alt_u32 timer_isr_function(void* context);
static alt_alarm camera_timer;
static alt_alarm timer;
static alt_alarm tlc_timer;

// Other Functions
void PedestrianReset(void);
void timeout_data_handler(void);

// GLOBAL VARIABLES
static volatile unsigned int mode = 0;
static volatile unsigned int ModeChange = 0;
static volatile int timerCount = 0;

// Traffic light timeouts
static unsigned int timeout[6] = { 500, 6000, 2000, 500, 6000, 2000 };
enum traffic_states {
	RR_NS, GR_NS, YR_NS, RR_EW, RG_EW, RY_EW
};

// LED Traffic Lights & Buttons
static unsigned char traffic_lights[9] = { 0x24, 0x22, 0x21, 0x24, 0x14, 0x0C,
		0x61, 0x8C, 0xFF };
static unsigned char Button[3] = { 0x1, 0x2, 0x4 };

// Process states: use flag as initialization state
static volatile int flag = 0;
static int CurrentState = 0;

// Variables
static volatile int pedestrianNS = 0;
static volatile int pedestrianEW = 0;
static volatile int pedestrianState = 0;
static volatile int buttonValue = 1;

// Mode 3
static volatile int PacketRecieved = 0;

// Mode 4
static volatile int RedLightFlag = 0;
static volatile int timeCountMain = 0;
static volatile int carEnter = 0;
static volatile int carExit = 0;
static volatile int timerStart = 0;

////////////////////////////////////////////////////////////////////////////// Helper_functions_Start //////////////////////////////////////////////////////////////////////////////

// Function to start the traffic lights timer
void Call_Timer(int* state) {
	void* timerContext = (void*) &timerCount;
	alt_alarm_start(&tlc_timer, timeout[(*state)], tlc_timer_isr, timerContext);
}

void ChangeLED(int* state) {
	//Switch statement for which LED is on based on mode
	switch (*state) {
	case RR_NS:
		//LED bit 5,2 --> (0010 0100)
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[0]); //0x24
		printf("5,2: NS Red\n");
		break;
	case YR_NS:
		//LED bit 5,1 --> (0010 0010)
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[1]); //0x22
		printf("5,1: NS Yellow\n");
		break;
	case GR_NS:
		//LED bit 5,0 --> (0010 0001)
		if (pedestrianState == 0 || mode == 0) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[2]); //0x21
			printf("5,0: NS Green\n");
		} else { // NS Pedestrian LED bit 6,5,0 --> (0110 0001)
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[6]); //0x61
			printf("6,5,0, NS: Green_pedestrian\n");
		}
		break;
	case RR_EW:
		//LED bit 5,2 --> (0010 0100)
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[3]); //0x24
		printf("5,2: EW Red\n");
		break;
	case RY_EW:
		//LED bit 4,2 --> (0001 0100)
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[4]); //0x14
		printf("4,2: EW Yellow\n");
		break;
	case RG_EW:
		//LED bit 3,2 --> (0000 1100)
		if (pedestrianState == 0 || mode == 0) {
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[5]); //0x0C
			printf("3,2: EW Green\n");
		} else { // EW Pedestrian LED bit 2,3,7 -->(1000 1100)
			IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[7]); //0x8C
			printf("7,3,2, EW: Green_pedestrian\n");
		}
		break;
	default:
		//All LED off --> (0000 0000)
		IOWR_ALTERA_AVALON_PIO_DATA(LEDS_GREEN_BASE, traffic_lights[8]); // 0xFF
		printf("LED Off\n");
		break;
	}
}
////////////////////////////////////////////////////////////////////////////// Helper_functions_End //////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////// MODE_1_Start /////////////////////////////////////////////////////////////////////////////////////
void simple_tlc(int* state) {
	// Start initialization state once
	if (flag == 0) {
		Call_Timer(state);
		flag = 1;
		goto
		Jump;
	}

	// Set timer value based on what state it's in
	if (timerCount > 0) {
		(*state)++;
		timerCount = 0;

		if (*state == 6) {
			*state = 0;
		}
		Call_Timer(state);
		flag = 1;
	}
	Jump: ChangeLED(state);
}

alt_u32 tlc_timer_isr(void* context) {
	volatile int* tiktok = (volatile int*) context;
	*tiktok = 1;
	return 0;
}

////////////////////////////////////////////////////////////////////////////// MODE_1_End ////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////// MODE_2_Start //////////////////////////////////////////////////////////////////////////////
void pedestrian_tlc(int* state) {
	if (flag == 0) {
		// Process initialization state
		PedestrianReset();
		Call_Timer(state);
		flag = 1;
		return;
	}

	// Set timer value based on what state it's in
	if (timerCount > 0) {
		(*state)++;

		// Reset state after overflow (i.e > 5)
		if (*state == 6) {
			*state = 0;
		}

		// Pedestrian signal only affected at R-R state
		switch (*state) {
		case RR_NS:
			if (pedestrianNS == 1) {
				pedestrianState = 1;
			}
			break;
		case YR_NS:
			if (pedestrianState == 1) {
				pedestrianNS = 0;
			}
			pedestrianState = 0;
			break;
		case RR_EW:
			if (pedestrianEW == 1) {
				pedestrianState = 1;
			}
			break;
		case RY_EW:
			if (pedestrianState == 1) {
				pedestrianEW = 0;
			}
			pedestrianState = 0;
			break;
		default:
			break;
		}
		timerCount = 0;
		Call_Timer(state);
	}
	ChangeLED(state);
}

void PedestrianReset(void) {
	pedestrianState = 0;
	pedestrianEW = 0;
	pedestrianNS = 0;
	carEnter = 0;
	carExit = 0;
}

void NSEW_ped_isr(void* context, alt_u32 id) {
	int* temp = (int*) context; // need to cast the context first before using it
	(*temp) = IORD_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE);
	// clear the edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);

	// Process key0 for pedestrianNS
	if (((*temp) & Button[0]) > 0) {
		pedestrianNS = 1;
	}

	// Process key1 for pedestrianEW
	if (((*temp) & Button[1]) > 0) {
		pedestrianEW = 1;
	}

	// Process key2 for car enter and exit
	if (((*temp) & Button[2]) > 0) {
		if (carEnter == 0) {
			carEnter = 1;
		} else {
			carExit = 1;
		}
	}
}

////////////////////////////////////////////////////////////////////////////// MODE_2_End ////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////// MODE_3_Start //////////////////////////////////////////////////////////////////////////////

void configurable_tlc(int* state) {
	//Only enable switch in a safe state (i.e. R-R situations)
	// Switch 2 up for inputting new time out values and switch 2 down for activating traffic light
	if (((CurrentState == RR_NS) || (CurrentState == RR_EW))
			&& ((IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) & Button[2]) > 0)) {
		timeout_data_handler();
	} else {
		// Reuse mode 2 function
		pedestrian_tlc(state);
		PacketRecieved = 0;
	}
}

void timeout_data_handler() {

	// Local function variables
	char S[100];
	char getChar;
	char *StringSplit;
	int unsigned Index = 0;
	int unsigned char_logic = 0;
	int unsigned t_n_array[6];
	int unsigned length_t_n = 0;
	int unsigned Out_Of_Range = 0;
	int unsigned commaCount = 0;

	// Tell user when to input new data when packet not received correctly
	if (PacketRecieved != 1) {
		fprintf(Storeuart, "\rType timeout values: ");
		fflush(Storeuart);
		printf("\rType timeout values: ");
	}

	// Get data from user
	while (PacketRecieved == 0) {
		getChar = fgetc(Storeuart);
		char_logic = getChar;

		// Convey received message if 'line feed' or 'carriage return' is inputed
		if ((char_logic == 13) || (char_logic == 10)) {
			printf("\r\nPacket received: ");
			fprintf(Storeuart, "\r\nPacket received: ");
			fflush(Storeuart);

			// Six digits so only five commas are allowed
			if (commaCount == 5) {
				PacketRecieved = 1;
				// Format the input data into an array of integer type
				StringSplit = strtok(S, ",");
				while (StringSplit) {
					t_n_array[length_t_n] = atoi(StringSplit);
					length_t_n++;
					StringSplit = strtok(NULL, ",");
				}

				// Print out input data for user to see
				for (int i = 0; i < length_t_n; i++) {
					fprintf(Storeuart, "%d ", t_n_array[i]);
					printf("%d ", t_n_array[i]);
					if (((t_n_array[i] > 9999) && (t_n_array[i] < 0))) {
						Out_Of_Range = 1;
						goto invalid;
					}
				}
				fflush(Storeuart);

				// Input data can only be positive and 4 digits only
				if (Out_Of_Range == 0) {
					for (int i = 0; i < 6; i++) {
						timeout[i] = t_n_array[i];
					}
					goto JumpToEnd;
				}

			} else {
				invalid: PacketRecieved = 0;
				fprintf(Storeuart, "\r\nInvalid packet received\r\n");
				fflush(Storeuart);
				printf("\r\nInvalid packet received\r\n");
				memset(S, 0, sizeof(S)); // clear the string
				goto JumpToEnd;
			}

		}
		S[Index] = getChar;
		printf("%c", S[Index]);

		// Check for comma
		if (S[Index] == 44) {
			commaCount++;
			// Check for for anything that is not a digit (i.e letters)
		} else if ((S[Index] > 57) || (S[Index] < 48)) {
			Out_Of_Range = 1;
			goto invalid;
		}
		Index++;
	}

	JumpToEnd: memset(S, 0, sizeof(S)); // clear the string

}
////////////////////////////////////////////////////////////////////////////// MODE_3_End ////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////// MODE_4_Start //////////////////////////////////////////////////////////////////////////////

alt_u32 camera_timer_isr(void* context) {
	volatile int* tiktok = (volatile int*) context;
	*tiktok = 1;
	return 0;
}

alt_u32 timer_isr_function(void* context) {
	int *timeCount = (int*) context;
	(*timeCount)++;
	return 1000;
}

void camera_tlc(int* state) {
	if (timerStart == 0) {
		if (carEnter == 1) {
			RedLightFlag = 0;
			// key2 activation in a red light state
			if ((*state) == RR_NS || (*state) == RR_EW) {
				fprintf(Storeuart, "Camera Activated and Snapshot Taken\r");
				fflush(Storeuart);
			}
			// key2 activation in yellow light state
			else if ((*state) == YR_NS || (*state) == RY_EW) {
				fprintf(Storeuart, "Camera Activated\r");
				fflush(Storeuart);
				void* timerContext = (void*) &RedLightFlag;
				// Activate timer to count to 2s for yellow light duration
				alt_alarm_start(&camera_timer, 2000, camera_timer_isr,
						timerContext);
			}
			timeCountMain = 0;
			void* timerContextMain = (void*) &timeCountMain;
			// Count timer in seconds until key2 is pressed the second time
			alt_alarm_start(&timer, 1000, timer_isr_function, timerContextMain);
			timerStart = 1;
		}
	}

	// Print appropriate statements for when key2 is pressed
	if ((carEnter == 1)) {
		if (carExit == 1) {
			carEnter = 0;
			if (RedLightFlag == 1) {
				fprintf(Storeuart, "Snapshot Taken\r");
				fprintf(Storeuart,
						"Vehicle Left and Time in Intersection: %d sec\r\n",
						timeCountMain);
				fflush(Storeuart);
				alt_alarm_stop(&timer);
				timerStart = 0;
				carExit = 0;
			} else {
				fprintf(Storeuart,
						"Vehicle Left and Time in Intersection: %d sec\r\n",
						timeCountMain);
				fflush(Storeuart);
				alt_alarm_stop(&timer);
				timerStart = 0;
				carExit = 0;
			}
		}
	}
	// Reuse mode 3 function
	configurable_tlc(state);
}
////////////////////////////////////////////////////////////////////////////// MODE_4_End ////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////// Interrupts&Buttons&LCD_Start //////////////////////////////////////////////////////////////////////////////
void init_buttons_pio(void) {
	void* context = (void*) &buttonValue;
	// clears the edge capture register
	IOWR_ALTERA_AVALON_PIO_EDGE_CAP(BUTTONS_BASE, 0);
	// enable interrupts for all buttons
	IOWR_ALTERA_AVALON_PIO_IRQ_MASK(BUTTONS_BASE, 0x7);
	// register the ISR
	alt_irq_register(BUTTONS_IRQ, context, NSEW_ped_isr);
}

void handle_mode_button() {
	// Only SW1 and SW0 work for mode changes
	ModeChange = IORD_ALTERA_AVALON_PIO_DATA(SWITCHES_BASE) & 0x03;
	printf("Mode: %d: ", ModeChange + 1);

	//Check for mode change
	if (ModeChange != mode) {
		// Change mode when safe (i.e red light states) and restart the variables
		if ((CurrentState == RR_NS) | (CurrentState == RR_EW)) {
			CurrentState = 0;
			flag = 0;
			mode = ModeChange;
			lcd_set_mode(mode);
			alt_alarm_stop(&tlc_timer);
			alt_alarm_stop(&camera_timer);
			alt_alarm_stop(&timer);
		}
	}
}

void lcd_set_mode(unsigned int mode) {
	if (lcd != NULL) {
		fprintf(lcd, "%c%s", 27, "[2J");
		fprintf(lcd, "Mode: %d\n", mode + 1);
	}
}
////////////////////////////////////////////////////////////////////////////// Interrupts&Buttons&LCD_End ///////////////////////////////////////////////////////////////////////////////////

int main(void) {
	// LCD
	lcd = fopen(LCD_NAME, "w");
	Storeuart = fopen(UART_NAME, "w+");
	lcd_set_mode(0);

	init_buttons_pio();

	while (1) {
		handle_mode_button();

		switch (mode) {
		//Mode 1
		case 0:
			simple_tlc(&CurrentState);
			break;
			//Mode 2
		case 1:
			pedestrian_tlc(&CurrentState);
			break;
			//Mode 3
		case 2:
			configurable_tlc(&CurrentState);
			break;
			//Mode 4
		case 3:
			camera_tlc(&CurrentState);
			break;
		}
	}

	fclose(Storeuart);
	fclose(lcd);
	return 1;
}
