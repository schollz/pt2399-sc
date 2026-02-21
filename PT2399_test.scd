(
Server.default = s = Server.local;

s.boot;

s.waitForBoot {
    var b;

    // read a whole sound into memory
    b = Buffer.read(s, Platform.resourceDir +/+ "sounds/a11wlk01.wav");

    SynthDef(\help_PlayBuf, { |out = 0, bufnum = 0|
        var src;
        var fx;

        src = PlayBuf.ar(1, bufnum, BufRateScale.kr(bufnum), loop: 1);
        fx = PT2399.ar(
            src,
            inputLevel: 0,
            dryLevel: -6,
            wetLevel: 0,
            delayTime: MouseX.kr(50,500),
            feedbackHpf: 80,
            feedback: MouseY.kr(0.0,2.0),
            c3: 100,
            c6: 100,
            brightness: 20,
            boostActivated: 0,
            passthrough: 0,
            oversample: 5
        );

        Out.ar(out, fx ! 2);
    }).play(s, [\out, 0, \bufnum, b]);

    SystemClock.sched(10.0, {
        b.free;
        s.quit;
        0.exit;
        nil;
    });
};
)
