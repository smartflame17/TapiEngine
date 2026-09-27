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

### AudioClip component

Add **Audio Clip** from the inspector's **Add Component** menu, or create it from
a script. Each component stores one WAV path and owns at most one controllable
instance. Multiple AudioClip components may be attached to the same GameObject.

```cpp
#include "Components/AudioClip.h"
#include "Scene/GameObject.h"

auto& music = GetGameObject().AddComponent<AudioClip>("Audio/Sounds/music.wav");
music.SetLooping(true);
music.SetVolume(0.6f);
if (!music.Play())
    spdlog::warn("Music: {}", music.GetLastError());

// Other components/scripts can find it through GetComponent<AudioClip>().
if (auto* clip = GetComponent<AudioClip>())
{
    clip->Pause();
    clip->SetPitch(-0.2f);
    clip->SetPan(0.25f);
    clip->Resume();
    clip->Stop();
}
```

`Play()` starts from the beginning and replaces the component's previous
instance. `Resume()` continues a paused instance. `SetSound()` stops the owned
instance when the path changes; an empty path clears the clip. Loop changes
apply on the next `Play()`. Volume, pitch and pan update both the saved settings
and the current instance. Invalid values are rejected without changing settings.
Playback is explicit: attaching or configuring the component does not start it.

`PlayOneShot()` uses the configured path, volume, pitch and pan, ignores looping,
and allows overlapping playback. These voices finish independently; component
controls and destruction affect only its owned instance. Global `Audio::StopAll()`
also stops one-shots. Removing a component, destroying its GameObject, or clearing
the scene stops its owned instance during cleanup.

Methods returning `bool` catch synchronous submission errors and expose them via
`GetLastError()`. Success only acknowledges a queued request. `GetHandle()` is
the last owned request handle, not a playback-state query: natural completion,
file loading failures and global stops can make it stale. `Play()` always obtains
a new handle. AudioClip does not expose a guessed `IsPlaying()` state.

The inspector provides an editable path, a WAV file picker, Clear, Loop, volume,
pitch and pan sliders, Play, Play One Shot, Pause, Resume, and Stop. Controls use
the same public API as scripts. Audio follows the App's global Play/Pause/Stop
state; Edit mode remains suspended. No global audio settings are changed by an
individual component.

### Tests

Build the engine in x64 Debug, then run
`./tests/RunAudioTests.ps1 -Configuration Debug` and repeat with `Release`.
Command tests use an instrumented backend; native smoke tests link the installed
DirectXTK package and generate a silent WAV, requiring no game assets or audible
output. The Components suite links the engine objects and checks AudioClip
playback, validation, cleanup, failures and ImGui controls against the instrumented
backend. Use `-Suite Commands` or `-Suite Native` to run the service tests without
an engine build, or `-Suite Components` to run only component tests.
See the [DirectXTK setup guide](https://github.com/microsoft/DirectXTK/wiki/Adding-audio-to-your-project)
and [threading contract](https://github.com/microsoft/DirectXTK/wiki/Audio#threading-model).
