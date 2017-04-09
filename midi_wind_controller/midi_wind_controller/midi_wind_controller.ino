/*
 Name:		midi_wind_controller.ino
 Created:	2017-01-30 오전 2:22:41
 Author:	Sanghoon Cho
*/

#include <MIDI.h>
#include <CapacitiveSensor.h>

#define MIDI_CHANNEL 1
#define NOTE_ON_THRESHOLD 50
#define MAX_PRESSURE 305
#define NOTE_OFF 1
#define RISE_WAIT 2
#define NOTE_ON 3
#define CC_INTERVAL 20
#define RISE_TIME 3
#define DB_DELAY 3

MIDI_CREATE_DEFAULT_INSTANCE();

byte k1;
byte k2;
byte k3;
byte k4;
byte k5;
byte k6;
byte k7;
byte k8;
byte fingering;
byte octave;
byte lastFingering = 61;
byte fingeredNote = 61;
byte activeNote = 61;
byte volume = 0;
byte pastVolume = 0;
byte state = NOTE_OFF;
int pressureValue;
int pitchDown;
int pitchUp;
int pitch;
unsigned long breath_on_time = 0L;
unsigned long ccSendTime = 0L;
unsigned long lastDebounceTime = 0L;
CapacitiveSensor cs1 = CapacitiveSensor(10, 11);
CapacitiveSensor cs2 = CapacitiveSensor(12, 13);

void setup()
{
	pinMode(2, INPUT);
	pinMode(3, INPUT);
	pinMode(4, INPUT);
	pinMode(5, INPUT);
	pinMode(6, INPUT);
	pinMode(7, INPUT);
	pinMode(8, INPUT);
	pinMode(9, INPUT);
	MIDI.begin(MIDI_CHANNEL_OMNI);
}

void loop()
{
	pressureValue = analogRead(A0);

	if (state == NOTE_OFF)
	{
		if (pressureValue > NOTE_ON_THRESHOLD)
		{
			// Value has risen above threshold. Move to the RISE_TIME
			// state. Record time and initial breath value.
			breath_on_time = millis();
			state = RISE_WAIT;  // Go to next state
		}
	}
	else if (state == RISE_WAIT)
	{
		pressureValue = analogRead(A0);
		if (pressureValue > NOTE_ON_THRESHOLD)
		{
			// Has enough time passed for us to collect our second
			// sample?
			if (millis() - breath_on_time > RISE_TIME)
			{
				// Yes, so calculate MIDI note and velocity, then send a note on event
				fingeredNote = getNote();
				activeNote = fingeredNote;
				//pitch = getPitch();
				MIDI.sendNoteOn(activeNote, 127, MIDI_CHANNEL);
				//MIDI.sendPitchBend(pitch, MIDI_CHANNEL);
				state = NOTE_ON;
			}
		}
		else
		{
			// Value fell below threshold before RISE_TIME passed. Return to
			// NOTE_OFF state (e.g. we're ignoring a short blip of breath)
			state = NOTE_OFF;
		}
	}
	else if (state == NOTE_ON)
	{
		if (pressureValue < NOTE_ON_THRESHOLD)
		{
			// Value has fallen below threshold - turn the note off
			MIDI.sendNoteOff(activeNote, 127, MIDI_CHANNEL);
			state = NOTE_OFF;
		}
		else
		{
			// Is it time to send more cc data?
			if (millis() - ccSendTime > CC_INTERVAL)
			{
				pressureValue = analogRead(A0);
				volume = (getVolume(pressureValue) + pastVolume) >> 1;
				pastVolume = volume;
				pitch = getPitch();
				MIDI.sendControlChange(7, volume, MIDI_CHANNEL);
				MIDI.sendPitchBend(pitch, MIDI_CHANNEL);
				ccSendTime = millis();
			}
		}

		fingeredNote = getNote();

		if (fingeredNote != lastFingering)
		{
			lastDebounceTime = millis();
		}

		if ((millis() - lastDebounceTime) > DB_DELAY)
		{
			if (fingeredNote != activeNote)
			{
				// Player has moved to a new fingering while still blowing.
				// Send a note off for the current node and a note on for
				// the new note.
				MIDI.sendNoteOff(activeNote, 127, MIDI_CHANNEL);
				activeNote = fingeredNote;
				//pitch = getPitch();
				MIDI.sendNoteOn(activeNote, 127, MIDI_CHANNEL);
				//MIDI.sendPitchBend(pitch, MIDI_CHANNEL);
				lastDebounceTime = millis();
			}
		}
	}
	lastFingering = fingeredNote;
}

byte getFingering()
{
	k1 = digitalRead(2);
	k2 = digitalRead(3);
	k3 = digitalRead(4);
	k4 = digitalRead(5);
	k5 = digitalRead(6);
	k6 = digitalRead(7);

	return (((k1&(!k4)&k5) | (k1&(!k5)) | (k4&k5&(!k6)) | k2 | k3) << 3)
		| ((((!k1)&k6) | (k4&k5&(!k6)) | (k1&(!k4)&(!k6)) | k2 | k3) << 2)
		| (((k4&(!k5)&k6) | (k1&(!k4)&k5) | ((!k1)&k4) | (k5&(!k6)) | (k1&(!k4)&(!k6)) | k2 | k3) << 1)
		| (((!k1)&(!k5)&(!k6)) | ((!k1)&k5&k6) | (k1&(!k5)&k6) | (k4&k5&(!k6)) | (k1&(!k4)&(!k6)) | k2 | k3);
}

byte getOctave()
{	
	k7 = digitalRead(8);
	k8 = digitalRead(9);

	return ((k7 << 1) | (((!k7)&(!k8)) | (k7&k8))) * 12 + 48;
}

byte getNote()
{
	fingering = getFingering();
	octave = getOctave();

	return ((fingering != 15) * (fingering + octave))
		| ((fingering == 15) * activeNote);
}

byte getVolume(int pressureValue)
{
	return (((pressureValue - 50) >> 31) & 0)
		| ((~((pressureValue - 50) >> 31)) & constrain(map(pressureValue, NOTE_ON_THRESHOLD, MAX_PRESSURE, 0, 127), 0, 127));
}

int getPitch()
{
	pitchDown = cs1.capacitiveSensor(30);
	pitchUp = cs2.capacitiveSensor(30);

	return constrain(map(pitchUp, 0, 1023, 0, 8191), 0, 8191) - constrain(map(pitchDown, 0, 1023, 0, 8191), 0, 8191);
}