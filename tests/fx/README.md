# FX DSP tests

Host-side impulse-response tests for the send-FX buses (delay/chorus/reverb).

Build and run:

    g++ -O2 -I shim -I ../../sources main.cpp ../../sources/Application/Mixer/SendBus.cpp -o fxtest && ./fxtest

