PT2399 : UGen {
    *ar { |in=0, inputLevel=0, dryLevel=0, wetLevel=0, delayTime=144.3, feedbackHpf=10, feedback=0,
        c3=100, c6=100, brightness=0, boostActivated=0, passthrough=0, oversample=5|
        ^this.multiNew('audio', in, inputLevel, dryLevel, wetLevel, delayTime, feedbackHpf, feedback,
            c3, c6, brightness, boostActivated, passthrough, oversample)
    }

    checkInputs {
        if(this.rate == 'audio' and: { this.inputs.at(0).rate != 'audio' }) {
            ^"PT2399: input must be audio rate".error;
        };
        ^this.checkValidInputs
    }
}
