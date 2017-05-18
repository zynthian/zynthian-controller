/*
 * MIDIUSB_loop.ino
 *
 * Created: 4/6/2015 10:47:08 AM
 * Author: gurbrinder grewal
 * Modified by Arduino LLC (2015)
 */ 

#include <MIDIUSB.h>
#include <Adafruit_NeoPixel.h>

//-----------------------------------------------------------------------------
// Encoders Stuff
//-----------------------------------------------------------------------------

//#define DEBUG 1
#define NUM_LAYERS 3
#define NUM_ENCODERS 8

struct midiCC_str {
	unsigned int midiCH;
	unsigned int midiCC;
	int pos;
	int minPos;
	int maxPos;
	int lastPosSent;
};

struct encoder_str {
	unsigned int pinA;
	unsigned int pinB;
	struct midiCC_str midiCC[NUM_LAYERS];
	
	unsigned int lastEncoded;
	unsigned int activeCC;
};

struct encoder_str encoders[8]={
	(struct encoder_str) { 0, 1, {
		(struct midiCC_str) { 0, 70, 1, 0, 127 },
		(struct midiCC_str) { 0, 80, 1, 0, 127 },
		(struct midiCC_str) { 0, 90, 1, 0, 127 }
	}},
	(struct encoder_str) { 3, 2, {
		(struct midiCC_str) { 0, 71, 1, 0, 127 },
		(struct midiCC_str) { 0, 81, 1, 0, 127 },
		(struct midiCC_str) { 0, 91, 1, 0, 127 }
	}},
	(struct encoder_str) { 5, 4, {
		(struct midiCC_str) { 0, 72, 1, 0, 127 },
		(struct midiCC_str) { 0, 82, 1, 0, 127 },
		(struct midiCC_str) { 0, 92, 1, 0, 127 }
	}},
	(struct encoder_str) { 7, 6, {
		(struct midiCC_str) { 0, 73, 1, 0, 127 },
		(struct midiCC_str) { 0, 83, 1, 0, 127 },
		(struct midiCC_str) { 0, 93, 1, 0, 127 }
	}},
	(struct encoder_str) { 16, 10, {
		(struct midiCC_str) { 0, 74, 1, 0, 127 },
		(struct midiCC_str) { 0, 84, 1, 0, 127 },
		(struct midiCC_str) { 0, 94, 1, 0, 127 }
	}},
	(struct encoder_str) { 15, 14, {
		(struct midiCC_str) { 0, 75, 1, 0, 127 },
		(struct midiCC_str) { 0, 85, 1, 0, 127 },
		(struct midiCC_str) { 0, 95, 1, 0, 127 }
	}},
	(struct encoder_str) { A1, A0, {
		(struct midiCC_str) { 0, 76, 1, 0, 127 },
		(struct midiCC_str) { 0, 86, 1, 0, 127 },
		(struct midiCC_str) { 0, 96, 1, 0, 127 }
	}},
	(struct encoder_str) { A3, A2, {
		(struct midiCC_str) { 0, 77, 1, 0, 127 },
		(struct midiCC_str) { 0, 87, 1, 0, 127 },
		(struct midiCC_str) { 0, 97, 1, 0, 127 }
	}}
};

unsigned int activeCC=0;

void setupEncoder(int i) {
	struct encoder_str *enc=encoders+i;

	//Init encoder data
	enc->lastEncoded=0;
	enc->activeCC=activeCC;
	for (int i=0;i<NUM_LAYERS;i++) enc->midiCC[i].lastPosSent=0;

	//Setup GPIO Pins
  pinMode(enc->pinA,INPUT);
  digitalWrite(enc->pinA, HIGH);
  pinMode(enc->pinB,INPUT);
  digitalWrite(enc->pinB, HIGH);

  //Setup GPIO Interrupts
  //attachInterrupt(digitalPinToInterrupt(enc->pinA), doEncoder, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(enc->pinB), doEncoder, CHANGE);
}

void readEncoder(int i) {
	struct encoder_str *enc=encoders+i;
	struct midiCC_str *cc=enc->midiCC+enc->activeCC;

	unsigned int MSB = digitalRead(enc->pinA);
	unsigned int LSB = digitalRead(enc->pinB);
	unsigned int encoded = (MSB << 1) | LSB;
	unsigned int sum = (enc->lastEncoded << 2) | encoded;
	unsigned int up=(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011);
	unsigned int down=0;
	if (!up) down=(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000);
	enc->lastEncoded=encoded;
	if (cc->pos<cc->maxPos && up) cc->pos++;
	else if (cc->pos>cc->minPos && down) cc->pos--;
}

void sendEncoderMidiCC(int i) {
	struct encoder_str *enc=encoders+i;
	struct midiCC_str *cc=enc->midiCC+enc->activeCC;

	if (cc->lastPosSent!=cc->pos) {
		controlChange(cc->midiCH,cc->midiCC,cc->pos);
		cc->lastPosSent=cc->pos;
	}
}

void readEncoders() {
	for (int i=0;i<NUM_ENCODERS;i++) {
		readEncoder(i);
	}
}

void sendEncodersMidiCC() {
	for (int i=0;i<NUM_ENCODERS;i++) {
		sendEncoderMidiCC(i);
	}
}

//-----------------------------------------------------------------------------
// Switch Stuff
//-----------------------------------------------------------------------------

#define SW1_PIN 9

unsigned int lastRead=0;

void setupSwitch(int pin) {
	//Setup GPIO Pin
  pinMode(pin,INPUT);
  digitalWrite(pin, HIGH);
}

int readSwitch(int pin) {
	unsigned int v1 = digitalRead(pin);
	if (v1!=lastRead) {
		delay(10);
		unsigned int v2 = digitalRead(pin);
		if (v2==v1) {
			lastRead=v1;
			if (v1==0) return 1;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// LEDs Stuff
//-----------------------------------------------------------------------------

#define LED_PIN 8
#define LEDS_BY_ENCODER 8
#define LED_COUNT NUM_ENCODERS*LEDS_BY_ENCODER

uint32_t leds_colors[NUM_LAYERS]={ 
	0xFF0000,
	0x00FF00,
	0x0000FF
};
uint32_t leds_color=leds_colors[activeCC];

Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setEncoderLED(int encoder, int led, uint32_t rgb) {
	if (led>0) led--;
	else led=7;
	leds.setPixelColor((NUM_ENCODERS-encoder-1)*LEDS_BY_ENCODER+led, rgb);
}

void testEncoderLEDs() {
	for (int i=0;i<LEDS_BY_ENCODER;i++) {
		for (int j=0;j<NUM_ENCODERS;j++) {
			setEncoderLED(j, i, 0xFF0000);
			if (i>0) setEncoderLED(j, i-1, 0x000000);
			else setEncoderLED(j, NUM_ENCODERS-1, 0x000000);
		}
		leds.show();
		delay(50);
	}
	for (int i=0;i<LEDS_BY_ENCODER;i++) {
		for (int j=0;j<NUM_ENCODERS;j++) {
			setEncoderLED(j, i, 0x00FF00);
			if (i>0) setEncoderLED(j, i-1, 0x000000);
			else setEncoderLED(j, NUM_ENCODERS-1, 0x000000);
		}
		leds.show();
		delay(50);
	}
	for (int i=0;i<LEDS_BY_ENCODER;i++) {
		for (int j=0;j<NUM_ENCODERS;j++) {
			setEncoderLED(j, i, 0x0000FF);
			if (i>0) setEncoderLED(j, i-1, 0x000000);
			else setEncoderLED(j, NUM_ENCODERS-1, 0x000000);
		}
		leds.show();
		delay(50);
	}
}

void setEncoderLEDs(int j) {
	struct encoder_str *enc=encoders+j;
	struct midiCC_str *cc=enc->midiCC+enc->activeCC;

	int n=(LEDS_BY_ENCODER*cc->pos)>>7;
	uint32_t brmax=0x07 | ((LEDS_BY_ENCODER*cc->pos) & 0x7F);
	int i=0;
	for (;i<n;i++) {
		//uint32_t br=0x07; //*(i+1)/LEDS_BY_ENCODER;
		uint32_t br=0x00;
		setEncoderLED(j, i, ((br<<16) | (br<<8) | br) & leds_color);
	}
	if (i<LEDS_BY_ENCODER) {
		setEncoderLED(j, i++, ((brmax<<16) | (brmax<<8) | brmax) & leds_color);
		for (;i<LEDS_BY_ENCODER;i++) {
			setEncoderLED(j, i, 0x000000);
		}
	}
}

void setEncodersLEDs() {
	for (int j=0;j<NUM_ENCODERS;j++) {
		setEncoderLEDs(j);
	}
}

//-----------------------------------------------------------------------------
// MIDI Stuff
//-----------------------------------------------------------------------------

// First parameter is the event type (0x09 = note on, 0x08 = note off).
// Second parameter is note-on/note-off, combined with the channel.
// Channel can be anything between 0-15. Typically reported to the user as 1-16.
// Third parameter is the note number (48 = middle C).
// Fourth parameter is the velocity (64 = normal, 127 = fastest).

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, byte(0x90 | channel), pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, byte(0x80 | channel), pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

// First parameter is the event type (0x0B = control change).
// Second parameter is the event type, combined with the channel.
// Third parameter is the control number number (0-119).
// Fourth parameter is the control value (0-127).

void controlChange(byte channel, byte control, byte value) {
	midiEventPacket_t event = {0x0B, byte(0xB0 | channel), control, value};
	MidiUSB.sendMIDI(event);
#if defined(DEBUG)
	char msg[80];
	sprintf(msg,"MIDI CONTROL (CH %d, CC %d) => %d",channel,control,value);
	Serial.println(msg);
#endif
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

void setup() {
	//Init Serial Port
	Serial.begin(115200);
	Serial.println("MIDIUSB serial console...");

	//Init Builtin Led
	pinMode(LED_BUILTIN, OUTPUT);

	//Init LEDs
	leds.begin();
	leds.setBrightness(0x0F);
	testEncoderLEDs();
	leds.setBrightness(0x3F);

	//Init Encoders
	for (int i=0;i<NUM_ENCODERS;i++) {
		setupEncoder(i);
	}

	//Init Switch
	setupSwitch(SW1_PIN);
}

//-----------------------------------------------------------------------------
// Main Loop
//-----------------------------------------------------------------------------

void loop() {

	if (readSwitch(SW1_PIN)) {
		activeCC++;
		if (activeCC>=NUM_LAYERS) activeCC=0;
		leds_color=leds_colors[activeCC];
		for (int i=0;i<NUM_ENCODERS;i++) encoders[i].activeCC=activeCC;
	}

	//Read Encoders
	readEncoders();

	//Send MIDI CC events
	sendEncodersMidiCC();

	//Flush MIDI buffer
  MidiUSB.flush();

	//Set LEDs
	setEncodersLEDs();
	leds.show();
}
