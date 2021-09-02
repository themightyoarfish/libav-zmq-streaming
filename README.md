# libav-zmq-streaming
Using libav to send video stream over zmq with h264.

# Dependencies
Unfortunately this is a bit shitty because there is no cmake support for libav. I pilfered a cmake script for finding ffmpeg from VTK (i think), 
but they do not include a bunch of library dependencies (not sure if forgotten or not necessary for certain versions of libav), so I hacked them in there until it worked.

Basically you need most (but strangely not all) thats outlined here: https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu

# Current state
- You can send a spinnaker cam over zmq, but the publisher is much faster than the subscriber so frames are queueing up and we get a delay (i think the fps on the receiver are just lower than on the sender). We can set `sendhwm` 
to limit output at the expense of decoding errors on the client, but this seems to help only a bit. 
- Despite the `zerolatency` tuning option, some delay appears to be incurred by buffering in the encoder
- Decoding on macos uses all available CPU. Encoding on the supermicro however is not as expensive. My guess would be that the linux machine can use hardware encoding and my macbook somehow cant and must do it in software.
