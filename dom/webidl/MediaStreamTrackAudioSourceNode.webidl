/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary MediaStreamTrackAudioSourceOptions {
    required MediaStreamTrack mediaStreamTrack;
};

[Pref="dom.webaudio.enabled",
 Exposed=Window]
interface MediaStreamTrackAudioSourceNode : AudioNode {
  [Throws]
  constructor(AudioContext context, MediaStreamTrackAudioSourceOptions options);
};

// Mozilla extensions
MediaStreamTrackAudioSourceNode includes AudioNodePassThrough;
