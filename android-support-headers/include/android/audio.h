/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @addtogroup Audio
 * @{
 */

/**
 * @file aaudio/AAudio.h
 */

/**
 * This is the 'C' API for AAudio.
 */
#ifndef AAUDIO_AAUDIO_H
#define AAUDIO_AAUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#if !defined(__INTRODUCED_IN)
#define __INTRODUCED_IN(__api_level) /* nothing */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is used to represent a value that has not been specified.
 * For example, an application could use {@link #AAUDIO_UNSPECIFIED} to indicate
 * that it did not care what the specific value of a parameter was
 * and would accept whatever it was given.
 */
#define AAUDIO_UNSPECIFIED 0

enum {
    /**
     * Audio data will travel out of the device, for example through a speaker.
     */
    AAUDIO_DIRECTION_OUTPUT,

    /**
     * Audio data will travel into the device, for example from a microphone.
     */
    AAUDIO_DIRECTION_INPUT
};
typedef int32_t aaudio_direction_t;

enum {
    AAUDIO_FORMAT_INVALID = -1,
    AAUDIO_FORMAT_UNSPECIFIED = 0,

    /**
     * This format uses the int16_t data type.
     * The maximum range of the data is -32768 (0x8000) to 32767 (0x7FFF).
     */
    AAUDIO_FORMAT_PCM_I16,

    /**
     * This format uses the float data type.
     * The nominal range of the data is [-1.0f, 1.0f).
     * Values outside that range may be clipped.
     *
     * See also the float Data in
     * <a href="/reference/android/media/AudioTrack#write(float[],%20int,%20int,%20int)">
     *   write(float[], int, int, int)</a>.
     */
    AAUDIO_FORMAT_PCM_FLOAT,

    /**
     * This format uses 24-bit samples packed into 3 bytes.
     * The bytes are in little-endian order, so the least significant byte
     * comes first in the byte array.
     *
     * The maximum range of the data is -8388608 (0x800000)
     * to 8388607 (0x7FFFFF).
     *
     * Note that the lower precision bits may be ignored by the device.
     *
     * Available since API level 31.
     */
    AAUDIO_FORMAT_PCM_I24_PACKED,

    /**
     * This format uses 32-bit samples stored in an int32_t data type.
     * The maximum range of the data is -2147483648 (0x80000000)
     * to 2147483647 (0x7FFFFFFF).
     *
     * Note that the lower precision bits may be ignored by the device.
     *
     * Available since API level 31.
     */
    AAUDIO_FORMAT_PCM_I32,

    /**
     * This format is used for compressed audio wrapped in IEC61937 for HDMI
     * or S/PDIF passthrough.
     *
     * Unlike PCM playback, the Android framework is not able to do format
     * conversion for IEC61937. In that case, when IEC61937 is requested, sampling
     * rate and channel count or channel mask must be specified. Otherwise, it may
     * fail when opening the stream. Apps are able to get the correct configuration
     * for the playback by calling
     * <a href="/reference/android/media/AudioManager#getDevices(int)">
     *   AudioManager#getDevices(int)</a>.
     *
     * Available since API level 34.
     */
    AAUDIO_FORMAT_IEC61937

};
typedef int32_t aaudio_format_t;

/**
 * These result codes are returned from AAudio functions to indicate success or failure.
 * Note that error return codes may change in the future so applications should generally
 * not rely on specific return codes.
 */
enum {
    /**
     * The call was successful.
     */
    AAUDIO_OK,

    /**
     * Reserved. This should not be returned.
     */
    AAUDIO_ERROR_BASE = -900,

    /**
     * The audio device was disconnected. This could occur, for example, when headphones
     * are plugged in or unplugged. The stream cannot be used after the device is disconnected.
     * Applications should stop and close the stream.
     * If this error is received in an error callback then another thread should be
     * used to stop and close the stream.
     */
    AAUDIO_ERROR_DISCONNECTED,

    /**
     * An invalid parameter was passed to AAudio.
     */
    AAUDIO_ERROR_ILLEGAL_ARGUMENT,
    // reserved

    /**
     * An internal error occurred.
     */
    AAUDIO_ERROR_INTERNAL = AAUDIO_ERROR_ILLEGAL_ARGUMENT + 2,

    /**
     * The requested operation is not appropriate for the current state of AAudio.
     */
    AAUDIO_ERROR_INVALID_STATE,
    // reserved
    // reserved

    /**
     * The server rejected the handle used to identify the stream.
     */
    AAUDIO_ERROR_INVALID_HANDLE = AAUDIO_ERROR_INVALID_STATE + 3,
    // reserved

    /**
     * The function is not implemented for this stream.
     */
    AAUDIO_ERROR_UNIMPLEMENTED = AAUDIO_ERROR_INVALID_HANDLE + 2,

    /**
     * A resource or information is unavailable.
     * This could occur when an application tries to open too many streams,
     * or a timestamp is not available.
     */
    AAUDIO_ERROR_UNAVAILABLE,

    /**
     * Reserved. This should not be returned.
     */
    AAUDIO_ERROR_NO_FREE_HANDLES,

    /**
     * Memory could not be allocated.
     */
    AAUDIO_ERROR_NO_MEMORY,

    /**
     * A NULL pointer was passed to AAudio.
     * Or a NULL pointer was detected internally.
     */
    AAUDIO_ERROR_NULL,

    /**
     * An operation took longer than expected.
     */
    AAUDIO_ERROR_TIMEOUT,

    /**
     * A queue is full. This queue would be blocked.
     */
    AAUDIO_ERROR_WOULD_BLOCK,

    /**
     * The requested data format is not supported.
     */
    AAUDIO_ERROR_INVALID_FORMAT,

    /**
     * A requested was out of range.
     */
    AAUDIO_ERROR_OUT_OF_RANGE,

    /**
     * The audio service was not available.
     */
    AAUDIO_ERROR_NO_SERVICE,

    /**
     * The requested sample rate was not supported.
     */
    AAUDIO_ERROR_INVALID_RATE
};
typedef int32_t aaudio_result_t;

/**
 * AAudio Stream states, for details, refer to
 * <a href="/ndk/guides/audio/aaudio/aaudio#using-streams">Using an Audio Stream</a>
 */
enum {

    /**
     * The stream is created but not initialized yet.
     */
    AAUDIO_STREAM_STATE_UNINITIALIZED = 0,
    /**
     * The stream is in an unrecognized state.
     */
    AAUDIO_STREAM_STATE_UNKNOWN,

    /**
     * The stream is open and ready to use.
     */
    AAUDIO_STREAM_STATE_OPEN,
    /**
     * The stream is just starting up.
     */
    AAUDIO_STREAM_STATE_STARTING,
    /**
     * The stream has started.
     */
    AAUDIO_STREAM_STATE_STARTED,
    /**
     * The stream is pausing.
     */
    AAUDIO_STREAM_STATE_PAUSING,
    /**
     * The stream has paused, could be restarted or flushed.
     */
    AAUDIO_STREAM_STATE_PAUSED,
    /**
     * The stream is being flushed.
     */
    AAUDIO_STREAM_STATE_FLUSHING,
    /**
     * The stream is flushed, ready to be restarted.
     */
    AAUDIO_STREAM_STATE_FLUSHED,
    /**
     * The stream is stopping.
     */
    AAUDIO_STREAM_STATE_STOPPING,
    /**
     * The stream has been stopped.
     */
    AAUDIO_STREAM_STATE_STOPPED,
    /**
     * The stream is closing.
     */
    AAUDIO_STREAM_STATE_CLOSING,
    /**
     * The stream has been closed.
     */
    AAUDIO_STREAM_STATE_CLOSED,
    /**
     * The stream is disconnected from audio device.
     * @deprecated
     */
    AAUDIO_STREAM_STATE_DISCONNECTED
};
typedef int32_t aaudio_stream_state_t;

enum {
    /**
     * This will be the only stream using a particular source or sink.
     * This mode will provide the lowest possible latency.
     * You should close EXCLUSIVE streams immediately when you are not using them.
     */
    AAUDIO_SHARING_MODE_EXCLUSIVE,
    /**
     * Multiple applications will be mixed by the AAudio Server.
     * This will have higher latency than the EXCLUSIVE mode.
     */
    AAUDIO_SHARING_MODE_SHARED
};
typedef int32_t aaudio_sharing_mode_t;

enum {
    /**
     * No particular performance needs. Default.
     */
    AAUDIO_PERFORMANCE_MODE_NONE = 10,

    /**
     * Extending battery life is more important than low latency.
     *
     * This mode is not supported in input streams.
     * For input, mode NONE will be used if this is requested.
     */
    AAUDIO_PERFORMANCE_MODE_POWER_SAVING,

    /**
     * Reducing latency is more important than battery life.
     */
    AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
};
typedef int32_t aaudio_performance_mode_t;

#define AAUDIO_SYSTEM_USAGE_OFFSET 1000

/**
 * The USAGE attribute expresses "why" you are playing a sound, what is this sound used for.
 * This information is used by certain platforms or routing policies
 * to make more refined volume or routing decisions.
 *
 * Note that these match the equivalent values in
 * <a href="/reference/android/media/AudioAttributes">AudioAttributes</a>
 * in the Android Java API.
 *
 * Added in API level 28.
 */
enum {
    /**
     * Use this for streaming media, music performance, video, podcasts, etcetera.
     */
    AAUDIO_USAGE_MEDIA = 1,

    /**
     * Use this for voice over IP, telephony, etcetera.
     */
    AAUDIO_USAGE_VOICE_COMMUNICATION = 2,

    /**
     * Use this for sounds associated with telephony such as busy tones, DTMF, etcetera.
     */
    AAUDIO_USAGE_VOICE_COMMUNICATION_SIGNALLING = 3,

    /**
     * Use this to demand the users attention.
     */
    AAUDIO_USAGE_ALARM = 4,

    /**
     * Use this for notifying the user when a message has arrived or some
     * other background event has occured.
     */
    AAUDIO_USAGE_NOTIFICATION = 5,

    /**
     * Use this when the phone rings.
     */
    AAUDIO_USAGE_NOTIFICATION_RINGTONE = 6,

    /**
     * Use this to attract the users attention when, for example, the battery is low.
     */
    AAUDIO_USAGE_NOTIFICATION_EVENT = 10,

    /**
     * Use this for screen readers, etcetera.
     */
    AAUDIO_USAGE_ASSISTANCE_ACCESSIBILITY = 11,

    /**
     * Use this for driving or navigation directions.
     */
    AAUDIO_USAGE_ASSISTANCE_NAVIGATION_GUIDANCE = 12,

    /**
     * Use this for user interface sounds, beeps, etcetera.
     */
    AAUDIO_USAGE_ASSISTANCE_SONIFICATION = 13,

    /**
     * Use this for game audio and sound effects.
     */
    AAUDIO_USAGE_GAME = 14,

    /**
     * Use this for audio responses to user queries, audio instructions or help utterances.
     */
    AAUDIO_USAGE_ASSISTANT = 16,

    /**
     * Use this in case of playing sounds in an emergency.
     * Privileged MODIFY_AUDIO_ROUTING permission required.
     */
    AAUDIO_SYSTEM_USAGE_EMERGENCY = AAUDIO_SYSTEM_USAGE_OFFSET,

    /**
     * Use this for safety sounds and alerts, for example backup camera obstacle detection.
     * Privileged MODIFY_AUDIO_ROUTING permission required.
     */
    AAUDIO_SYSTEM_USAGE_SAFETY = AAUDIO_SYSTEM_USAGE_OFFSET + 1,

    /**
     * Use this for vehicle status alerts and information, for example the check engine light.
     * Privileged MODIFY_AUDIO_ROUTING permission required.
     */
    AAUDIO_SYSTEM_USAGE_VEHICLE_STATUS = AAUDIO_SYSTEM_USAGE_OFFSET + 2,

    /**
     * Use this for traffic announcements, etc.
     * Privileged MODIFY_AUDIO_ROUTING permission required.
     */
    AAUDIO_SYSTEM_USAGE_ANNOUNCEMENT = AAUDIO_SYSTEM_USAGE_OFFSET + 3,
};
typedef int32_t aaudio_usage_t;

/**
 * The CONTENT_TYPE attribute describes "what" you are playing.
 * It expresses the general category of the content. This information is optional.
 * But in case it is known (for instance AAUDIO_CONTENT_TYPE_MOVIE for a
 * movie streaming service or AAUDIO_CONTENT_TYPE_SPEECH for
 * an audio book application) this information might be used by the audio framework to
 * enforce audio focus.
 *
 * Note that these match the equivalent values in
 * <a href="/reference/android/media/AudioAttributes">AudioAttributes</a>
 * in the Android Java API.
 *
 * Added in API level 28.
 */
enum {

    /**
     * Use this for spoken voice, audio books, etcetera.
     */
    AAUDIO_CONTENT_TYPE_SPEECH = 1,

    /**
     * Use this for pre-recorded or live music.
     */
    AAUDIO_CONTENT_TYPE_MUSIC = 2,

    /**
     * Use this for a movie or video soundtrack.
     */
    AAUDIO_CONTENT_TYPE_MOVIE = 3,

    /**
     * Use this for sound is designed to accompany a user action,
     * such as a click or beep sound made when the user presses a button.
     */
    AAUDIO_CONTENT_TYPE_SONIFICATION = 4
};
typedef int32_t aaudio_content_type_t;

enum {

    /**
     * Constant indicating the audio content associated with these attributes will follow the
     * default platform behavior with regards to which content will be spatialized or not.
     */
    AAUDIO_SPATIALIZATION_BEHAVIOR_AUTO = 1,

    /**
     * Constant indicating the audio content associated with these attributes should never
     * be spatialized.
     */
    AAUDIO_SPATIALIZATION_BEHAVIOR_NEVER = 2,
};
typedef int32_t aaudio_spatialization_behavior_t;

/**
 * Defines the audio source.
 * An audio source defines both a default physical source of audio signal, and a recording
 * configuration.
 *
 * Note that these match the equivalent values in MediaRecorder.AudioSource in the Android Java API.
 *
 * Added in API level 28.
 */
enum {
    /**
     * Use this preset when other presets do not apply.
     */
    AAUDIO_INPUT_PRESET_GENERIC = 1,

    /**
     * Use this preset when recording video.
     */
    AAUDIO_INPUT_PRESET_CAMCORDER = 5,

    /**
     * Use this preset when doing speech recognition.
     */
    AAUDIO_INPUT_PRESET_VOICE_RECOGNITION = 6,

    /**
     * Use this preset when doing telephony or voice messaging.
     */
    AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION = 7,

    /**
     * Use this preset to obtain an input with no effects.
     * Note that this input will not have automatic gain control
     * so the recorded volume may be very low.
     */
    AAUDIO_INPUT_PRESET_UNPROCESSED = 9,

    /**
     * Use this preset for capturing audio meant to be processed in real time
     * and played back for live performance (e.g karaoke).
     * The capture path will minimize latency and coupling with playback path.
     * Available since API level 29.
     */
    AAUDIO_INPUT_PRESET_VOICE_PERFORMANCE = 10,

    /**
     * Use this preset for an echo canceller to capture the reference signal.
     * Reserved for system components.
     * Requires CAPTURE_AUDIO_OUTPUT permission
     * Available since API level 35.
     */
    AAUDIO_INPUT_PRESET_SYSTEM_ECHO_REFERENCE = 1997,

    /**
     * Use this preset for preemptible, low-priority software hotword detection.
     * Reserved for system components.
     * Requires CAPTURE_AUDIO_HOTWORD permission.
     * Available since API level 35.
     */
    AAUDIO_INPUT_PRESET_SYSTEM_HOTWORD = 1999,
};
typedef int32_t aaudio_input_preset_t;

/**
 * Specifying if audio may or may not be captured by other apps or the system.
 *
 * Note that these match the equivalent values in
 * <a href="/reference/android/media/AudioAttributes">AudioAttributes</a>
 * in the Android Java API.
 *
 * Added in API level 29.
 */
enum {
    /**
     * Indicates that the audio may be captured by any app.
     *
     * For privacy, the following usages can not be recorded: AAUDIO_VOICE_COMMUNICATION*,
     * AAUDIO_USAGE_NOTIFICATION*, AAUDIO_USAGE_ASSISTANCE* and {@link #AAUDIO_USAGE_ASSISTANT}.
     *
     * On <a href="/reference/android/os/Build.VERSION_CODES#Q">Build.VERSION_CODES</a>,
     * this means only {@link #AAUDIO_USAGE_MEDIA} and {@link #AAUDIO_USAGE_GAME} may be captured.
     *
     * See <a href="/reference/android/media/AudioAttributes.html#ALLOW_CAPTURE_BY_ALL">
     * ALLOW_CAPTURE_BY_ALL</a>.
     */
    AAUDIO_ALLOW_CAPTURE_BY_ALL = 1,
    /**
     * Indicates that the audio may only be captured by system apps.
     *
     * System apps can capture for many purposes like accessibility, user guidance...
     * but have strong restriction. See
     * <a href="/reference/android/media/AudioAttributes.html#ALLOW_CAPTURE_BY_SYSTEM">
     * ALLOW_CAPTURE_BY_SYSTEM</a>
     * for what the system apps can do with the capture audio.
     */
    AAUDIO_ALLOW_CAPTURE_BY_SYSTEM = 2,
    /**
     * Indicates that the audio may not be recorded by any app, even if it is a system app.
     *
     * It is encouraged to use {@link #AAUDIO_ALLOW_CAPTURE_BY_SYSTEM} instead of this value as system apps
     * provide significant and useful features for the user (eg. accessibility).
     * See <a href="/reference/android/media/AudioAttributes.html#ALLOW_CAPTURE_BY_NONE">
     * ALLOW_CAPTURE_BY_NONE</a>.
     */
    AAUDIO_ALLOW_CAPTURE_BY_NONE = 3,
};

typedef int32_t aaudio_allowed_capture_policy_t;

/**
 * These may be used with AAudioStreamBuilder_setSessionId().
 *
 * Added in API level 28.
 */
enum {
    /**
     * Do not allocate a session ID.
     * Effects cannot be used with this stream.
     * Default.
     *
     * Added in API level 28.
     */
    AAUDIO_SESSION_ID_NONE = -1,

    /**
     * Allocate a session ID that can be used to attach and control
     * effects using the Java AudioEffects API.
     * Note that using this may result in higher latency.
     *
     * Note that this matches the value of AudioManager.AUDIO_SESSION_ID_GENERATE.
     *
     * Added in API level 28.
     */
    AAUDIO_SESSION_ID_ALLOCATE = 0,
};
typedef int32_t aaudio_session_id_t;

/**
 * Defines the audio channel mask.
 * Channel masks are used to describe the samples and their
 * arrangement in the audio frame. They are also used in the endpoint
 * (e.g. a USB audio interface, a DAC connected to headphones) to
 * specify allowable configurations of a particular device.
 *
 * Channel masks are for input only, output only, or both input and output.
 * These channel masks are different than those defined in AudioFormat.java.
 * If an app gets a channel mask from Java API and wants to use it in AAudio,
 * conversion should be done by the app.
 *
 * Added in API level 32.
 */
enum {
    /**
     * Invalid channel mask
     */
    AAUDIO_CHANNEL_INVALID = -1,
    AAUDIO_CHANNEL_FRONT_LEFT = 1 << 0,
    AAUDIO_CHANNEL_FRONT_RIGHT = 1 << 1,
    AAUDIO_CHANNEL_FRONT_CENTER = 1 << 2,
    AAUDIO_CHANNEL_LOW_FREQUENCY = 1 << 3,
    AAUDIO_CHANNEL_BACK_LEFT = 1 << 4,
    AAUDIO_CHANNEL_BACK_RIGHT = 1 << 5,
    AAUDIO_CHANNEL_FRONT_LEFT_OF_CENTER = 1 << 6,
    AAUDIO_CHANNEL_FRONT_RIGHT_OF_CENTER = 1 << 7,
    AAUDIO_CHANNEL_BACK_CENTER = 1 << 8,
    AAUDIO_CHANNEL_SIDE_LEFT = 1 << 9,
    AAUDIO_CHANNEL_SIDE_RIGHT = 1 << 10,
    AAUDIO_CHANNEL_TOP_CENTER = 1 << 11,
    AAUDIO_CHANNEL_TOP_FRONT_LEFT = 1 << 12,
    AAUDIO_CHANNEL_TOP_FRONT_CENTER = 1 << 13,
    AAUDIO_CHANNEL_TOP_FRONT_RIGHT = 1 << 14,
    AAUDIO_CHANNEL_TOP_BACK_LEFT = 1 << 15,
    AAUDIO_CHANNEL_TOP_BACK_CENTER = 1 << 16,
    AAUDIO_CHANNEL_TOP_BACK_RIGHT = 1 << 17,
    AAUDIO_CHANNEL_TOP_SIDE_LEFT = 1 << 18,
    AAUDIO_CHANNEL_TOP_SIDE_RIGHT = 1 << 19,
    AAUDIO_CHANNEL_BOTTOM_FRONT_LEFT = 1 << 20,
    AAUDIO_CHANNEL_BOTTOM_FRONT_CENTER = 1 << 21,
    AAUDIO_CHANNEL_BOTTOM_FRONT_RIGHT = 1 << 22,
    AAUDIO_CHANNEL_LOW_FREQUENCY_2 = 1 << 23,
    AAUDIO_CHANNEL_FRONT_WIDE_LEFT = 1 << 24,
    AAUDIO_CHANNEL_FRONT_WIDE_RIGHT = 1 << 25,

    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_MONO = AAUDIO_CHANNEL_FRONT_LEFT,
    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_STEREO = AAUDIO_CHANNEL_FRONT_LEFT |
                            AAUDIO_CHANNEL_FRONT_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_2POINT1 = AAUDIO_CHANNEL_FRONT_LEFT |
                             AAUDIO_CHANNEL_FRONT_RIGHT |
                             AAUDIO_CHANNEL_LOW_FREQUENCY,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_TRI = AAUDIO_CHANNEL_FRONT_LEFT |
                         AAUDIO_CHANNEL_FRONT_RIGHT |
                         AAUDIO_CHANNEL_FRONT_CENTER,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_TRI_BACK = AAUDIO_CHANNEL_FRONT_LEFT |
                              AAUDIO_CHANNEL_FRONT_RIGHT |
                              AAUDIO_CHANNEL_BACK_CENTER,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_3POINT1 = AAUDIO_CHANNEL_FRONT_LEFT |
                             AAUDIO_CHANNEL_FRONT_RIGHT |
                             AAUDIO_CHANNEL_FRONT_CENTER |
                             AAUDIO_CHANNEL_LOW_FREQUENCY,
    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_2POINT0POINT2 = AAUDIO_CHANNEL_FRONT_LEFT |
                                   AAUDIO_CHANNEL_FRONT_RIGHT |
                                   AAUDIO_CHANNEL_TOP_SIDE_LEFT |
                                   AAUDIO_CHANNEL_TOP_SIDE_RIGHT,
    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_2POINT1POINT2 = AAUDIO_CHANNEL_2POINT0POINT2 |
                                   AAUDIO_CHANNEL_LOW_FREQUENCY,
    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_3POINT0POINT2 = AAUDIO_CHANNEL_FRONT_LEFT |
                                   AAUDIO_CHANNEL_FRONT_RIGHT |
                                   AAUDIO_CHANNEL_FRONT_CENTER |
                                   AAUDIO_CHANNEL_TOP_SIDE_LEFT |
                                   AAUDIO_CHANNEL_TOP_SIDE_RIGHT,
    /**
     * Supported for Input and Output
     */
    AAUDIO_CHANNEL_3POINT1POINT2 = AAUDIO_CHANNEL_3POINT0POINT2 |
                                   AAUDIO_CHANNEL_LOW_FREQUENCY,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_QUAD = AAUDIO_CHANNEL_FRONT_LEFT |
                          AAUDIO_CHANNEL_FRONT_RIGHT |
                          AAUDIO_CHANNEL_BACK_LEFT |
                          AAUDIO_CHANNEL_BACK_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_QUAD_SIDE = AAUDIO_CHANNEL_FRONT_LEFT |
                               AAUDIO_CHANNEL_FRONT_RIGHT |
                               AAUDIO_CHANNEL_SIDE_LEFT |
                               AAUDIO_CHANNEL_SIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_SURROUND = AAUDIO_CHANNEL_FRONT_LEFT |
                              AAUDIO_CHANNEL_FRONT_RIGHT |
                              AAUDIO_CHANNEL_FRONT_CENTER |
                              AAUDIO_CHANNEL_BACK_CENTER,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_PENTA = AAUDIO_CHANNEL_QUAD |
                           AAUDIO_CHANNEL_FRONT_CENTER,
    /**
     * Supported for Input and Output. aka 5POINT1_BACK
     */
    AAUDIO_CHANNEL_5POINT1 = AAUDIO_CHANNEL_FRONT_LEFT |
                             AAUDIO_CHANNEL_FRONT_RIGHT |
                             AAUDIO_CHANNEL_FRONT_CENTER |
                             AAUDIO_CHANNEL_LOW_FREQUENCY |
                             AAUDIO_CHANNEL_BACK_LEFT |
                             AAUDIO_CHANNEL_BACK_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_5POINT1_SIDE = AAUDIO_CHANNEL_FRONT_LEFT |
                                  AAUDIO_CHANNEL_FRONT_RIGHT |
                                  AAUDIO_CHANNEL_FRONT_CENTER |
                                  AAUDIO_CHANNEL_LOW_FREQUENCY |
                                  AAUDIO_CHANNEL_SIDE_LEFT |
                                  AAUDIO_CHANNEL_SIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_6POINT1 = AAUDIO_CHANNEL_FRONT_LEFT |
                             AAUDIO_CHANNEL_FRONT_RIGHT |
                             AAUDIO_CHANNEL_FRONT_CENTER |
                             AAUDIO_CHANNEL_LOW_FREQUENCY |
                             AAUDIO_CHANNEL_BACK_LEFT |
                             AAUDIO_CHANNEL_BACK_RIGHT |
                             AAUDIO_CHANNEL_BACK_CENTER,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_7POINT1 = AAUDIO_CHANNEL_5POINT1 |
                             AAUDIO_CHANNEL_SIDE_LEFT |
                             AAUDIO_CHANNEL_SIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_5POINT1POINT2 = AAUDIO_CHANNEL_5POINT1 |
                                   AAUDIO_CHANNEL_TOP_SIDE_LEFT |
                                   AAUDIO_CHANNEL_TOP_SIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_5POINT1POINT4 = AAUDIO_CHANNEL_5POINT1 |
                                   AAUDIO_CHANNEL_TOP_FRONT_LEFT |
                                   AAUDIO_CHANNEL_TOP_FRONT_RIGHT |
                                   AAUDIO_CHANNEL_TOP_BACK_LEFT |
                                   AAUDIO_CHANNEL_TOP_BACK_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_7POINT1POINT2 = AAUDIO_CHANNEL_7POINT1 |
                                   AAUDIO_CHANNEL_TOP_SIDE_LEFT |
                                   AAUDIO_CHANNEL_TOP_SIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_7POINT1POINT4 = AAUDIO_CHANNEL_7POINT1 |
                                   AAUDIO_CHANNEL_TOP_FRONT_LEFT |
                                   AAUDIO_CHANNEL_TOP_FRONT_RIGHT |
                                   AAUDIO_CHANNEL_TOP_BACK_LEFT |
                                   AAUDIO_CHANNEL_TOP_BACK_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_9POINT1POINT4 = AAUDIO_CHANNEL_7POINT1POINT4 |
                                   AAUDIO_CHANNEL_FRONT_WIDE_LEFT |
                                   AAUDIO_CHANNEL_FRONT_WIDE_RIGHT,
    /**
     * Supported for only Output
     */
    AAUDIO_CHANNEL_9POINT1POINT6 = AAUDIO_CHANNEL_9POINT1POINT4 |
                                   AAUDIO_CHANNEL_TOP_SIDE_LEFT |
                                   AAUDIO_CHANNEL_TOP_SIDE_RIGHT,
    /**
     * Supported for only Input
     */
    AAUDIO_CHANNEL_FRONT_BACK = AAUDIO_CHANNEL_FRONT_CENTER |
                                AAUDIO_CHANNEL_BACK_CENTER,
};
typedef uint32_t aaudio_channel_mask_t;

typedef struct AAudioStreamStruct AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;

#ifndef AAUDIO_API
#define AAUDIO_API /* export this symbol */
#endif

// ============================================================
// Audio System
// ============================================================

/**
 * The text is the ASCII symbol corresponding to the returnCode,
 * or an English message saying the returnCode is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for display to users.
 *
 * Available since API level 26.
 *
 * @return pointer to a text representation of an AAudio result code.
 */
AAUDIO_API const char* _Nonnull AAudio_convertResultToText(aaudio_result_t returnCode)
    __INTRODUCED_IN(26);

/**
 * The text is the ASCII symbol corresponding to the stream state,
 * or an English message saying the state is unrecognized.
 * This is intended for developers to use when debugging.
 * It is not for display to users.
 *
 * Available since API level 26.
 *
 * @return pointer to a text representation of an AAudio state.
 */
AAUDIO_API const char* _Nonnull AAudio_convertStreamStateToText(aaudio_stream_state_t state)
    __INTRODUCED_IN(26);

// ============================================================
// StreamBuilder
// ============================================================

/**
 * Create a StreamBuilder that can be used to open a Stream.
 *
 * The deviceId is initially unspecified, meaning that the current default device will be used.
 *
 * The default direction is {@link #AAUDIO_DIRECTION_OUTPUT}.
 * The default sharing mode is {@link #AAUDIO_SHARING_MODE_SHARED}.
 * The data format, samplesPerFrames and sampleRate are unspecified and will be
 * chosen by the device when it is opened.
 *
 * AAudioStreamBuilder_delete() must be called when you are done using the builder.
 *
 * Available since API level 26.
 */
AAUDIO_API aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder* _Nullable* _Nonnull builder) __INTRODUCED_IN(26);

/**
 * Request an audio device identified by an ID.
 *
 * The ID could be obtained from the Java AudioManager.
 * AudioManager.getDevices() returns an array of {@link AudioDeviceInfo},
 * which contains a getId() method. That ID can be passed to this function.
 *
 * It is possible that you may not get the device that you requested.
 * So if it is important to you, you should call
 * AAudioStream_getDeviceId() after the stream is opened to
 * verify the actual ID.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED},
 * in which case the primary device will be used.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param deviceId device identifier or {@link #AAUDIO_UNSPECIFIED}
 */
AAUDIO_API void AAudioStreamBuilder_setDeviceId(AAudioStreamBuilder* _Nonnull builder,
                                                int32_t deviceId) __INTRODUCED_IN(26);

/**
 * Declare the name of the package creating the stream.
 *
 * This is usually {@code Context#getPackageName()}.
 *
 * The default, if you do not call this function, is a random package in the calling uid.
 * The vast majority of apps have only one package per calling UID.
 * If an invalid package name is set, input streams may not be given permission to
 * record when started.
 *
 * The package name is usually the applicationId in your app's build.gradle file.
 *
 * Available since API level 31.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param packageName packageName of the calling app.
 */
AAUDIO_API void AAudioStreamBuilder_setPackageName(AAudioStreamBuilder* _Nonnull builder,
                                                   const char* _Nonnull packageName) __INTRODUCED_IN(31);

/**
 * Declare the attribution tag of the context creating the stream.
 *
 * This is usually {@code Context#getAttributionTag()}.
 *
 * The default, if you do not call this function, is null.
 *
 * Available since API level 31.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param attributionTag attributionTag of the calling context.
 */
AAUDIO_API void AAudioStreamBuilder_setAttributionTag(AAudioStreamBuilder* _Nonnull builder,
                                                      const char* _Nonnull attributionTag) __INTRODUCED_IN(31);

/**
 * Request a sample rate in Hertz.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 * An optimal value will then be chosen when the stream is opened.
 * After opening a stream with an unspecified value, the application must
 * query for the actual value, which may vary by device.
 *
 * If an exact value is specified then an opened stream will use that value.
 * If a stream cannot be opened with the specified value then the open will fail.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param sampleRate frames per second. Common rates include 44100 and 48000 Hz.
 */
AAUDIO_API void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder* _Nonnull builder,
                                                  int32_t sampleRate) __INTRODUCED_IN(26);

/**
 * Request a number of channels for the stream.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 * An optimal value will then be chosen when the stream is opened.
 * After opening a stream with an unspecified value, the application must
 * query for the actual value, which may vary by device.
 *
 * If an exact value is specified then an opened stream will use that value.
 * If a stream cannot be opened with the specified value then the open will fail.
 *
 * As the channel count provided here may be different from the corresponding channel count
 * of channel mask used in {@link AAudioStreamBuilder_setChannelMask}, the last called function
 * will be respected if both this function and {@link AAudioStreamBuilder_setChannelMask} are
 * called.
 *
 * Note that if the channel count is two then it may get mixed to mono when the device only supports
 * one channel. If the channel count is greater than two but the device's supported channel count is
 * less than the requested value, the channels higher than the device channel will be dropped. If
 * higher channels should be mixed or spatialized, use {@link AAudioStreamBuilder_setChannelMask}
 * instead.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param channelCount Number of channels desired.
 */
AAUDIO_API void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder* _Nonnull builder,
                                                    int32_t channelCount) __INTRODUCED_IN(26);

/**
 * Identical to AAudioStreamBuilder_setChannelCount().
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param samplesPerFrame Number of samples in a frame.
 *
 * @deprecated use {@link AAudioStreamBuilder_setChannelCount}
 */
AAUDIO_API void AAudioStreamBuilder_setSamplesPerFrame(AAudioStreamBuilder* _Nonnull builder,
                                                       int32_t samplesPerFrame) __INTRODUCED_IN(26);

/**
 * Request a sample data format, for example {@link #AAUDIO_FORMAT_PCM_I16}.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 * An optimal value will then be chosen when the stream is opened.
 * After opening a stream with an unspecified value, the application must
 * query for the actual value, which may vary by device.
 *
 * If an exact value is specified then an opened stream will use that value.
 * If a stream cannot be opened with the specified value then the open will fail.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param format common formats are {@link #AAUDIO_FORMAT_PCM_FLOAT} and
 *               {@link #AAUDIO_FORMAT_PCM_I16}.
 */
AAUDIO_API void AAudioStreamBuilder_setFormat(AAudioStreamBuilder* _Nonnull builder,
                                              aaudio_format_t format) __INTRODUCED_IN(26);

/**
 * Request a mode for sharing the device.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_SHARING_MODE_SHARED}.
 *
 * The requested sharing mode may not be available.
 * The application can query for the actual mode after the stream is opened.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param sharingMode {@link #AAUDIO_SHARING_MODE_SHARED} or {@link #AAUDIO_SHARING_MODE_EXCLUSIVE}
 */
AAUDIO_API void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder* _Nonnull builder,
                                                   aaudio_sharing_mode_t sharingMode) __INTRODUCED_IN(26);

/**
 * Request the direction for a stream.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_DIRECTION_OUTPUT}.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param direction {@link #AAUDIO_DIRECTION_OUTPUT} or {@link #AAUDIO_DIRECTION_INPUT}
 */
AAUDIO_API void AAudioStreamBuilder_setDirection(AAudioStreamBuilder* _Nonnull builder,
                                                 aaudio_direction_t direction) __INTRODUCED_IN(26);

/**
 * Set the requested buffer capacity in frames.
 * The final AAudioStream capacity may differ, but will probably be at least this big.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param numFrames the desired buffer capacity in frames or {@link #AAUDIO_UNSPECIFIED}
 */
AAUDIO_API void AAudioStreamBuilder_setBufferCapacityInFrames(
    AAudioStreamBuilder* _Nonnull builder, int32_t numFrames) __INTRODUCED_IN(26);

/**
 * Set the requested performance mode.
 *
 * Supported modes are {@link #AAUDIO_PERFORMANCE_MODE_NONE},
 * {@link #AAUDIO_PERFORMANCE_MODE_POWER_SAVING} * and {@link #AAUDIO_PERFORMANCE_MODE_LOW_LATENCY}.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_PERFORMANCE_MODE_NONE}.
 *
 * You may not get the mode you requested.
 * You can call AAudioStream_getPerformanceMode()
 * to find out the final mode for the stream.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param mode the desired performance mode, eg. {@link #AAUDIO_PERFORMANCE_MODE_LOW_LATENCY}
 */
AAUDIO_API void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder* _Nonnull builder,
                                                       aaudio_performance_mode_t mode) __INTRODUCED_IN(26);

/**
 * Set the intended use case for the output stream.
 *
 * The AAudio system will use this information to optimize the
 * behavior of the stream.
 * This could, for example, affect how volume and focus is handled for the stream.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_USAGE_MEDIA}.
 *
 * Available since API level 28.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param usage the desired usage, eg. {@link #AAUDIO_USAGE_GAME}
 */
AAUDIO_API void AAudioStreamBuilder_setUsage(AAudioStreamBuilder* _Nonnull builder,
                                             aaudio_usage_t usage) __INTRODUCED_IN(28);

/**
 * Set the type of audio data that the output stream will carry.
 *
 * The AAudio system will use this information to optimize the
 * behavior of the stream.
 * This could, for example, affect whether a stream is paused when a notification occurs.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_CONTENT_TYPE_MUSIC}.
 *
 * Available since API level 28.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param contentType the type of audio data, eg. {@link #AAUDIO_CONTENT_TYPE_SPEECH}
 */
AAUDIO_API void AAudioStreamBuilder_setContentType(AAudioStreamBuilder* _Nonnull builder,
                                                   aaudio_content_type_t contentType) __INTRODUCED_IN(28);

/**
 * Sets the behavior affecting whether spatialization will be used.
 *
 * The AAudio system will use this information to select whether the stream will go
 * through a spatializer effect or not when the effect is supported and enabled.
 *
 * Available since API level 32.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param spatializationBehavior the desired behavior with regards to spatialization, eg.
 *     {@link #AAUDIO_SPATIALIZATION_BEHAVIOR_AUTO}
 */
AAUDIO_API void AAudioStreamBuilder_setSpatializationBehavior(
    AAudioStreamBuilder* _Nonnull builder,
    aaudio_spatialization_behavior_t spatializationBehavior) __INTRODUCED_IN(32);

/**
 * Specifies whether the audio data of this output stream has already been processed for
 * spatialization.
 *
 * If the stream has been processed for spatialization, setting this to true will prevent
 * issues such as double-processing on platforms that will spatialize audio data.
 *
 * Available since API level 32.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param isSpatialized true if the content is already processed for binaural or transaural spatial
 *     rendering, false otherwise.
 */
AAUDIO_API void AAudioStreamBuilder_setIsContentSpatialized(AAudioStreamBuilder* _Nonnull builder,
                                                            bool isSpatialized) __INTRODUCED_IN(32);

/**
 * Set the input (capture) preset for the stream.
 *
 * The AAudio system will use this information to optimize the
 * behavior of the stream.
 * This could, for example, affect which microphones are used and how the
 * recorded data is processed.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_INPUT_PRESET_VOICE_RECOGNITION}.
 * That is because VOICE_RECOGNITION is the preset with the lowest latency
 * on many platforms.
 *
 * Available since API level 28.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param inputPreset the desired configuration for recording
 */
AAUDIO_API void AAudioStreamBuilder_setInputPreset(AAudioStreamBuilder* _Nonnull builder,
                                                   aaudio_input_preset_t inputPreset) __INTRODUCED_IN(28);

/**
 * Specify whether this stream audio may or may not be captured by other apps or the system.
 *
 * The default is {@link #AAUDIO_ALLOW_CAPTURE_BY_ALL}.
 *
 * Note that an application can also set its global policy, in which case the most restrictive
 * policy is always applied. See
 * <a href="/reference/android/media/AudioManager#setAllowedCapturePolicy(int)">
 * setAllowedCapturePolicy(int)</a>
 *
 * Available since API level 29.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param capturePolicy the desired level of opt-out from being captured.
 */
AAUDIO_API void AAudioStreamBuilder_setAllowedCapturePolicy(AAudioStreamBuilder* _Nonnull builder,
                                                            aaudio_allowed_capture_policy_t capturePolicy) __INTRODUCED_IN(29);

/** Set the requested session ID.
 *
 * The session ID can be used to associate a stream with effects processors.
 * The effects are controlled using the Android AudioEffect Java API.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_SESSION_ID_NONE}.
 *
 * If set to {@link #AAUDIO_SESSION_ID_ALLOCATE} then a session ID will be allocated
 * when the stream is opened.
 *
 * The allocated session ID can be obtained by calling AAudioStream_getSessionId()
 * and then used with this function when opening another stream.
 * This allows effects to be shared between streams.
 *
 * Session IDs from AAudio can be used with the Android Java APIs and vice versa.
 * So a session ID from an AAudio stream can be passed to Java
 * and effects applied using the Java AudioEffect API.
 *
 * Note that allocating or setting a session ID may result in a stream with higher latency.
 *
 * Allocated session IDs will always be positive and nonzero.
 *
 * Available since API level 28.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param sessionId an allocated sessionID or {@link #AAUDIO_SESSION_ID_ALLOCATE}
 */
AAUDIO_API void AAudioStreamBuilder_setSessionId(AAudioStreamBuilder* _Nonnull builder,
                                                 aaudio_session_id_t sessionId) __INTRODUCED_IN(28);

/** Indicates whether this input stream must be marked as privacy sensitive or not.
 *
 * When true, this input stream is privacy sensitive and any concurrent capture
 * is not permitted.
 *
 * This is off (false) by default except when the input preset is {@link #AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION}
 * or {@link #AAUDIO_INPUT_PRESET_CAMCORDER}.
 *
 * Always takes precedence over default from input preset when set explicitly.
 *
 * Only relevant if the stream direction is {@link #AAUDIO_DIRECTION_INPUT}.
 *
 * Added in API level 30.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param privacySensitive true if capture from this stream must be marked as privacy sensitive,
 * false otherwise.
 */
AAUDIO_API void AAudioStreamBuilder_setPrivacySensitive(AAudioStreamBuilder* _Nonnull builder,
                                                        bool privacySensitive) __INTRODUCED_IN(30);

/**
 * Return one of these values from the data callback function.
 */
enum {

    /**
     * Continue calling the callback.
     */
    AAUDIO_CALLBACK_RESULT_CONTINUE = 0,

    /**
     * Stop calling the callback.
     *
     * The application will still need to call AAudioStream_requestPause()
     * or AAudioStream_requestStop().
     */
    AAUDIO_CALLBACK_RESULT_STOP,

};
typedef int32_t aaudio_data_callback_result_t;

/**
 * Prototype for the data function that is passed to AAudioStreamBuilder_setDataCallback().
 *
 * For an output stream, this function should render and write numFrames of data
 * in the streams current data format to the audioData buffer.
 *
 * For an input stream, this function should read and process numFrames of data
 * from the audioData buffer. The data in the audioData buffer must not be modified
 * directly. Instead, it should be copied to another buffer before doing any modification.
 * In many cases, writing to the audioData buffer of an input stream will result in a
 * native exception.
 *
 * The audio data is passed through the buffer. So do NOT call AAudioStream_read() or
 * AAudioStream_write() on the stream that is making the callback.
 *
 * Note that numFrames can vary unless AAudioStreamBuilder_setFramesPerDataCallback()
 * is called.
 *
 * Also note that this callback function should be considered a "real-time" function.
 * It must not do anything that could cause an unbounded delay because that can cause the
 * audio to glitch or pop.
 *
 * These are things the function should NOT do:
 * <ul>
 * <li>allocate memory using, for example, malloc() or new</li>
 * <li>any file operations such as opening, closing, reading or writing</li>
 * <li>any network operations such as streaming</li>
 * <li>use any mutexes or other synchronization primitives</li>
 * <li>sleep</li>
 * <li>stop or close the stream</li>
 * <li>AAudioStream_read()</li>
 * <li>AAudioStream_write()</li>
 * </ul>
 *
 * The following are OK to call from the data callback:
 * <ul>
 * <li>AAudioStream_get*()</li>
 * <li>AAudio_convertResultToText()</li>
 * </ul>
 *
 * If you need to move data, eg. MIDI commands, in or out of the callback function then
 * we recommend the use of non-blocking techniques such as an atomic FIFO.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param userData the same address that was passed to AAudioStreamBuilder_setCallback()
 * @param audioData a pointer to the audio data
 * @param numFrames the number of frames to be processed, which can vary
 * @return AAUDIO_CALLBACK_RESULT_*
 */
typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(
    AAudioStream* _Nonnull stream,
    void* _Nullable userData,
    void* _Nonnull audioData,
    int32_t numFrames);

/**
 * Request that AAudio call this functions when the stream is running.
 *
 * Note that when using this callback, the audio data will be passed in or out
 * of the function as an argument.
 * So you cannot call AAudioStream_write() or AAudioStream_read()
 * on the same stream that has an active data callback.
 *
 * The callback function will start being called after AAudioStream_requestStart()
 * is called.
 * It will stop being called after AAudioStream_requestPause() or
 * AAudioStream_requestStop() is called.
 *
 * This callback function will be called on a real-time thread owned by AAudio.
 * The low latency streams may have callback threads with higher priority than normal streams.
 * See {@link #AAudioStream_dataCallback} for more information.
 *
 * Note that the AAudio callbacks will never be called simultaneously from multiple threads.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param callback pointer to a function that will process audio data.
 * @param userData pointer to an application data structure that will be passed
 *          to the callback functions.
 */
AAUDIO_API void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder* _Nonnull builder,
                                                    AAudioStream_dataCallback _Nullable callback, void* _Nullable userData)
    __INTRODUCED_IN(26);

/**
 * Set the requested data callback buffer size in frames.
 * See {@link #AAudioStream_dataCallback}.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 *
 * For the lowest possible latency, do not call this function. AAudio will then
 * call the dataProc callback function with whatever size is optimal.
 * That size may vary from one callback to another.
 *
 * Only use this function if the application requires a specific number of frames for processing.
 * The application might, for example, be using an FFT that requires
 * a specific power-of-two sized buffer.
 *
 * AAudio may need to add additional buffering in order to adapt between the internal
 * buffer size and the requested buffer size.
 *
 * If you do call this function then the requested size should be less than
 * half the buffer capacity, to allow double buffering.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param numFrames the desired buffer size in frames or {@link #AAUDIO_UNSPECIFIED}
 */
AAUDIO_API void AAudioStreamBuilder_setFramesPerDataCallback(AAudioStreamBuilder* _Nonnull builder,
                                                             int32_t numFrames) __INTRODUCED_IN(26);

/**
 * Prototype for the callback function that is passed to
 * AAudioStreamBuilder_setErrorCallback().
 *
 * The following may NOT be called from the error callback:
 * <ul>
 * <li>AAudioStream_requestStop()</li>
 * <li>AAudioStream_requestPause()</li>
 * <li>AAudioStream_close()</li>
 * <li>AAudioStream_waitForStateChange()</li>
 * <li>AAudioStream_read()</li>
 * <li>AAudioStream_write()</li>
 * </ul>
 *
 * The following are OK to call from the error callback:
 * <ul>
 * <li>AAudioStream_get*()</li>
 * <li>AAudio_convertResultToText()</li>
 * </ul>
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param userData the same address that was passed to AAudioStreamBuilder_setErrorCallback()
 * @param error an AAUDIO_ERROR_* value.
 */
typedef void (*AAudioStream_errorCallback)(
    AAudioStream* _Nonnull stream,
    void* _Nullable userData,
    aaudio_result_t error);

/**
 * Request that AAudio call this function if any error occurs or the stream is disconnected.
 *
 * It will be called, for example, if a headset or a USB device is unplugged causing the stream's
 * device to be unavailable or "disconnected".
 * Another possible cause of error would be a timeout or an unanticipated internal error.
 *
 * In response, this function should signal or create another thread to stop
 * and close this stream. The other thread could then reopen a stream on another device.
 * Do not stop or close the stream, or reopen the new stream, directly from this callback.
 *
 * This callback will not be called because of actions by the application, such as stopping
 * or closing a stream.
 *
 * Note that the AAudio callbacks will never be called simultaneously from multiple threads.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param callback pointer to a function that will be called if an error occurs.
 * @param userData pointer to an application data structure that will be passed
 *          to the callback functions.
 */
AAUDIO_API void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder* _Nonnull builder,
                                                     AAudioStream_errorCallback _Nullable callback, void* _Nullable userData)
    __INTRODUCED_IN(26);

/**
 * Open a stream based on the options in the StreamBuilder.
 *
 * AAudioStream_close() must be called when finished with the stream to recover
 * the memory and to free the associated resources.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param stream pointer to a variable to receive the new stream reference
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder* _Nonnull builder,
                                                          AAudioStream* _Nullable* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * Delete the resources associated with the StreamBuilder.
 *
 * Available since API level 26.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder* _Nonnull builder)
    __INTRODUCED_IN(26);

/**
 * Set audio channel mask for the stream.
 *
 * The default, if you do not call this function, is {@link #AAUDIO_UNSPECIFIED}.
 * If both channel mask and count are not set, then stereo will then be chosen when the
 * stream is opened.
 * After opening a stream with an unspecified value, the application must query for the
 * actual value, which may vary by device.
 *
 * If an exact value is specified then an opened stream will use that value.
 * If a stream cannot be opened with the specified value then the open will fail.
 *
 * As the corresponding channel count of provided channel mask here may be different
 * from the channel count used in {@link AAudioStreamBuilder_setChannelCount} or
 * {@link AAudioStreamBuilder_setSamplesPerFrame}, the last called function will be
 * respected if this function and {@link AAudioStreamBuilder_setChannelCount} or
 * {@link AAudioStreamBuilder_setSamplesPerFrame} are called.
 *
 * Available since API level 32.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param channelMask Audio channel mask desired.
 */
AAUDIO_API void AAudioStreamBuilder_setChannelMask(AAudioStreamBuilder* _Nonnull builder,
                                                   aaudio_channel_mask_t channelMask) __INTRODUCED_IN(32);

// ============================================================
// Stream Control
// ============================================================

/**
 * Free the audio resources associated with a stream created by
 * AAudioStreamBuilder_openStream().
 * AAudioStream_close() should be called at some point after calling
 * this function.
 *
 * After this call, the stream will be in {@link #AAUDIO_STREAM_STATE_CLOSING}
 *
 * This function is useful if you want to release the audio resources immediately,
 * but still allow queries to the stream to occur from other threads. This often
 * happens if you are monitoring stream progress from a UI thread.
 *
 * NOTE: This function is only fully implemented for MMAP streams,
 * which are low latency streams supported by some devices.
 * On other "Legacy" streams some audio resources will still be in use
 * and some callbacks may still be in process after this call.
 *
 * Available since API level 30.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_release(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(30);

/**
 * Delete the internal data structures associated with the stream created
 * by AAudioStreamBuilder_openStream().
 *
 * If AAudioStream_release() has not been called then it will be called automatically.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_close(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * Asynchronously request to start playing the stream. For output streams, one should
 * write to the stream to fill the buffer before starting.
 * Otherwise it will underflow.
 * After this call the state will be in {@link #AAUDIO_STREAM_STATE_STARTING} or
 * {@link #AAUDIO_STREAM_STATE_STARTED}.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_requestStart(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Asynchronous request for the stream to pause.
 * Pausing a stream will freeze the data flow but not flush any buffers.
 * Use AAudioStream_requestStart() to resume playback after a pause.
 * After this call the state will be in {@link #AAUDIO_STREAM_STATE_PAUSING} or
 * {@link #AAUDIO_STREAM_STATE_PAUSED}.
 *
 * This will return {@link #AAUDIO_ERROR_UNIMPLEMENTED} for input streams.
 * For input streams use AAudioStream_requestStop().
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_requestPause(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Asynchronous request for the stream to flush.
 * Flushing will discard any pending data.
 * This call only works if the stream is OPEN, PAUSED, STOPPED, or FLUSHED.
 * Calling this function when in other states,
 * or calling from an AAudio callback function,
 * will have no effect and an error will be returned.
 * Frame counters are not reset by a flush. They may be advanced.
 * After this call the state will be in {@link #AAUDIO_STREAM_STATE_FLUSHING} or
 * {@link #AAUDIO_STREAM_STATE_FLUSHED}.
 *
 * This will return {@link #AAUDIO_ERROR_UNIMPLEMENTED} for input streams.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_requestFlush(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Asynchronous request for the stream to stop.
 * The stream will stop after all of the data currently buffered has been played.
 * After this call the state will be in {@link #AAUDIO_STREAM_STATE_STOPPING} or
 * {@link #AAUDIO_STREAM_STATE_STOPPED}.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_requestStop(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Query the current state of the client, eg. {@link #AAUDIO_STREAM_STATE_PAUSING}
 *
 * This function will immediately return the state without updating the state.
 * If you want to update the client state based on the server state then
 * call AAudioStream_waitForStateChange() with currentState
 * set to {@link #AAUDIO_STREAM_STATE_UNKNOWN} and a zero timeout.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 */
AAUDIO_API aaudio_stream_state_t AAudioStream_getState(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Wait until the current state no longer matches the input state.
 *
 * This will update the current client state.
 *
 * <pre><code>
 * aaudio_result_t result = AAUDIO_OK;
 * aaudio_stream_state_t currentState = AAudioStream_getState(stream);
 * aaudio_stream_state_t inputState = currentState;
 * while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_PAUSED) {
 *     result = AAudioStream_waitForStateChange(
 *                                   stream, inputState, &currentState, MY_TIMEOUT_NANOS);
 *     inputState = currentState;
 * }
 * </code></pre>
 *
 * Available since API level 26.
 *
 * @param stream A reference provided by AAudioStreamBuilder_openStream()
 * @param inputState The state we want to avoid.
 * @param nextState Pointer to a variable that will be set to the new state.
 * @param timeoutNanoseconds Maximum number of nanoseconds to wait for completion.
 * @return {@link #AAUDIO_OK} or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_waitForStateChange(AAudioStream* _Nonnull stream,
                                                           aaudio_stream_state_t inputState, aaudio_stream_state_t* _Nullable nextState,
                                                           int64_t timeoutNanoseconds) __INTRODUCED_IN(26);

// ============================================================
// Stream I/O
// ============================================================

/**
 * Read data from the stream.
 *
 * The call will wait until the read is complete or until it runs out of time.
 * If timeoutNanos is zero then this call will not wait.
 *
 * Note that timeoutNanoseconds is a relative duration in wall clock time.
 * Time will not stop if the thread is asleep.
 * So it will be implemented using CLOCK_BOOTTIME.
 *
 * This call is "strong non-blocking" unless it has to wait for data.
 *
 * If the call times out then zero or a partial frame count will be returned.
 *
 * Available since API level 26.
 *
 * @param stream A stream created using AAudioStreamBuilder_openStream().
 * @param buffer The address of the first sample.
 * @param numFrames Number of frames to read. Only complete frames will be written.
 * @param timeoutNanoseconds Maximum number of nanoseconds to wait for completion.
 * @return The number of frames actually read or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_read(AAudioStream* _Nonnull stream,
                                             void* _Nonnull buffer, int32_t numFrames, int64_t timeoutNanoseconds) __INTRODUCED_IN(26);

/**
 * Write data to the stream.
 *
 * The call will wait until the write is complete or until it runs out of time.
 * If timeoutNanos is zero then this call will not wait.
 *
 * Note that timeoutNanoseconds is a relative duration in wall clock time.
 * Time will not stop if the thread is asleep.
 * So it will be implemented using CLOCK_BOOTTIME.
 *
 * This call is "strong non-blocking" unless it has to wait for room in the buffer.
 *
 * If the call times out then zero or a partial frame count will be returned.
 *
 * Available since API level 26.
 *
 * @param stream A stream created using AAudioStreamBuilder_openStream().
 * @param buffer The address of the first sample.
 * @param numFrames Number of frames to write. Only complete frames will be written.
 * @param timeoutNanoseconds Maximum number of nanoseconds to wait for completion.
 * @return The number of frames actually written or a negative error.
 */
AAUDIO_API aaudio_result_t AAudioStream_write(AAudioStream* _Nonnull stream,
                                              const void* _Nonnull buffer, int32_t numFrames, int64_t timeoutNanoseconds)
    __INTRODUCED_IN(26);

// ============================================================
// Stream - queries
// ============================================================

/**
 * This can be used to adjust the latency of the buffer by changing
 * the threshold where blocking will occur.
 * By combining this with AAudioStream_getXRunCount(), the latency can be tuned
 * at run-time for each device.
 *
 * This cannot be set higher than AAudioStream_getBufferCapacityInFrames().
 *
 * Note that you will probably not get the exact size you request.
 * You can check the return value or call AAudioStream_getBufferSizeInFrames()
 * to see what the actual final size is.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param numFrames requested number of frames that can be filled without blocking
 * @return actual buffer size in frames or a negative error
 */
AAUDIO_API aaudio_result_t AAudioStream_setBufferSizeInFrames(AAudioStream* _Nonnull stream,
                                                              int32_t numFrames) __INTRODUCED_IN(26);

/**
 * Query the maximum number of frames that can be filled without blocking.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return buffer size in frames.
 */
AAUDIO_API int32_t AAudioStream_getBufferSizeInFrames(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Query the number of frames that the application should read or write at
 * one time for optimal performance. It is OK if an application writes
 * a different number of frames. But the buffer size may need to be larger
 * in order to avoid underruns or overruns.
 *
 * Note that this may or may not match the actual device burst size.
 * For some endpoints, the burst size can vary dynamically.
 * But these tend to be devices with high latency.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return burst size
 */
AAUDIO_API int32_t AAudioStream_getFramesPerBurst(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Query maximum buffer capacity in frames.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return  buffer capacity in frames
 */
AAUDIO_API int32_t AAudioStream_getBufferCapacityInFrames(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Query the size of the buffer that will be passed to the dataProc callback
 * in the numFrames parameter.
 *
 * This call can be used if the application needs to know the value of numFrames before
 * the stream is started. This is not normally necessary.
 *
 * If a specific size was requested by calling
 * AAudioStreamBuilder_setFramesPerDataCallback() then this will be the same size.
 *
 * If AAudioStreamBuilder_setFramesPerDataCallback() was not called then this will
 * return the size chosen by AAudio, or {@link #AAUDIO_UNSPECIFIED}.
 *
 * {@link #AAUDIO_UNSPECIFIED} indicates that the callback buffer size for this stream
 * may vary from one dataProc callback to the next.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return callback buffer size in frames or {@link #AAUDIO_UNSPECIFIED}
 */
AAUDIO_API int32_t AAudioStream_getFramesPerDataCallback(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * An XRun is an Underrun or an Overrun.
 * During playing, an underrun will occur if the stream is not written in time
 * and the system runs out of valid data.
 * During recording, an overrun will occur if the stream is not read in time
 * and there is no place to put the incoming data so it is discarded.
 *
 * An underrun or overrun can cause an audible "pop" or "glitch".
 *
 * Note that some INPUT devices may not support this function.
 * In that case a 0 will always be returned.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return the underrun or overrun count
 */
AAUDIO_API int32_t AAudioStream_getXRunCount(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual sample rate of the stream
 */
AAUDIO_API int32_t AAudioStream_getSampleRate(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * There may be sample rate conversions in the Audio framework.
 * The sample rate set in the stream builder may not be actual sample rate used in the hardware.
 *
 * This returns the sample rate used by the hardware in Hertz.
 *
 * If AAudioStreamBuilder_openStream() returned AAUDIO_OK, the result should always be valid.
 *
 * Available since API level 34.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual sample rate of the underlying hardware
 */
AAUDIO_API int32_t AAudioStream_getHardwareSampleRate(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(__ANDROID_API_U__);

/**
 * A stream has one or more channels of data.
 * A frame will contain one sample for each channel.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual number of channels of the stream
 */
AAUDIO_API int32_t AAudioStream_getChannelCount(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * There may be channel conversions in the Audio framework.
 * The channel count or channel mask set in the stream builder may not be actual number of
 * channels used in the hardware.
 *
 * This returns the channel count used by the hardware.
 *
 * If AAudioStreamBuilder_openStream() returned AAUDIO_OK, the result should always be valid.
 *
 * Available since API level 34.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual number of channels of the underlying hardware
 */
AAUDIO_API int32_t AAudioStream_getHardwareChannelCount(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(__ANDROID_API_U__);

/**
 * Identical to AAudioStream_getChannelCount().
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual number of samples frame
 */
AAUDIO_API int32_t AAudioStream_getSamplesPerFrame(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual device ID
 */
AAUDIO_API int32_t AAudioStream_getDeviceId(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual data format of the stream
 */
AAUDIO_API aaudio_format_t AAudioStream_getFormat(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * There may be data format conversions in the Audio framework.
 * The data format set in the stream builder may not be actual format used in the hardware.
 *
 * This returns the audio format used by the hardware.
 *
 * If AAudioStreamBuilder_openStream() returned AAUDIO_OK, this should always return an
 * aaudio_format_t.
 *
 * AUDIO_FORMAT_PCM_8_24_BIT is currently not supported in AAudio, but the hardware may use it.
 * If AUDIO_FORMAT_PCM_8_24_BIT is used by the hardware, return AAUDIO_FORMAT_PCM_I24_PACKED.
 *
 * If any other format used by the hardware is not supported by AAudio, this will return
 * AAUDIO_FORMAT_INVALID.
 *
 * Available since API level 34.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual data format of the underlying hardware.
 */
AAUDIO_API aaudio_format_t AAudioStream_getHardwareFormat(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(__ANDROID_API_U__);

/**
 * Provide actual sharing mode.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return  actual sharing mode
 */
AAUDIO_API aaudio_sharing_mode_t AAudioStream_getSharingMode(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Get the performance mode used by the stream.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 */
AAUDIO_API aaudio_performance_mode_t AAudioStream_getPerformanceMode(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return direction
 */
AAUDIO_API aaudio_direction_t AAudioStream_getDirection(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Passes back the number of frames that have been written since the stream was created.
 * For an output stream, this will be advanced by the application calling write()
 * or by a data callback.
 * For an input stream, this will be advanced by the endpoint.
 *
 * The frame position is monotonically increasing.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return frames written
 */
AAUDIO_API int64_t AAudioStream_getFramesWritten(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(26);

/**
 * Passes back the number of frames that have been read since the stream was created.
 * For an output stream, this will be advanced by the endpoint.
 * For an input stream, this will be advanced by the application calling read()
 * or by a data callback.
 *
 * The frame position is monotonically increasing.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return frames read
 */
AAUDIO_API int64_t AAudioStream_getFramesRead(AAudioStream* _Nonnull stream) __INTRODUCED_IN(26);

/**
 * Passes back the session ID associated with this stream.
 *
 * The session ID can be used to associate a stream with effects processors.
 * The effects are controlled using the Android AudioEffect Java API.
 *
 * If AAudioStreamBuilder_setSessionId() was
 * called with {@link #AAUDIO_SESSION_ID_ALLOCATE}
 * then a new session ID should be allocated once when the stream is opened.
 *
 * If AAudioStreamBuilder_setSessionId() was called with a previously allocated
 * session ID then that value should be returned.
 *
 * If AAudioStreamBuilder_setSessionId() was not called then this function should
 * return {@link #AAUDIO_SESSION_ID_NONE}.
 *
 * The sessionID for a stream should not change once the stream has been opened.
 *
 * Available since API level 28.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return session ID or {@link #AAUDIO_SESSION_ID_NONE}
 */
AAUDIO_API aaudio_session_id_t AAudioStream_getSessionId(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(28);

/**
 * Passes back the time at which a particular frame was presented.
 * This can be used to synchronize audio with video or MIDI.
 * It can also be used to align a recorded stream with a playback stream.
 *
 * Timestamps are only valid when the stream is in {@link #AAUDIO_STREAM_STATE_STARTED}.
 * {@link #AAUDIO_ERROR_INVALID_STATE} will be returned if the stream is not started.
 * Note that because requestStart() is asynchronous, timestamps will not be valid until
 * a short time after calling requestStart().
 * So {@link #AAUDIO_ERROR_INVALID_STATE} should not be considered a fatal error.
 * Just try calling again later.
 *
 * If an error occurs, then the position and time will not be modified.
 *
 * The position and time passed back are monotonically increasing.
 *
 * Available since API level 26.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param clockid CLOCK_MONOTONIC or CLOCK_BOOTTIME
 * @param framePosition pointer to a variable to receive the position
 * @param timeNanoseconds pointer to a variable to receive the time
 * @return {@link #AAUDIO_OK} or a negative error
 */
AAUDIO_API aaudio_result_t AAudioStream_getTimestamp(AAudioStream* _Nonnull stream,
                                                     clockid_t clockid, int64_t* _Nonnull framePosition, int64_t* _Nonnull timeNanoseconds)
    __INTRODUCED_IN(26);

/**
 * Return the use case for the stream.
 *
 * Available since API level 28.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return frames read
 */
AAUDIO_API aaudio_usage_t AAudioStream_getUsage(AAudioStream* _Nonnull stream) __INTRODUCED_IN(28);

/**
 * Return the content type for the stream.
 *
 * Available since API level 28.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return content type, for example {@link #AAUDIO_CONTENT_TYPE_MUSIC}
 */
AAUDIO_API aaudio_content_type_t AAudioStream_getContentType(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(28);

/**
 * Return the spatialization behavior for the stream.
 *
 * If none was explicitly set, it will return the default
 * {@link #AAUDIO_SPATIALIZATION_BEHAVIOR_AUTO} behavior.
 *
 * Available since API level 32.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return spatialization behavior, for example {@link #AAUDIO_SPATIALIZATION_BEHAVIOR_AUTO}
 */
AAUDIO_API aaudio_spatialization_behavior_t AAudioStream_getSpatializationBehavior(
    AAudioStream* _Nonnull stream) __INTRODUCED_IN(32);

/**
 * Return whether the content of the stream is spatialized.
 *
 * Available since API level 32.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return true if the content is spatialized
 */
AAUDIO_API bool AAudioStream_isContentSpatialized(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(32);

/**
 * Return the input preset for the stream.
 *
 * Available since API level 28.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return input preset, for example {@link #AAUDIO_INPUT_PRESET_CAMCORDER}
 */
AAUDIO_API aaudio_input_preset_t AAudioStream_getInputPreset(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(28);

/**
 * Return the policy that determines whether the audio may or may not be captured
 * by other apps or the system.
 *
 * Available since API level 29.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return the allowed capture policy, for example {@link #AAUDIO_ALLOW_CAPTURE_BY_ALL}
 */
AAUDIO_API aaudio_allowed_capture_policy_t AAudioStream_getAllowedCapturePolicy(
    AAudioStream* _Nonnull stream) __INTRODUCED_IN(29);

/**
 * Return whether this input stream is marked as privacy sensitive or not.
 *
 * See {@link #AAudioStreamBuilder_setPrivacySensitive()}.
 *
 * Added in API level 30.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return true if privacy sensitive, false otherwise
 */
AAUDIO_API bool AAudioStream_isPrivacySensitive(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(30);

/**
 * Return the channel mask for the stream. This will be the mask set using
 * {@link #AAudioStreamBuilder_setChannelMask}, or {@link #AAUDIO_UNSPECIFIED} otherwise.
 *
 * Available since API level 32.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @return actual channel mask
 */
AAUDIO_API aaudio_channel_mask_t AAudioStream_getChannelMask(AAudioStream* _Nonnull stream)
    __INTRODUCED_IN(32);

#ifdef __cplusplus
}
#endif

#endif  // AAUDIO_AAUDIO_H

/** @} */