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

dictionary MediaStreamAudioSourceOptions {
    required MediaStream mediaStream;
};

[Pref="dom.webaudio.enabled",
 Exposed=Window]
interface MediaStreamAudioSourceNode : AudioNode {
  [Throws]
  constructor(AudioContext context, MediaStreamAudioSourceOptions options);

  [BinaryName="GetMediaStream"]
  readonly attribute MediaStream mediaStream;
};

// Mozilla extensions
MediaStreamAudioSourceNode includes AudioNodePassThrough;

