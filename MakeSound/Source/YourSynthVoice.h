/*
  ==============================================================================

    YourSynthVoice.h
    Created: 7 Mar 2020 4:27:57pm

    Contains classes MySynthSound, MySynthVoice

    Inherits from synthesiser class

    Requires <JuceHeader.h>
    Requires "Oscillator.h" to generate oscillators 
    Requires "KeySignatures.h" to set the key of the chords
    Requires "Delay.h" for delays

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Oscillator.h"
#include "KeySignatures.h"
#include "Delay.h"

// ===========================
// ===========================
// SOUND
class MySynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int noteIn) override 
    { 
        if (noteIn <= 35) 
            return true;
        else
            return false;
    }
    //--------------------------------------------------------------------------
    bool appliesToChannel(int) override { return true; }
};

class MySynthVoice : public juce::SynthesiserVoice
{
public:
    MySynthVoice() {}
    void init(float sampleRate)
    {
        sr = sampleRate;

        // set sample rate for oscillators and envelop
        triOsc.setSampleRate(sampleRate);
        sineOsc.setSampleRate(sampleRate);
        sqOsc.setSampleRate(sampleRate);
        detuneOsc.setSampleRate(sampleRate);
        env.setSampleRate(sampleRate);

        delay.setSize(sampleRate);
        delay.setDelayTime(0.5 * sampleRate);

        key.setOscillatorParams(sampleRate);
        key.generateNotesForModes(4);

        // smooth value setting for volume
        smoothVolume.reset(sampleRate, 1.0f);
        smoothVolume.setCurrentAndTargetValue(0.0);

        envParams.attack = 2.0f; // fade in
        envParams.decay = 0.75f;  // fade down to sustain level
        envParams.sustain = 0.25f; // vol level
        envParams.release = 3.0f; // fade out
        env.setParameters(envParams); // set the envelop parameters
        
    }

    /**
    * set pointer to volume parameter
    * 
    * @param volumeInput pointer to volume input
    */
    void setVolumePointer(std::atomic<float>* volumeInput)
    {
        volume = volumeInput;
    }


    void setMode(int _baseNote, int _mode)
    {
        baseNote = _baseNote;
        mode = _mode;
    }

    //--------------------------------------------------------------------------
    /**
     Called when a note starts

     @param midiNoteNumber
     @param velocity
     @param SynthesiserSound unused variable
     @param / unused variable
     */
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        // set bool values
        playing = true;
        ending = false;

        float vel = (float) velocity * 20.0;
        velocityDetune = (float) exp(0.2 * vel) / (float) exp(4.0) * 20.0; // set detune paramter

        DBG(midiNoteNumber);
        delay.setDelayTime(velocity * sr);              // set delay time according to velocity 
        setEnv(velocity, midiNoteNumber);               // set envelope according to velocity and midi
        setFrequencyVelocity(velocity, midiNoteNumber); // set frequency according to velocity and midi
        
        // set freqeuncies 
        triOsc.setFrequency(freq);                      
        sineOsc.setFrequency(freq);
        sqOsc.setFrequency(freq);

        // reset envelopes
        env.reset(); 
        env.noteOn();

    }

    /**
    * set envelope parameters based on velocity and midi
    * 
    * @param velocity (float)
    * @param midiNoteNumber (int) 
    */
    void setEnv(float velocity, int midiNoteNumber)
    {
        if (midiNoteNumber > 23) //  if midi > 23
        {
            // randomise the combination of oscillators
            triVolume = random.nextInt(2);
            sineVolume = random.nextInt(2);
            sqVolume = random.nextInt(2);
            oscCount = triVolume + sineVolume + sqVolume;
            if (oscCount == 0)
            {
                triVolume = 1;
                oscCount = 1;
            }
            DBG(oscCount);
            DBG(sqVolume);

            if (velocity > 0.6f) // shorter attack, sustain and release if velocity is > 0.6f
            {
                envParams.attack = juce::jmap(random.nextFloat(), 0.01f, 0.05f);  // fade in
                envParams.sustain = juce::jmap(random.nextFloat(), 0.01f, 0.05f); // vol level
                envParams.release = juce::jmap(random.nextFloat(), 0.25f, 0.75f); // fade out
                env.setParameters(envParams);                                     // set the envelop parameters

                float a = envParams.attack;
                DBG(a);
            }

            else
            {
                float envelopeRelease = velocity * 5.0f;
                envParams.release = envelopeRelease;                            // fade out
                env.setParameters(envParams);                                   // set the envelop parameters
            }
        }

        else 
        {
            clearCurrentNote(); // let play

            // only play triOsc if midi <= 23
            triVolume = 1;
            sineVolume = 0;
            sqVolume = 0;
            oscCount = triVolume + sineVolume + sqVolume;

            float envelopeRelease = velocity * 12.0f;
            envParams.release = envelopeRelease;                            // fade out
            env.setParameters(envParams);                                   // set the envelop parameters

        }
    }

    /** 
    * frequency based on velocity and midi
    *
    * @param velocity (float)
    * @param midiNoteNumber (int)
    */
    void setFrequencyVelocity(float velocity, int midiNoteNumber)
    {
        freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber + 24);
        int scaledVelocity = ceil(velocity * 3.0) + 1;
        int addOctave = 12 * (random.nextInt(2) + scaledVelocity);

        if (midiNoteNumber > 23)
        {
            key.changeMode(baseNote, mode, 4);
            float midiFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            std::vector<float> possibleNotes = key.getNoteVector();

            // if the midi is within the range of the mode
            if (std::find(possibleNotes.begin(), possibleNotes.end(), midiFreq) != possibleNotes.end())
            {
                freq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber + addOctave);

            }

            // else pick a random note from the mode
            else
            {
                int pickNote = random.nextInt(7) + 7 * (random.nextInt(3));
                freq = key.getNotes(pickNote);

            }
        }
    }

    //--------------------------------------------------------------------------
    /// Called when a MIDI noteOff message is received
    /**
     What should be done when a note stops

     @param velocity (unused variable)
     @param allowTailOff bool to decie if the should be any volume decay
     */
    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff) // allow slow release of note
        {
            env.noteOff();
            ending = true;
        }
        else // shut off note
        {
            clearCurrentNote();
            playing = false;
        }
    }

    //--------------------------------------------------------------------------
    /**
     The Main DSP Block: Put your DSP code in here

     If the sound that the voice is playing finishes during the course of this rendered block, it must call clearCurrentNote(), to tell the synthesiser that it has finished

     @param outputBuffer pointer to output
     @param startSample position of first sample in buffer
     @param numSamples number of smaples in output buffer
     */
    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override
    {

        if (playing) // check to see if this voice should be playing
        {
            smoothVolume.setTargetValue(*volume); // smooth value
            float gainVal = smoothVolume.getNextValue();

            detuneOsc.setFrequency(freq - velocityDetune); // set the detune amount

            // DSP loop (from startSample up to startSample + numSamples)
            for (int sampleIndex = startSample; sampleIndex < (startSample + numSamples); sampleIndex++)
            {
                float envVal = env.getNextSample();
                float delayEnv = delay.process(envVal);
                float totalOscs; // = (triOsc.process() * triVolume + sineOsc.process() * sineVolume + sqOsc.process() * sqVolume) / oscCount;
                totalOscs = (triOsc.process() * triVolume + sineOsc.process() * sineVolume + sqOsc.process() * sqVolume / 2) / oscCount;
                totalOscs = (totalOscs + detuneOsc.process()) / 2;
                float delayOutput = delay.process(totalOscs) * 0.5;
                float currentSample = (totalOscs * envVal + delayOutput * delayEnv); // apply envelop to oscillator 

                // for each channel, write the currentSample float to the output
                for (int chan = 0; chan < outputBuffer.getNumChannels(); chan++)
                {                    
                    outputBuffer.addSample(chan, sampleIndex, gainVal * currentSample);
                }

                if (ending)
                {
                    if (delayEnv < 0.0001 && envVal < 0.0001) // turn off sound when both envelopes are < 0.0001
                    {
                        playing = false;
                    }
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    void pitchWheelMoved(int) override {}

    //--------------------------------------------------------------------------
    void controllerMoved(int, int) override {}

    //--------------------------------------------------------------------------
    /**
     Can this voice play a sound. I wouldn't worry about this for the time being

     @param sound a juce::SynthesiserSound* base class pointer
     @return sound cast as a pointer to an instance of YourSynthSound
     */
    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MySynthSound*> (sound) != nullptr;
    }
    //--------------------------------------------------------------------------
private:
    //--------------------------------------------------------------------------
    bool playing = false;               // set default value for playing to be false
    bool ending = false;                // bool to determine the moment the note is released
    float sr;                           // sample rate
    float freq;                         // frequency

    juce::ADSR env;                     // envelope for synthesiser
    juce::ADSR::Parameters envParams;   // create insatnce of ADSR envelop

    // Oscillators
    TriOsc triOsc;
    SineOsc sineOsc;
    SquareOsc sqOsc;
    TriOsc detuneOsc;

    // volume for each oscillator
    int triVolume;
    int sineVolume;
    int sqVolume;
    int oscCount;

    float velocityDetune;                    // detune oscillator velocity
    std::atomic<float>* volume;              // volume parameter
    juce::SmoothedValue<float> smoothVolume; // smooth value
    
    // variables for setting chords
    KeySignatures key;
    int mode = 0;      // default value
    int baseNote = 24; // default value

    Delay delay;            // effects
    juce::Random random;    // to generate random values

};
