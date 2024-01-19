
#include <FS.h> // Include the SPIFFS library

// RC-5 control change supported
/*
CC#80 FUNC
CC#81 FUNC
CC#82 FUNC
CC#83 FUNC
CC#84 FUNC
CC#85 FUNC
CC#86 FUNC
CC#87 FUNC
*/

#define MIN_CC 80
#define MAX_CC 87

// Led pin are 2 and 16
// Serial is 1 and 3
// Serial1 is 2 D4
#define BUTTON_PIN1 10 //
#define BUTTON_PIN2 D1 // 5
#define BUTTON_PIN3 D2 // 4
#define BUTTON_PIN4 D3 // 0
#define BUTTON_PIN5 D6 // 12
#define BUTTON_PIN6 D7 // 13

// MIDI OUT serial port 1
#define MIDI_OUT_Serial Serial1

// MIDI Message Types
const uint8_t NOTE_OFF = 0x80;
const uint8_t NOTE_ON = 0x90;
const uint8_t KEY_PRESSURE = 0xA0;
const uint8_t CC = 0xB0;
const uint8_t PROGRAM_CHANGE = 0xC0;
const uint8_t CHANNEL_PRESSURE = 0xD0;
const uint8_t PITCH_BEND = 0xE0;

// Custom MIDI message types for var inc/dec
const uint8_t VAR_INC = 0xF0;
const uint8_t VAR_DEC = 0xF1;

// MIDI notes
const uint8_t note_A1 = 21;
const uint8_t note_C9 = 108;

// Roland TR-08 MIDI notes
const uint8_t note_BassDrum = 36;
const uint8_t note_RimShot = 37;
const uint8_t note_SnareDrum = 38;
const uint8_t note_HandClap = 39;
const uint8_t note_ClosedHiHat = 42;
const uint8_t note_LowTom = 43;
const uint8_t note_OpenHiHat = 46;
const uint8_t note_MidTom = 47;
const uint8_t note_Cymbal = 49;
const uint8_t note_HighTom = 50;
const uint8_t note_CowBell = 56;
const uint8_t note_HighConga = 62;
const uint8_t note_MidConga = 63;
const uint8_t note_LowConga = 64;
const uint8_t note_Maracas = 70;
const uint8_t note_Claves = 75;

// MIDI velocities
const uint8_t velocity_off = 0;
const uint8_t velocity_pianississimo = 16;
const uint8_t velocity_pianissimo = 32;
const uint8_t velocity_piupiano = 48;
const uint8_t velocity_mezzopiano = 64;
const uint8_t velocity_mezzoforte = 80;
const uint8_t velocity_forte = 96;
const uint8_t velocity_fortissimo = 112;
const uint8_t velocity_fortississimo = 127;

// MIDI channel
const uint8_t channel = 1;

// MIDI tempo
const uint8_t tempo = 110;                // Tempo in beats per minute
const int eight_note = 60000 / tempo / 2; // 8th note duration in milliseconds

void sendMIDI(uint8_t messageType, uint8_t channel, uint8_t dataByte1, uint8_t dataByte2 = 0)
{

    // Adjust zero-based MIDI channel
    channel--;

    // Create MIDI status byte
    uint8_t statusByte = 0b10000000 | messageType | channel;

    // Send MIDI status and data
    MIDI_OUT_Serial.write(statusByte);
    MIDI_OUT_Serial.write(dataByte1);
    MIDI_OUT_Serial.write(dataByte2);
}

void sendCC(uint8_t ccNumber)
{
    sendMIDI(CC, 1, ccNumber, 0);
    sendMIDI(CC, 1, ccNumber, 127);
}

void sendNoteOn(uint8_t note, uint8_t velocity)
{
    sendMIDI(NOTE_ON, 1, note, velocity);
}

// Struct for MIDI command
struct MIDICommand
{
    uint8_t command = -1;
    uint8_t channel = 0;
    int data1 = 0;
    int data2 = 0;
    String toString() const
    {
        String commandString = command == NOTE_OFF ? "NOTE_OFF" : command == NOTE_ON        ? "NOTE_ON"
                                                              : command == KEY_PRESSURE     ? "KEY_PRESSURE"
                                                              : command == CC               ? "CC"
                                                              : command == PROGRAM_CHANGE   ? "PC"
                                                              : command == CHANNEL_PRESSURE ? "CHANNEL_PRESSURE"
                                                              : command == PITCH_BEND       ? "PITCH_BEND"
                                                              : command == VAR_INC          ? "VAR_INC"
                                                              : command == VAR_DEC          ? "VAR_DEC"
                                                                                            : "UNKNOWN";

        if (commandString == "UNKNOWN")
        {
            return "";
        }

        commandString += " ";
        commandString += String(channel);
        commandString += " ";
        commandString += data1 < 0 ? "VAR" : String(data1);
        commandString += " ";
        commandString += data2 < 0 ? "VAR" : String(data2);
        return commandString;
    }
};

// Struct for up to 32 midi commands
struct MIDICommandList
{
    MIDICommand commands[32];
    uint8_t count = 0;
    String toString() const
    {
        String commandString = "";
        for (int i = 0; i < count; i++)
        {
            const String currentCommandString = commands[i].toString();
            if (currentCommandString == "")
            {
                continue;
            }
            if (i > 0)
            {
                commandString += ",";
            }
            commandString += currentCommandString;
        }
        return commandString;
    }
};

struct MIDICommandFlags
{
    bool repeatOnHold = false;
    //bool disableDoublePush = false;
    String toString() const
    {
        uint8_t flags = 0;
        if (repeatOnHold)
        {
            flags |= 1;
        }
        /*
        if (disableDoublePush)
        {
            flags |= 1 << 1;
        }
        */
        return String(flags);
    }
    void fromString(String flagsString)
    {
        uint8_t flags = flagsString.toInt();
#ifdef DEBUG
        Serial.println("Flags " + String(flags));
#endif
        repeatOnHold = flags & 1;
        //disableDoublePush = flags & (1 << 1);
    }
};

struct MDIDIButtonVar
{
    int min = 0;
    int max = 127;
    int value = 0;
    int step = 1;
};

// Struct for a single button with commands for push, hold and double push
struct MIDIButtonCommands
{
    MIDICommandList push;
    MIDICommandList hold;
    MIDICommandList doublePush;
    MIDICommandFlags flags;
    MDIDIButtonVar var;
};

void saveMIDIButtonVar(const MIDIButtonCommands &button, const String &filename)
{
    File file = SPIFFS.open(filename + ".var", "w");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + ".var for writing");
    }
    else
    {
        file.print(button.var.value);
        file.println();
        file.print(button.var.min);
        file.println();
        file.print(button.var.max);
        file.println();
        file.print(button.var.step);
        file.println();
        file.close();
    }
}

// Send Midi command list
void sendMIDICommandList(const MIDICommandList &commandList, MIDIButtonCommands &button)
{
#ifdef DEBUG
    if (commandList.count == 0)
    {
        Serial.println("Send empty MIDI command list");
    }
    else
    {
        Serial.println("Send MIDI command list");
        Serial.println(commandList.toString());
    }
#endif
    for (int i = 0; i < commandList.count; i++)
    {
        MIDICommand command = commandList.commands[i];

        // Handle special commands
        if (command.command == VAR_INC)
        {
            button.var.value += button.var.step;
            if (button.var.value > button.var.max)
            {
                button.var.value = button.var.min;
            }
            // Write value to file
            saveMIDIButtonVar(button, "button" + String(i + 1));
            continue;
        }
        else if (command.command == VAR_DEC)
        {
            button.var.value -= button.var.step;
            if (button.var.value < button.var.min)
            {
                button.var.value = button.var.max;
            }
            // Write value to file
            saveMIDIButtonVar(button, "button" + String(i + 1));
            continue;
        }

        // Replace VAR with current value
        if (command.data1 == -255)
        {
            command.data1 = button.var.value;
        }
        if (command.data2 == -255)
        {
            command.data2 = button.var.value;
        }

        sendMIDI(command.command, command.channel, command.data1, command.data2);
    }
}

// Parse MIDI commands from a string containing a comma-separated list of commands
MIDICommandList parseMIDICommands(String commandString)
{

#ifdef DEBUG
    Serial.println("Parse MIDI commands '" + commandString + "'");
#endif

    MIDICommandList commandList;

    // Return if command string is empty
    if (commandString.length() == 0)
    {
        return commandList;
    }

    // Split string into commands
    int commandStart = 0;
    int commandEnd = commandString.indexOf(',');
    if (commandEnd == -1)
    {
        commandEnd = commandString.length();
    }
    while (commandEnd != -1 && commandStart < commandString.length())
    {
        // Get command string
        String command = commandString.substring(commandStart, commandEnd);

#ifdef DEBUG
        Serial.println("Parsing command " + command);
#endif

        // Split command into parts
        int partStart = 0;
        int partEnd = command.indexOf(' ');

        // Skip initial spaces
        while (partEnd == partStart)
        {
            partStart++;
            partEnd = command.indexOf(' ', partStart);
        }

        String parts[4];
        int partIndex = 0;
        while (partEnd != -1)
        {
            parts[partIndex++] = command.substring(partStart, partEnd);
            partStart = partEnd + 1;
            partEnd = command.indexOf(' ', partStart);
        }
        parts[partIndex++] = command.substring(partStart);

        // Parse command
        MIDICommand midiCommand;
        midiCommand.command = -1;

        // Parse command type
        const String commandType = parts[0];
        if (commandType == "NOTE_OFF")
        {
            midiCommand.command = NOTE_OFF;
        }
        else if (commandType == "NOTE_ON")
        {
            midiCommand.command = NOTE_ON;
        }
        else if (commandType == "KEY_PRESSURE")
        {
            midiCommand.command = KEY_PRESSURE;
        }
        else if (commandType == "CC")
        {
            midiCommand.command = CC;
        }
        else if (commandType == "PC")
        {
            midiCommand.command = PROGRAM_CHANGE;
        }
        else if (commandType == "CHANNEL_PRESSURE")
        {
            midiCommand.command = CHANNEL_PRESSURE;
        }
        else if (commandType == "PITCH_BEND")
        {
            midiCommand.command = PITCH_BEND;
        }
        else if (commandType == "VAR_INC")
        {
            midiCommand.command = VAR_INC;
        }
        else if (commandType == "VAR_DEC")
        {
            midiCommand.command = VAR_DEC;
        }
        else
        {
            midiCommand.command = -1;
#ifdef DEBUG
            Serial.println("Unknown command type " + commandType);
#endif
        }

        if (midiCommand.command != -1 && commandList.count < 32)
        {

            midiCommand.channel = parts[1].toInt();
            // Parse VAR
            if (parts[2] == "VAR")
            {
                midiCommand.data1 = -255;
            }
            else
            {
                midiCommand.data1 = parts[2].toInt();
            }
            if (parts[3] == "VAR")
            {
                midiCommand.data2 = -255;
            }
            else
            {
                midiCommand.data2 = parts[3].toInt();
            }

            // Add command to list
            commandList.commands[commandList.count++] = midiCommand;
        }

        // Move to next command
        commandStart = commandEnd + 1;
        commandEnd = commandString.indexOf(',', commandStart);
        if (commandEnd == -1)
        {
            commandEnd = commandString.length();
        }
    }

    return commandList;
}

// Writes the command list for a midi button to SPIFFS
void writeMIDICommands(String filename, MIDICommandList commands)
{
    File file = SPIFFS.open(filename, "w");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + " for writing");
        return;
    }

    const String commandString = commands.toString();
    file.print(commandString);
    file.println();
    file.close();

#ifdef DEBUG
    Serial.println("Wrote file " + filename + " with " + String(commands.count) + " commands: " + commandString);
#endif
}

// Reads the command list for a midi button from SPIFFS
MIDICommandList readMIDICommands(String filename)
{
    MIDICommandList commands;
    commands.count = 0;

    File file = SPIFFS.open(filename, "r");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + " for reading");
        return commands;
    }

    String commandString = file.readStringUntil('\r');
    file.close();

#ifdef DEBUG
    Serial.println("File " + filename + " read");
    Serial.println("Command '" + commandString + "'");
#endif

    return parseMIDICommands(commandString);
}

void initMIDIButtonVar(MIDIButtonCommands &button, String filename)
{
    File file = SPIFFS.open(filename + ".var", "r");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + ".var for reading");
    }
    else
    {
        button.var.value = file.readStringUntil('\n').toInt();
        button.var.min = file.readStringUntil('\n').toInt();
        button.var.max = file.readStringUntil('\n').toInt();
        button.var.step = file.readStringUntil('\n').toInt();
        file.close();
    }
}

// Initialize a midi button with commands for push, hold and double push from SPIFFS
void initMIDIButton(MIDIButtonCommands &button, String filename)
{
    button.push = readMIDICommands(filename + ".push");
    button.hold = readMIDICommands(filename + ".hold");
    button.doublePush = readMIDICommands(filename + ".doublepush");

    File file = SPIFFS.open(filename + ".flags", "r");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + ".flags for reading");
    }
    else
    {
        String flagsString = file.readStringUntil('\n');
        file.close();
        button.flags.fromString(flagsString);
    }

    initMIDIButtonVar(button, filename);
}

// Save the command list for a midi button to SPIFFS
void saveMIDIButton(const MIDIButtonCommands &button, String filename)
{
    writeMIDICommands(filename + ".push", button.push);
    writeMIDICommands(filename + ".hold", button.hold);
    writeMIDICommands(filename + ".doublepush", button.doublePush);

    File file = SPIFFS.open(filename + ".flags", "w");
    if (!file)
    {
        Serial.println("Failed to open file " + filename + ".flags for writing");
    }
    else
    {
        file.print(button.flags.toString());
        file.println();
        file.close();
    }

    saveMIDIButtonVar(button, filename);
}
