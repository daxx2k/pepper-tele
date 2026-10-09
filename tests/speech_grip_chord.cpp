#include "../quest/src/main/cpp/speech_grip_chord.h"
#include <cassert>
#include <cstdio>
int main(){
    SpeechGripChord chord;
    assert(!chord.update(0,true,false,true)&&!chord.suppress); // Normal left PTT.
    assert(!chord.update(.1,true,true,true)&&chord.entered&&chord.suppress);
    assert(chord.update(.46,true,true,true));
    assert(!chord.update(1,true,true,true)); // One toggle per squeeze.
    assert(!chord.update(2,true,false,true)&&chord.suppress);
    chord.update(3,false,false,true);
    assert(!chord.update(4,false,true,true)&&!chord.suppress); // Normal pointer.
    assert(!chord.update(5,true,true,true)&&chord.suppress);
    assert(chord.update(5.36,true,true,true)); // Both grips work even when squeezed sequentially.
    chord.update(6,false,false,true);
    assert(!chord.update(7,true,true,false)); // Focus/disconnect can't toggle.
    assert(!chord.update(8,true,true,true));
    chord.update(9,false,false,true);
    assert(!chord.update(10,true,true,true));
    assert(chord.update(10.36,true,true,true)); // Toggle off after release.
    SpeechGripChord pointer;
    pointer.update(0,false,true,false); // Window manipulation disables the chord.
    assert(!pointer.update(1,false,true,true)&&!pointer.suppress); // Keep the single-grip pointer.
    puts("PASS speech grip chord: singles, debounce, release, focus loss");
}
