## Audio

TapiEngine uses the DirectXTK provided XAudio2 solution for audio handling. It creates a separate dedicated thread for handling audio, communicating with the main game thread via command queue.


### Details
`App` owns one `Audio` service, accessible from main-thread systems and scripts
through `Audio::GetInstance()`. Its public header contains no DirectXTK objects.

```cpp
#include "Audio/Audio.h"

auto& audio = Audio::GetInstance();
audio.PlayOneShot("Audio/Sounds/click.wav");
AudioHandle music = audio.Play("Audio/Sounds/music.wav", true, 0.6f);
audio.SetVolume(music, 0.4f);
audio.SetPitch(music, -0.2f);
audio.SetPan(music, 0.25f);
audio.Pause(music);
audio.Resume(music);
audio.Stop(music);
```

Paths are UTF-8 WAV filenames relative to the process working directory (absolute
paths also work). `Play` creates a controllable instance, optionally looped;
`PlayOneShot` uses DirectXTK's fire-and-forget voices and returns no handle.
Volume and master volume accept finite values in `[0, 1]`; pitch and pan accept
`[-1, 1]`. Invalid arguments throw on the submitting thread.

Handles are allocated immediately on the main thread, so a setter or `Stop` can
follow `Play` before the sound has loaded. IDs are never reused, including across
App lifetimes. Zero is invalid. Stopped, naturally completed, failed and unknown
handles are ignored. `Stop` destroys the instance; play again to obtain a new
handle. Paused instances retain their handles. A returned handle acknowledges a
queued request, not successful file loading or audible playback.

All public calls and service lifecycle operations belong on the main thread.
Only construction waits for worker initialization. Playback and controls submit
owned command data to a mutex-protected FIFO and notify its condition variable.
The worker swaps the shared queue with a local queue and performs loading,
playback, updates and destruction outside the lock. It wakes every 10 ms while
idle, and also updates between commands during long batches. File loading and
OS scheduling can delay this cadence.

The worker initializes its own COM MTA and owns the AudioEngine, WAV cache and
SoundEffectInstances for their entire lifetimes. WAVs are loaded into memory on
first use and cached by filename until `StopAll` or shutdown; this is not a
streaming music implementation. Completed nonlooping instances are reclaimed.
`StopAll` stops both instances and one-shots and releases the cache.

`Suspend` / the parameterless `Resume` control global playback. App starts
suspended in Edit mode, resumes on Play, suspends on Pause/Stop, and clears sounds
when resetting the scene. These transitions enqueue commands once; the render
thread never updates DirectXTK.

Without an audio device, the worker remains in silent mode and retries device
reset at most once per second. `ResetDevice` requests an immediate attempt.
Successful resets restart retained loops from the beginning, preserving their
controls and pause state; one-shots and nonlooping playback are not replayed.

File/command failures are reported to stderr and the debugger, and later commands
continue. Startup errors propagate to App construction after joining the worker.
Unexpected worker failures stop audio and are rethrown on later submissions.
Destruction requests shutdown, wakes and joins the worker, and discards commands
still in the shared queue; an already detached batch finishes first. The engine
is suspended before instances and source data are destroyed, and COM is released
last. Audio outlives the scene so component destructors may still submit commands.

Run `./tests/RunAudioTests.ps1 -Configuration Debug` and repeat with `Release`.
Command tests use an instrumented backend; native smoke tests link the installed
DirectXTK package and generate a silent WAV, requiring no game assets or audible
output. See the [DirectXTK setup guide](https://github.com/microsoft/DirectXTK/wiki/Adding-audio-to-your-project)
and [threading contract](https://github.com/microsoft/DirectXTK/wiki/Audio#threading-model).