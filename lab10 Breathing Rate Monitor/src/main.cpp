// OPEN A NEW SKETCH WINDOW IN ARDUINO
// CLICK IN THIS BOX, CTL-A, CTL-C (Copy code from text box.)
// CLICK IN SKETCH, CTL-A, CTL-V (Paste code into sketch.)

// Breathing Rate Detection System -- Final Integration
//
// Pieced together from code created by: Clark Hochgraf and David Orlicki Oct
// 18, 2018 Modified by: Mark Thompson April 2020 to integrate MATLAB read and
// Write and integrate the system

#include <Arduino.h>
#include <MsTimer2.h>
#include <SPI.h>
#include <core.hpp>
#include <fir.hpp>
#include <iir.hpp>

//**********************************************************************
void setup() {

    core::configureArduino();
    Serial.begin(115200);
    delay(5);

    // Handshake with MATLAB
    Serial.println(F("%Arduino Ready"));
    while (Serial.read() != 'g');  // spin

    MsTimer2::set(core::TSAMP_MSEC, core::ISR_Sample);  // Set sample msec, ISR name
    MsTimer2::start();                                  // start running the Timer
}

////**********************************************************************
void loop() {
    // core::setAlarm(1, true);
    // syncSample();  // Wait for the interupt when actually reading ADC data

    // Breathing Rate Detection

    // Declare variables

    float readValue, floatOutput;  //  Input data from ADC after dither averaging or from MATLAB
    long fxdInputValue;
    long eqOutput;  //  Equalizer output
    long noiseOutput; // FIR Windowed Sinc Output
    int alarmCode;  //  Alarm code

    // ******************************************************************
    //  When finding the impulse responses of the filters use this as an input
    //  Create a Delta function in time with the first sample a 1 and all others 0
    //  core::xv = (core::loopTick == 0) ? 1.0 : 0.0;  // impulse test input

    // ******************************************************************
    //  Use this when the test vector generator is used as an input
    //  core::xv = testVector();

    // ******************************************************************
    //  Read input value in ADC counts  -- Get simulated data from MATLAB
    // readValue = core::ReadFromMATLAB();

    // ******************************************************************
    //  Read input value from ADC using Dithering, and averaging
    readValue = core::analogReadDitherAve();

    //  Convert the floating point number to a fixed point value.  First
    //  scale the floating point value by a number to increase its resolution
    //  (use DATA_FXPT).  Then round the value and truncate to a fixed point
    //  INT datatype

    fxdInputValue = long(core::DATA_FXPT * readValue + 0.5);

    //  Execute the equalizer
    eqOutput = fir::EqualizerFIR( fxdInputValue, core::loopTick );

    //  Execute the noise filter.
    eqOutput = fir::NoiseFilter( eqOutput, core::loopTick );

    //  Convert the output of the equalizer by scaling floating point
    core::xv = float(eqOutput) * core::INV_FXPT;
   
    //*******************************************************************
    // Uncomment this when measuring execution times
    // core::startUsec = micros();

    // ******************************************************************
    //  Compute the output of the filter using the cascaded SOS sections
    core::yLF = iir::IIR_LPF(core::xv);
    core::yBF = iir::IIR_BPF(core::xv);
    core::yHF = iir::IIR_HPF(core::xv);

    //  Compute the latest output of the running stats for the output of the
    //  filters. Pass the entire set of output values, the latest stats structure
    //  and the reset flag
    core::statsReset = ((core::statsLF.tick % 100) == 0);
    core::getStats(core::yLF, core::statsLF, core::statsReset);
    core::stdLF = core::statsLF.stdev;

    core::statsReset = ((core::statsMF.tick % 100) == 0);
    core::getStats(core::yBF, core::statsMF, core::statsReset);
    core::stdBF = core::statsMF.stdev;

    core::statsReset = ((core::statsHF.tick % 100) == 0);
    core::getStats(core::yHF, core::statsHF, core::statsReset);
    core::stdHF = core::statsHF.stdev;

    //*******************************************************************
    // Uncomment this when measuring execution times
    // endUsec = micros();
    // execUsec = execUsec + (endUsec-startUsec);

    //  Call the alarm check function to determine what breathing range
    alarmCode = core::AlarmCheck( core::stdLF, core::stdBF, core::stdHF );

    if (alarmCode > 0) core::isToneEn = true;
    else core::isToneEn = false;

    //  Call the alarm function to turn on or off the tone
    core::setAlarm( alarmCode, core::isToneEn );

    // To print data to the serial port, use the WriteToSerial function.
    //
    //  This is a generic way to print out variable number of values
    //
    //  There are two input arguments to the function:
    //  printArray -- An array of values that are to be printed starting with the
    //  first column numValues -- An integer indicating the number of values in
    //  the array.

    core::printArray[0] = core::loopTick;  //  The sample number -- always print this
    // core::printArray[1] = eqOutput;
    // core::printArray[2] = noiseOutput;
    core::printArray[1] = core::xv;

    core::printArray[2] = core::yLF;       //  Column 3
    core::printArray[3] = core::yBF;       //  Column 4, etc...
    core::printArray[4] = core::yHF;
    core::printArray[5] = core::stdLF;
    core::printArray[6] = core::stdBF;
    core::printArray[7] = core::stdHF;
    core::printArray[8] = float(alarmCode);

    core::numValues = 9;  // The number of columns to be sent to the serial monitor (or MATLAB)

    core::WriteToSerial(core::numValues, core::printArray);  //  Write to the serial monitor (or MATLAB)

    if (core::loopTick++ >= core::NUM_SAMPLES) {
        // Serial.print("Average execution time (uSec) = ");Serial.println(
        // float(execUsec)/NUM_SAMPLES );
        while (true) {};  // spin forever
    }

}  // loop()

////**********************************************************************