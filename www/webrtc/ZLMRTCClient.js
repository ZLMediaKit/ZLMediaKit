var ZLMRTCClient = (function (exports) {
	'use strict';

	const Events$1 = {
	  WEBRTC_NOT_SUPPORT: 'WEBRTC_NOT_SUPPORT',
	  WEBRTC_ICE_CANDIDATE_ERROR: 'WEBRTC_ICE_CANDIDATE_ERROR',
	  WEBRTC_OFFER_ANWSER_EXCHANGE_FAILED: 'WEBRTC_OFFER_ANWSER_EXCHANGE_FAILED',
	  WEBRTC_ON_REMOTE_STREAMS: 'WEBRTC_ON_REMOTE_STREAMS',
	  WEBRTC_ON_LOCAL_STREAM: 'WEBRTC_ON_LOCAL_STREAM',
	  WEBRTC_ON_CONNECTION_STATE_CHANGE: 'WEBRTC_ON_CONNECTION_STATE_CHANGE',
	  WEBRTC_ON_DATA_CHANNEL_OPEN: 'WEBRTC_ON_DATA_CHANNEL_OPEN',
	  WEBRTC_ON_DATA_CHANNEL_CLOSE: 'WEBRTC_ON_DATA_CHANNEL_CLOSE',
	  WEBRTC_ON_DATA_CHANNEL_ERR: 'WEBRTC_ON_DATA_CHANNEL_ERR',
	  WEBRTC_ON_DATA_CHANNEL_MSG: 'WEBRTC_ON_DATA_CHANNEL_MSG',
	  CAPTURE_STREAM_FAILED: 'CAPTURE_STREAM_FAILED'
	};

	const VERSION$1 = '1.0.1';
	const BUILD_DATE = 'Mon Mar 27 2023 19:11:59 GMT+0800 (China Standard Time)';

	// Copyright (C) <2018> Intel Corporation
	//
	// SPDX-License-Identifier: Apache-2.0

	// eslint-disable-next-line require-jsdoc
	function isFirefox() {
	  return window.navigator.userAgent.match('Firefox') !== null;
	}
	// eslint-disable-next-line require-jsdoc
	function isChrome() {
	  return window.navigator.userAgent.match('Chrome') !== null;
	}
	// eslint-disable-next-line require-jsdoc
	function isEdge() {
	  return window.navigator.userAgent.match(/Edge\/(\d+).(\d+)$/) !== null;
	}

	// Copyright (C) <2018> Intel Corporation

	/**
	 * @class AudioSourceInfo
	 * @classDesc Source info about an audio track. Values: 'mic', 'screen-cast', 'file', 'mixed'.
	 * @memberOf Owt.Base
	 * @readonly
	 * @enum {string}
	 */
	const AudioSourceInfo = {
	  MIC: 'mic',
	  SCREENCAST: 'screen-cast',
	  FILE: 'file',
	  MIXED: 'mixed'
	};

	/**
	 * @class VideoSourceInfo
	 * @classDesc Source info about a video track. Values: 'camera', 'screen-cast', 'file', 'mixed'.
	 * @memberOf Owt.Base
	 * @readonly
	 * @enum {string}
	 */
	const VideoSourceInfo = {
	  CAMERA: 'camera',
	  SCREENCAST: 'screen-cast',
	  FILE: 'file',
	  MIXED: 'mixed'
	};

	/**
	 * @class TrackKind
	 * @classDesc Kind of a track. Values: 'audio' for audio track, 'video' for video track, 'av' for both audio and video tracks.
	 * @memberOf Owt.Base
	 * @readonly
	 * @enum {string}
	 */
	const TrackKind = {
	  /**
	   * Audio tracks.
	   * @type string
	   */
	  AUDIO: 'audio',
	  /**
	   * Video tracks.
	   * @type string
	   */
	  VIDEO: 'video',
	  /**
	   * Both audio and video tracks.
	   * @type string
	   */
	  AUDIO_AND_VIDEO: 'av'
	};
	/**
	 * @class Resolution
	 * @memberOf Owt.Base
	 * @classDesc The Resolution defines the size of a rectangle.
	 * @constructor
	 * @param {number} width
	 * @param {number} height
	 */
	class Resolution {
	  // eslint-disable-next-line require-jsdoc
	  constructor(width, height) {
	    /**
	     * @member {number} width
	     * @instance
	     * @memberof Owt.Base.Resolution
	     */
	    this.width = width;
	    /**
	     * @member {number} height
	     * @instance
	     * @memberof Owt.Base.Resolution
	     */
	    this.height = height;
	  }
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	let logDisabled_ = true;
	let deprecationWarnings_ = true;

	/**
	 * Extract browser version out of the provided user agent string.
	 *
	 * @param {!string} uastring userAgent string.
	 * @param {!string} expr Regular expression used as match criteria.
	 * @param {!number} pos position in the version string to be returned.
	 * @return {!number} browser version.
	 */
	function extractVersion(uastring, expr, pos) {
	  const match = uastring.match(expr);
	  return match && match.length >= pos && parseInt(match[pos], 10);
	}

	// Wraps the peerconnection event eventNameToWrap in a function
	// which returns the modified event object (or false to prevent
	// the event).
	function wrapPeerConnectionEvent(window, eventNameToWrap, wrapper) {
	  if (!window.RTCPeerConnection) {
	    return;
	  }
	  const proto = window.RTCPeerConnection.prototype;
	  const nativeAddEventListener = proto.addEventListener;
	  proto.addEventListener = function(nativeEventName, cb) {
	    if (nativeEventName !== eventNameToWrap) {
	      return nativeAddEventListener.apply(this, arguments);
	    }
	    const wrappedCallback = (e) => {
	      const modifiedEvent = wrapper(e);
	      if (modifiedEvent) {
	        if (cb.handleEvent) {
	          cb.handleEvent(modifiedEvent);
	        } else {
	          cb(modifiedEvent);
	        }
	      }
	    };
	    this._eventMap = this._eventMap || {};
	    if (!this._eventMap[eventNameToWrap]) {
	      this._eventMap[eventNameToWrap] = new Map();
	    }
	    this._eventMap[eventNameToWrap].set(cb, wrappedCallback);
	    return nativeAddEventListener.apply(this, [nativeEventName,
	      wrappedCallback]);
	  };

	  const nativeRemoveEventListener = proto.removeEventListener;
	  proto.removeEventListener = function(nativeEventName, cb) {
	    if (nativeEventName !== eventNameToWrap || !this._eventMap
	        || !this._eventMap[eventNameToWrap]) {
	      return nativeRemoveEventListener.apply(this, arguments);
	    }
	    if (!this._eventMap[eventNameToWrap].has(cb)) {
	      return nativeRemoveEventListener.apply(this, arguments);
	    }
	    const unwrappedCb = this._eventMap[eventNameToWrap].get(cb);
	    this._eventMap[eventNameToWrap].delete(cb);
	    if (this._eventMap[eventNameToWrap].size === 0) {
	      delete this._eventMap[eventNameToWrap];
	    }
	    if (Object.keys(this._eventMap).length === 0) {
	      delete this._eventMap;
	    }
	    return nativeRemoveEventListener.apply(this, [nativeEventName,
	      unwrappedCb]);
	  };

	  Object.defineProperty(proto, 'on' + eventNameToWrap, {
	    get() {
	      return this['_on' + eventNameToWrap];
	    },
	    set(cb) {
	      if (this['_on' + eventNameToWrap]) {
	        this.removeEventListener(eventNameToWrap,
	            this['_on' + eventNameToWrap]);
	        delete this['_on' + eventNameToWrap];
	      }
	      if (cb) {
	        this.addEventListener(eventNameToWrap,
	            this['_on' + eventNameToWrap] = cb);
	      }
	    },
	    enumerable: true,
	    configurable: true
	  });
	}

	function disableLog(bool) {
	  if (typeof bool !== 'boolean') {
	    return new Error('Argument type: ' + typeof bool +
	        '. Please use a boolean.');
	  }
	  logDisabled_ = bool;
	  return (bool) ? 'adapter.js logging disabled' :
	      'adapter.js logging enabled';
	}

	/**
	 * Disable or enable deprecation warnings
	 * @param {!boolean} bool set to true to disable warnings.
	 */
	function disableWarnings(bool) {
	  if (typeof bool !== 'boolean') {
	    return new Error('Argument type: ' + typeof bool +
	        '. Please use a boolean.');
	  }
	  deprecationWarnings_ = !bool;
	  return 'adapter.js deprecation warnings ' + (bool ? 'disabled' : 'enabled');
	}

	function log$1() {
	  if (typeof window === 'object') {
	    if (logDisabled_) {
	      return;
	    }
	    if (typeof console !== 'undefined' && typeof console.log === 'function') {
	      console.log.apply(console, arguments);
	    }
	  }
	}

	/**
	 * Shows a deprecation warning suggesting the modern and spec-compatible API.
	 */
	function deprecated(oldMethod, newMethod) {
	  if (!deprecationWarnings_) {
	    return;
	  }
	  console.warn(oldMethod + ' is deprecated, please use ' + newMethod +
	      ' instead.');
	}

	/**
	 * Browser detector.
	 *
	 * @return {object} result containing browser and version
	 *     properties.
	 */
	function detectBrowser(window) {
	  // Returned result object.
	  const result = {browser: null, version: null};

	  // Fail early if it's not a browser
	  if (typeof window === 'undefined' || !window.navigator) {
	    result.browser = 'Not a browser.';
	    return result;
	  }

	  const {navigator} = window;

	  if (navigator.mozGetUserMedia) { // Firefox.
	    result.browser = 'firefox';
	    result.version = extractVersion(navigator.userAgent,
	        /Firefox\/(\d+)\./, 1);
	  } else if (navigator.webkitGetUserMedia ||
	      (window.isSecureContext === false && window.webkitRTCPeerConnection &&
	       !window.RTCIceGatherer)) {
	    // Chrome, Chromium, Webview, Opera.
	    // Version matches Chrome/WebRTC version.
	    // Chrome 74 removed webkitGetUserMedia on http as well so we need the
	    // more complicated fallback to webkitRTCPeerConnection.
	    result.browser = 'chrome';
	    result.version = extractVersion(navigator.userAgent,
	        /Chrom(e|ium)\/(\d+)\./, 2);
	  } else if (navigator.mediaDevices &&
	      navigator.userAgent.match(/Edge\/(\d+).(\d+)$/)) { // Edge.
	    result.browser = 'edge';
	    result.version = extractVersion(navigator.userAgent,
	        /Edge\/(\d+).(\d+)$/, 2);
	  } else if (window.RTCPeerConnection &&
	      navigator.userAgent.match(/AppleWebKit\/(\d+)\./)) { // Safari.
	    result.browser = 'safari';
	    result.version = extractVersion(navigator.userAgent,
	        /AppleWebKit\/(\d+)\./, 1);
	    result.supportsUnifiedPlan = window.RTCRtpTransceiver &&
	        'currentDirection' in window.RTCRtpTransceiver.prototype;
	  } else { // Default fallthrough: not supported.
	    result.browser = 'Not a supported browser.';
	    return result;
	  }

	  return result;
	}

	/**
	 * Checks if something is an object.
	 *
	 * @param {*} val The something you want to check.
	 * @return true if val is an object, false otherwise.
	 */
	function isObject$1(val) {
	  return Object.prototype.toString.call(val) === '[object Object]';
	}

	/**
	 * Remove all empty objects and undefined values
	 * from a nested object -- an enhanced and vanilla version
	 * of Lodash's `compact`.
	 */
	function compactObject(data) {
	  if (!isObject$1(data)) {
	    return data;
	  }

	  return Object.keys(data).reduce(function(accumulator, key) {
	    const isObj = isObject$1(data[key]);
	    const value = isObj ? compactObject(data[key]) : data[key];
	    const isEmptyObject = isObj && !Object.keys(value).length;
	    if (value === undefined || isEmptyObject) {
	      return accumulator;
	    }
	    return Object.assign(accumulator, {[key]: value});
	  }, {});
	}

	/* iterates the stats graph recursively. */
	function walkStats(stats, base, resultSet) {
	  if (!base || resultSet.has(base.id)) {
	    return;
	  }
	  resultSet.set(base.id, base);
	  Object.keys(base).forEach(name => {
	    if (name.endsWith('Id')) {
	      walkStats(stats, stats.get(base[name]), resultSet);
	    } else if (name.endsWith('Ids')) {
	      base[name].forEach(id => {
	        walkStats(stats, stats.get(id), resultSet);
	      });
	    }
	  });
	}

	/* filter getStats for a sender/receiver track. */
	function filterStats(result, track, outbound) {
	  const streamStatsType = outbound ? 'outbound-rtp' : 'inbound-rtp';
	  const filteredResult = new Map();
	  if (track === null) {
	    return filteredResult;
	  }
	  const trackStats = [];
	  result.forEach(value => {
	    if (value.type === 'track' &&
	        value.trackIdentifier === track.id) {
	      trackStats.push(value);
	    }
	  });
	  trackStats.forEach(trackStat => {
	    result.forEach(stats => {
	      if (stats.type === streamStatsType && stats.trackId === trackStat.id) {
	        walkStats(result, stats, filteredResult);
	      }
	    });
	  });
	  return filteredResult;
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */
	const logging = log$1;

	function shimGetUserMedia$3(window, browserDetails) {
	  const navigator = window && window.navigator;

	  if (!navigator.mediaDevices) {
	    return;
	  }

	  const constraintsToChrome_ = function(c) {
	    if (typeof c !== 'object' || c.mandatory || c.optional) {
	      return c;
	    }
	    const cc = {};
	    Object.keys(c).forEach(key => {
	      if (key === 'require' || key === 'advanced' || key === 'mediaSource') {
	        return;
	      }
	      const r = (typeof c[key] === 'object') ? c[key] : {ideal: c[key]};
	      if (r.exact !== undefined && typeof r.exact === 'number') {
	        r.min = r.max = r.exact;
	      }
	      const oldname_ = function(prefix, name) {
	        if (prefix) {
	          return prefix + name.charAt(0).toUpperCase() + name.slice(1);
	        }
	        return (name === 'deviceId') ? 'sourceId' : name;
	      };
	      if (r.ideal !== undefined) {
	        cc.optional = cc.optional || [];
	        let oc = {};
	        if (typeof r.ideal === 'number') {
	          oc[oldname_('min', key)] = r.ideal;
	          cc.optional.push(oc);
	          oc = {};
	          oc[oldname_('max', key)] = r.ideal;
	          cc.optional.push(oc);
	        } else {
	          oc[oldname_('', key)] = r.ideal;
	          cc.optional.push(oc);
	        }
	      }
	      if (r.exact !== undefined && typeof r.exact !== 'number') {
	        cc.mandatory = cc.mandatory || {};
	        cc.mandatory[oldname_('', key)] = r.exact;
	      } else {
	        ['min', 'max'].forEach(mix => {
	          if (r[mix] !== undefined) {
	            cc.mandatory = cc.mandatory || {};
	            cc.mandatory[oldname_(mix, key)] = r[mix];
	          }
	        });
	      }
	    });
	    if (c.advanced) {
	      cc.optional = (cc.optional || []).concat(c.advanced);
	    }
	    return cc;
	  };

	  const shimConstraints_ = function(constraints, func) {
	    if (browserDetails.version >= 61) {
	      return func(constraints);
	    }
	    constraints = JSON.parse(JSON.stringify(constraints));
	    if (constraints && typeof constraints.audio === 'object') {
	      const remap = function(obj, a, b) {
	        if (a in obj && !(b in obj)) {
	          obj[b] = obj[a];
	          delete obj[a];
	        }
	      };
	      constraints = JSON.parse(JSON.stringify(constraints));
	      remap(constraints.audio, 'autoGainControl', 'googAutoGainControl');
	      remap(constraints.audio, 'noiseSuppression', 'googNoiseSuppression');
	      constraints.audio = constraintsToChrome_(constraints.audio);
	    }
	    if (constraints && typeof constraints.video === 'object') {
	      // Shim facingMode for mobile & surface pro.
	      let face = constraints.video.facingMode;
	      face = face && ((typeof face === 'object') ? face : {ideal: face});
	      const getSupportedFacingModeLies = browserDetails.version < 66;

	      if ((face && (face.exact === 'user' || face.exact === 'environment' ||
	                    face.ideal === 'user' || face.ideal === 'environment')) &&
	          !(navigator.mediaDevices.getSupportedConstraints &&
	            navigator.mediaDevices.getSupportedConstraints().facingMode &&
	            !getSupportedFacingModeLies)) {
	        delete constraints.video.facingMode;
	        let matches;
	        if (face.exact === 'environment' || face.ideal === 'environment') {
	          matches = ['back', 'rear'];
	        } else if (face.exact === 'user' || face.ideal === 'user') {
	          matches = ['front'];
	        }
	        if (matches) {
	          // Look for matches in label, or use last cam for back (typical).
	          return navigator.mediaDevices.enumerateDevices()
	          .then(devices => {
	            devices = devices.filter(d => d.kind === 'videoinput');
	            let dev = devices.find(d => matches.some(match =>
	              d.label.toLowerCase().includes(match)));
	            if (!dev && devices.length && matches.includes('back')) {
	              dev = devices[devices.length - 1]; // more likely the back cam
	            }
	            if (dev) {
	              constraints.video.deviceId = face.exact ? {exact: dev.deviceId} :
	                                                        {ideal: dev.deviceId};
	            }
	            constraints.video = constraintsToChrome_(constraints.video);
	            logging('chrome: ' + JSON.stringify(constraints));
	            return func(constraints);
	          });
	        }
	      }
	      constraints.video = constraintsToChrome_(constraints.video);
	    }
	    logging('chrome: ' + JSON.stringify(constraints));
	    return func(constraints);
	  };

	  const shimError_ = function(e) {
	    if (browserDetails.version >= 64) {
	      return e;
	    }
	    return {
	      name: {
	        PermissionDeniedError: 'NotAllowedError',
	        PermissionDismissedError: 'NotAllowedError',
	        InvalidStateError: 'NotAllowedError',
	        DevicesNotFoundError: 'NotFoundError',
	        ConstraintNotSatisfiedError: 'OverconstrainedError',
	        TrackStartError: 'NotReadableError',
	        MediaDeviceFailedDueToShutdown: 'NotAllowedError',
	        MediaDeviceKillSwitchOn: 'NotAllowedError',
	        TabCaptureError: 'AbortError',
	        ScreenCaptureError: 'AbortError',
	        DeviceCaptureError: 'AbortError'
	      }[e.name] || e.name,
	      message: e.message,
	      constraint: e.constraint || e.constraintName,
	      toString() {
	        return this.name + (this.message && ': ') + this.message;
	      }
	    };
	  };

	  const getUserMedia_ = function(constraints, onSuccess, onError) {
	    shimConstraints_(constraints, c => {
	      navigator.webkitGetUserMedia(c, onSuccess, e => {
	        if (onError) {
	          onError(shimError_(e));
	        }
	      });
	    });
	  };
	  navigator.getUserMedia = getUserMedia_.bind(navigator);

	  // Even though Chrome 45 has navigator.mediaDevices and a getUserMedia
	  // function which returns a Promise, it does not accept spec-style
	  // constraints.
	  if (navigator.mediaDevices.getUserMedia) {
	    const origGetUserMedia = navigator.mediaDevices.getUserMedia.
	        bind(navigator.mediaDevices);
	    navigator.mediaDevices.getUserMedia = function(cs) {
	      return shimConstraints_(cs, c => origGetUserMedia(c).then(stream => {
	        if (c.audio && !stream.getAudioTracks().length ||
	            c.video && !stream.getVideoTracks().length) {
	          stream.getTracks().forEach(track => {
	            track.stop();
	          });
	          throw new DOMException('', 'NotFoundError');
	        }
	        return stream;
	      }, e => Promise.reject(shimError_(e))));
	    };
	  }
	}

	/*
	 *  Copyright (c) 2018 The adapter.js project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */
	function shimGetDisplayMedia$2(window, getSourceId) {
	  if (window.navigator.mediaDevices &&
	    'getDisplayMedia' in window.navigator.mediaDevices) {
	    return;
	  }
	  if (!(window.navigator.mediaDevices)) {
	    return;
	  }
	  // getSourceId is a function that returns a promise resolving with
	  // the sourceId of the screen/window/tab to be shared.
	  if (typeof getSourceId !== 'function') {
	    console.error('shimGetDisplayMedia: getSourceId argument is not ' +
	        'a function');
	    return;
	  }
	  window.navigator.mediaDevices.getDisplayMedia =
	    function getDisplayMedia(constraints) {
	      return getSourceId(constraints)
	        .then(sourceId => {
	          const widthSpecified = constraints.video && constraints.video.width;
	          const heightSpecified = constraints.video &&
	            constraints.video.height;
	          const frameRateSpecified = constraints.video &&
	            constraints.video.frameRate;
	          constraints.video = {
	            mandatory: {
	              chromeMediaSource: 'desktop',
	              chromeMediaSourceId: sourceId,
	              maxFrameRate: frameRateSpecified || 3
	            }
	          };
	          if (widthSpecified) {
	            constraints.video.mandatory.maxWidth = widthSpecified;
	          }
	          if (heightSpecified) {
	            constraints.video.mandatory.maxHeight = heightSpecified;
	          }
	          return window.navigator.mediaDevices.getUserMedia(constraints);
	        });
	    };
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimMediaStream(window) {
	  window.MediaStream = window.MediaStream || window.webkitMediaStream;
	}

	function shimOnTrack$1(window) {
	  if (typeof window === 'object' && window.RTCPeerConnection && !('ontrack' in
	      window.RTCPeerConnection.prototype)) {
	    Object.defineProperty(window.RTCPeerConnection.prototype, 'ontrack', {
	      get() {
	        return this._ontrack;
	      },
	      set(f) {
	        if (this._ontrack) {
	          this.removeEventListener('track', this._ontrack);
	        }
	        this.addEventListener('track', this._ontrack = f);
	      },
	      enumerable: true,
	      configurable: true
	    });
	    const origSetRemoteDescription =
	        window.RTCPeerConnection.prototype.setRemoteDescription;
	    window.RTCPeerConnection.prototype.setRemoteDescription =
	      function setRemoteDescription() {
	        if (!this._ontrackpoly) {
	          this._ontrackpoly = (e) => {
	            // onaddstream does not fire when a track is added to an existing
	            // stream. But stream.onaddtrack is implemented so we use that.
	            e.stream.addEventListener('addtrack', te => {
	              let receiver;
	              if (window.RTCPeerConnection.prototype.getReceivers) {
	                receiver = this.getReceivers()
	                  .find(r => r.track && r.track.id === te.track.id);
	              } else {
	                receiver = {track: te.track};
	              }

	              const event = new Event('track');
	              event.track = te.track;
	              event.receiver = receiver;
	              event.transceiver = {receiver};
	              event.streams = [e.stream];
	              this.dispatchEvent(event);
	            });
	            e.stream.getTracks().forEach(track => {
	              let receiver;
	              if (window.RTCPeerConnection.prototype.getReceivers) {
	                receiver = this.getReceivers()
	                  .find(r => r.track && r.track.id === track.id);
	              } else {
	                receiver = {track};
	              }
	              const event = new Event('track');
	              event.track = track;
	              event.receiver = receiver;
	              event.transceiver = {receiver};
	              event.streams = [e.stream];
	              this.dispatchEvent(event);
	            });
	          };
	          this.addEventListener('addstream', this._ontrackpoly);
	        }
	        return origSetRemoteDescription.apply(this, arguments);
	      };
	  } else {
	    // even if RTCRtpTransceiver is in window, it is only used and
	    // emitted in unified-plan. Unfortunately this means we need
	    // to unconditionally wrap the event.
	    wrapPeerConnectionEvent(window, 'track', e => {
	      if (!e.transceiver) {
	        Object.defineProperty(e, 'transceiver',
	          {value: {receiver: e.receiver}});
	      }
	      return e;
	    });
	  }
	}

	function shimGetSendersWithDtmf(window) {
	  // Overrides addTrack/removeTrack, depends on shimAddTrackRemoveTrack.
	  if (typeof window === 'object' && window.RTCPeerConnection &&
	      !('getSenders' in window.RTCPeerConnection.prototype) &&
	      'createDTMFSender' in window.RTCPeerConnection.prototype) {
	    const shimSenderWithDtmf = function(pc, track) {
	      return {
	        track,
	        get dtmf() {
	          if (this._dtmf === undefined) {
	            if (track.kind === 'audio') {
	              this._dtmf = pc.createDTMFSender(track);
	            } else {
	              this._dtmf = null;
	            }
	          }
	          return this._dtmf;
	        },
	        _pc: pc
	      };
	    };

	    // augment addTrack when getSenders is not available.
	    if (!window.RTCPeerConnection.prototype.getSenders) {
	      window.RTCPeerConnection.prototype.getSenders = function getSenders() {
	        this._senders = this._senders || [];
	        return this._senders.slice(); // return a copy of the internal state.
	      };
	      const origAddTrack = window.RTCPeerConnection.prototype.addTrack;
	      window.RTCPeerConnection.prototype.addTrack =
	        function addTrack(track, stream) {
	          let sender = origAddTrack.apply(this, arguments);
	          if (!sender) {
	            sender = shimSenderWithDtmf(this, track);
	            this._senders.push(sender);
	          }
	          return sender;
	        };

	      const origRemoveTrack = window.RTCPeerConnection.prototype.removeTrack;
	      window.RTCPeerConnection.prototype.removeTrack =
	        function removeTrack(sender) {
	          origRemoveTrack.apply(this, arguments);
	          const idx = this._senders.indexOf(sender);
	          if (idx !== -1) {
	            this._senders.splice(idx, 1);
	          }
	        };
	    }
	    const origAddStream = window.RTCPeerConnection.prototype.addStream;
	    window.RTCPeerConnection.prototype.addStream = function addStream(stream) {
	      this._senders = this._senders || [];
	      origAddStream.apply(this, [stream]);
	      stream.getTracks().forEach(track => {
	        this._senders.push(shimSenderWithDtmf(this, track));
	      });
	    };

	    const origRemoveStream = window.RTCPeerConnection.prototype.removeStream;
	    window.RTCPeerConnection.prototype.removeStream =
	      function removeStream(stream) {
	        this._senders = this._senders || [];
	        origRemoveStream.apply(this, [stream]);

	        stream.getTracks().forEach(track => {
	          const sender = this._senders.find(s => s.track === track);
	          if (sender) { // remove sender
	            this._senders.splice(this._senders.indexOf(sender), 1);
	          }
	        });
	      };
	  } else if (typeof window === 'object' && window.RTCPeerConnection &&
	             'getSenders' in window.RTCPeerConnection.prototype &&
	             'createDTMFSender' in window.RTCPeerConnection.prototype &&
	             window.RTCRtpSender &&
	             !('dtmf' in window.RTCRtpSender.prototype)) {
	    const origGetSenders = window.RTCPeerConnection.prototype.getSenders;
	    window.RTCPeerConnection.prototype.getSenders = function getSenders() {
	      const senders = origGetSenders.apply(this, []);
	      senders.forEach(sender => sender._pc = this);
	      return senders;
	    };

	    Object.defineProperty(window.RTCRtpSender.prototype, 'dtmf', {
	      get() {
	        if (this._dtmf === undefined) {
	          if (this.track.kind === 'audio') {
	            this._dtmf = this._pc.createDTMFSender(this.track);
	          } else {
	            this._dtmf = null;
	          }
	        }
	        return this._dtmf;
	      }
	    });
	  }
	}

	function shimGetStats(window) {
	  if (!window.RTCPeerConnection) {
	    return;
	  }

	  const origGetStats = window.RTCPeerConnection.prototype.getStats;
	  window.RTCPeerConnection.prototype.getStats = function getStats() {
	    const [selector, onSucc, onErr] = arguments;

	    // If selector is a function then we are in the old style stats so just
	    // pass back the original getStats format to avoid breaking old users.
	    if (arguments.length > 0 && typeof selector === 'function') {
	      return origGetStats.apply(this, arguments);
	    }

	    // When spec-style getStats is supported, return those when called with
	    // either no arguments or the selector argument is null.
	    if (origGetStats.length === 0 && (arguments.length === 0 ||
	        typeof selector !== 'function')) {
	      return origGetStats.apply(this, []);
	    }

	    const fixChromeStats_ = function(response) {
	      const standardReport = {};
	      const reports = response.result();
	      reports.forEach(report => {
	        const standardStats = {
	          id: report.id,
	          timestamp: report.timestamp,
	          type: {
	            localcandidate: 'local-candidate',
	            remotecandidate: 'remote-candidate'
	          }[report.type] || report.type
	        };
	        report.names().forEach(name => {
	          standardStats[name] = report.stat(name);
	        });
	        standardReport[standardStats.id] = standardStats;
	      });

	      return standardReport;
	    };

	    // shim getStats with maplike support
	    const makeMapStats = function(stats) {
	      return new Map(Object.keys(stats).map(key => [key, stats[key]]));
	    };

	    if (arguments.length >= 2) {
	      const successCallbackWrapper_ = function(response) {
	        onSucc(makeMapStats(fixChromeStats_(response)));
	      };

	      return origGetStats.apply(this, [successCallbackWrapper_,
	        selector]);
	    }

	    // promise-support
	    return new Promise((resolve, reject) => {
	      origGetStats.apply(this, [
	        function(response) {
	          resolve(makeMapStats(fixChromeStats_(response)));
	        }, reject]);
	    }).then(onSucc, onErr);
	  };
	}

	function shimSenderReceiverGetStats(window) {
	  if (!(typeof window === 'object' && window.RTCPeerConnection &&
	      window.RTCRtpSender && window.RTCRtpReceiver)) {
	    return;
	  }

	  // shim sender stats.
	  if (!('getStats' in window.RTCRtpSender.prototype)) {
	    const origGetSenders = window.RTCPeerConnection.prototype.getSenders;
	    if (origGetSenders) {
	      window.RTCPeerConnection.prototype.getSenders = function getSenders() {
	        const senders = origGetSenders.apply(this, []);
	        senders.forEach(sender => sender._pc = this);
	        return senders;
	      };
	    }

	    const origAddTrack = window.RTCPeerConnection.prototype.addTrack;
	    if (origAddTrack) {
	      window.RTCPeerConnection.prototype.addTrack = function addTrack() {
	        const sender = origAddTrack.apply(this, arguments);
	        sender._pc = this;
	        return sender;
	      };
	    }
	    window.RTCRtpSender.prototype.getStats = function getStats() {
	      const sender = this;
	      return this._pc.getStats().then(result =>
	        /* Note: this will include stats of all senders that
	         *   send a track with the same id as sender.track as
	         *   it is not possible to identify the RTCRtpSender.
	         */
	        filterStats(result, sender.track, true));
	    };
	  }

	  // shim receiver stats.
	  if (!('getStats' in window.RTCRtpReceiver.prototype)) {
	    const origGetReceivers = window.RTCPeerConnection.prototype.getReceivers;
	    if (origGetReceivers) {
	      window.RTCPeerConnection.prototype.getReceivers =
	        function getReceivers() {
	          const receivers = origGetReceivers.apply(this, []);
	          receivers.forEach(receiver => receiver._pc = this);
	          return receivers;
	        };
	    }
	    wrapPeerConnectionEvent(window, 'track', e => {
	      e.receiver._pc = e.srcElement;
	      return e;
	    });
	    window.RTCRtpReceiver.prototype.getStats = function getStats() {
	      const receiver = this;
	      return this._pc.getStats().then(result =>
	        filterStats(result, receiver.track, false));
	    };
	  }

	  if (!('getStats' in window.RTCRtpSender.prototype &&
	      'getStats' in window.RTCRtpReceiver.prototype)) {
	    return;
	  }

	  // shim RTCPeerConnection.getStats(track).
	  const origGetStats = window.RTCPeerConnection.prototype.getStats;
	  window.RTCPeerConnection.prototype.getStats = function getStats() {
	    if (arguments.length > 0 &&
	        arguments[0] instanceof window.MediaStreamTrack) {
	      const track = arguments[0];
	      let sender;
	      let receiver;
	      let err;
	      this.getSenders().forEach(s => {
	        if (s.track === track) {
	          if (sender) {
	            err = true;
	          } else {
	            sender = s;
	          }
	        }
	      });
	      this.getReceivers().forEach(r => {
	        if (r.track === track) {
	          if (receiver) {
	            err = true;
	          } else {
	            receiver = r;
	          }
	        }
	        return r.track === track;
	      });
	      if (err || (sender && receiver)) {
	        return Promise.reject(new DOMException(
	          'There are more than one sender or receiver for the track.',
	          'InvalidAccessError'));
	      } else if (sender) {
	        return sender.getStats();
	      } else if (receiver) {
	        return receiver.getStats();
	      }
	      return Promise.reject(new DOMException(
	        'There is no sender or receiver for the track.',
	        'InvalidAccessError'));
	    }
	    return origGetStats.apply(this, arguments);
	  };
	}

	function shimAddTrackRemoveTrackWithNative(window) {
	  // shim addTrack/removeTrack with native variants in order to make
	  // the interactions with legacy getLocalStreams behave as in other browsers.
	  // Keeps a mapping stream.id => [stream, rtpsenders...]
	  window.RTCPeerConnection.prototype.getLocalStreams =
	    function getLocalStreams() {
	      this._shimmedLocalStreams = this._shimmedLocalStreams || {};
	      return Object.keys(this._shimmedLocalStreams)
	        .map(streamId => this._shimmedLocalStreams[streamId][0]);
	    };

	  const origAddTrack = window.RTCPeerConnection.prototype.addTrack;
	  window.RTCPeerConnection.prototype.addTrack =
	    function addTrack(track, stream) {
	      if (!stream) {
	        return origAddTrack.apply(this, arguments);
	      }
	      this._shimmedLocalStreams = this._shimmedLocalStreams || {};

	      const sender = origAddTrack.apply(this, arguments);
	      if (!this._shimmedLocalStreams[stream.id]) {
	        this._shimmedLocalStreams[stream.id] = [stream, sender];
	      } else if (this._shimmedLocalStreams[stream.id].indexOf(sender) === -1) {
	        this._shimmedLocalStreams[stream.id].push(sender);
	      }
	      return sender;
	    };

	  const origAddStream = window.RTCPeerConnection.prototype.addStream;
	  window.RTCPeerConnection.prototype.addStream = function addStream(stream) {
	    this._shimmedLocalStreams = this._shimmedLocalStreams || {};

	    stream.getTracks().forEach(track => {
	      const alreadyExists = this.getSenders().find(s => s.track === track);
	      if (alreadyExists) {
	        throw new DOMException('Track already exists.',
	            'InvalidAccessError');
	      }
	    });
	    const existingSenders = this.getSenders();
	    origAddStream.apply(this, arguments);
	    const newSenders = this.getSenders()
	      .filter(newSender => existingSenders.indexOf(newSender) === -1);
	    this._shimmedLocalStreams[stream.id] = [stream].concat(newSenders);
	  };

	  const origRemoveStream = window.RTCPeerConnection.prototype.removeStream;
	  window.RTCPeerConnection.prototype.removeStream =
	    function removeStream(stream) {
	      this._shimmedLocalStreams = this._shimmedLocalStreams || {};
	      delete this._shimmedLocalStreams[stream.id];
	      return origRemoveStream.apply(this, arguments);
	    };

	  const origRemoveTrack = window.RTCPeerConnection.prototype.removeTrack;
	  window.RTCPeerConnection.prototype.removeTrack =
	    function removeTrack(sender) {
	      this._shimmedLocalStreams = this._shimmedLocalStreams || {};
	      if (sender) {
	        Object.keys(this._shimmedLocalStreams).forEach(streamId => {
	          const idx = this._shimmedLocalStreams[streamId].indexOf(sender);
	          if (idx !== -1) {
	            this._shimmedLocalStreams[streamId].splice(idx, 1);
	          }
	          if (this._shimmedLocalStreams[streamId].length === 1) {
	            delete this._shimmedLocalStreams[streamId];
	          }
	        });
	      }
	      return origRemoveTrack.apply(this, arguments);
	    };
	}

	function shimAddTrackRemoveTrack(window, browserDetails) {
	  if (!window.RTCPeerConnection) {
	    return;
	  }
	  // shim addTrack and removeTrack.
	  if (window.RTCPeerConnection.prototype.addTrack &&
	      browserDetails.version >= 65) {
	    return shimAddTrackRemoveTrackWithNative(window);
	  }

	  // also shim pc.getLocalStreams when addTrack is shimmed
	  // to return the original streams.
	  const origGetLocalStreams = window.RTCPeerConnection.prototype
	      .getLocalStreams;
	  window.RTCPeerConnection.prototype.getLocalStreams =
	    function getLocalStreams() {
	      const nativeStreams = origGetLocalStreams.apply(this);
	      this._reverseStreams = this._reverseStreams || {};
	      return nativeStreams.map(stream => this._reverseStreams[stream.id]);
	    };

	  const origAddStream = window.RTCPeerConnection.prototype.addStream;
	  window.RTCPeerConnection.prototype.addStream = function addStream(stream) {
	    this._streams = this._streams || {};
	    this._reverseStreams = this._reverseStreams || {};

	    stream.getTracks().forEach(track => {
	      const alreadyExists = this.getSenders().find(s => s.track === track);
	      if (alreadyExists) {
	        throw new DOMException('Track already exists.',
	            'InvalidAccessError');
	      }
	    });
	    // Add identity mapping for consistency with addTrack.
	    // Unless this is being used with a stream from addTrack.
	    if (!this._reverseStreams[stream.id]) {
	      const newStream = new window.MediaStream(stream.getTracks());
	      this._streams[stream.id] = newStream;
	      this._reverseStreams[newStream.id] = stream;
	      stream = newStream;
	    }
	    origAddStream.apply(this, [stream]);
	  };

	  const origRemoveStream = window.RTCPeerConnection.prototype.removeStream;
	  window.RTCPeerConnection.prototype.removeStream =
	    function removeStream(stream) {
	      this._streams = this._streams || {};
	      this._reverseStreams = this._reverseStreams || {};

	      origRemoveStream.apply(this, [(this._streams[stream.id] || stream)]);
	      delete this._reverseStreams[(this._streams[stream.id] ?
	          this._streams[stream.id].id : stream.id)];
	      delete this._streams[stream.id];
	    };

	  window.RTCPeerConnection.prototype.addTrack =
	    function addTrack(track, stream) {
	      if (this.signalingState === 'closed') {
	        throw new DOMException(
	          'The RTCPeerConnection\'s signalingState is \'closed\'.',
	          'InvalidStateError');
	      }
	      const streams = [].slice.call(arguments, 1);
	      if (streams.length !== 1 ||
	          !streams[0].getTracks().find(t => t === track)) {
	        // this is not fully correct but all we can manage without
	        // [[associated MediaStreams]] internal slot.
	        throw new DOMException(
	          'The adapter.js addTrack polyfill only supports a single ' +
	          ' stream which is associated with the specified track.',
	          'NotSupportedError');
	      }

	      const alreadyExists = this.getSenders().find(s => s.track === track);
	      if (alreadyExists) {
	        throw new DOMException('Track already exists.',
	            'InvalidAccessError');
	      }

	      this._streams = this._streams || {};
	      this._reverseStreams = this._reverseStreams || {};
	      const oldStream = this._streams[stream.id];
	      if (oldStream) {
	        // this is using odd Chrome behaviour, use with caution:
	        // https://bugs.chromium.org/p/webrtc/issues/detail?id=7815
	        // Note: we rely on the high-level addTrack/dtmf shim to
	        // create the sender with a dtmf sender.
	        oldStream.addTrack(track);

	        // Trigger ONN async.
	        Promise.resolve().then(() => {
	          this.dispatchEvent(new Event('negotiationneeded'));
	        });
	      } else {
	        const newStream = new window.MediaStream([track]);
	        this._streams[stream.id] = newStream;
	        this._reverseStreams[newStream.id] = stream;
	        this.addStream(newStream);
	      }
	      return this.getSenders().find(s => s.track === track);
	    };

	  // replace the internal stream id with the external one and
	  // vice versa.
	  function replaceInternalStreamId(pc, description) {
	    let sdp = description.sdp;
	    Object.keys(pc._reverseStreams || []).forEach(internalId => {
	      const externalStream = pc._reverseStreams[internalId];
	      const internalStream = pc._streams[externalStream.id];
	      sdp = sdp.replace(new RegExp(internalStream.id, 'g'),
	          externalStream.id);
	    });
	    return new RTCSessionDescription({
	      type: description.type,
	      sdp
	    });
	  }
	  function replaceExternalStreamId(pc, description) {
	    let sdp = description.sdp;
	    Object.keys(pc._reverseStreams || []).forEach(internalId => {
	      const externalStream = pc._reverseStreams[internalId];
	      const internalStream = pc._streams[externalStream.id];
	      sdp = sdp.replace(new RegExp(externalStream.id, 'g'),
	          internalStream.id);
	    });
	    return new RTCSessionDescription({
	      type: description.type,
	      sdp
	    });
	  }
	  ['createOffer', 'createAnswer'].forEach(function(method) {
	    const nativeMethod = window.RTCPeerConnection.prototype[method];
	    const methodObj = {[method]() {
	      const args = arguments;
	      const isLegacyCall = arguments.length &&
	          typeof arguments[0] === 'function';
	      if (isLegacyCall) {
	        return nativeMethod.apply(this, [
	          (description) => {
	            const desc = replaceInternalStreamId(this, description);
	            args[0].apply(null, [desc]);
	          },
	          (err) => {
	            if (args[1]) {
	              args[1].apply(null, err);
	            }
	          }, arguments[2]
	        ]);
	      }
	      return nativeMethod.apply(this, arguments)
	      .then(description => replaceInternalStreamId(this, description));
	    }};
	    window.RTCPeerConnection.prototype[method] = methodObj[method];
	  });

	  const origSetLocalDescription =
	      window.RTCPeerConnection.prototype.setLocalDescription;
	  window.RTCPeerConnection.prototype.setLocalDescription =
	    function setLocalDescription() {
	      if (!arguments.length || !arguments[0].type) {
	        return origSetLocalDescription.apply(this, arguments);
	      }
	      arguments[0] = replaceExternalStreamId(this, arguments[0]);
	      return origSetLocalDescription.apply(this, arguments);
	    };

	  // TODO: mangle getStats: https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamstats-streamidentifier

	  const origLocalDescription = Object.getOwnPropertyDescriptor(
	      window.RTCPeerConnection.prototype, 'localDescription');
	  Object.defineProperty(window.RTCPeerConnection.prototype,
	      'localDescription', {
	        get() {
	          const description = origLocalDescription.get.apply(this);
	          if (description.type === '') {
	            return description;
	          }
	          return replaceInternalStreamId(this, description);
	        }
	      });

	  window.RTCPeerConnection.prototype.removeTrack =
	    function removeTrack(sender) {
	      if (this.signalingState === 'closed') {
	        throw new DOMException(
	          'The RTCPeerConnection\'s signalingState is \'closed\'.',
	          'InvalidStateError');
	      }
	      // We can not yet check for sender instanceof RTCRtpSender
	      // since we shim RTPSender. So we check if sender._pc is set.
	      if (!sender._pc) {
	        throw new DOMException('Argument 1 of RTCPeerConnection.removeTrack ' +
	            'does not implement interface RTCRtpSender.', 'TypeError');
	      }
	      const isLocal = sender._pc === this;
	      if (!isLocal) {
	        throw new DOMException('Sender was not created by this connection.',
	            'InvalidAccessError');
	      }

	      // Search for the native stream the senders track belongs to.
	      this._streams = this._streams || {};
	      let stream;
	      Object.keys(this._streams).forEach(streamid => {
	        const hasTrack = this._streams[streamid].getTracks()
	          .find(track => sender.track === track);
	        if (hasTrack) {
	          stream = this._streams[streamid];
	        }
	      });

	      if (stream) {
	        if (stream.getTracks().length === 1) {
	          // if this is the last track of the stream, remove the stream. This
	          // takes care of any shimmed _senders.
	          this.removeStream(this._reverseStreams[stream.id]);
	        } else {
	          // relying on the same odd chrome behaviour as above.
	          stream.removeTrack(sender.track);
	        }
	        this.dispatchEvent(new Event('negotiationneeded'));
	      }
	    };
	}

	function shimPeerConnection$2(window, browserDetails) {
	  if (!window.RTCPeerConnection && window.webkitRTCPeerConnection) {
	    // very basic support for old versions.
	    window.RTCPeerConnection = window.webkitRTCPeerConnection;
	  }
	  if (!window.RTCPeerConnection) {
	    return;
	  }

	  // shim implicit creation of RTCSessionDescription/RTCIceCandidate
	  if (browserDetails.version < 53) {
	    ['setLocalDescription', 'setRemoteDescription', 'addIceCandidate']
	        .forEach(function(method) {
	          const nativeMethod = window.RTCPeerConnection.prototype[method];
	          const methodObj = {[method]() {
	            arguments[0] = new ((method === 'addIceCandidate') ?
	                window.RTCIceCandidate :
	                window.RTCSessionDescription)(arguments[0]);
	            return nativeMethod.apply(this, arguments);
	          }};
	          window.RTCPeerConnection.prototype[method] = methodObj[method];
	        });
	  }
	}

	// Attempt to fix ONN in plan-b mode.
	function fixNegotiationNeeded(window, browserDetails) {
	  wrapPeerConnectionEvent(window, 'negotiationneeded', e => {
	    const pc = e.target;
	    if (browserDetails.version < 72 || (pc.getConfiguration &&
	        pc.getConfiguration().sdpSemantics === 'plan-b')) {
	      if (pc.signalingState !== 'stable') {
	        return;
	      }
	    }
	    return e;
	  });
	}

	var chromeShim = /*#__PURE__*/Object.freeze({
		__proto__: null,
		shimMediaStream: shimMediaStream,
		shimOnTrack: shimOnTrack$1,
		shimGetSendersWithDtmf: shimGetSendersWithDtmf,
		shimGetStats: shimGetStats,
		shimSenderReceiverGetStats: shimSenderReceiverGetStats,
		shimAddTrackRemoveTrackWithNative: shimAddTrackRemoveTrackWithNative,
		shimAddTrackRemoveTrack: shimAddTrackRemoveTrack,
		shimPeerConnection: shimPeerConnection$2,
		fixNegotiationNeeded: fixNegotiationNeeded,
		shimGetUserMedia: shimGetUserMedia$3,
		shimGetDisplayMedia: shimGetDisplayMedia$2
	});

	/*
	 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */
	// Edge does not like
	// 1) stun: filtered after 14393 unless ?transport=udp is present
	// 2) turn: that does not have all of turn:host:port?transport=udp
	// 3) turn: with ipv6 addresses
	// 4) turn: occurring muliple times
	function filterIceServers$1(iceServers, edgeVersion) {
	  let hasTurn = false;
	  iceServers = JSON.parse(JSON.stringify(iceServers));
	  return iceServers.filter(server => {
	    if (server && (server.urls || server.url)) {
	      let urls = server.urls || server.url;
	      if (server.url && !server.urls) {
	        deprecated('RTCIceServer.url', 'RTCIceServer.urls');
	      }
	      const isString = typeof urls === 'string';
	      if (isString) {
	        urls = [urls];
	      }
	      urls = urls.filter(url => {
	        // filter STUN unconditionally.
	        if (url.indexOf('stun:') === 0) {
	          return false;
	        }

	        const validTurn = url.startsWith('turn') &&
	            !url.startsWith('turn:[') &&
	            url.includes('transport=udp');
	        if (validTurn && !hasTurn) {
	          hasTurn = true;
	          return true;
	        }
	        return validTurn && !hasTurn;
	      });

	      delete server.url;
	      server.urls = isString ? urls[0] : urls;
	      return !!urls.length;
	    }
	  });
	}

	function createCommonjsModule(fn) {
	  var module = { exports: {} };
		return fn(module, module.exports), module.exports;
	}

	/* eslint-env node */

	var sdp = createCommonjsModule(function (module) {

	// SDP helpers.
	var SDPUtils = {};

	// Generate an alphanumeric identifier for cname or mids.
	// TODO: use UUIDs instead? https://gist.github.com/jed/982883
	SDPUtils.generateIdentifier = function() {
	  return Math.random().toString(36).substr(2, 10);
	};

	// The RTCP CNAME used by all peerconnections from the same JS.
	SDPUtils.localCName = SDPUtils.generateIdentifier();

	// Splits SDP into lines, dealing with both CRLF and LF.
	SDPUtils.splitLines = function(blob) {
	  return blob.trim().split('\n').map(function(line) {
	    return line.trim();
	  });
	};
	// Splits SDP into sessionpart and mediasections. Ensures CRLF.
	SDPUtils.splitSections = function(blob) {
	  var parts = blob.split('\nm=');
	  return parts.map(function(part, index) {
	    return (index > 0 ? 'm=' + part : part).trim() + '\r\n';
	  });
	};

	// returns the session description.
	SDPUtils.getDescription = function(blob) {
	  var sections = SDPUtils.splitSections(blob);
	  return sections && sections[0];
	};

	// returns the individual media sections.
	SDPUtils.getMediaSections = function(blob) {
	  var sections = SDPUtils.splitSections(blob);
	  sections.shift();
	  return sections;
	};

	// Returns lines that start with a certain prefix.
	SDPUtils.matchPrefix = function(blob, prefix) {
	  return SDPUtils.splitLines(blob).filter(function(line) {
	    return line.indexOf(prefix) === 0;
	  });
	};

	// Parses an ICE candidate line. Sample input:
	// candidate:702786350 2 udp 41819902 8.8.8.8 60769 typ relay raddr 8.8.8.8
	// rport 55996"
	SDPUtils.parseCandidate = function(line) {
	  var parts;
	  // Parse both variants.
	  if (line.indexOf('a=candidate:') === 0) {
	    parts = line.substring(12).split(' ');
	  } else {
	    parts = line.substring(10).split(' ');
	  }

	  var candidate = {
	    foundation: parts[0],
	    component: parseInt(parts[1], 10),
	    protocol: parts[2].toLowerCase(),
	    priority: parseInt(parts[3], 10),
	    ip: parts[4],
	    address: parts[4], // address is an alias for ip.
	    port: parseInt(parts[5], 10),
	    // skip parts[6] == 'typ'
	    type: parts[7]
	  };

	  for (var i = 8; i < parts.length; i += 2) {
	    switch (parts[i]) {
	      case 'raddr':
	        candidate.relatedAddress = parts[i + 1];
	        break;
	      case 'rport':
	        candidate.relatedPort = parseInt(parts[i + 1], 10);
	        break;
	      case 'tcptype':
	        candidate.tcpType = parts[i + 1];
	        break;
	      case 'ufrag':
	        candidate.ufrag = parts[i + 1]; // for backward compability.
	        candidate.usernameFragment = parts[i + 1];
	        break;
	      default: // extension handling, in particular ufrag
	        candidate[parts[i]] = parts[i + 1];
	        break;
	    }
	  }
	  return candidate;
	};

	// Translates a candidate object into SDP candidate attribute.
	SDPUtils.writeCandidate = function(candidate) {
	  var sdp = [];
	  sdp.push(candidate.foundation);
	  sdp.push(candidate.component);
	  sdp.push(candidate.protocol.toUpperCase());
	  sdp.push(candidate.priority);
	  sdp.push(candidate.address || candidate.ip);
	  sdp.push(candidate.port);

	  var type = candidate.type;
	  sdp.push('typ');
	  sdp.push(type);
	  if (type !== 'host' && candidate.relatedAddress &&
	      candidate.relatedPort) {
	    sdp.push('raddr');
	    sdp.push(candidate.relatedAddress);
	    sdp.push('rport');
	    sdp.push(candidate.relatedPort);
	  }
	  if (candidate.tcpType && candidate.protocol.toLowerCase() === 'tcp') {
	    sdp.push('tcptype');
	    sdp.push(candidate.tcpType);
	  }
	  if (candidate.usernameFragment || candidate.ufrag) {
	    sdp.push('ufrag');
	    sdp.push(candidate.usernameFragment || candidate.ufrag);
	  }
	  return 'candidate:' + sdp.join(' ');
	};

	// Parses an ice-options line, returns an array of option tags.
	// a=ice-options:foo bar
	SDPUtils.parseIceOptions = function(line) {
	  return line.substr(14).split(' ');
	};

	// Parses an rtpmap line, returns RTCRtpCoddecParameters. Sample input:
	// a=rtpmap:111 opus/48000/2
	SDPUtils.parseRtpMap = function(line) {
	  var parts = line.substr(9).split(' ');
	  var parsed = {
	    payloadType: parseInt(parts.shift(), 10) // was: id
	  };

	  parts = parts[0].split('/');

	  parsed.name = parts[0];
	  parsed.clockRate = parseInt(parts[1], 10); // was: clockrate
	  parsed.channels = parts.length === 3 ? parseInt(parts[2], 10) : 1;
	  // legacy alias, got renamed back to channels in ORTC.
	  parsed.numChannels = parsed.channels;
	  return parsed;
	};

	// Generate an a=rtpmap line from RTCRtpCodecCapability or
	// RTCRtpCodecParameters.
	SDPUtils.writeRtpMap = function(codec) {
	  var pt = codec.payloadType;
	  if (codec.preferredPayloadType !== undefined) {
	    pt = codec.preferredPayloadType;
	  }
	  var channels = codec.channels || codec.numChannels || 1;
	  return 'a=rtpmap:' + pt + ' ' + codec.name + '/' + codec.clockRate +
	      (channels !== 1 ? '/' + channels : '') + '\r\n';
	};

	// Parses an a=extmap line (headerextension from RFC 5285). Sample input:
	// a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
	// a=extmap:2/sendonly urn:ietf:params:rtp-hdrext:toffset
	SDPUtils.parseExtmap = function(line) {
	  var parts = line.substr(9).split(' ');
	  return {
	    id: parseInt(parts[0], 10),
	    direction: parts[0].indexOf('/') > 0 ? parts[0].split('/')[1] : 'sendrecv',
	    uri: parts[1]
	  };
	};

	// Generates a=extmap line from RTCRtpHeaderExtensionParameters or
	// RTCRtpHeaderExtension.
	SDPUtils.writeExtmap = function(headerExtension) {
	  return 'a=extmap:' + (headerExtension.id || headerExtension.preferredId) +
	      (headerExtension.direction && headerExtension.direction !== 'sendrecv'
	        ? '/' + headerExtension.direction
	        : '') +
	      ' ' + headerExtension.uri + '\r\n';
	};

	// Parses an ftmp line, returns dictionary. Sample input:
	// a=fmtp:96 vbr=on;cng=on
	// Also deals with vbr=on; cng=on
	SDPUtils.parseFmtp = function(line) {
	  var parsed = {};
	  var kv;
	  var parts = line.substr(line.indexOf(' ') + 1).split(';');
	  for (var j = 0; j < parts.length; j++) {
	    kv = parts[j].trim().split('=');
	    parsed[kv[0].trim()] = kv[1];
	  }
	  return parsed;
	};

	// Generates an a=ftmp line from RTCRtpCodecCapability or RTCRtpCodecParameters.
	SDPUtils.writeFmtp = function(codec) {
	  var line = '';
	  var pt = codec.payloadType;
	  if (codec.preferredPayloadType !== undefined) {
	    pt = codec.preferredPayloadType;
	  }
	  if (codec.parameters && Object.keys(codec.parameters).length) {
	    var params = [];
	    Object.keys(codec.parameters).forEach(function(param) {
	      if (codec.parameters[param]) {
	        params.push(param + '=' + codec.parameters[param]);
	      } else {
	        params.push(param);
	      }
	    });
	    line += 'a=fmtp:' + pt + ' ' + params.join(';') + '\r\n';
	  }
	  return line;
	};

	// Parses an rtcp-fb line, returns RTCPRtcpFeedback object. Sample input:
	// a=rtcp-fb:98 nack rpsi
	SDPUtils.parseRtcpFb = function(line) {
	  var parts = line.substr(line.indexOf(' ') + 1).split(' ');
	  return {
	    type: parts.shift(),
	    parameter: parts.join(' ')
	  };
	};
	// Generate a=rtcp-fb lines from RTCRtpCodecCapability or RTCRtpCodecParameters.
	SDPUtils.writeRtcpFb = function(codec) {
	  var lines = '';
	  var pt = codec.payloadType;
	  if (codec.preferredPayloadType !== undefined) {
	    pt = codec.preferredPayloadType;
	  }
	  if (codec.rtcpFeedback && codec.rtcpFeedback.length) {
	    // FIXME: special handling for trr-int?
	    codec.rtcpFeedback.forEach(function(fb) {
	      lines += 'a=rtcp-fb:' + pt + ' ' + fb.type +
	      (fb.parameter && fb.parameter.length ? ' ' + fb.parameter : '') +
	          '\r\n';
	    });
	  }
	  return lines;
	};

	// Parses an RFC 5576 ssrc media attribute. Sample input:
	// a=ssrc:3735928559 cname:something
	SDPUtils.parseSsrcMedia = function(line) {
	  var sp = line.indexOf(' ');
	  var parts = {
	    ssrc: parseInt(line.substr(7, sp - 7), 10)
	  };
	  var colon = line.indexOf(':', sp);
	  if (colon > -1) {
	    parts.attribute = line.substr(sp + 1, colon - sp - 1);
	    parts.value = line.substr(colon + 1);
	  } else {
	    parts.attribute = line.substr(sp + 1);
	  }
	  return parts;
	};

	SDPUtils.parseSsrcGroup = function(line) {
	  var parts = line.substr(13).split(' ');
	  return {
	    semantics: parts.shift(),
	    ssrcs: parts.map(function(ssrc) {
	      return parseInt(ssrc, 10);
	    })
	  };
	};

	// Extracts the MID (RFC 5888) from a media section.
	// returns the MID or undefined if no mid line was found.
	SDPUtils.getMid = function(mediaSection) {
	  var mid = SDPUtils.matchPrefix(mediaSection, 'a=mid:')[0];
	  if (mid) {
	    return mid.substr(6);
	  }
	};

	SDPUtils.parseFingerprint = function(line) {
	  var parts = line.substr(14).split(' ');
	  return {
	    algorithm: parts[0].toLowerCase(), // algorithm is case-sensitive in Edge.
	    value: parts[1]
	  };
	};

	// Extracts DTLS parameters from SDP media section or sessionpart.
	// FIXME: for consistency with other functions this should only
	//   get the fingerprint line as input. See also getIceParameters.
	SDPUtils.getDtlsParameters = function(mediaSection, sessionpart) {
	  var lines = SDPUtils.matchPrefix(mediaSection + sessionpart,
	    'a=fingerprint:');
	  // Note: a=setup line is ignored since we use the 'auto' role.
	  // Note2: 'algorithm' is not case sensitive except in Edge.
	  return {
	    role: 'auto',
	    fingerprints: lines.map(SDPUtils.parseFingerprint)
	  };
	};

	// Serializes DTLS parameters to SDP.
	SDPUtils.writeDtlsParameters = function(params, setupType) {
	  var sdp = 'a=setup:' + setupType + '\r\n';
	  params.fingerprints.forEach(function(fp) {
	    sdp += 'a=fingerprint:' + fp.algorithm + ' ' + fp.value + '\r\n';
	  });
	  return sdp;
	};

	// Parses a=crypto lines into
	//   https://rawgit.com/aboba/edgertc/master/msortc-rs4.html#dictionary-rtcsrtpsdesparameters-members
	SDPUtils.parseCryptoLine = function(line) {
	  var parts = line.substr(9).split(' ');
	  return {
	    tag: parseInt(parts[0], 10),
	    cryptoSuite: parts[1],
	    keyParams: parts[2],
	    sessionParams: parts.slice(3),
	  };
	};

	SDPUtils.writeCryptoLine = function(parameters) {
	  return 'a=crypto:' + parameters.tag + ' ' +
	    parameters.cryptoSuite + ' ' +
	    (typeof parameters.keyParams === 'object'
	      ? SDPUtils.writeCryptoKeyParams(parameters.keyParams)
	      : parameters.keyParams) +
	    (parameters.sessionParams ? ' ' + parameters.sessionParams.join(' ') : '') +
	    '\r\n';
	};

	// Parses the crypto key parameters into
	//   https://rawgit.com/aboba/edgertc/master/msortc-rs4.html#rtcsrtpkeyparam*
	SDPUtils.parseCryptoKeyParams = function(keyParams) {
	  if (keyParams.indexOf('inline:') !== 0) {
	    return null;
	  }
	  var parts = keyParams.substr(7).split('|');
	  return {
	    keyMethod: 'inline',
	    keySalt: parts[0],
	    lifeTime: parts[1],
	    mkiValue: parts[2] ? parts[2].split(':')[0] : undefined,
	    mkiLength: parts[2] ? parts[2].split(':')[1] : undefined,
	  };
	};

	SDPUtils.writeCryptoKeyParams = function(keyParams) {
	  return keyParams.keyMethod + ':'
	    + keyParams.keySalt +
	    (keyParams.lifeTime ? '|' + keyParams.lifeTime : '') +
	    (keyParams.mkiValue && keyParams.mkiLength
	      ? '|' + keyParams.mkiValue + ':' + keyParams.mkiLength
	      : '');
	};

	// Extracts all SDES paramters.
	SDPUtils.getCryptoParameters = function(mediaSection, sessionpart) {
	  var lines = SDPUtils.matchPrefix(mediaSection + sessionpart,
	    'a=crypto:');
	  return lines.map(SDPUtils.parseCryptoLine);
	};

	// Parses ICE information from SDP media section or sessionpart.
	// FIXME: for consistency with other functions this should only
	//   get the ice-ufrag and ice-pwd lines as input.
	SDPUtils.getIceParameters = function(mediaSection, sessionpart) {
	  var ufrag = SDPUtils.matchPrefix(mediaSection + sessionpart,
	    'a=ice-ufrag:')[0];
	  var pwd = SDPUtils.matchPrefix(mediaSection + sessionpart,
	    'a=ice-pwd:')[0];
	  if (!(ufrag && pwd)) {
	    return null;
	  }
	  return {
	    usernameFragment: ufrag.substr(12),
	    password: pwd.substr(10),
	  };
	};

	// Serializes ICE parameters to SDP.
	SDPUtils.writeIceParameters = function(params) {
	  return 'a=ice-ufrag:' + params.usernameFragment + '\r\n' +
	      'a=ice-pwd:' + params.password + '\r\n';
	};

	// Parses the SDP media section and returns RTCRtpParameters.
	SDPUtils.parseRtpParameters = function(mediaSection) {
	  var description = {
	    codecs: [],
	    headerExtensions: [],
	    fecMechanisms: [],
	    rtcp: []
	  };
	  var lines = SDPUtils.splitLines(mediaSection);
	  var mline = lines[0].split(' ');
	  for (var i = 3; i < mline.length; i++) { // find all codecs from mline[3..]
	    var pt = mline[i];
	    var rtpmapline = SDPUtils.matchPrefix(
	      mediaSection, 'a=rtpmap:' + pt + ' ')[0];
	    if (rtpmapline) {
	      var codec = SDPUtils.parseRtpMap(rtpmapline);
	      var fmtps = SDPUtils.matchPrefix(
	        mediaSection, 'a=fmtp:' + pt + ' ');
	      // Only the first a=fmtp:<pt> is considered.
	      codec.parameters = fmtps.length ? SDPUtils.parseFmtp(fmtps[0]) : {};
	      codec.rtcpFeedback = SDPUtils.matchPrefix(
	        mediaSection, 'a=rtcp-fb:' + pt + ' ')
	        .map(SDPUtils.parseRtcpFb);
	      description.codecs.push(codec);
	      // parse FEC mechanisms from rtpmap lines.
	      switch (codec.name.toUpperCase()) {
	        case 'RED':
	        case 'ULPFEC':
	          description.fecMechanisms.push(codec.name.toUpperCase());
	          break;
	      }
	    }
	  }
	  SDPUtils.matchPrefix(mediaSection, 'a=extmap:').forEach(function(line) {
	    description.headerExtensions.push(SDPUtils.parseExtmap(line));
	  });
	  // FIXME: parse rtcp.
	  return description;
	};

	// Generates parts of the SDP media section describing the capabilities /
	// parameters.
	SDPUtils.writeRtpDescription = function(kind, caps) {
	  var sdp = '';

	  // Build the mline.
	  sdp += 'm=' + kind + ' ';
	  sdp += caps.codecs.length > 0 ? '9' : '0'; // reject if no codecs.
	  sdp += ' UDP/TLS/RTP/SAVPF ';
	  sdp += caps.codecs.map(function(codec) {
	    if (codec.preferredPayloadType !== undefined) {
	      return codec.preferredPayloadType;
	    }
	    return codec.payloadType;
	  }).join(' ') + '\r\n';

	  sdp += 'c=IN IP4 0.0.0.0\r\n';
	  sdp += 'a=rtcp:9 IN IP4 0.0.0.0\r\n';

	  // Add a=rtpmap lines for each codec. Also fmtp and rtcp-fb.
	  caps.codecs.forEach(function(codec) {
	    sdp += SDPUtils.writeRtpMap(codec);
	    sdp += SDPUtils.writeFmtp(codec);
	    sdp += SDPUtils.writeRtcpFb(codec);
	  });
	  var maxptime = 0;
	  caps.codecs.forEach(function(codec) {
	    if (codec.maxptime > maxptime) {
	      maxptime = codec.maxptime;
	    }
	  });
	  if (maxptime > 0) {
	    sdp += 'a=maxptime:' + maxptime + '\r\n';
	  }
	  sdp += 'a=rtcp-mux\r\n';

	  if (caps.headerExtensions) {
	    caps.headerExtensions.forEach(function(extension) {
	      sdp += SDPUtils.writeExtmap(extension);
	    });
	  }
	  // FIXME: write fecMechanisms.
	  return sdp;
	};

	// Parses the SDP media section and returns an array of
	// RTCRtpEncodingParameters.
	SDPUtils.parseRtpEncodingParameters = function(mediaSection) {
	  var encodingParameters = [];
	  var description = SDPUtils.parseRtpParameters(mediaSection);
	  var hasRed = description.fecMechanisms.indexOf('RED') !== -1;
	  var hasUlpfec = description.fecMechanisms.indexOf('ULPFEC') !== -1;

	  // filter a=ssrc:... cname:, ignore PlanB-msid
	  var ssrcs = SDPUtils.matchPrefix(mediaSection, 'a=ssrc:')
	    .map(function(line) {
	      return SDPUtils.parseSsrcMedia(line);
	    })
	    .filter(function(parts) {
	      return parts.attribute === 'cname';
	    });
	  var primarySsrc = ssrcs.length > 0 && ssrcs[0].ssrc;
	  var secondarySsrc;

	  var flows = SDPUtils.matchPrefix(mediaSection, 'a=ssrc-group:FID')
	    .map(function(line) {
	      var parts = line.substr(17).split(' ');
	      return parts.map(function(part) {
	        return parseInt(part, 10);
	      });
	    });
	  if (flows.length > 0 && flows[0].length > 1 && flows[0][0] === primarySsrc) {
	    secondarySsrc = flows[0][1];
	  }

	  description.codecs.forEach(function(codec) {
	    if (codec.name.toUpperCase() === 'RTX' && codec.parameters.apt) {
	      var encParam = {
	        ssrc: primarySsrc,
	        codecPayloadType: parseInt(codec.parameters.apt, 10)
	      };
	      if (primarySsrc && secondarySsrc) {
	        encParam.rtx = {ssrc: secondarySsrc};
	      }
	      encodingParameters.push(encParam);
	      if (hasRed) {
	        encParam = JSON.parse(JSON.stringify(encParam));
	        encParam.fec = {
	          ssrc: primarySsrc,
	          mechanism: hasUlpfec ? 'red+ulpfec' : 'red'
	        };
	        encodingParameters.push(encParam);
	      }
	    }
	  });
	  if (encodingParameters.length === 0 && primarySsrc) {
	    encodingParameters.push({
	      ssrc: primarySsrc
	    });
	  }

	  // we support both b=AS and b=TIAS but interpret AS as TIAS.
	  var bandwidth = SDPUtils.matchPrefix(mediaSection, 'b=');
	  if (bandwidth.length) {
	    if (bandwidth[0].indexOf('b=TIAS:') === 0) {
	      bandwidth = parseInt(bandwidth[0].substr(7), 10);
	    } else if (bandwidth[0].indexOf('b=AS:') === 0) {
	      // use formula from JSEP to convert b=AS to TIAS value.
	      bandwidth = parseInt(bandwidth[0].substr(5), 10) * 1000 * 0.95
	          - (50 * 40 * 8);
	    } else {
	      bandwidth = undefined;
	    }
	    encodingParameters.forEach(function(params) {
	      params.maxBitrate = bandwidth;
	    });
	  }
	  return encodingParameters;
	};

	// parses http://draft.ortc.org/#rtcrtcpparameters*
	SDPUtils.parseRtcpParameters = function(mediaSection) {
	  var rtcpParameters = {};

	  // Gets the first SSRC. Note tha with RTX there might be multiple
	  // SSRCs.
	  var remoteSsrc = SDPUtils.matchPrefix(mediaSection, 'a=ssrc:')
	    .map(function(line) {
	      return SDPUtils.parseSsrcMedia(line);
	    })
	    .filter(function(obj) {
	      return obj.attribute === 'cname';
	    })[0];
	  if (remoteSsrc) {
	    rtcpParameters.cname = remoteSsrc.value;
	    rtcpParameters.ssrc = remoteSsrc.ssrc;
	  }

	  // Edge uses the compound attribute instead of reducedSize
	  // compound is !reducedSize
	  var rsize = SDPUtils.matchPrefix(mediaSection, 'a=rtcp-rsize');
	  rtcpParameters.reducedSize = rsize.length > 0;
	  rtcpParameters.compound = rsize.length === 0;

	  // parses the rtcp-mux attrіbute.
	  // Note that Edge does not support unmuxed RTCP.
	  var mux = SDPUtils.matchPrefix(mediaSection, 'a=rtcp-mux');
	  rtcpParameters.mux = mux.length > 0;

	  return rtcpParameters;
	};

	// parses either a=msid: or a=ssrc:... msid lines and returns
	// the id of the MediaStream and MediaStreamTrack.
	SDPUtils.parseMsid = function(mediaSection) {
	  var parts;
	  var spec = SDPUtils.matchPrefix(mediaSection, 'a=msid:');
	  if (spec.length === 1) {
	    parts = spec[0].substr(7).split(' ');
	    return {stream: parts[0], track: parts[1]};
	  }
	  var planB = SDPUtils.matchPrefix(mediaSection, 'a=ssrc:')
	    .map(function(line) {
	      return SDPUtils.parseSsrcMedia(line);
	    })
	    .filter(function(msidParts) {
	      return msidParts.attribute === 'msid';
	    });
	  if (planB.length > 0) {
	    parts = planB[0].value.split(' ');
	    return {stream: parts[0], track: parts[1]};
	  }
	};

	// SCTP
	// parses draft-ietf-mmusic-sctp-sdp-26 first and falls back
	// to draft-ietf-mmusic-sctp-sdp-05
	SDPUtils.parseSctpDescription = function(mediaSection) {
	  var mline = SDPUtils.parseMLine(mediaSection);
	  var maxSizeLine = SDPUtils.matchPrefix(mediaSection, 'a=max-message-size:');
	  var maxMessageSize;
	  if (maxSizeLine.length > 0) {
	    maxMessageSize = parseInt(maxSizeLine[0].substr(19), 10);
	  }
	  if (isNaN(maxMessageSize)) {
	    maxMessageSize = 65536;
	  }
	  var sctpPort = SDPUtils.matchPrefix(mediaSection, 'a=sctp-port:');
	  if (sctpPort.length > 0) {
	    return {
	      port: parseInt(sctpPort[0].substr(12), 10),
	      protocol: mline.fmt,
	      maxMessageSize: maxMessageSize
	    };
	  }
	  var sctpMapLines = SDPUtils.matchPrefix(mediaSection, 'a=sctpmap:');
	  if (sctpMapLines.length > 0) {
	    var parts = SDPUtils.matchPrefix(mediaSection, 'a=sctpmap:')[0]
	      .substr(10)
	      .split(' ');
	    return {
	      port: parseInt(parts[0], 10),
	      protocol: parts[1],
	      maxMessageSize: maxMessageSize
	    };
	  }
	};

	// SCTP
	// outputs the draft-ietf-mmusic-sctp-sdp-26 version that all browsers
	// support by now receiving in this format, unless we originally parsed
	// as the draft-ietf-mmusic-sctp-sdp-05 format (indicated by the m-line
	// protocol of DTLS/SCTP -- without UDP/ or TCP/)
	SDPUtils.writeSctpDescription = function(media, sctp) {
	  var output = [];
	  if (media.protocol !== 'DTLS/SCTP') {
	    output = [
	      'm=' + media.kind + ' 9 ' + media.protocol + ' ' + sctp.protocol + '\r\n',
	      'c=IN IP4 0.0.0.0\r\n',
	      'a=sctp-port:' + sctp.port + '\r\n'
	    ];
	  } else {
	    output = [
	      'm=' + media.kind + ' 9 ' + media.protocol + ' ' + sctp.port + '\r\n',
	      'c=IN IP4 0.0.0.0\r\n',
	      'a=sctpmap:' + sctp.port + ' ' + sctp.protocol + ' 65535\r\n'
	    ];
	  }
	  if (sctp.maxMessageSize !== undefined) {
	    output.push('a=max-message-size:' + sctp.maxMessageSize + '\r\n');
	  }
	  return output.join('');
	};

	// Generate a session ID for SDP.
	// https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-20#section-5.2.1
	// recommends using a cryptographically random +ve 64-bit value
	// but right now this should be acceptable and within the right range
	SDPUtils.generateSessionId = function() {
	  return Math.random().toString().substr(2, 21);
	};

	// Write boilder plate for start of SDP
	// sessId argument is optional - if not supplied it will
	// be generated randomly
	// sessVersion is optional and defaults to 2
	// sessUser is optional and defaults to 'thisisadapterortc'
	SDPUtils.writeSessionBoilerplate = function(sessId, sessVer, sessUser) {
	  var sessionId;
	  var version = sessVer !== undefined ? sessVer : 2;
	  if (sessId) {
	    sessionId = sessId;
	  } else {
	    sessionId = SDPUtils.generateSessionId();
	  }
	  var user = sessUser || 'thisisadapterortc';
	  // FIXME: sess-id should be an NTP timestamp.
	  return 'v=0\r\n' +
	      'o=' + user + ' ' + sessionId + ' ' + version +
	        ' IN IP4 127.0.0.1\r\n' +
	      's=-\r\n' +
	      't=0 0\r\n';
	};

	SDPUtils.writeMediaSection = function(transceiver, caps, type, stream) {
	  var sdp = SDPUtils.writeRtpDescription(transceiver.kind, caps);

	  // Map ICE parameters (ufrag, pwd) to SDP.
	  sdp += SDPUtils.writeIceParameters(
	    transceiver.iceGatherer.getLocalParameters());

	  // Map DTLS parameters to SDP.
	  sdp += SDPUtils.writeDtlsParameters(
	    transceiver.dtlsTransport.getLocalParameters(),
	    type === 'offer' ? 'actpass' : 'active');

	  sdp += 'a=mid:' + transceiver.mid + '\r\n';

	  if (transceiver.direction) {
	    sdp += 'a=' + transceiver.direction + '\r\n';
	  } else if (transceiver.rtpSender && transceiver.rtpReceiver) {
	    sdp += 'a=sendrecv\r\n';
	  } else if (transceiver.rtpSender) {
	    sdp += 'a=sendonly\r\n';
	  } else if (transceiver.rtpReceiver) {
	    sdp += 'a=recvonly\r\n';
	  } else {
	    sdp += 'a=inactive\r\n';
	  }

	  if (transceiver.rtpSender) {
	    // spec.
	    var msid = 'msid:' + stream.id + ' ' +
	        transceiver.rtpSender.track.id + '\r\n';
	    sdp += 'a=' + msid;

	    // for Chrome.
	    sdp += 'a=ssrc:' + transceiver.sendEncodingParameters[0].ssrc +
	        ' ' + msid;
	    if (transceiver.sendEncodingParameters[0].rtx) {
	      sdp += 'a=ssrc:' + transceiver.sendEncodingParameters[0].rtx.ssrc +
	          ' ' + msid;
	      sdp += 'a=ssrc-group:FID ' +
	          transceiver.sendEncodingParameters[0].ssrc + ' ' +
	          transceiver.sendEncodingParameters[0].rtx.ssrc +
	          '\r\n';
	    }
	  }
	  // FIXME: this should be written by writeRtpDescription.
	  sdp += 'a=ssrc:' + transceiver.sendEncodingParameters[0].ssrc +
	      ' cname:' + SDPUtils.localCName + '\r\n';
	  if (transceiver.rtpSender && transceiver.sendEncodingParameters[0].rtx) {
	    sdp += 'a=ssrc:' + transceiver.sendEncodingParameters[0].rtx.ssrc +
	        ' cname:' + SDPUtils.localCName + '\r\n';
	  }
	  return sdp;
	};

	// Gets the direction from the mediaSection or the sessionpart.
	SDPUtils.getDirection = function(mediaSection, sessionpart) {
	  // Look for sendrecv, sendonly, recvonly, inactive, default to sendrecv.
	  var lines = SDPUtils.splitLines(mediaSection);
	  for (var i = 0; i < lines.length; i++) {
	    switch (lines[i]) {
	      case 'a=sendrecv':
	      case 'a=sendonly':
	      case 'a=recvonly':
	      case 'a=inactive':
	        return lines[i].substr(2);
	        // FIXME: What should happen here?
	    }
	  }
	  if (sessionpart) {
	    return SDPUtils.getDirection(sessionpart);
	  }
	  return 'sendrecv';
	};

	SDPUtils.getKind = function(mediaSection) {
	  var lines = SDPUtils.splitLines(mediaSection);
	  var mline = lines[0].split(' ');
	  return mline[0].substr(2);
	};

	SDPUtils.isRejected = function(mediaSection) {
	  return mediaSection.split(' ', 2)[1] === '0';
	};

	SDPUtils.parseMLine = function(mediaSection) {
	  var lines = SDPUtils.splitLines(mediaSection);
	  var parts = lines[0].substr(2).split(' ');
	  return {
	    kind: parts[0],
	    port: parseInt(parts[1], 10),
	    protocol: parts[2],
	    fmt: parts.slice(3).join(' ')
	  };
	};

	SDPUtils.parseOLine = function(mediaSection) {
	  var line = SDPUtils.matchPrefix(mediaSection, 'o=')[0];
	  var parts = line.substr(2).split(' ');
	  return {
	    username: parts[0],
	    sessionId: parts[1],
	    sessionVersion: parseInt(parts[2], 10),
	    netType: parts[3],
	    addressType: parts[4],
	    address: parts[5]
	  };
	};

	// a very naive interpretation of a valid SDP.
	SDPUtils.isValidSDP = function(blob) {
	  if (typeof blob !== 'string' || blob.length === 0) {
	    return false;
	  }
	  var lines = SDPUtils.splitLines(blob);
	  for (var i = 0; i < lines.length; i++) {
	    if (lines[i].length < 2 || lines[i].charAt(1) !== '=') {
	      return false;
	    }
	    // TODO: check the modifier a bit more.
	  }
	  return true;
	};

	// Expose public methods.
	{
	  module.exports = SDPUtils;
	}
	});

	/*
	 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */



	function fixStatsType(stat) {
	  return {
	    inboundrtp: 'inbound-rtp',
	    outboundrtp: 'outbound-rtp',
	    candidatepair: 'candidate-pair',
	    localcandidate: 'local-candidate',
	    remotecandidate: 'remote-candidate'
	  }[stat.type] || stat.type;
	}

	function writeMediaSection(transceiver, caps, type, stream, dtlsRole) {
	  var sdp$1 = sdp.writeRtpDescription(transceiver.kind, caps);

	  // Map ICE parameters (ufrag, pwd) to SDP.
	  sdp$1 += sdp.writeIceParameters(
	      transceiver.iceGatherer.getLocalParameters());

	  // Map DTLS parameters to SDP.
	  sdp$1 += sdp.writeDtlsParameters(
	      transceiver.dtlsTransport.getLocalParameters(),
	      type === 'offer' ? 'actpass' : dtlsRole || 'active');

	  sdp$1 += 'a=mid:' + transceiver.mid + '\r\n';

	  if (transceiver.rtpSender && transceiver.rtpReceiver) {
	    sdp$1 += 'a=sendrecv\r\n';
	  } else if (transceiver.rtpSender) {
	    sdp$1 += 'a=sendonly\r\n';
	  } else if (transceiver.rtpReceiver) {
	    sdp$1 += 'a=recvonly\r\n';
	  } else {
	    sdp$1 += 'a=inactive\r\n';
	  }

	  if (transceiver.rtpSender) {
	    var trackId = transceiver.rtpSender._initialTrackId ||
	        transceiver.rtpSender.track.id;
	    transceiver.rtpSender._initialTrackId = trackId;
	    // spec.
	    var msid = 'msid:' + (stream ? stream.id : '-') + ' ' +
	        trackId + '\r\n';
	    sdp$1 += 'a=' + msid;
	    // for Chrome. Legacy should no longer be required.
	    sdp$1 += 'a=ssrc:' + transceiver.sendEncodingParameters[0].ssrc +
	        ' ' + msid;

	    // RTX
	    if (transceiver.sendEncodingParameters[0].rtx) {
	      sdp$1 += 'a=ssrc:' + transceiver.sendEncodingParameters[0].rtx.ssrc +
	          ' ' + msid;
	      sdp$1 += 'a=ssrc-group:FID ' +
	          transceiver.sendEncodingParameters[0].ssrc + ' ' +
	          transceiver.sendEncodingParameters[0].rtx.ssrc +
	          '\r\n';
	    }
	  }
	  // FIXME: this should be written by writeRtpDescription.
	  sdp$1 += 'a=ssrc:' + transceiver.sendEncodingParameters[0].ssrc +
	      ' cname:' + sdp.localCName + '\r\n';
	  if (transceiver.rtpSender && transceiver.sendEncodingParameters[0].rtx) {
	    sdp$1 += 'a=ssrc:' + transceiver.sendEncodingParameters[0].rtx.ssrc +
	        ' cname:' + sdp.localCName + '\r\n';
	  }
	  return sdp$1;
	}

	// Edge does not like
	// 1) stun: filtered after 14393 unless ?transport=udp is present
	// 2) turn: that does not have all of turn:host:port?transport=udp
	// 3) turn: with ipv6 addresses
	// 4) turn: occurring muliple times
	function filterIceServers(iceServers, edgeVersion) {
	  var hasTurn = false;
	  iceServers = JSON.parse(JSON.stringify(iceServers));
	  return iceServers.filter(function(server) {
	    if (server && (server.urls || server.url)) {
	      var urls = server.urls || server.url;
	      if (server.url && !server.urls) {
	        console.warn('RTCIceServer.url is deprecated! Use urls instead.');
	      }
	      var isString = typeof urls === 'string';
	      if (isString) {
	        urls = [urls];
	      }
	      urls = urls.filter(function(url) {
	        var validTurn = url.indexOf('turn:') === 0 &&
	            url.indexOf('transport=udp') !== -1 &&
	            url.indexOf('turn:[') === -1 &&
	            !hasTurn;

	        if (validTurn) {
	          hasTurn = true;
	          return true;
	        }
	        return url.indexOf('stun:') === 0 && edgeVersion >= 14393 &&
	            url.indexOf('?transport=udp') === -1;
	      });

	      delete server.url;
	      server.urls = isString ? urls[0] : urls;
	      return !!urls.length;
	    }
	  });
	}

	// Determines the intersection of local and remote capabilities.
	function getCommonCapabilities(localCapabilities, remoteCapabilities) {
	  var commonCapabilities = {
	    codecs: [],
	    headerExtensions: [],
	    fecMechanisms: []
	  };

	  var findCodecByPayloadType = function(pt, codecs) {
	    pt = parseInt(pt, 10);
	    for (var i = 0; i < codecs.length; i++) {
	      if (codecs[i].payloadType === pt ||
	          codecs[i].preferredPayloadType === pt) {
	        return codecs[i];
	      }
	    }
	  };

	  var rtxCapabilityMatches = function(lRtx, rRtx, lCodecs, rCodecs) {
	    var lCodec = findCodecByPayloadType(lRtx.parameters.apt, lCodecs);
	    var rCodec = findCodecByPayloadType(rRtx.parameters.apt, rCodecs);
	    return lCodec && rCodec &&
	        lCodec.name.toLowerCase() === rCodec.name.toLowerCase();
	  };

	  localCapabilities.codecs.forEach(function(lCodec) {
	    for (var i = 0; i < remoteCapabilities.codecs.length; i++) {
	      var rCodec = remoteCapabilities.codecs[i];
	      if (lCodec.name.toLowerCase() === rCodec.name.toLowerCase() &&
	          lCodec.clockRate === rCodec.clockRate) {
	        if (lCodec.name.toLowerCase() === 'rtx' &&
	            lCodec.parameters && rCodec.parameters.apt) {
	          // for RTX we need to find the local rtx that has a apt
	          // which points to the same local codec as the remote one.
	          if (!rtxCapabilityMatches(lCodec, rCodec,
	              localCapabilities.codecs, remoteCapabilities.codecs)) {
	            continue;
	          }
	        }
	        rCodec = JSON.parse(JSON.stringify(rCodec)); // deepcopy
	        // number of channels is the highest common number of channels
	        rCodec.numChannels = Math.min(lCodec.numChannels,
	            rCodec.numChannels);
	        // push rCodec so we reply with offerer payload type
	        commonCapabilities.codecs.push(rCodec);

	        // determine common feedback mechanisms
	        rCodec.rtcpFeedback = rCodec.rtcpFeedback.filter(function(fb) {
	          for (var j = 0; j < lCodec.rtcpFeedback.length; j++) {
	            if (lCodec.rtcpFeedback[j].type === fb.type &&
	                lCodec.rtcpFeedback[j].parameter === fb.parameter) {
	              return true;
	            }
	          }
	          return false;
	        });
	        // FIXME: also need to determine .parameters
	        //  see https://github.com/openpeer/ortc/issues/569
	        break;
	      }
	    }
	  });

	  localCapabilities.headerExtensions.forEach(function(lHeaderExtension) {
	    for (var i = 0; i < remoteCapabilities.headerExtensions.length;
	         i++) {
	      var rHeaderExtension = remoteCapabilities.headerExtensions[i];
	      if (lHeaderExtension.uri === rHeaderExtension.uri) {
	        commonCapabilities.headerExtensions.push(rHeaderExtension);
	        break;
	      }
	    }
	  });

	  // FIXME: fecMechanisms
	  return commonCapabilities;
	}

	// is action=setLocalDescription with type allowed in signalingState
	function isActionAllowedInSignalingState(action, type, signalingState) {
	  return {
	    offer: {
	      setLocalDescription: ['stable', 'have-local-offer'],
	      setRemoteDescription: ['stable', 'have-remote-offer']
	    },
	    answer: {
	      setLocalDescription: ['have-remote-offer', 'have-local-pranswer'],
	      setRemoteDescription: ['have-local-offer', 'have-remote-pranswer']
	    }
	  }[type][action].indexOf(signalingState) !== -1;
	}

	function maybeAddCandidate(iceTransport, candidate) {
	  // Edge's internal representation adds some fields therefore
	  // not all fieldѕ are taken into account.
	  var alreadyAdded = iceTransport.getRemoteCandidates()
	      .find(function(remoteCandidate) {
	        return candidate.foundation === remoteCandidate.foundation &&
	            candidate.ip === remoteCandidate.ip &&
	            candidate.port === remoteCandidate.port &&
	            candidate.priority === remoteCandidate.priority &&
	            candidate.protocol === remoteCandidate.protocol &&
	            candidate.type === remoteCandidate.type;
	      });
	  if (!alreadyAdded) {
	    iceTransport.addRemoteCandidate(candidate);
	  }
	  return !alreadyAdded;
	}


	function makeError(name, description) {
	  var e = new Error(description);
	  e.name = name;
	  // legacy error codes from https://heycam.github.io/webidl/#idl-DOMException-error-names
	  e.code = {
	    NotSupportedError: 9,
	    InvalidStateError: 11,
	    InvalidAccessError: 15,
	    TypeError: undefined,
	    OperationError: undefined
	  }[name];
	  return e;
	}

	var rtcpeerconnection = function(window, edgeVersion) {
	  // https://w3c.github.io/mediacapture-main/#mediastream
	  // Helper function to add the track to the stream and
	  // dispatch the event ourselves.
	  function addTrackToStreamAndFireEvent(track, stream) {
	    stream.addTrack(track);
	    stream.dispatchEvent(new window.MediaStreamTrackEvent('addtrack',
	        {track: track}));
	  }

	  function removeTrackFromStreamAndFireEvent(track, stream) {
	    stream.removeTrack(track);
	    stream.dispatchEvent(new window.MediaStreamTrackEvent('removetrack',
	        {track: track}));
	  }

	  function fireAddTrack(pc, track, receiver, streams) {
	    var trackEvent = new Event('track');
	    trackEvent.track = track;
	    trackEvent.receiver = receiver;
	    trackEvent.transceiver = {receiver: receiver};
	    trackEvent.streams = streams;
	    window.setTimeout(function() {
	      pc._dispatchEvent('track', trackEvent);
	    });
	  }

	  var RTCPeerConnection = function(config) {
	    var pc = this;

	    var _eventTarget = document.createDocumentFragment();
	    ['addEventListener', 'removeEventListener', 'dispatchEvent']
	        .forEach(function(method) {
	          pc[method] = _eventTarget[method].bind(_eventTarget);
	        });

	    this.canTrickleIceCandidates = null;

	    this.needNegotiation = false;

	    this.localStreams = [];
	    this.remoteStreams = [];

	    this._localDescription = null;
	    this._remoteDescription = null;

	    this.signalingState = 'stable';
	    this.iceConnectionState = 'new';
	    this.connectionState = 'new';
	    this.iceGatheringState = 'new';

	    config = JSON.parse(JSON.stringify(config || {}));

	    this.usingBundle = config.bundlePolicy === 'max-bundle';
	    if (config.rtcpMuxPolicy === 'negotiate') {
	      throw(makeError('NotSupportedError',
	          'rtcpMuxPolicy \'negotiate\' is not supported'));
	    } else if (!config.rtcpMuxPolicy) {
	      config.rtcpMuxPolicy = 'require';
	    }

	    switch (config.iceTransportPolicy) {
	      case 'all':
	      case 'relay':
	        break;
	      default:
	        config.iceTransportPolicy = 'all';
	        break;
	    }

	    switch (config.bundlePolicy) {
	      case 'balanced':
	      case 'max-compat':
	      case 'max-bundle':
	        break;
	      default:
	        config.bundlePolicy = 'balanced';
	        break;
	    }

	    config.iceServers = filterIceServers(config.iceServers || [], edgeVersion);

	    this._iceGatherers = [];
	    if (config.iceCandidatePoolSize) {
	      for (var i = config.iceCandidatePoolSize; i > 0; i--) {
	        this._iceGatherers.push(new window.RTCIceGatherer({
	          iceServers: config.iceServers,
	          gatherPolicy: config.iceTransportPolicy
	        }));
	      }
	    } else {
	      config.iceCandidatePoolSize = 0;
	    }

	    this._config = config;

	    // per-track iceGathers, iceTransports, dtlsTransports, rtpSenders, ...
	    // everything that is needed to describe a SDP m-line.
	    this.transceivers = [];

	    this._sdpSessionId = sdp.generateSessionId();
	    this._sdpSessionVersion = 0;

	    this._dtlsRole = undefined; // role for a=setup to use in answers.

	    this._isClosed = false;
	  };

	  Object.defineProperty(RTCPeerConnection.prototype, 'localDescription', {
	    configurable: true,
	    get: function() {
	      return this._localDescription;
	    }
	  });
	  Object.defineProperty(RTCPeerConnection.prototype, 'remoteDescription', {
	    configurable: true,
	    get: function() {
	      return this._remoteDescription;
	    }
	  });

	  // set up event handlers on prototype
	  RTCPeerConnection.prototype.onicecandidate = null;
	  RTCPeerConnection.prototype.onaddstream = null;
	  RTCPeerConnection.prototype.ontrack = null;
	  RTCPeerConnection.prototype.onremovestream = null;
	  RTCPeerConnection.prototype.onsignalingstatechange = null;
	  RTCPeerConnection.prototype.oniceconnectionstatechange = null;
	  RTCPeerConnection.prototype.onconnectionstatechange = null;
	  RTCPeerConnection.prototype.onicegatheringstatechange = null;
	  RTCPeerConnection.prototype.onnegotiationneeded = null;
	  RTCPeerConnection.prototype.ondatachannel = null;

	  RTCPeerConnection.prototype._dispatchEvent = function(name, event) {
	    if (this._isClosed) {
	      return;
	    }
	    this.dispatchEvent(event);
	    if (typeof this['on' + name] === 'function') {
	      this['on' + name](event);
	    }
	  };

	  RTCPeerConnection.prototype._emitGatheringStateChange = function() {
	    var event = new Event('icegatheringstatechange');
	    this._dispatchEvent('icegatheringstatechange', event);
	  };

	  RTCPeerConnection.prototype.getConfiguration = function() {
	    return this._config;
	  };

	  RTCPeerConnection.prototype.getLocalStreams = function() {
	    return this.localStreams;
	  };

	  RTCPeerConnection.prototype.getRemoteStreams = function() {
	    return this.remoteStreams;
	  };

	  // internal helper to create a transceiver object.
	  // (which is not yet the same as the WebRTC 1.0 transceiver)
	  RTCPeerConnection.prototype._createTransceiver = function(kind, doNotAdd) {
	    var hasBundleTransport = this.transceivers.length > 0;
	    var transceiver = {
	      track: null,
	      iceGatherer: null,
	      iceTransport: null,
	      dtlsTransport: null,
	      localCapabilities: null,
	      remoteCapabilities: null,
	      rtpSender: null,
	      rtpReceiver: null,
	      kind: kind,
	      mid: null,
	      sendEncodingParameters: null,
	      recvEncodingParameters: null,
	      stream: null,
	      associatedRemoteMediaStreams: [],
	      wantReceive: true
	    };
	    if (this.usingBundle && hasBundleTransport) {
	      transceiver.iceTransport = this.transceivers[0].iceTransport;
	      transceiver.dtlsTransport = this.transceivers[0].dtlsTransport;
	    } else {
	      var transports = this._createIceAndDtlsTransports();
	      transceiver.iceTransport = transports.iceTransport;
	      transceiver.dtlsTransport = transports.dtlsTransport;
	    }
	    if (!doNotAdd) {
	      this.transceivers.push(transceiver);
	    }
	    return transceiver;
	  };

	  RTCPeerConnection.prototype.addTrack = function(track, stream) {
	    if (this._isClosed) {
	      throw makeError('InvalidStateError',
	          'Attempted to call addTrack on a closed peerconnection.');
	    }

	    var alreadyExists = this.transceivers.find(function(s) {
	      return s.track === track;
	    });

	    if (alreadyExists) {
	      throw makeError('InvalidAccessError', 'Track already exists.');
	    }

	    var transceiver;
	    for (var i = 0; i < this.transceivers.length; i++) {
	      if (!this.transceivers[i].track &&
	          this.transceivers[i].kind === track.kind) {
	        transceiver = this.transceivers[i];
	      }
	    }
	    if (!transceiver) {
	      transceiver = this._createTransceiver(track.kind);
	    }

	    this._maybeFireNegotiationNeeded();

	    if (this.localStreams.indexOf(stream) === -1) {
	      this.localStreams.push(stream);
	    }

	    transceiver.track = track;
	    transceiver.stream = stream;
	    transceiver.rtpSender = new window.RTCRtpSender(track,
	        transceiver.dtlsTransport);
	    return transceiver.rtpSender;
	  };

	  RTCPeerConnection.prototype.addStream = function(stream) {
	    var pc = this;
	    if (edgeVersion >= 15025) {
	      stream.getTracks().forEach(function(track) {
	        pc.addTrack(track, stream);
	      });
	    } else {
	      // Clone is necessary for local demos mostly, attaching directly
	      // to two different senders does not work (build 10547).
	      // Fixed in 15025 (or earlier)
	      var clonedStream = stream.clone();
	      stream.getTracks().forEach(function(track, idx) {
	        var clonedTrack = clonedStream.getTracks()[idx];
	        track.addEventListener('enabled', function(event) {
	          clonedTrack.enabled = event.enabled;
	        });
	      });
	      clonedStream.getTracks().forEach(function(track) {
	        pc.addTrack(track, clonedStream);
	      });
	    }
	  };

	  RTCPeerConnection.prototype.removeTrack = function(sender) {
	    if (this._isClosed) {
	      throw makeError('InvalidStateError',
	          'Attempted to call removeTrack on a closed peerconnection.');
	    }

	    if (!(sender instanceof window.RTCRtpSender)) {
	      throw new TypeError('Argument 1 of RTCPeerConnection.removeTrack ' +
	          'does not implement interface RTCRtpSender.');
	    }

	    var transceiver = this.transceivers.find(function(t) {
	      return t.rtpSender === sender;
	    });

	    if (!transceiver) {
	      throw makeError('InvalidAccessError',
	          'Sender was not created by this connection.');
	    }
	    var stream = transceiver.stream;

	    transceiver.rtpSender.stop();
	    transceiver.rtpSender = null;
	    transceiver.track = null;
	    transceiver.stream = null;

	    // remove the stream from the set of local streams
	    var localStreams = this.transceivers.map(function(t) {
	      return t.stream;
	    });
	    if (localStreams.indexOf(stream) === -1 &&
	        this.localStreams.indexOf(stream) > -1) {
	      this.localStreams.splice(this.localStreams.indexOf(stream), 1);
	    }

	    this._maybeFireNegotiationNeeded();
	  };

	  RTCPeerConnection.prototype.removeStream = function(stream) {
	    var pc = this;
	    stream.getTracks().forEach(function(track) {
	      var sender = pc.getSenders().find(function(s) {
	        return s.track === track;
	      });
	      if (sender) {
	        pc.removeTrack(sender);
	      }
	    });
	  };

	  RTCPeerConnection.prototype.getSenders = function() {
	    return this.transceivers.filter(function(transceiver) {
	      return !!transceiver.rtpSender;
	    })
	    .map(function(transceiver) {
	      return transceiver.rtpSender;
	    });
	  };

	  RTCPeerConnection.prototype.getReceivers = function() {
	    return this.transceivers.filter(function(transceiver) {
	      return !!transceiver.rtpReceiver;
	    })
	    .map(function(transceiver) {
	      return transceiver.rtpReceiver;
	    });
	  };


	  RTCPeerConnection.prototype._createIceGatherer = function(sdpMLineIndex,
	      usingBundle) {
	    var pc = this;
	    if (usingBundle && sdpMLineIndex > 0) {
	      return this.transceivers[0].iceGatherer;
	    } else if (this._iceGatherers.length) {
	      return this._iceGatherers.shift();
	    }
	    var iceGatherer = new window.RTCIceGatherer({
	      iceServers: this._config.iceServers,
	      gatherPolicy: this._config.iceTransportPolicy
	    });
	    Object.defineProperty(iceGatherer, 'state',
	        {value: 'new', writable: true}
	    );

	    this.transceivers[sdpMLineIndex].bufferedCandidateEvents = [];
	    this.transceivers[sdpMLineIndex].bufferCandidates = function(event) {
	      var end = !event.candidate || Object.keys(event.candidate).length === 0;
	      // polyfill since RTCIceGatherer.state is not implemented in
	      // Edge 10547 yet.
	      iceGatherer.state = end ? 'completed' : 'gathering';
	      if (pc.transceivers[sdpMLineIndex].bufferedCandidateEvents !== null) {
	        pc.transceivers[sdpMLineIndex].bufferedCandidateEvents.push(event);
	      }
	    };
	    iceGatherer.addEventListener('localcandidate',
	      this.transceivers[sdpMLineIndex].bufferCandidates);
	    return iceGatherer;
	  };

	  // start gathering from an RTCIceGatherer.
	  RTCPeerConnection.prototype._gather = function(mid, sdpMLineIndex) {
	    var pc = this;
	    var iceGatherer = this.transceivers[sdpMLineIndex].iceGatherer;
	    if (iceGatherer.onlocalcandidate) {
	      return;
	    }
	    var bufferedCandidateEvents =
	      this.transceivers[sdpMLineIndex].bufferedCandidateEvents;
	    this.transceivers[sdpMLineIndex].bufferedCandidateEvents = null;
	    iceGatherer.removeEventListener('localcandidate',
	      this.transceivers[sdpMLineIndex].bufferCandidates);
	    iceGatherer.onlocalcandidate = function(evt) {
	      if (pc.usingBundle && sdpMLineIndex > 0) {
	        // if we know that we use bundle we can drop candidates with
	        // ѕdpMLineIndex > 0. If we don't do this then our state gets
	        // confused since we dispose the extra ice gatherer.
	        return;
	      }
	      var event = new Event('icecandidate');
	      event.candidate = {sdpMid: mid, sdpMLineIndex: sdpMLineIndex};

	      var cand = evt.candidate;
	      // Edge emits an empty object for RTCIceCandidateComplete‥
	      var end = !cand || Object.keys(cand).length === 0;
	      if (end) {
	        // polyfill since RTCIceGatherer.state is not implemented in
	        // Edge 10547 yet.
	        if (iceGatherer.state === 'new' || iceGatherer.state === 'gathering') {
	          iceGatherer.state = 'completed';
	        }
	      } else {
	        if (iceGatherer.state === 'new') {
	          iceGatherer.state = 'gathering';
	        }
	        // RTCIceCandidate doesn't have a component, needs to be added
	        cand.component = 1;
	        // also the usernameFragment. TODO: update SDP to take both variants.
	        cand.ufrag = iceGatherer.getLocalParameters().usernameFragment;

	        var serializedCandidate = sdp.writeCandidate(cand);
	        event.candidate = Object.assign(event.candidate,
	            sdp.parseCandidate(serializedCandidate));

	        event.candidate.candidate = serializedCandidate;
	        event.candidate.toJSON = function() {
	          return {
	            candidate: event.candidate.candidate,
	            sdpMid: event.candidate.sdpMid,
	            sdpMLineIndex: event.candidate.sdpMLineIndex,
	            usernameFragment: event.candidate.usernameFragment
	          };
	        };
	      }

	      // update local description.
	      var sections = sdp.getMediaSections(pc._localDescription.sdp);
	      if (!end) {
	        sections[event.candidate.sdpMLineIndex] +=
	            'a=' + event.candidate.candidate + '\r\n';
	      } else {
	        sections[event.candidate.sdpMLineIndex] +=
	            'a=end-of-candidates\r\n';
	      }
	      pc._localDescription.sdp =
	          sdp.getDescription(pc._localDescription.sdp) +
	          sections.join('');
	      var complete = pc.transceivers.every(function(transceiver) {
	        return transceiver.iceGatherer &&
	            transceiver.iceGatherer.state === 'completed';
	      });

	      if (pc.iceGatheringState !== 'gathering') {
	        pc.iceGatheringState = 'gathering';
	        pc._emitGatheringStateChange();
	      }

	      // Emit candidate. Also emit null candidate when all gatherers are
	      // complete.
	      if (!end) {
	        pc._dispatchEvent('icecandidate', event);
	      }
	      if (complete) {
	        pc._dispatchEvent('icecandidate', new Event('icecandidate'));
	        pc.iceGatheringState = 'complete';
	        pc._emitGatheringStateChange();
	      }
	    };

	    // emit already gathered candidates.
	    window.setTimeout(function() {
	      bufferedCandidateEvents.forEach(function(e) {
	        iceGatherer.onlocalcandidate(e);
	      });
	    }, 0);
	  };

	  // Create ICE transport and DTLS transport.
	  RTCPeerConnection.prototype._createIceAndDtlsTransports = function() {
	    var pc = this;
	    var iceTransport = new window.RTCIceTransport(null);
	    iceTransport.onicestatechange = function() {
	      pc._updateIceConnectionState();
	      pc._updateConnectionState();
	    };

	    var dtlsTransport = new window.RTCDtlsTransport(iceTransport);
	    dtlsTransport.ondtlsstatechange = function() {
	      pc._updateConnectionState();
	    };
	    dtlsTransport.onerror = function() {
	      // onerror does not set state to failed by itself.
	      Object.defineProperty(dtlsTransport, 'state',
	          {value: 'failed', writable: true});
	      pc._updateConnectionState();
	    };

	    return {
	      iceTransport: iceTransport,
	      dtlsTransport: dtlsTransport
	    };
	  };

	  // Destroy ICE gatherer, ICE transport and DTLS transport.
	  // Without triggering the callbacks.
	  RTCPeerConnection.prototype._disposeIceAndDtlsTransports = function(
	      sdpMLineIndex) {
	    var iceGatherer = this.transceivers[sdpMLineIndex].iceGatherer;
	    if (iceGatherer) {
	      delete iceGatherer.onlocalcandidate;
	      delete this.transceivers[sdpMLineIndex].iceGatherer;
	    }
	    var iceTransport = this.transceivers[sdpMLineIndex].iceTransport;
	    if (iceTransport) {
	      delete iceTransport.onicestatechange;
	      delete this.transceivers[sdpMLineIndex].iceTransport;
	    }
	    var dtlsTransport = this.transceivers[sdpMLineIndex].dtlsTransport;
	    if (dtlsTransport) {
	      delete dtlsTransport.ondtlsstatechange;
	      delete dtlsTransport.onerror;
	      delete this.transceivers[sdpMLineIndex].dtlsTransport;
	    }
	  };

	  // Start the RTP Sender and Receiver for a transceiver.
	  RTCPeerConnection.prototype._transceive = function(transceiver,
	      send, recv) {
	    var params = getCommonCapabilities(transceiver.localCapabilities,
	        transceiver.remoteCapabilities);
	    if (send && transceiver.rtpSender) {
	      params.encodings = transceiver.sendEncodingParameters;
	      params.rtcp = {
	        cname: sdp.localCName,
	        compound: transceiver.rtcpParameters.compound
	      };
	      if (transceiver.recvEncodingParameters.length) {
	        params.rtcp.ssrc = transceiver.recvEncodingParameters[0].ssrc;
	      }
	      transceiver.rtpSender.send(params);
	    }
	    if (recv && transceiver.rtpReceiver && params.codecs.length > 0) {
	      // remove RTX field in Edge 14942
	      if (transceiver.kind === 'video'
	          && transceiver.recvEncodingParameters
	          && edgeVersion < 15019) {
	        transceiver.recvEncodingParameters.forEach(function(p) {
	          delete p.rtx;
	        });
	      }
	      if (transceiver.recvEncodingParameters.length) {
	        params.encodings = transceiver.recvEncodingParameters;
	      } else {
	        params.encodings = [{}];
	      }
	      params.rtcp = {
	        compound: transceiver.rtcpParameters.compound
	      };
	      if (transceiver.rtcpParameters.cname) {
	        params.rtcp.cname = transceiver.rtcpParameters.cname;
	      }
	      if (transceiver.sendEncodingParameters.length) {
	        params.rtcp.ssrc = transceiver.sendEncodingParameters[0].ssrc;
	      }
	      transceiver.rtpReceiver.receive(params);
	    }
	  };

	  RTCPeerConnection.prototype.setLocalDescription = function(description) {
	    var pc = this;

	    // Note: pranswer is not supported.
	    if (['offer', 'answer'].indexOf(description.type) === -1) {
	      return Promise.reject(makeError('TypeError',
	          'Unsupported type "' + description.type + '"'));
	    }

	    if (!isActionAllowedInSignalingState('setLocalDescription',
	        description.type, pc.signalingState) || pc._isClosed) {
	      return Promise.reject(makeError('InvalidStateError',
	          'Can not set local ' + description.type +
	          ' in state ' + pc.signalingState));
	    }

	    var sections;
	    var sessionpart;
	    if (description.type === 'offer') {
	      // VERY limited support for SDP munging. Limited to:
	      // * changing the order of codecs
	      sections = sdp.splitSections(description.sdp);
	      sessionpart = sections.shift();
	      sections.forEach(function(mediaSection, sdpMLineIndex) {
	        var caps = sdp.parseRtpParameters(mediaSection);
	        pc.transceivers[sdpMLineIndex].localCapabilities = caps;
	      });

	      pc.transceivers.forEach(function(transceiver, sdpMLineIndex) {
	        pc._gather(transceiver.mid, sdpMLineIndex);
	      });
	    } else if (description.type === 'answer') {
	      sections = sdp.splitSections(pc._remoteDescription.sdp);
	      sessionpart = sections.shift();
	      var isIceLite = sdp.matchPrefix(sessionpart,
	          'a=ice-lite').length > 0;
	      sections.forEach(function(mediaSection, sdpMLineIndex) {
	        var transceiver = pc.transceivers[sdpMLineIndex];
	        var iceGatherer = transceiver.iceGatherer;
	        var iceTransport = transceiver.iceTransport;
	        var dtlsTransport = transceiver.dtlsTransport;
	        var localCapabilities = transceiver.localCapabilities;
	        var remoteCapabilities = transceiver.remoteCapabilities;

	        // treat bundle-only as not-rejected.
	        var rejected = sdp.isRejected(mediaSection) &&
	            sdp.matchPrefix(mediaSection, 'a=bundle-only').length === 0;

	        if (!rejected && !transceiver.rejected) {
	          var remoteIceParameters = sdp.getIceParameters(
	              mediaSection, sessionpart);
	          var remoteDtlsParameters = sdp.getDtlsParameters(
	              mediaSection, sessionpart);
	          if (isIceLite) {
	            remoteDtlsParameters.role = 'server';
	          }

	          if (!pc.usingBundle || sdpMLineIndex === 0) {
	            pc._gather(transceiver.mid, sdpMLineIndex);
	            if (iceTransport.state === 'new') {
	              iceTransport.start(iceGatherer, remoteIceParameters,
	                  isIceLite ? 'controlling' : 'controlled');
	            }
	            if (dtlsTransport.state === 'new') {
	              dtlsTransport.start(remoteDtlsParameters);
	            }
	          }

	          // Calculate intersection of capabilities.
	          var params = getCommonCapabilities(localCapabilities,
	              remoteCapabilities);

	          // Start the RTCRtpSender. The RTCRtpReceiver for this
	          // transceiver has already been started in setRemoteDescription.
	          pc._transceive(transceiver,
	              params.codecs.length > 0,
	              false);
	        }
	      });
	    }

	    pc._localDescription = {
	      type: description.type,
	      sdp: description.sdp
	    };
	    if (description.type === 'offer') {
	      pc._updateSignalingState('have-local-offer');
	    } else {
	      pc._updateSignalingState('stable');
	    }

	    return Promise.resolve();
	  };

	  RTCPeerConnection.prototype.setRemoteDescription = function(description) {
	    var pc = this;

	    // Note: pranswer is not supported.
	    if (['offer', 'answer'].indexOf(description.type) === -1) {
	      return Promise.reject(makeError('TypeError',
	          'Unsupported type "' + description.type + '"'));
	    }

	    if (!isActionAllowedInSignalingState('setRemoteDescription',
	        description.type, pc.signalingState) || pc._isClosed) {
	      return Promise.reject(makeError('InvalidStateError',
	          'Can not set remote ' + description.type +
	          ' in state ' + pc.signalingState));
	    }

	    var streams = {};
	    pc.remoteStreams.forEach(function(stream) {
	      streams[stream.id] = stream;
	    });
	    var receiverList = [];
	    var sections = sdp.splitSections(description.sdp);
	    var sessionpart = sections.shift();
	    var isIceLite = sdp.matchPrefix(sessionpart,
	        'a=ice-lite').length > 0;
	    var usingBundle = sdp.matchPrefix(sessionpart,
	        'a=group:BUNDLE ').length > 0;
	    pc.usingBundle = usingBundle;
	    var iceOptions = sdp.matchPrefix(sessionpart,
	        'a=ice-options:')[0];
	    if (iceOptions) {
	      pc.canTrickleIceCandidates = iceOptions.substr(14).split(' ')
	          .indexOf('trickle') >= 0;
	    } else {
	      pc.canTrickleIceCandidates = false;
	    }

	    sections.forEach(function(mediaSection, sdpMLineIndex) {
	      var lines = sdp.splitLines(mediaSection);
	      var kind = sdp.getKind(mediaSection);
	      // treat bundle-only as not-rejected.
	      var rejected = sdp.isRejected(mediaSection) &&
	          sdp.matchPrefix(mediaSection, 'a=bundle-only').length === 0;
	      var protocol = lines[0].substr(2).split(' ')[2];

	      var direction = sdp.getDirection(mediaSection, sessionpart);
	      var remoteMsid = sdp.parseMsid(mediaSection);

	      var mid = sdp.getMid(mediaSection) || sdp.generateIdentifier();

	      // Reject datachannels which are not implemented yet.
	      if (rejected || (kind === 'application' && (protocol === 'DTLS/SCTP' ||
	          protocol === 'UDP/DTLS/SCTP'))) {
	        // TODO: this is dangerous in the case where a non-rejected m-line
	        //     becomes rejected.
	        pc.transceivers[sdpMLineIndex] = {
	          mid: mid,
	          kind: kind,
	          protocol: protocol,
	          rejected: true
	        };
	        return;
	      }

	      if (!rejected && pc.transceivers[sdpMLineIndex] &&
	          pc.transceivers[sdpMLineIndex].rejected) {
	        // recycle a rejected transceiver.
	        pc.transceivers[sdpMLineIndex] = pc._createTransceiver(kind, true);
	      }

	      var transceiver;
	      var iceGatherer;
	      var iceTransport;
	      var dtlsTransport;
	      var rtpReceiver;
	      var sendEncodingParameters;
	      var recvEncodingParameters;
	      var localCapabilities;

	      var track;
	      // FIXME: ensure the mediaSection has rtcp-mux set.
	      var remoteCapabilities = sdp.parseRtpParameters(mediaSection);
	      var remoteIceParameters;
	      var remoteDtlsParameters;
	      if (!rejected) {
	        remoteIceParameters = sdp.getIceParameters(mediaSection,
	            sessionpart);
	        remoteDtlsParameters = sdp.getDtlsParameters(mediaSection,
	            sessionpart);
	        remoteDtlsParameters.role = 'client';
	      }
	      recvEncodingParameters =
	          sdp.parseRtpEncodingParameters(mediaSection);

	      var rtcpParameters = sdp.parseRtcpParameters(mediaSection);

	      var isComplete = sdp.matchPrefix(mediaSection,
	          'a=end-of-candidates', sessionpart).length > 0;
	      var cands = sdp.matchPrefix(mediaSection, 'a=candidate:')
	          .map(function(cand) {
	            return sdp.parseCandidate(cand);
	          })
	          .filter(function(cand) {
	            return cand.component === 1;
	          });

	      // Check if we can use BUNDLE and dispose transports.
	      if ((description.type === 'offer' || description.type === 'answer') &&
	          !rejected && usingBundle && sdpMLineIndex > 0 &&
	          pc.transceivers[sdpMLineIndex]) {
	        pc._disposeIceAndDtlsTransports(sdpMLineIndex);
	        pc.transceivers[sdpMLineIndex].iceGatherer =
	            pc.transceivers[0].iceGatherer;
	        pc.transceivers[sdpMLineIndex].iceTransport =
	            pc.transceivers[0].iceTransport;
	        pc.transceivers[sdpMLineIndex].dtlsTransport =
	            pc.transceivers[0].dtlsTransport;
	        if (pc.transceivers[sdpMLineIndex].rtpSender) {
	          pc.transceivers[sdpMLineIndex].rtpSender.setTransport(
	              pc.transceivers[0].dtlsTransport);
	        }
	        if (pc.transceivers[sdpMLineIndex].rtpReceiver) {
	          pc.transceivers[sdpMLineIndex].rtpReceiver.setTransport(
	              pc.transceivers[0].dtlsTransport);
	        }
	      }
	      if (description.type === 'offer' && !rejected) {
	        transceiver = pc.transceivers[sdpMLineIndex] ||
	            pc._createTransceiver(kind);
	        transceiver.mid = mid;

	        if (!transceiver.iceGatherer) {
	          transceiver.iceGatherer = pc._createIceGatherer(sdpMLineIndex,
	              usingBundle);
	        }

	        if (cands.length && transceiver.iceTransport.state === 'new') {
	          if (isComplete && (!usingBundle || sdpMLineIndex === 0)) {
	            transceiver.iceTransport.setRemoteCandidates(cands);
	          } else {
	            cands.forEach(function(candidate) {
	              maybeAddCandidate(transceiver.iceTransport, candidate);
	            });
	          }
	        }

	        localCapabilities = window.RTCRtpReceiver.getCapabilities(kind);

	        // filter RTX until additional stuff needed for RTX is implemented
	        // in adapter.js
	        if (edgeVersion < 15019) {
	          localCapabilities.codecs = localCapabilities.codecs.filter(
	              function(codec) {
	                return codec.name !== 'rtx';
	              });
	        }

	        sendEncodingParameters = transceiver.sendEncodingParameters || [{
	          ssrc: (2 * sdpMLineIndex + 2) * 1001
	        }];

	        // TODO: rewrite to use http://w3c.github.io/webrtc-pc/#set-associated-remote-streams
	        var isNewTrack = false;
	        if (direction === 'sendrecv' || direction === 'sendonly') {
	          isNewTrack = !transceiver.rtpReceiver;
	          rtpReceiver = transceiver.rtpReceiver ||
	              new window.RTCRtpReceiver(transceiver.dtlsTransport, kind);

	          if (isNewTrack) {
	            var stream;
	            track = rtpReceiver.track;
	            // FIXME: does not work with Plan B.
	            if (remoteMsid && remoteMsid.stream === '-') ; else if (remoteMsid) {
	              if (!streams[remoteMsid.stream]) {
	                streams[remoteMsid.stream] = new window.MediaStream();
	                Object.defineProperty(streams[remoteMsid.stream], 'id', {
	                  get: function() {
	                    return remoteMsid.stream;
	                  }
	                });
	              }
	              Object.defineProperty(track, 'id', {
	                get: function() {
	                  return remoteMsid.track;
	                }
	              });
	              stream = streams[remoteMsid.stream];
	            } else {
	              if (!streams.default) {
	                streams.default = new window.MediaStream();
	              }
	              stream = streams.default;
	            }
	            if (stream) {
	              addTrackToStreamAndFireEvent(track, stream);
	              transceiver.associatedRemoteMediaStreams.push(stream);
	            }
	            receiverList.push([track, rtpReceiver, stream]);
	          }
	        } else if (transceiver.rtpReceiver && transceiver.rtpReceiver.track) {
	          transceiver.associatedRemoteMediaStreams.forEach(function(s) {
	            var nativeTrack = s.getTracks().find(function(t) {
	              return t.id === transceiver.rtpReceiver.track.id;
	            });
	            if (nativeTrack) {
	              removeTrackFromStreamAndFireEvent(nativeTrack, s);
	            }
	          });
	          transceiver.associatedRemoteMediaStreams = [];
	        }

	        transceiver.localCapabilities = localCapabilities;
	        transceiver.remoteCapabilities = remoteCapabilities;
	        transceiver.rtpReceiver = rtpReceiver;
	        transceiver.rtcpParameters = rtcpParameters;
	        transceiver.sendEncodingParameters = sendEncodingParameters;
	        transceiver.recvEncodingParameters = recvEncodingParameters;

	        // Start the RTCRtpReceiver now. The RTPSender is started in
	        // setLocalDescription.
	        pc._transceive(pc.transceivers[sdpMLineIndex],
	            false,
	            isNewTrack);
	      } else if (description.type === 'answer' && !rejected) {
	        transceiver = pc.transceivers[sdpMLineIndex];
	        iceGatherer = transceiver.iceGatherer;
	        iceTransport = transceiver.iceTransport;
	        dtlsTransport = transceiver.dtlsTransport;
	        rtpReceiver = transceiver.rtpReceiver;
	        sendEncodingParameters = transceiver.sendEncodingParameters;
	        localCapabilities = transceiver.localCapabilities;

	        pc.transceivers[sdpMLineIndex].recvEncodingParameters =
	            recvEncodingParameters;
	        pc.transceivers[sdpMLineIndex].remoteCapabilities =
	            remoteCapabilities;
	        pc.transceivers[sdpMLineIndex].rtcpParameters = rtcpParameters;

	        if (cands.length && iceTransport.state === 'new') {
	          if ((isIceLite || isComplete) &&
	              (!usingBundle || sdpMLineIndex === 0)) {
	            iceTransport.setRemoteCandidates(cands);
	          } else {
	            cands.forEach(function(candidate) {
	              maybeAddCandidate(transceiver.iceTransport, candidate);
	            });
	          }
	        }

	        if (!usingBundle || sdpMLineIndex === 0) {
	          if (iceTransport.state === 'new') {
	            iceTransport.start(iceGatherer, remoteIceParameters,
	                'controlling');
	          }
	          if (dtlsTransport.state === 'new') {
	            dtlsTransport.start(remoteDtlsParameters);
	          }
	        }

	        // If the offer contained RTX but the answer did not,
	        // remove RTX from sendEncodingParameters.
	        var commonCapabilities = getCommonCapabilities(
	          transceiver.localCapabilities,
	          transceiver.remoteCapabilities);

	        var hasRtx = commonCapabilities.codecs.filter(function(c) {
	          return c.name.toLowerCase() === 'rtx';
	        }).length;
	        if (!hasRtx && transceiver.sendEncodingParameters[0].rtx) {
	          delete transceiver.sendEncodingParameters[0].rtx;
	        }

	        pc._transceive(transceiver,
	            direction === 'sendrecv' || direction === 'recvonly',
	            direction === 'sendrecv' || direction === 'sendonly');

	        // TODO: rewrite to use http://w3c.github.io/webrtc-pc/#set-associated-remote-streams
	        if (rtpReceiver &&
	            (direction === 'sendrecv' || direction === 'sendonly')) {
	          track = rtpReceiver.track;
	          if (remoteMsid) {
	            if (!streams[remoteMsid.stream]) {
	              streams[remoteMsid.stream] = new window.MediaStream();
	            }
	            addTrackToStreamAndFireEvent(track, streams[remoteMsid.stream]);
	            receiverList.push([track, rtpReceiver, streams[remoteMsid.stream]]);
	          } else {
	            if (!streams.default) {
	              streams.default = new window.MediaStream();
	            }
	            addTrackToStreamAndFireEvent(track, streams.default);
	            receiverList.push([track, rtpReceiver, streams.default]);
	          }
	        } else {
	          // FIXME: actually the receiver should be created later.
	          delete transceiver.rtpReceiver;
	        }
	      }
	    });

	    if (pc._dtlsRole === undefined) {
	      pc._dtlsRole = description.type === 'offer' ? 'active' : 'passive';
	    }

	    pc._remoteDescription = {
	      type: description.type,
	      sdp: description.sdp
	    };
	    if (description.type === 'offer') {
	      pc._updateSignalingState('have-remote-offer');
	    } else {
	      pc._updateSignalingState('stable');
	    }
	    Object.keys(streams).forEach(function(sid) {
	      var stream = streams[sid];
	      if (stream.getTracks().length) {
	        if (pc.remoteStreams.indexOf(stream) === -1) {
	          pc.remoteStreams.push(stream);
	          var event = new Event('addstream');
	          event.stream = stream;
	          window.setTimeout(function() {
	            pc._dispatchEvent('addstream', event);
	          });
	        }

	        receiverList.forEach(function(item) {
	          var track = item[0];
	          var receiver = item[1];
	          if (stream.id !== item[2].id) {
	            return;
	          }
	          fireAddTrack(pc, track, receiver, [stream]);
	        });
	      }
	    });
	    receiverList.forEach(function(item) {
	      if (item[2]) {
	        return;
	      }
	      fireAddTrack(pc, item[0], item[1], []);
	    });

	    // check whether addIceCandidate({}) was called within four seconds after
	    // setRemoteDescription.
	    window.setTimeout(function() {
	      if (!(pc && pc.transceivers)) {
	        return;
	      }
	      pc.transceivers.forEach(function(transceiver) {
	        if (transceiver.iceTransport &&
	            transceiver.iceTransport.state === 'new' &&
	            transceiver.iceTransport.getRemoteCandidates().length > 0) {
	          console.warn('Timeout for addRemoteCandidate. Consider sending ' +
	              'an end-of-candidates notification');
	          transceiver.iceTransport.addRemoteCandidate({});
	        }
	      });
	    }, 4000);

	    return Promise.resolve();
	  };

	  RTCPeerConnection.prototype.close = function() {
	    this.transceivers.forEach(function(transceiver) {
	      /* not yet
	      if (transceiver.iceGatherer) {
	        transceiver.iceGatherer.close();
	      }
	      */
	      if (transceiver.iceTransport) {
	        transceiver.iceTransport.stop();
	      }
	      if (transceiver.dtlsTransport) {
	        transceiver.dtlsTransport.stop();
	      }
	      if (transceiver.rtpSender) {
	        transceiver.rtpSender.stop();
	      }
	      if (transceiver.rtpReceiver) {
	        transceiver.rtpReceiver.stop();
	      }
	    });
	    // FIXME: clean up tracks, local streams, remote streams, etc
	    this._isClosed = true;
	    this._updateSignalingState('closed');
	  };

	  // Update the signaling state.
	  RTCPeerConnection.prototype._updateSignalingState = function(newState) {
	    this.signalingState = newState;
	    var event = new Event('signalingstatechange');
	    this._dispatchEvent('signalingstatechange', event);
	  };

	  // Determine whether to fire the negotiationneeded event.
	  RTCPeerConnection.prototype._maybeFireNegotiationNeeded = function() {
	    var pc = this;
	    if (this.signalingState !== 'stable' || this.needNegotiation === true) {
	      return;
	    }
	    this.needNegotiation = true;
	    window.setTimeout(function() {
	      if (pc.needNegotiation) {
	        pc.needNegotiation = false;
	        var event = new Event('negotiationneeded');
	        pc._dispatchEvent('negotiationneeded', event);
	      }
	    }, 0);
	  };

	  // Update the ice connection state.
	  RTCPeerConnection.prototype._updateIceConnectionState = function() {
	    var newState;
	    var states = {
	      'new': 0,
	      closed: 0,
	      checking: 0,
	      connected: 0,
	      completed: 0,
	      disconnected: 0,
	      failed: 0
	    };
	    this.transceivers.forEach(function(transceiver) {
	      if (transceiver.iceTransport && !transceiver.rejected) {
	        states[transceiver.iceTransport.state]++;
	      }
	    });

	    newState = 'new';
	    if (states.failed > 0) {
	      newState = 'failed';
	    } else if (states.checking > 0) {
	      newState = 'checking';
	    } else if (states.disconnected > 0) {
	      newState = 'disconnected';
	    } else if (states.new > 0) {
	      newState = 'new';
	    } else if (states.connected > 0) {
	      newState = 'connected';
	    } else if (states.completed > 0) {
	      newState = 'completed';
	    }

	    if (newState !== this.iceConnectionState) {
	      this.iceConnectionState = newState;
	      var event = new Event('iceconnectionstatechange');
	      this._dispatchEvent('iceconnectionstatechange', event);
	    }
	  };

	  // Update the connection state.
	  RTCPeerConnection.prototype._updateConnectionState = function() {
	    var newState;
	    var states = {
	      'new': 0,
	      closed: 0,
	      connecting: 0,
	      connected: 0,
	      completed: 0,
	      disconnected: 0,
	      failed: 0
	    };
	    this.transceivers.forEach(function(transceiver) {
	      if (transceiver.iceTransport && transceiver.dtlsTransport &&
	          !transceiver.rejected) {
	        states[transceiver.iceTransport.state]++;
	        states[transceiver.dtlsTransport.state]++;
	      }
	    });
	    // ICETransport.completed and connected are the same for this purpose.
	    states.connected += states.completed;

	    newState = 'new';
	    if (states.failed > 0) {
	      newState = 'failed';
	    } else if (states.connecting > 0) {
	      newState = 'connecting';
	    } else if (states.disconnected > 0) {
	      newState = 'disconnected';
	    } else if (states.new > 0) {
	      newState = 'new';
	    } else if (states.connected > 0) {
	      newState = 'connected';
	    }

	    if (newState !== this.connectionState) {
	      this.connectionState = newState;
	      var event = new Event('connectionstatechange');
	      this._dispatchEvent('connectionstatechange', event);
	    }
	  };

	  RTCPeerConnection.prototype.createOffer = function() {
	    var pc = this;

	    if (pc._isClosed) {
	      return Promise.reject(makeError('InvalidStateError',
	          'Can not call createOffer after close'));
	    }

	    var numAudioTracks = pc.transceivers.filter(function(t) {
	      return t.kind === 'audio';
	    }).length;
	    var numVideoTracks = pc.transceivers.filter(function(t) {
	      return t.kind === 'video';
	    }).length;

	    // Determine number of audio and video tracks we need to send/recv.
	    var offerOptions = arguments[0];
	    if (offerOptions) {
	      // Reject Chrome legacy constraints.
	      if (offerOptions.mandatory || offerOptions.optional) {
	        throw new TypeError(
	            'Legacy mandatory/optional constraints not supported.');
	      }
	      if (offerOptions.offerToReceiveAudio !== undefined) {
	        if (offerOptions.offerToReceiveAudio === true) {
	          numAudioTracks = 1;
	        } else if (offerOptions.offerToReceiveAudio === false) {
	          numAudioTracks = 0;
	        } else {
	          numAudioTracks = offerOptions.offerToReceiveAudio;
	        }
	      }
	      if (offerOptions.offerToReceiveVideo !== undefined) {
	        if (offerOptions.offerToReceiveVideo === true) {
	          numVideoTracks = 1;
	        } else if (offerOptions.offerToReceiveVideo === false) {
	          numVideoTracks = 0;
	        } else {
	          numVideoTracks = offerOptions.offerToReceiveVideo;
	        }
	      }
	    }

	    pc.transceivers.forEach(function(transceiver) {
	      if (transceiver.kind === 'audio') {
	        numAudioTracks--;
	        if (numAudioTracks < 0) {
	          transceiver.wantReceive = false;
	        }
	      } else if (transceiver.kind === 'video') {
	        numVideoTracks--;
	        if (numVideoTracks < 0) {
	          transceiver.wantReceive = false;
	        }
	      }
	    });

	    // Create M-lines for recvonly streams.
	    while (numAudioTracks > 0 || numVideoTracks > 0) {
	      if (numAudioTracks > 0) {
	        pc._createTransceiver('audio');
	        numAudioTracks--;
	      }
	      if (numVideoTracks > 0) {
	        pc._createTransceiver('video');
	        numVideoTracks--;
	      }
	    }

	    var sdp$1 = sdp.writeSessionBoilerplate(pc._sdpSessionId,
	        pc._sdpSessionVersion++);
	    pc.transceivers.forEach(function(transceiver, sdpMLineIndex) {
	      // For each track, create an ice gatherer, ice transport,
	      // dtls transport, potentially rtpsender and rtpreceiver.
	      var track = transceiver.track;
	      var kind = transceiver.kind;
	      var mid = transceiver.mid || sdp.generateIdentifier();
	      transceiver.mid = mid;

	      if (!transceiver.iceGatherer) {
	        transceiver.iceGatherer = pc._createIceGatherer(sdpMLineIndex,
	            pc.usingBundle);
	      }

	      var localCapabilities = window.RTCRtpSender.getCapabilities(kind);
	      // filter RTX until additional stuff needed for RTX is implemented
	      // in adapter.js
	      if (edgeVersion < 15019) {
	        localCapabilities.codecs = localCapabilities.codecs.filter(
	            function(codec) {
	              return codec.name !== 'rtx';
	            });
	      }
	      localCapabilities.codecs.forEach(function(codec) {
	        // work around https://bugs.chromium.org/p/webrtc/issues/detail?id=6552
	        // by adding level-asymmetry-allowed=1
	        if (codec.name === 'H264' &&
	            codec.parameters['level-asymmetry-allowed'] === undefined) {
	          codec.parameters['level-asymmetry-allowed'] = '1';
	        }

	        // for subsequent offers, we might have to re-use the payload
	        // type of the last offer.
	        if (transceiver.remoteCapabilities &&
	            transceiver.remoteCapabilities.codecs) {
	          transceiver.remoteCapabilities.codecs.forEach(function(remoteCodec) {
	            if (codec.name.toLowerCase() === remoteCodec.name.toLowerCase() &&
	                codec.clockRate === remoteCodec.clockRate) {
	              codec.preferredPayloadType = remoteCodec.payloadType;
	            }
	          });
	        }
	      });
	      localCapabilities.headerExtensions.forEach(function(hdrExt) {
	        var remoteExtensions = transceiver.remoteCapabilities &&
	            transceiver.remoteCapabilities.headerExtensions || [];
	        remoteExtensions.forEach(function(rHdrExt) {
	          if (hdrExt.uri === rHdrExt.uri) {
	            hdrExt.id = rHdrExt.id;
	          }
	        });
	      });

	      // generate an ssrc now, to be used later in rtpSender.send
	      var sendEncodingParameters = transceiver.sendEncodingParameters || [{
	        ssrc: (2 * sdpMLineIndex + 1) * 1001
	      }];
	      if (track) {
	        // add RTX
	        if (edgeVersion >= 15019 && kind === 'video' &&
	            !sendEncodingParameters[0].rtx) {
	          sendEncodingParameters[0].rtx = {
	            ssrc: sendEncodingParameters[0].ssrc + 1
	          };
	        }
	      }

	      if (transceiver.wantReceive) {
	        transceiver.rtpReceiver = new window.RTCRtpReceiver(
	            transceiver.dtlsTransport, kind);
	      }

	      transceiver.localCapabilities = localCapabilities;
	      transceiver.sendEncodingParameters = sendEncodingParameters;
	    });

	    // always offer BUNDLE and dispose on return if not supported.
	    if (pc._config.bundlePolicy !== 'max-compat') {
	      sdp$1 += 'a=group:BUNDLE ' + pc.transceivers.map(function(t) {
	        return t.mid;
	      }).join(' ') + '\r\n';
	    }
	    sdp$1 += 'a=ice-options:trickle\r\n';

	    pc.transceivers.forEach(function(transceiver, sdpMLineIndex) {
	      sdp$1 += writeMediaSection(transceiver, transceiver.localCapabilities,
	          'offer', transceiver.stream, pc._dtlsRole);
	      sdp$1 += 'a=rtcp-rsize\r\n';

	      if (transceiver.iceGatherer && pc.iceGatheringState !== 'new' &&
	          (sdpMLineIndex === 0 || !pc.usingBundle)) {
	        transceiver.iceGatherer.getLocalCandidates().forEach(function(cand) {
	          cand.component = 1;
	          sdp$1 += 'a=' + sdp.writeCandidate(cand) + '\r\n';
	        });

	        if (transceiver.iceGatherer.state === 'completed') {
	          sdp$1 += 'a=end-of-candidates\r\n';
	        }
	      }
	    });

	    var desc = new window.RTCSessionDescription({
	      type: 'offer',
	      sdp: sdp$1
	    });
	    return Promise.resolve(desc);
	  };

	  RTCPeerConnection.prototype.createAnswer = function() {
	    var pc = this;

	    if (pc._isClosed) {
	      return Promise.reject(makeError('InvalidStateError',
	          'Can not call createAnswer after close'));
	    }

	    if (!(pc.signalingState === 'have-remote-offer' ||
	        pc.signalingState === 'have-local-pranswer')) {
	      return Promise.reject(makeError('InvalidStateError',
	          'Can not call createAnswer in signalingState ' + pc.signalingState));
	    }

	    var sdp$1 = sdp.writeSessionBoilerplate(pc._sdpSessionId,
	        pc._sdpSessionVersion++);
	    if (pc.usingBundle) {
	      sdp$1 += 'a=group:BUNDLE ' + pc.transceivers.map(function(t) {
	        return t.mid;
	      }).join(' ') + '\r\n';
	    }
	    sdp$1 += 'a=ice-options:trickle\r\n';

	    var mediaSectionsInOffer = sdp.getMediaSections(
	        pc._remoteDescription.sdp).length;
	    pc.transceivers.forEach(function(transceiver, sdpMLineIndex) {
	      if (sdpMLineIndex + 1 > mediaSectionsInOffer) {
	        return;
	      }
	      if (transceiver.rejected) {
	        if (transceiver.kind === 'application') {
	          if (transceiver.protocol === 'DTLS/SCTP') { // legacy fmt
	            sdp$1 += 'm=application 0 DTLS/SCTP 5000\r\n';
	          } else {
	            sdp$1 += 'm=application 0 ' + transceiver.protocol +
	                ' webrtc-datachannel\r\n';
	          }
	        } else if (transceiver.kind === 'audio') {
	          sdp$1 += 'm=audio 0 UDP/TLS/RTP/SAVPF 0\r\n' +
	              'a=rtpmap:0 PCMU/8000\r\n';
	        } else if (transceiver.kind === 'video') {
	          sdp$1 += 'm=video 0 UDP/TLS/RTP/SAVPF 120\r\n' +
	              'a=rtpmap:120 VP8/90000\r\n';
	        }
	        sdp$1 += 'c=IN IP4 0.0.0.0\r\n' +
	            'a=inactive\r\n' +
	            'a=mid:' + transceiver.mid + '\r\n';
	        return;
	      }

	      // FIXME: look at direction.
	      if (transceiver.stream) {
	        var localTrack;
	        if (transceiver.kind === 'audio') {
	          localTrack = transceiver.stream.getAudioTracks()[0];
	        } else if (transceiver.kind === 'video') {
	          localTrack = transceiver.stream.getVideoTracks()[0];
	        }
	        if (localTrack) {
	          // add RTX
	          if (edgeVersion >= 15019 && transceiver.kind === 'video' &&
	              !transceiver.sendEncodingParameters[0].rtx) {
	            transceiver.sendEncodingParameters[0].rtx = {
	              ssrc: transceiver.sendEncodingParameters[0].ssrc + 1
	            };
	          }
	        }
	      }

	      // Calculate intersection of capabilities.
	      var commonCapabilities = getCommonCapabilities(
	          transceiver.localCapabilities,
	          transceiver.remoteCapabilities);

	      var hasRtx = commonCapabilities.codecs.filter(function(c) {
	        return c.name.toLowerCase() === 'rtx';
	      }).length;
	      if (!hasRtx && transceiver.sendEncodingParameters[0].rtx) {
	        delete transceiver.sendEncodingParameters[0].rtx;
	      }

	      sdp$1 += writeMediaSection(transceiver, commonCapabilities,
	          'answer', transceiver.stream, pc._dtlsRole);
	      if (transceiver.rtcpParameters &&
	          transceiver.rtcpParameters.reducedSize) {
	        sdp$1 += 'a=rtcp-rsize\r\n';
	      }
	    });

	    var desc = new window.RTCSessionDescription({
	      type: 'answer',
	      sdp: sdp$1
	    });
	    return Promise.resolve(desc);
	  };

	  RTCPeerConnection.prototype.addIceCandidate = function(candidate) {
	    var pc = this;
	    var sections;
	    if (candidate && !(candidate.sdpMLineIndex !== undefined ||
	        candidate.sdpMid)) {
	      return Promise.reject(new TypeError('sdpMLineIndex or sdpMid required'));
	    }

	    // TODO: needs to go into ops queue.
	    return new Promise(function(resolve, reject) {
	      if (!pc._remoteDescription) {
	        return reject(makeError('InvalidStateError',
	            'Can not add ICE candidate without a remote description'));
	      } else if (!candidate || candidate.candidate === '') {
	        for (var j = 0; j < pc.transceivers.length; j++) {
	          if (pc.transceivers[j].rejected) {
	            continue;
	          }
	          pc.transceivers[j].iceTransport.addRemoteCandidate({});
	          sections = sdp.getMediaSections(pc._remoteDescription.sdp);
	          sections[j] += 'a=end-of-candidates\r\n';
	          pc._remoteDescription.sdp =
	              sdp.getDescription(pc._remoteDescription.sdp) +
	              sections.join('');
	          if (pc.usingBundle) {
	            break;
	          }
	        }
	      } else {
	        var sdpMLineIndex = candidate.sdpMLineIndex;
	        if (candidate.sdpMid) {
	          for (var i = 0; i < pc.transceivers.length; i++) {
	            if (pc.transceivers[i].mid === candidate.sdpMid) {
	              sdpMLineIndex = i;
	              break;
	            }
	          }
	        }
	        var transceiver = pc.transceivers[sdpMLineIndex];
	        if (transceiver) {
	          if (transceiver.rejected) {
	            return resolve();
	          }
	          var cand = Object.keys(candidate.candidate).length > 0 ?
	              sdp.parseCandidate(candidate.candidate) : {};
	          // Ignore Chrome's invalid candidates since Edge does not like them.
	          if (cand.protocol === 'tcp' && (cand.port === 0 || cand.port === 9)) {
	            return resolve();
	          }
	          // Ignore RTCP candidates, we assume RTCP-MUX.
	          if (cand.component && cand.component !== 1) {
	            return resolve();
	          }
	          // when using bundle, avoid adding candidates to the wrong
	          // ice transport. And avoid adding candidates added in the SDP.
	          if (sdpMLineIndex === 0 || (sdpMLineIndex > 0 &&
	              transceiver.iceTransport !== pc.transceivers[0].iceTransport)) {
	            if (!maybeAddCandidate(transceiver.iceTransport, cand)) {
	              return reject(makeError('OperationError',
	                  'Can not add ICE candidate'));
	            }
	          }

	          // update the remoteDescription.
	          var candidateString = candidate.candidate.trim();
	          if (candidateString.indexOf('a=') === 0) {
	            candidateString = candidateString.substr(2);
	          }
	          sections = sdp.getMediaSections(pc._remoteDescription.sdp);
	          sections[sdpMLineIndex] += 'a=' +
	              (cand.type ? candidateString : 'end-of-candidates')
	              + '\r\n';
	          pc._remoteDescription.sdp =
	              sdp.getDescription(pc._remoteDescription.sdp) +
	              sections.join('');
	        } else {
	          return reject(makeError('OperationError',
	              'Can not add ICE candidate'));
	        }
	      }
	      resolve();
	    });
	  };

	  RTCPeerConnection.prototype.getStats = function(selector) {
	    if (selector && selector instanceof window.MediaStreamTrack) {
	      var senderOrReceiver = null;
	      this.transceivers.forEach(function(transceiver) {
	        if (transceiver.rtpSender &&
	            transceiver.rtpSender.track === selector) {
	          senderOrReceiver = transceiver.rtpSender;
	        } else if (transceiver.rtpReceiver &&
	            transceiver.rtpReceiver.track === selector) {
	          senderOrReceiver = transceiver.rtpReceiver;
	        }
	      });
	      if (!senderOrReceiver) {
	        throw makeError('InvalidAccessError', 'Invalid selector.');
	      }
	      return senderOrReceiver.getStats();
	    }

	    var promises = [];
	    this.transceivers.forEach(function(transceiver) {
	      ['rtpSender', 'rtpReceiver', 'iceGatherer', 'iceTransport',
	          'dtlsTransport'].forEach(function(method) {
	            if (transceiver[method]) {
	              promises.push(transceiver[method].getStats());
	            }
	          });
	    });
	    return Promise.all(promises).then(function(allStats) {
	      var results = new Map();
	      allStats.forEach(function(stats) {
	        stats.forEach(function(stat) {
	          results.set(stat.id, stat);
	        });
	      });
	      return results;
	    });
	  };

	  // fix low-level stat names and return Map instead of object.
	  var ortcObjects = ['RTCRtpSender', 'RTCRtpReceiver', 'RTCIceGatherer',
	    'RTCIceTransport', 'RTCDtlsTransport'];
	  ortcObjects.forEach(function(ortcObjectName) {
	    var obj = window[ortcObjectName];
	    if (obj && obj.prototype && obj.prototype.getStats) {
	      var nativeGetstats = obj.prototype.getStats;
	      obj.prototype.getStats = function() {
	        return nativeGetstats.apply(this)
	        .then(function(nativeStats) {
	          var mapStats = new Map();
	          Object.keys(nativeStats).forEach(function(id) {
	            nativeStats[id].type = fixStatsType(nativeStats[id]);
	            mapStats.set(id, nativeStats[id]);
	          });
	          return mapStats;
	        });
	      };
	    }
	  });

	  // legacy callback shims. Should be moved to adapter.js some days.
	  var methods = ['createOffer', 'createAnswer'];
	  methods.forEach(function(method) {
	    var nativeMethod = RTCPeerConnection.prototype[method];
	    RTCPeerConnection.prototype[method] = function() {
	      var args = arguments;
	      if (typeof args[0] === 'function' ||
	          typeof args[1] === 'function') { // legacy
	        return nativeMethod.apply(this, [arguments[2]])
	        .then(function(description) {
	          if (typeof args[0] === 'function') {
	            args[0].apply(null, [description]);
	          }
	        }, function(error) {
	          if (typeof args[1] === 'function') {
	            args[1].apply(null, [error]);
	          }
	        });
	      }
	      return nativeMethod.apply(this, arguments);
	    };
	  });

	  methods = ['setLocalDescription', 'setRemoteDescription', 'addIceCandidate'];
	  methods.forEach(function(method) {
	    var nativeMethod = RTCPeerConnection.prototype[method];
	    RTCPeerConnection.prototype[method] = function() {
	      var args = arguments;
	      if (typeof args[1] === 'function' ||
	          typeof args[2] === 'function') { // legacy
	        return nativeMethod.apply(this, arguments)
	        .then(function() {
	          if (typeof args[1] === 'function') {
	            args[1].apply(null);
	          }
	        }, function(error) {
	          if (typeof args[2] === 'function') {
	            args[2].apply(null, [error]);
	          }
	        });
	      }
	      return nativeMethod.apply(this, arguments);
	    };
	  });

	  // getStats is special. It doesn't have a spec legacy method yet we support
	  // getStats(something, cb) without error callbacks.
	  ['getStats'].forEach(function(method) {
	    var nativeMethod = RTCPeerConnection.prototype[method];
	    RTCPeerConnection.prototype[method] = function() {
	      var args = arguments;
	      if (typeof args[1] === 'function') {
	        return nativeMethod.apply(this, arguments)
	        .then(function() {
	          if (typeof args[1] === 'function') {
	            args[1].apply(null);
	          }
	        });
	      }
	      return nativeMethod.apply(this, arguments);
	    };
	  });

	  return RTCPeerConnection;
	};

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimGetUserMedia$2(window) {
	  const navigator = window && window.navigator;

	  const shimError_ = function(e) {
	    return {
	      name: {PermissionDeniedError: 'NotAllowedError'}[e.name] || e.name,
	      message: e.message,
	      constraint: e.constraint,
	      toString() {
	        return this.name;
	      }
	    };
	  };

	  // getUserMedia error shim.
	  const origGetUserMedia = navigator.mediaDevices.getUserMedia.
	      bind(navigator.mediaDevices);
	  navigator.mediaDevices.getUserMedia = function(c) {
	    return origGetUserMedia(c).catch(e => Promise.reject(shimError_(e)));
	  };
	}

	/*
	 *  Copyright (c) 2018 The adapter.js project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimGetDisplayMedia$1(window) {
	  if (!('getDisplayMedia' in window.navigator)) {
	    return;
	  }
	  if (!(window.navigator.mediaDevices)) {
	    return;
	  }
	  if (window.navigator.mediaDevices &&
	    'getDisplayMedia' in window.navigator.mediaDevices) {
	    return;
	  }
	  window.navigator.mediaDevices.getDisplayMedia =
	    window.navigator.getDisplayMedia.bind(window.navigator);
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimPeerConnection$1(window, browserDetails) {
	  if (window.RTCIceGatherer) {
	    if (!window.RTCIceCandidate) {
	      window.RTCIceCandidate = function RTCIceCandidate(args) {
	        return args;
	      };
	    }
	    if (!window.RTCSessionDescription) {
	      window.RTCSessionDescription = function RTCSessionDescription(args) {
	        return args;
	      };
	    }
	    // this adds an additional event listener to MediaStrackTrack that signals
	    // when a tracks enabled property was changed. Workaround for a bug in
	    // addStream, see below. No longer required in 15025+
	    if (browserDetails.version < 15025) {
	      const origMSTEnabled = Object.getOwnPropertyDescriptor(
	          window.MediaStreamTrack.prototype, 'enabled');
	      Object.defineProperty(window.MediaStreamTrack.prototype, 'enabled', {
	        set(value) {
	          origMSTEnabled.set.call(this, value);
	          const ev = new Event('enabled');
	          ev.enabled = value;
	          this.dispatchEvent(ev);
	        }
	      });
	    }
	  }

	  // ORTC defines the DTMF sender a bit different.
	  // https://github.com/w3c/ortc/issues/714
	  if (window.RTCRtpSender && !('dtmf' in window.RTCRtpSender.prototype)) {
	    Object.defineProperty(window.RTCRtpSender.prototype, 'dtmf', {
	      get() {
	        if (this._dtmf === undefined) {
	          if (this.track.kind === 'audio') {
	            this._dtmf = new window.RTCDtmfSender(this);
	          } else if (this.track.kind === 'video') {
	            this._dtmf = null;
	          }
	        }
	        return this._dtmf;
	      }
	    });
	  }
	  // Edge currently only implements the RTCDtmfSender, not the
	  // RTCDTMFSender alias. See http://draft.ortc.org/#rtcdtmfsender2*
	  if (window.RTCDtmfSender && !window.RTCDTMFSender) {
	    window.RTCDTMFSender = window.RTCDtmfSender;
	  }

	  const RTCPeerConnectionShim = rtcpeerconnection(window,
	      browserDetails.version);
	  window.RTCPeerConnection = function RTCPeerConnection(config) {
	    if (config && config.iceServers) {
	      config.iceServers = filterIceServers$1(config.iceServers,
	        browserDetails.version);
	      log$1('ICE servers after filtering:', config.iceServers);
	    }
	    return new RTCPeerConnectionShim(config);
	  };
	  window.RTCPeerConnection.prototype = RTCPeerConnectionShim.prototype;
	}

	function shimReplaceTrack(window) {
	  // ORTC has replaceTrack -- https://github.com/w3c/ortc/issues/614
	  if (window.RTCRtpSender &&
	      !('replaceTrack' in window.RTCRtpSender.prototype)) {
	    window.RTCRtpSender.prototype.replaceTrack =
	        window.RTCRtpSender.prototype.setTrack;
	  }
	}

	var edgeShim = /*#__PURE__*/Object.freeze({
		__proto__: null,
		shimPeerConnection: shimPeerConnection$1,
		shimReplaceTrack: shimReplaceTrack,
		shimGetUserMedia: shimGetUserMedia$2,
		shimGetDisplayMedia: shimGetDisplayMedia$1
	});

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimGetUserMedia$1(window, browserDetails) {
	  const navigator = window && window.navigator;
	  const MediaStreamTrack = window && window.MediaStreamTrack;

	  navigator.getUserMedia = function(constraints, onSuccess, onError) {
	    // Replace Firefox 44+'s deprecation warning with unprefixed version.
	    deprecated('navigator.getUserMedia',
	        'navigator.mediaDevices.getUserMedia');
	    navigator.mediaDevices.getUserMedia(constraints).then(onSuccess, onError);
	  };

	  if (!(browserDetails.version > 55 &&
	      'autoGainControl' in navigator.mediaDevices.getSupportedConstraints())) {
	    const remap = function(obj, a, b) {
	      if (a in obj && !(b in obj)) {
	        obj[b] = obj[a];
	        delete obj[a];
	      }
	    };

	    const nativeGetUserMedia = navigator.mediaDevices.getUserMedia.
	        bind(navigator.mediaDevices);
	    navigator.mediaDevices.getUserMedia = function(c) {
	      if (typeof c === 'object' && typeof c.audio === 'object') {
	        c = JSON.parse(JSON.stringify(c));
	        remap(c.audio, 'autoGainControl', 'mozAutoGainControl');
	        remap(c.audio, 'noiseSuppression', 'mozNoiseSuppression');
	      }
	      return nativeGetUserMedia(c);
	    };

	    if (MediaStreamTrack && MediaStreamTrack.prototype.getSettings) {
	      const nativeGetSettings = MediaStreamTrack.prototype.getSettings;
	      MediaStreamTrack.prototype.getSettings = function() {
	        const obj = nativeGetSettings.apply(this, arguments);
	        remap(obj, 'mozAutoGainControl', 'autoGainControl');
	        remap(obj, 'mozNoiseSuppression', 'noiseSuppression');
	        return obj;
	      };
	    }

	    if (MediaStreamTrack && MediaStreamTrack.prototype.applyConstraints) {
	      const nativeApplyConstraints =
	        MediaStreamTrack.prototype.applyConstraints;
	      MediaStreamTrack.prototype.applyConstraints = function(c) {
	        if (this.kind === 'audio' && typeof c === 'object') {
	          c = JSON.parse(JSON.stringify(c));
	          remap(c, 'autoGainControl', 'mozAutoGainControl');
	          remap(c, 'noiseSuppression', 'mozNoiseSuppression');
	        }
	        return nativeApplyConstraints.apply(this, [c]);
	      };
	    }
	  }
	}

	/*
	 *  Copyright (c) 2018 The adapter.js project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimGetDisplayMedia(window, preferredMediaSource) {
	  if (window.navigator.mediaDevices &&
	    'getDisplayMedia' in window.navigator.mediaDevices) {
	    return;
	  }
	  if (!(window.navigator.mediaDevices)) {
	    return;
	  }
	  window.navigator.mediaDevices.getDisplayMedia =
	    function getDisplayMedia(constraints) {
	      if (!(constraints && constraints.video)) {
	        const err = new DOMException('getDisplayMedia without video ' +
	            'constraints is undefined');
	        err.name = 'NotFoundError';
	        // from https://heycam.github.io/webidl/#idl-DOMException-error-names
	        err.code = 8;
	        return Promise.reject(err);
	      }
	      if (constraints.video === true) {
	        constraints.video = {mediaSource: preferredMediaSource};
	      } else {
	        constraints.video.mediaSource = preferredMediaSource;
	      }
	      return window.navigator.mediaDevices.getUserMedia(constraints);
	    };
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimOnTrack(window) {
	  if (typeof window === 'object' && window.RTCTrackEvent &&
	      ('receiver' in window.RTCTrackEvent.prototype) &&
	      !('transceiver' in window.RTCTrackEvent.prototype)) {
	    Object.defineProperty(window.RTCTrackEvent.prototype, 'transceiver', {
	      get() {
	        return {receiver: this.receiver};
	      }
	    });
	  }
	}

	function shimPeerConnection(window, browserDetails) {
	  if (typeof window !== 'object' ||
	      !(window.RTCPeerConnection || window.mozRTCPeerConnection)) {
	    return; // probably media.peerconnection.enabled=false in about:config
	  }
	  if (!window.RTCPeerConnection && window.mozRTCPeerConnection) {
	    // very basic support for old versions.
	    window.RTCPeerConnection = window.mozRTCPeerConnection;
	  }

	  if (browserDetails.version < 53) {
	    // shim away need for obsolete RTCIceCandidate/RTCSessionDescription.
	    ['setLocalDescription', 'setRemoteDescription', 'addIceCandidate']
	        .forEach(function(method) {
	          const nativeMethod = window.RTCPeerConnection.prototype[method];
	          const methodObj = {[method]() {
	            arguments[0] = new ((method === 'addIceCandidate') ?
	                window.RTCIceCandidate :
	                window.RTCSessionDescription)(arguments[0]);
	            return nativeMethod.apply(this, arguments);
	          }};
	          window.RTCPeerConnection.prototype[method] = methodObj[method];
	        });
	  }

	  const modernStatsTypes = {
	    inboundrtp: 'inbound-rtp',
	    outboundrtp: 'outbound-rtp',
	    candidatepair: 'candidate-pair',
	    localcandidate: 'local-candidate',
	    remotecandidate: 'remote-candidate'
	  };

	  const nativeGetStats = window.RTCPeerConnection.prototype.getStats;
	  window.RTCPeerConnection.prototype.getStats = function getStats() {
	    const [selector, onSucc, onErr] = arguments;
	    return nativeGetStats.apply(this, [selector || null])
	      .then(stats => {
	        if (browserDetails.version < 53 && !onSucc) {
	          // Shim only promise getStats with spec-hyphens in type names
	          // Leave callback version alone; misc old uses of forEach before Map
	          try {
	            stats.forEach(stat => {
	              stat.type = modernStatsTypes[stat.type] || stat.type;
	            });
	          } catch (e) {
	            if (e.name !== 'TypeError') {
	              throw e;
	            }
	            // Avoid TypeError: "type" is read-only, in old versions. 34-43ish
	            stats.forEach((stat, i) => {
	              stats.set(i, Object.assign({}, stat, {
	                type: modernStatsTypes[stat.type] || stat.type
	              }));
	            });
	          }
	        }
	        return stats;
	      })
	      .then(onSucc, onErr);
	  };
	}

	function shimSenderGetStats(window) {
	  if (!(typeof window === 'object' && window.RTCPeerConnection &&
	      window.RTCRtpSender)) {
	    return;
	  }
	  if (window.RTCRtpSender && 'getStats' in window.RTCRtpSender.prototype) {
	    return;
	  }
	  const origGetSenders = window.RTCPeerConnection.prototype.getSenders;
	  if (origGetSenders) {
	    window.RTCPeerConnection.prototype.getSenders = function getSenders() {
	      const senders = origGetSenders.apply(this, []);
	      senders.forEach(sender => sender._pc = this);
	      return senders;
	    };
	  }

	  const origAddTrack = window.RTCPeerConnection.prototype.addTrack;
	  if (origAddTrack) {
	    window.RTCPeerConnection.prototype.addTrack = function addTrack() {
	      const sender = origAddTrack.apply(this, arguments);
	      sender._pc = this;
	      return sender;
	    };
	  }
	  window.RTCRtpSender.prototype.getStats = function getStats() {
	    return this.track ? this._pc.getStats(this.track) :
	        Promise.resolve(new Map());
	  };
	}

	function shimReceiverGetStats(window) {
	  if (!(typeof window === 'object' && window.RTCPeerConnection &&
	      window.RTCRtpSender)) {
	    return;
	  }
	  if (window.RTCRtpSender && 'getStats' in window.RTCRtpReceiver.prototype) {
	    return;
	  }
	  const origGetReceivers = window.RTCPeerConnection.prototype.getReceivers;
	  if (origGetReceivers) {
	    window.RTCPeerConnection.prototype.getReceivers = function getReceivers() {
	      const receivers = origGetReceivers.apply(this, []);
	      receivers.forEach(receiver => receiver._pc = this);
	      return receivers;
	    };
	  }
	  wrapPeerConnectionEvent(window, 'track', e => {
	    e.receiver._pc = e.srcElement;
	    return e;
	  });
	  window.RTCRtpReceiver.prototype.getStats = function getStats() {
	    return this._pc.getStats(this.track);
	  };
	}

	function shimRemoveStream(window) {
	  if (!window.RTCPeerConnection ||
	      'removeStream' in window.RTCPeerConnection.prototype) {
	    return;
	  }
	  window.RTCPeerConnection.prototype.removeStream =
	    function removeStream(stream) {
	      deprecated('removeStream', 'removeTrack');
	      this.getSenders().forEach(sender => {
	        if (sender.track && stream.getTracks().includes(sender.track)) {
	          this.removeTrack(sender);
	        }
	      });
	    };
	}

	function shimRTCDataChannel(window) {
	  // rename DataChannel to RTCDataChannel (native fix in FF60):
	  // https://bugzilla.mozilla.org/show_bug.cgi?id=1173851
	  if (window.DataChannel && !window.RTCDataChannel) {
	    window.RTCDataChannel = window.DataChannel;
	  }
	}

	function shimAddTransceiver(window) {
	  // https://github.com/webrtcHacks/adapter/issues/998#issuecomment-516921647
	  // Firefox ignores the init sendEncodings options passed to addTransceiver
	  // https://bugzilla.mozilla.org/show_bug.cgi?id=1396918
	  if (!(typeof window === 'object' && window.RTCPeerConnection)) {
	    return;
	  }
	  const origAddTransceiver = window.RTCPeerConnection.prototype.addTransceiver;
	  if (origAddTransceiver) {
	    window.RTCPeerConnection.prototype.addTransceiver =
	      function addTransceiver() {
	        this.setParametersPromises = [];
	        const initParameters = arguments[1];
	        const shouldPerformCheck = initParameters &&
	                                  'sendEncodings' in initParameters;
	        if (shouldPerformCheck) {
	          // If sendEncodings params are provided, validate grammar
	          initParameters.sendEncodings.forEach((encodingParam) => {
	            if ('rid' in encodingParam) {
	              const ridRegex = /^[a-z0-9]{0,16}$/i;
	              if (!ridRegex.test(encodingParam.rid)) {
	                throw new TypeError('Invalid RID value provided.');
	              }
	            }
	            if ('scaleResolutionDownBy' in encodingParam) {
	              if (!(parseFloat(encodingParam.scaleResolutionDownBy) >= 1.0)) {
	                throw new RangeError('scale_resolution_down_by must be >= 1.0');
	              }
	            }
	            if ('maxFramerate' in encodingParam) {
	              if (!(parseFloat(encodingParam.maxFramerate) >= 0)) {
	                throw new RangeError('max_framerate must be >= 0.0');
	              }
	            }
	          });
	        }
	        const transceiver = origAddTransceiver.apply(this, arguments);
	        if (shouldPerformCheck) {
	          // Check if the init options were applied. If not we do this in an
	          // asynchronous way and save the promise reference in a global object.
	          // This is an ugly hack, but at the same time is way more robust than
	          // checking the sender parameters before and after the createOffer
	          // Also note that after the createoffer we are not 100% sure that
	          // the params were asynchronously applied so we might miss the
	          // opportunity to recreate offer.
	          const {sender} = transceiver;
	          const params = sender.getParameters();
	          if (!('encodings' in params) ||
	              // Avoid being fooled by patched getParameters() below.
	              (params.encodings.length === 1 &&
	               Object.keys(params.encodings[0]).length === 0)) {
	            params.encodings = initParameters.sendEncodings;
	            sender.sendEncodings = initParameters.sendEncodings;
	            this.setParametersPromises.push(sender.setParameters(params)
	              .then(() => {
	                delete sender.sendEncodings;
	              }).catch(() => {
	                delete sender.sendEncodings;
	              })
	            );
	          }
	        }
	        return transceiver;
	      };
	  }
	}

	function shimGetParameters(window) {
	  if (!(typeof window === 'object' && window.RTCRtpSender)) {
	    return;
	  }
	  const origGetParameters = window.RTCRtpSender.prototype.getParameters;
	  if (origGetParameters) {
	    window.RTCRtpSender.prototype.getParameters =
	      function getParameters() {
	        const params = origGetParameters.apply(this, arguments);
	        if (!('encodings' in params)) {
	          params.encodings = [].concat(this.sendEncodings || [{}]);
	        }
	        return params;
	      };
	  }
	}

	function shimCreateOffer(window) {
	  // https://github.com/webrtcHacks/adapter/issues/998#issuecomment-516921647
	  // Firefox ignores the init sendEncodings options passed to addTransceiver
	  // https://bugzilla.mozilla.org/show_bug.cgi?id=1396918
	  if (!(typeof window === 'object' && window.RTCPeerConnection)) {
	    return;
	  }
	  const origCreateOffer = window.RTCPeerConnection.prototype.createOffer;
	  window.RTCPeerConnection.prototype.createOffer = function createOffer() {
	    if (this.setParametersPromises && this.setParametersPromises.length) {
	      return Promise.all(this.setParametersPromises)
	      .then(() => {
	        return origCreateOffer.apply(this, arguments);
	      })
	      .finally(() => {
	        this.setParametersPromises = [];
	      });
	    }
	    return origCreateOffer.apply(this, arguments);
	  };
	}

	function shimCreateAnswer(window) {
	  // https://github.com/webrtcHacks/adapter/issues/998#issuecomment-516921647
	  // Firefox ignores the init sendEncodings options passed to addTransceiver
	  // https://bugzilla.mozilla.org/show_bug.cgi?id=1396918
	  if (!(typeof window === 'object' && window.RTCPeerConnection)) {
	    return;
	  }
	  const origCreateAnswer = window.RTCPeerConnection.prototype.createAnswer;
	  window.RTCPeerConnection.prototype.createAnswer = function createAnswer() {
	    if (this.setParametersPromises && this.setParametersPromises.length) {
	      return Promise.all(this.setParametersPromises)
	      .then(() => {
	        return origCreateAnswer.apply(this, arguments);
	      })
	      .finally(() => {
	        this.setParametersPromises = [];
	      });
	    }
	    return origCreateAnswer.apply(this, arguments);
	  };
	}

	var firefoxShim = /*#__PURE__*/Object.freeze({
		__proto__: null,
		shimOnTrack: shimOnTrack,
		shimPeerConnection: shimPeerConnection,
		shimSenderGetStats: shimSenderGetStats,
		shimReceiverGetStats: shimReceiverGetStats,
		shimRemoveStream: shimRemoveStream,
		shimRTCDataChannel: shimRTCDataChannel,
		shimAddTransceiver: shimAddTransceiver,
		shimGetParameters: shimGetParameters,
		shimCreateOffer: shimCreateOffer,
		shimCreateAnswer: shimCreateAnswer,
		shimGetUserMedia: shimGetUserMedia$1,
		shimGetDisplayMedia: shimGetDisplayMedia
	});

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimLocalStreamsAPI(window) {
	  if (typeof window !== 'object' || !window.RTCPeerConnection) {
	    return;
	  }
	  if (!('getLocalStreams' in window.RTCPeerConnection.prototype)) {
	    window.RTCPeerConnection.prototype.getLocalStreams =
	      function getLocalStreams() {
	        if (!this._localStreams) {
	          this._localStreams = [];
	        }
	        return this._localStreams;
	      };
	  }
	  if (!('addStream' in window.RTCPeerConnection.prototype)) {
	    const _addTrack = window.RTCPeerConnection.prototype.addTrack;
	    window.RTCPeerConnection.prototype.addStream = function addStream(stream) {
	      if (!this._localStreams) {
	        this._localStreams = [];
	      }
	      if (!this._localStreams.includes(stream)) {
	        this._localStreams.push(stream);
	      }
	      // Try to emulate Chrome's behaviour of adding in audio-video order.
	      // Safari orders by track id.
	      stream.getAudioTracks().forEach(track => _addTrack.call(this, track,
	        stream));
	      stream.getVideoTracks().forEach(track => _addTrack.call(this, track,
	        stream));
	    };

	    window.RTCPeerConnection.prototype.addTrack =
	      function addTrack(track, ...streams) {
	        if (streams) {
	          streams.forEach((stream) => {
	            if (!this._localStreams) {
	              this._localStreams = [stream];
	            } else if (!this._localStreams.includes(stream)) {
	              this._localStreams.push(stream);
	            }
	          });
	        }
	        return _addTrack.apply(this, arguments);
	      };
	  }
	  if (!('removeStream' in window.RTCPeerConnection.prototype)) {
	    window.RTCPeerConnection.prototype.removeStream =
	      function removeStream(stream) {
	        if (!this._localStreams) {
	          this._localStreams = [];
	        }
	        const index = this._localStreams.indexOf(stream);
	        if (index === -1) {
	          return;
	        }
	        this._localStreams.splice(index, 1);
	        const tracks = stream.getTracks();
	        this.getSenders().forEach(sender => {
	          if (tracks.includes(sender.track)) {
	            this.removeTrack(sender);
	          }
	        });
	      };
	  }
	}

	function shimRemoteStreamsAPI(window) {
	  if (typeof window !== 'object' || !window.RTCPeerConnection) {
	    return;
	  }
	  if (!('getRemoteStreams' in window.RTCPeerConnection.prototype)) {
	    window.RTCPeerConnection.prototype.getRemoteStreams =
	      function getRemoteStreams() {
	        return this._remoteStreams ? this._remoteStreams : [];
	      };
	  }
	  if (!('onaddstream' in window.RTCPeerConnection.prototype)) {
	    Object.defineProperty(window.RTCPeerConnection.prototype, 'onaddstream', {
	      get() {
	        return this._onaddstream;
	      },
	      set(f) {
	        if (this._onaddstream) {
	          this.removeEventListener('addstream', this._onaddstream);
	          this.removeEventListener('track', this._onaddstreampoly);
	        }
	        this.addEventListener('addstream', this._onaddstream = f);
	        this.addEventListener('track', this._onaddstreampoly = (e) => {
	          e.streams.forEach(stream => {
	            if (!this._remoteStreams) {
	              this._remoteStreams = [];
	            }
	            if (this._remoteStreams.includes(stream)) {
	              return;
	            }
	            this._remoteStreams.push(stream);
	            const event = new Event('addstream');
	            event.stream = stream;
	            this.dispatchEvent(event);
	          });
	        });
	      }
	    });
	    const origSetRemoteDescription =
	      window.RTCPeerConnection.prototype.setRemoteDescription;
	    window.RTCPeerConnection.prototype.setRemoteDescription =
	      function setRemoteDescription() {
	        const pc = this;
	        if (!this._onaddstreampoly) {
	          this.addEventListener('track', this._onaddstreampoly = function(e) {
	            e.streams.forEach(stream => {
	              if (!pc._remoteStreams) {
	                pc._remoteStreams = [];
	              }
	              if (pc._remoteStreams.indexOf(stream) >= 0) {
	                return;
	              }
	              pc._remoteStreams.push(stream);
	              const event = new Event('addstream');
	              event.stream = stream;
	              pc.dispatchEvent(event);
	            });
	          });
	        }
	        return origSetRemoteDescription.apply(pc, arguments);
	      };
	  }
	}

	function shimCallbacksAPI(window) {
	  if (typeof window !== 'object' || !window.RTCPeerConnection) {
	    return;
	  }
	  const prototype = window.RTCPeerConnection.prototype;
	  const origCreateOffer = prototype.createOffer;
	  const origCreateAnswer = prototype.createAnswer;
	  const setLocalDescription = prototype.setLocalDescription;
	  const setRemoteDescription = prototype.setRemoteDescription;
	  const addIceCandidate = prototype.addIceCandidate;

	  prototype.createOffer =
	    function createOffer(successCallback, failureCallback) {
	      const options = (arguments.length >= 2) ? arguments[2] : arguments[0];
	      const promise = origCreateOffer.apply(this, [options]);
	      if (!failureCallback) {
	        return promise;
	      }
	      promise.then(successCallback, failureCallback);
	      return Promise.resolve();
	    };

	  prototype.createAnswer =
	    function createAnswer(successCallback, failureCallback) {
	      const options = (arguments.length >= 2) ? arguments[2] : arguments[0];
	      const promise = origCreateAnswer.apply(this, [options]);
	      if (!failureCallback) {
	        return promise;
	      }
	      promise.then(successCallback, failureCallback);
	      return Promise.resolve();
	    };

	  let withCallback = function(description, successCallback, failureCallback) {
	    const promise = setLocalDescription.apply(this, [description]);
	    if (!failureCallback) {
	      return promise;
	    }
	    promise.then(successCallback, failureCallback);
	    return Promise.resolve();
	  };
	  prototype.setLocalDescription = withCallback;

	  withCallback = function(description, successCallback, failureCallback) {
	    const promise = setRemoteDescription.apply(this, [description]);
	    if (!failureCallback) {
	      return promise;
	    }
	    promise.then(successCallback, failureCallback);
	    return Promise.resolve();
	  };
	  prototype.setRemoteDescription = withCallback;

	  withCallback = function(candidate, successCallback, failureCallback) {
	    const promise = addIceCandidate.apply(this, [candidate]);
	    if (!failureCallback) {
	      return promise;
	    }
	    promise.then(successCallback, failureCallback);
	    return Promise.resolve();
	  };
	  prototype.addIceCandidate = withCallback;
	}

	function shimGetUserMedia(window) {
	  const navigator = window && window.navigator;

	  if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
	    // shim not needed in Safari 12.1
	    const mediaDevices = navigator.mediaDevices;
	    const _getUserMedia = mediaDevices.getUserMedia.bind(mediaDevices);
	    navigator.mediaDevices.getUserMedia = (constraints) => {
	      return _getUserMedia(shimConstraints(constraints));
	    };
	  }

	  if (!navigator.getUserMedia && navigator.mediaDevices &&
	    navigator.mediaDevices.getUserMedia) {
	    navigator.getUserMedia = function getUserMedia(constraints, cb, errcb) {
	      navigator.mediaDevices.getUserMedia(constraints)
	      .then(cb, errcb);
	    }.bind(navigator);
	  }
	}

	function shimConstraints(constraints) {
	  if (constraints && constraints.video !== undefined) {
	    return Object.assign({},
	      constraints,
	      {video: compactObject(constraints.video)}
	    );
	  }

	  return constraints;
	}

	function shimRTCIceServerUrls(window) {
	  if (!window.RTCPeerConnection) {
	    return;
	  }
	  // migrate from non-spec RTCIceServer.url to RTCIceServer.urls
	  const OrigPeerConnection = window.RTCPeerConnection;
	  window.RTCPeerConnection =
	    function RTCPeerConnection(pcConfig, pcConstraints) {
	      if (pcConfig && pcConfig.iceServers) {
	        const newIceServers = [];
	        for (let i = 0; i < pcConfig.iceServers.length; i++) {
	          let server = pcConfig.iceServers[i];
	          if (!server.hasOwnProperty('urls') &&
	              server.hasOwnProperty('url')) {
	            deprecated('RTCIceServer.url', 'RTCIceServer.urls');
	            server = JSON.parse(JSON.stringify(server));
	            server.urls = server.url;
	            delete server.url;
	            newIceServers.push(server);
	          } else {
	            newIceServers.push(pcConfig.iceServers[i]);
	          }
	        }
	        pcConfig.iceServers = newIceServers;
	      }
	      return new OrigPeerConnection(pcConfig, pcConstraints);
	    };
	  window.RTCPeerConnection.prototype = OrigPeerConnection.prototype;
	  // wrap static methods. Currently just generateCertificate.
	  if ('generateCertificate' in OrigPeerConnection) {
	    Object.defineProperty(window.RTCPeerConnection, 'generateCertificate', {
	      get() {
	        return OrigPeerConnection.generateCertificate;
	      }
	    });
	  }
	}

	function shimTrackEventTransceiver(window) {
	  // Add event.transceiver member over deprecated event.receiver
	  if (typeof window === 'object' && window.RTCTrackEvent &&
	      'receiver' in window.RTCTrackEvent.prototype &&
	      !('transceiver' in window.RTCTrackEvent.prototype)) {
	    Object.defineProperty(window.RTCTrackEvent.prototype, 'transceiver', {
	      get() {
	        return {receiver: this.receiver};
	      }
	    });
	  }
	}

	function shimCreateOfferLegacy(window) {
	  const origCreateOffer = window.RTCPeerConnection.prototype.createOffer;
	  window.RTCPeerConnection.prototype.createOffer =
	    function createOffer(offerOptions) {
	      if (offerOptions) {
	        if (typeof offerOptions.offerToReceiveAudio !== 'undefined') {
	          // support bit values
	          offerOptions.offerToReceiveAudio =
	            !!offerOptions.offerToReceiveAudio;
	        }
	        const audioTransceiver = this.getTransceivers().find(transceiver =>
	          transceiver.receiver.track.kind === 'audio');
	        if (offerOptions.offerToReceiveAudio === false && audioTransceiver) {
	          if (audioTransceiver.direction === 'sendrecv') {
	            if (audioTransceiver.setDirection) {
	              audioTransceiver.setDirection('sendonly');
	            } else {
	              audioTransceiver.direction = 'sendonly';
	            }
	          } else if (audioTransceiver.direction === 'recvonly') {
	            if (audioTransceiver.setDirection) {
	              audioTransceiver.setDirection('inactive');
	            } else {
	              audioTransceiver.direction = 'inactive';
	            }
	          }
	        } else if (offerOptions.offerToReceiveAudio === true &&
	            !audioTransceiver) {
	          this.addTransceiver('audio');
	        }

	        if (typeof offerOptions.offerToReceiveVideo !== 'undefined') {
	          // support bit values
	          offerOptions.offerToReceiveVideo =
	            !!offerOptions.offerToReceiveVideo;
	        }
	        const videoTransceiver = this.getTransceivers().find(transceiver =>
	          transceiver.receiver.track.kind === 'video');
	        if (offerOptions.offerToReceiveVideo === false && videoTransceiver) {
	          if (videoTransceiver.direction === 'sendrecv') {
	            if (videoTransceiver.setDirection) {
	              videoTransceiver.setDirection('sendonly');
	            } else {
	              videoTransceiver.direction = 'sendonly';
	            }
	          } else if (videoTransceiver.direction === 'recvonly') {
	            if (videoTransceiver.setDirection) {
	              videoTransceiver.setDirection('inactive');
	            } else {
	              videoTransceiver.direction = 'inactive';
	            }
	          }
	        } else if (offerOptions.offerToReceiveVideo === true &&
	            !videoTransceiver) {
	          this.addTransceiver('video');
	        }
	      }
	      return origCreateOffer.apply(this, arguments);
	    };
	}

	function shimAudioContext(window) {
	  if (typeof window !== 'object' || window.AudioContext) {
	    return;
	  }
	  window.AudioContext = window.webkitAudioContext;
	}

	var safariShim = /*#__PURE__*/Object.freeze({
		__proto__: null,
		shimLocalStreamsAPI: shimLocalStreamsAPI,
		shimRemoteStreamsAPI: shimRemoteStreamsAPI,
		shimCallbacksAPI: shimCallbacksAPI,
		shimGetUserMedia: shimGetUserMedia,
		shimConstraints: shimConstraints,
		shimRTCIceServerUrls: shimRTCIceServerUrls,
		shimTrackEventTransceiver: shimTrackEventTransceiver,
		shimCreateOfferLegacy: shimCreateOfferLegacy,
		shimAudioContext: shimAudioContext
	});

	/*
	 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	function shimRTCIceCandidate(window) {
	  // foundation is arbitrarily chosen as an indicator for full support for
	  // https://w3c.github.io/webrtc-pc/#rtcicecandidate-interface
	  if (!window.RTCIceCandidate || (window.RTCIceCandidate && 'foundation' in
	      window.RTCIceCandidate.prototype)) {
	    return;
	  }

	  const NativeRTCIceCandidate = window.RTCIceCandidate;
	  window.RTCIceCandidate = function RTCIceCandidate(args) {
	    // Remove the a= which shouldn't be part of the candidate string.
	    if (typeof args === 'object' && args.candidate &&
	        args.candidate.indexOf('a=') === 0) {
	      args = JSON.parse(JSON.stringify(args));
	      args.candidate = args.candidate.substr(2);
	    }

	    if (args.candidate && args.candidate.length) {
	      // Augment the native candidate with the parsed fields.
	      const nativeCandidate = new NativeRTCIceCandidate(args);
	      const parsedCandidate = sdp.parseCandidate(args.candidate);
	      const augmentedCandidate = Object.assign(nativeCandidate,
	          parsedCandidate);

	      // Add a serializer that does not serialize the extra attributes.
	      augmentedCandidate.toJSON = function toJSON() {
	        return {
	          candidate: augmentedCandidate.candidate,
	          sdpMid: augmentedCandidate.sdpMid,
	          sdpMLineIndex: augmentedCandidate.sdpMLineIndex,
	          usernameFragment: augmentedCandidate.usernameFragment,
	        };
	      };
	      return augmentedCandidate;
	    }
	    return new NativeRTCIceCandidate(args);
	  };
	  window.RTCIceCandidate.prototype = NativeRTCIceCandidate.prototype;

	  // Hook up the augmented candidate in onicecandidate and
	  // addEventListener('icecandidate', ...)
	  wrapPeerConnectionEvent(window, 'icecandidate', e => {
	    if (e.candidate) {
	      Object.defineProperty(e, 'candidate', {
	        value: new window.RTCIceCandidate(e.candidate),
	        writable: 'false'
	      });
	    }
	    return e;
	  });
	}

	function shimMaxMessageSize(window, browserDetails) {
	  if (!window.RTCPeerConnection) {
	    return;
	  }

	  if (!('sctp' in window.RTCPeerConnection.prototype)) {
	    Object.defineProperty(window.RTCPeerConnection.prototype, 'sctp', {
	      get() {
	        return typeof this._sctp === 'undefined' ? null : this._sctp;
	      }
	    });
	  }

	  const sctpInDescription = function(description) {
	    if (!description || !description.sdp) {
	      return false;
	    }
	    const sections = sdp.splitSections(description.sdp);
	    sections.shift();
	    return sections.some(mediaSection => {
	      const mLine = sdp.parseMLine(mediaSection);
	      return mLine && mLine.kind === 'application'
	          && mLine.protocol.indexOf('SCTP') !== -1;
	    });
	  };

	  const getRemoteFirefoxVersion = function(description) {
	    // TODO: Is there a better solution for detecting Firefox?
	    const match = description.sdp.match(/mozilla...THIS_IS_SDPARTA-(\d+)/);
	    if (match === null || match.length < 2) {
	      return -1;
	    }
	    const version = parseInt(match[1], 10);
	    // Test for NaN (yes, this is ugly)
	    return version !== version ? -1 : version;
	  };

	  const getCanSendMaxMessageSize = function(remoteIsFirefox) {
	    // Every implementation we know can send at least 64 KiB.
	    // Note: Although Chrome is technically able to send up to 256 KiB, the
	    //       data does not reach the other peer reliably.
	    //       See: https://bugs.chromium.org/p/webrtc/issues/detail?id=8419
	    let canSendMaxMessageSize = 65536;
	    if (browserDetails.browser === 'firefox') {
	      if (browserDetails.version < 57) {
	        if (remoteIsFirefox === -1) {
	          // FF < 57 will send in 16 KiB chunks using the deprecated PPID
	          // fragmentation.
	          canSendMaxMessageSize = 16384;
	        } else {
	          // However, other FF (and RAWRTC) can reassemble PPID-fragmented
	          // messages. Thus, supporting ~2 GiB when sending.
	          canSendMaxMessageSize = 2147483637;
	        }
	      } else if (browserDetails.version < 60) {
	        // Currently, all FF >= 57 will reset the remote maximum message size
	        // to the default value when a data channel is created at a later
	        // stage. :(
	        // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1426831
	        canSendMaxMessageSize =
	          browserDetails.version === 57 ? 65535 : 65536;
	      } else {
	        // FF >= 60 supports sending ~2 GiB
	        canSendMaxMessageSize = 2147483637;
	      }
	    }
	    return canSendMaxMessageSize;
	  };

	  const getMaxMessageSize = function(description, remoteIsFirefox) {
	    // Note: 65536 bytes is the default value from the SDP spec. Also,
	    //       every implementation we know supports receiving 65536 bytes.
	    let maxMessageSize = 65536;

	    // FF 57 has a slightly incorrect default remote max message size, so
	    // we need to adjust it here to avoid a failure when sending.
	    // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1425697
	    if (browserDetails.browser === 'firefox'
	         && browserDetails.version === 57) {
	      maxMessageSize = 65535;
	    }

	    const match = sdp.matchPrefix(description.sdp,
	      'a=max-message-size:');
	    if (match.length > 0) {
	      maxMessageSize = parseInt(match[0].substr(19), 10);
	    } else if (browserDetails.browser === 'firefox' &&
	                remoteIsFirefox !== -1) {
	      // If the maximum message size is not present in the remote SDP and
	      // both local and remote are Firefox, the remote peer can receive
	      // ~2 GiB.
	      maxMessageSize = 2147483637;
	    }
	    return maxMessageSize;
	  };

	  const origSetRemoteDescription =
	      window.RTCPeerConnection.prototype.setRemoteDescription;
	  window.RTCPeerConnection.prototype.setRemoteDescription =
	    function setRemoteDescription() {
	      this._sctp = null;
	      // Chrome decided to not expose .sctp in plan-b mode.
	      // As usual, adapter.js has to do an 'ugly worakaround'
	      // to cover up the mess.
	      if (browserDetails.browser === 'chrome' && browserDetails.version >= 76) {
	        const {sdpSemantics} = this.getConfiguration();
	        if (sdpSemantics === 'plan-b') {
	          Object.defineProperty(this, 'sctp', {
	            get() {
	              return typeof this._sctp === 'undefined' ? null : this._sctp;
	            },
	            enumerable: true,
	            configurable: true,
	          });
	        }
	      }

	      if (sctpInDescription(arguments[0])) {
	        // Check if the remote is FF.
	        const isFirefox = getRemoteFirefoxVersion(arguments[0]);

	        // Get the maximum message size the local peer is capable of sending
	        const canSendMMS = getCanSendMaxMessageSize(isFirefox);

	        // Get the maximum message size of the remote peer.
	        const remoteMMS = getMaxMessageSize(arguments[0], isFirefox);

	        // Determine final maximum message size
	        let maxMessageSize;
	        if (canSendMMS === 0 && remoteMMS === 0) {
	          maxMessageSize = Number.POSITIVE_INFINITY;
	        } else if (canSendMMS === 0 || remoteMMS === 0) {
	          maxMessageSize = Math.max(canSendMMS, remoteMMS);
	        } else {
	          maxMessageSize = Math.min(canSendMMS, remoteMMS);
	        }

	        // Create a dummy RTCSctpTransport object and the 'maxMessageSize'
	        // attribute.
	        const sctp = {};
	        Object.defineProperty(sctp, 'maxMessageSize', {
	          get() {
	            return maxMessageSize;
	          }
	        });
	        this._sctp = sctp;
	      }

	      return origSetRemoteDescription.apply(this, arguments);
	    };
	}

	function shimSendThrowTypeError(window) {
	  if (!(window.RTCPeerConnection &&
	      'createDataChannel' in window.RTCPeerConnection.prototype)) {
	    return;
	  }

	  // Note: Although Firefox >= 57 has a native implementation, the maximum
	  //       message size can be reset for all data channels at a later stage.
	  //       See: https://bugzilla.mozilla.org/show_bug.cgi?id=1426831

	  function wrapDcSend(dc, pc) {
	    const origDataChannelSend = dc.send;
	    dc.send = function send() {
	      const data = arguments[0];
	      const length = data.length || data.size || data.byteLength;
	      if (dc.readyState === 'open' &&
	          pc.sctp && length > pc.sctp.maxMessageSize) {
	        throw new TypeError('Message too large (can send a maximum of ' +
	          pc.sctp.maxMessageSize + ' bytes)');
	      }
	      return origDataChannelSend.apply(dc, arguments);
	    };
	  }
	  const origCreateDataChannel =
	    window.RTCPeerConnection.prototype.createDataChannel;
	  window.RTCPeerConnection.prototype.createDataChannel =
	    function createDataChannel() {
	      const dataChannel = origCreateDataChannel.apply(this, arguments);
	      wrapDcSend(dataChannel, this);
	      return dataChannel;
	    };
	  wrapPeerConnectionEvent(window, 'datachannel', e => {
	    wrapDcSend(e.channel, e.target);
	    return e;
	  });
	}


	/* shims RTCConnectionState by pretending it is the same as iceConnectionState.
	 * See https://bugs.chromium.org/p/webrtc/issues/detail?id=6145#c12
	 * for why this is a valid hack in Chrome. In Firefox it is slightly incorrect
	 * since DTLS failures would be hidden. See
	 * https://bugzilla.mozilla.org/show_bug.cgi?id=1265827
	 * for the Firefox tracking bug.
	 */
	function shimConnectionState(window) {
	  if (!window.RTCPeerConnection ||
	      'connectionState' in window.RTCPeerConnection.prototype) {
	    return;
	  }
	  const proto = window.RTCPeerConnection.prototype;
	  Object.defineProperty(proto, 'connectionState', {
	    get() {
	      return {
	        completed: 'connected',
	        checking: 'connecting'
	      }[this.iceConnectionState] || this.iceConnectionState;
	    },
	    enumerable: true,
	    configurable: true
	  });
	  Object.defineProperty(proto, 'onconnectionstatechange', {
	    get() {
	      return this._onconnectionstatechange || null;
	    },
	    set(cb) {
	      if (this._onconnectionstatechange) {
	        this.removeEventListener('connectionstatechange',
	            this._onconnectionstatechange);
	        delete this._onconnectionstatechange;
	      }
	      if (cb) {
	        this.addEventListener('connectionstatechange',
	            this._onconnectionstatechange = cb);
	      }
	    },
	    enumerable: true,
	    configurable: true
	  });

	  ['setLocalDescription', 'setRemoteDescription'].forEach((method) => {
	    const origMethod = proto[method];
	    proto[method] = function() {
	      if (!this._connectionstatechangepoly) {
	        this._connectionstatechangepoly = e => {
	          const pc = e.target;
	          if (pc._lastConnectionState !== pc.connectionState) {
	            pc._lastConnectionState = pc.connectionState;
	            const newEvent = new Event('connectionstatechange', e);
	            pc.dispatchEvent(newEvent);
	          }
	          return e;
	        };
	        this.addEventListener('iceconnectionstatechange',
	          this._connectionstatechangepoly);
	      }
	      return origMethod.apply(this, arguments);
	    };
	  });
	}

	function removeExtmapAllowMixed(window, browserDetails) {
	  /* remove a=extmap-allow-mixed for webrtc.org < M71 */
	  if (!window.RTCPeerConnection) {
	    return;
	  }
	  if (browserDetails.browser === 'chrome' && browserDetails.version >= 71) {
	    return;
	  }
	  if (browserDetails.browser === 'safari' && browserDetails.version >= 605) {
	    return;
	  }
	  const nativeSRD = window.RTCPeerConnection.prototype.setRemoteDescription;
	  window.RTCPeerConnection.prototype.setRemoteDescription =
	  function setRemoteDescription(desc) {
	    if (desc && desc.sdp && desc.sdp.indexOf('\na=extmap-allow-mixed') !== -1) {
	      const sdp = desc.sdp.split('\n').filter((line) => {
	        return line.trim() !== 'a=extmap-allow-mixed';
	      }).join('\n');
	      // Safari enforces read-only-ness of RTCSessionDescription fields.
	      if (window.RTCSessionDescription &&
	          desc instanceof window.RTCSessionDescription) {
	        arguments[0] = new window.RTCSessionDescription({
	          type: desc.type,
	          sdp,
	        });
	      } else {
	        desc.sdp = sdp;
	      }
	    }
	    return nativeSRD.apply(this, arguments);
	  };
	}

	function shimAddIceCandidateNullOrEmpty(window, browserDetails) {
	  // Support for addIceCandidate(null or undefined)
	  // as well as addIceCandidate({candidate: "", ...})
	  // https://bugs.chromium.org/p/chromium/issues/detail?id=978582
	  // Note: must be called before other polyfills which change the signature.
	  if (!(window.RTCPeerConnection && window.RTCPeerConnection.prototype)) {
	    return;
	  }
	  const nativeAddIceCandidate =
	      window.RTCPeerConnection.prototype.addIceCandidate;
	  if (!nativeAddIceCandidate || nativeAddIceCandidate.length === 0) {
	    return;
	  }
	  window.RTCPeerConnection.prototype.addIceCandidate =
	    function addIceCandidate() {
	      if (!arguments[0]) {
	        if (arguments[1]) {
	          arguments[1].apply(null);
	        }
	        return Promise.resolve();
	      }
	      // Firefox 68+ emits and processes {candidate: "", ...}, ignore
	      // in older versions.
	      // Native support for ignoring exists for Chrome M77+.
	      // Safari ignores as well, exact version unknown but works in the same
	      // version that also ignores addIceCandidate(null).
	      if (((browserDetails.browser === 'chrome' && browserDetails.version < 78)
	           || (browserDetails.browser === 'firefox'
	               && browserDetails.version < 68)
	           || (browserDetails.browser === 'safari'))
	          && arguments[0] && arguments[0].candidate === '') {
	        return Promise.resolve();
	      }
	      return nativeAddIceCandidate.apply(this, arguments);
	    };
	}

	var commonShim = /*#__PURE__*/Object.freeze({
		__proto__: null,
		shimRTCIceCandidate: shimRTCIceCandidate,
		shimMaxMessageSize: shimMaxMessageSize,
		shimSendThrowTypeError: shimSendThrowTypeError,
		shimConnectionState: shimConnectionState,
		removeExtmapAllowMixed: removeExtmapAllowMixed,
		shimAddIceCandidateNullOrEmpty: shimAddIceCandidateNullOrEmpty
	});

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	// Shimming starts here.
	function adapterFactory({window} = {}, options = {
	  shimChrome: true,
	  shimFirefox: true,
	  shimEdge: true,
	  shimSafari: true,
	}) {
	  // Utils.
	  const logging = log$1;
	  const browserDetails = detectBrowser(window);

	  const adapter = {
	    browserDetails,
	    commonShim,
	    extractVersion: extractVersion,
	    disableLog: disableLog,
	    disableWarnings: disableWarnings
	  };

	  // Shim browser if found.
	  switch (browserDetails.browser) {
	    case 'chrome':
	      if (!chromeShim || !shimPeerConnection$2 ||
	          !options.shimChrome) {
	        logging('Chrome shim is not included in this adapter release.');
	        return adapter;
	      }
	      if (browserDetails.version === null) {
	        logging('Chrome shim can not determine version, not shimming.');
	        return adapter;
	      }
	      logging('adapter.js shimming chrome.');
	      // Export to the adapter global object visible in the browser.
	      adapter.browserShim = chromeShim;

	      // Must be called before shimPeerConnection.
	      shimAddIceCandidateNullOrEmpty(window, browserDetails);

	      shimGetUserMedia$3(window, browserDetails);
	      shimMediaStream(window);
	      shimPeerConnection$2(window, browserDetails);
	      shimOnTrack$1(window);
	      shimAddTrackRemoveTrack(window, browserDetails);
	      shimGetSendersWithDtmf(window);
	      shimGetStats(window);
	      shimSenderReceiverGetStats(window);
	      fixNegotiationNeeded(window, browserDetails);

	      shimRTCIceCandidate(window);
	      shimConnectionState(window);
	      shimMaxMessageSize(window, browserDetails);
	      shimSendThrowTypeError(window);
	      removeExtmapAllowMixed(window, browserDetails);
	      break;
	    case 'firefox':
	      if (!firefoxShim || !shimPeerConnection ||
	          !options.shimFirefox) {
	        logging('Firefox shim is not included in this adapter release.');
	        return adapter;
	      }
	      logging('adapter.js shimming firefox.');
	      // Export to the adapter global object visible in the browser.
	      adapter.browserShim = firefoxShim;

	      // Must be called before shimPeerConnection.
	      shimAddIceCandidateNullOrEmpty(window, browserDetails);

	      shimGetUserMedia$1(window, browserDetails);
	      shimPeerConnection(window, browserDetails);
	      shimOnTrack(window);
	      shimRemoveStream(window);
	      shimSenderGetStats(window);
	      shimReceiverGetStats(window);
	      shimRTCDataChannel(window);
	      shimAddTransceiver(window);
	      shimGetParameters(window);
	      shimCreateOffer(window);
	      shimCreateAnswer(window);

	      shimRTCIceCandidate(window);
	      shimConnectionState(window);
	      shimMaxMessageSize(window, browserDetails);
	      shimSendThrowTypeError(window);
	      break;
	    case 'edge':
	      if (!edgeShim || !shimPeerConnection$1 || !options.shimEdge) {
	        logging('MS edge shim is not included in this adapter release.');
	        return adapter;
	      }
	      logging('adapter.js shimming edge.');
	      // Export to the adapter global object visible in the browser.
	      adapter.browserShim = edgeShim;

	      shimGetUserMedia$2(window);
	      shimGetDisplayMedia$1(window);
	      shimPeerConnection$1(window, browserDetails);
	      shimReplaceTrack(window);

	      // the edge shim implements the full RTCIceCandidate object.

	      shimMaxMessageSize(window, browserDetails);
	      shimSendThrowTypeError(window);
	      break;
	    case 'safari':
	      if (!safariShim || !options.shimSafari) {
	        logging('Safari shim is not included in this adapter release.');
	        return adapter;
	      }
	      logging('adapter.js shimming safari.');
	      // Export to the adapter global object visible in the browser.
	      adapter.browserShim = safariShim;

	      // Must be called before shimCallbackAPI.
	      shimAddIceCandidateNullOrEmpty(window, browserDetails);

	      shimRTCIceServerUrls(window);
	      shimCreateOfferLegacy(window);
	      shimCallbacksAPI(window);
	      shimLocalStreamsAPI(window);
	      shimRemoteStreamsAPI(window);
	      shimTrackEventTransceiver(window);
	      shimGetUserMedia(window);
	      shimAudioContext(window);

	      shimRTCIceCandidate(window);
	      shimMaxMessageSize(window, browserDetails);
	      shimSendThrowTypeError(window);
	      removeExtmapAllowMixed(window, browserDetails);
	      break;
	    default:
	      logging('Unsupported browser!');
	      break;
	  }

	  return adapter;
	}

	/*
	 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
	 *
	 *  Use of this source code is governed by a BSD-style license
	 *  that can be found in the LICENSE file in the root of the source
	 *  tree.
	 */

	adapterFactory({window: typeof window === 'undefined' ? undefined : window});

	/**
	 * @class AudioTrackConstraints
	 * @classDesc Constraints for creating an audio MediaStreamTrack.
	 * @memberof Owt.Base
	 * @constructor
	 * @param {Owt.Base.AudioSourceInfo} source Source info of this audio track.
	 */
	class AudioTrackConstraints {
	  // eslint-disable-next-line require-jsdoc
	  constructor(source) {
	    if (!Object.values(AudioSourceInfo).some(v => v === source)) {
	      throw new TypeError('Invalid source.');
	    }
	    /**
	     * @member {string} source
	     * @memberof Owt.Base.AudioTrackConstraints
	     * @desc Values could be "mic", "screen-cast", "file" or "mixed".
	     * @instance
	     */
	    this.source = source;
	    /**
	     * @member {string} deviceId
	     * @memberof Owt.Base.AudioTrackConstraints
	     * @desc Do not provide deviceId if source is not "mic".
	     * @instance
	     * @see https://w3c.github.io/mediacapture-main/#def-constraint-deviceId
	     */
	    this.deviceId = undefined;
	  }
	}

	/**
	 * @class VideoTrackConstraints
	 * @classDesc Constraints for creating a video MediaStreamTrack.
	 * @memberof Owt.Base
	 * @constructor
	 * @param {Owt.Base.VideoSourceInfo} source Source info of this video track.
	 */
	class VideoTrackConstraints {
	  // eslint-disable-next-line require-jsdoc
	  constructor(source) {
	    if (!Object.values(VideoSourceInfo).some(v => v === source)) {
	      throw new TypeError('Invalid source.');
	    }
	    /**
	     * @member {string} source
	     * @memberof Owt.Base.VideoTrackConstraints
	     * @desc Values could be "camera", "screen-cast", "file" or "mixed".
	     * @instance
	     */
	    this.source = source;
	    /**
	     * @member {string} deviceId
	     * @memberof Owt.Base.VideoTrackConstraints
	     * @desc Do not provide deviceId if source is not "camera".
	     * @instance
	     * @see https://w3c.github.io/mediacapture-main/#def-constraint-deviceId
	     */

	    this.deviceId = undefined;

	    /**
	     * @member {Owt.Base.Resolution} resolution
	     * @memberof Owt.Base.VideoTrackConstraints
	     * @instance
	     */
	    this.resolution = undefined;

	    /**
	     * @member {number} frameRate
	     * @memberof Owt.Base.VideoTrackConstraints
	     * @instance
	     */
	    this.frameRate = undefined;
	  }
	}
	/**
	 * @class StreamConstraints
	 * @classDesc Constraints for creating a MediaStream from screen mic and camera.
	 * @memberof Owt.Base
	 * @constructor
	 * @param {?Owt.Base.AudioTrackConstraints} audioConstraints
	 * @param {?Owt.Base.VideoTrackConstraints} videoConstraints
	 */
	class StreamConstraints {
	  // eslint-disable-next-line require-jsdoc
	  constructor(audioConstraints = false, videoConstraints = false) {
	    /**
	     * @member {Owt.Base.MediaStreamTrackDeviceConstraintsForAudio} audio
	     * @memberof Owt.Base.MediaStreamDeviceConstraints
	     * @instance
	     */
	    this.audio = audioConstraints;
	    /**
	     * @member {Owt.Base.MediaStreamTrackDeviceConstraintsForVideo} Video
	     * @memberof Owt.Base.MediaStreamDeviceConstraints
	     * @instance
	     */
	    this.video = videoConstraints;
	  }
	}

	// eslint-disable-next-line require-jsdoc
	function isVideoConstrainsForScreenCast(constraints) {
	  return typeof constraints.video === 'object' && constraints.video.source === VideoSourceInfo.SCREENCAST;
	}

	/**
	 * @class MediaStreamFactory
	 * @classDesc A factory to create MediaStream. You can also create MediaStream by yourself.
	 * @memberof Owt.Base
	 */
	class MediaStreamFactory {
	  /**
	   * @function createMediaStream
	   * @static
	   * @desc Create a MediaStream with given constraints. If you want to create a MediaStream for screen cast, please make sure both audio and video's source are "screen-cast".
	   * @memberof Owt.Base.MediaStreamFactory
	   * @return {Promise<MediaStream, Error>} Return a promise that is resolved when stream is successfully created, or rejected if one of the following error happened:
	   * - One or more parameters cannot be satisfied.
	   * - Specified device is busy.
	   * - Cannot obtain necessary permission or operation is canceled by user.
	   * - Video source is screen cast, while audio source is not.
	   * - Audio source is screen cast, while video source is disabled.
	   * @param {Owt.Base.StreamConstraints} constraints
	   */
	  static createMediaStream(constraints) {
	    if (typeof constraints !== 'object' || !constraints.audio && !constraints.video) {
	      return Promise.reject(new TypeError('Invalid constrains'));
	    }
	    if (!isVideoConstrainsForScreenCast(constraints) && typeof constraints.audio === 'object' && constraints.audio.source === AudioSourceInfo.SCREENCAST) {
	      return Promise.reject(new TypeError('Cannot share screen without video.'));
	    }
	    if (isVideoConstrainsForScreenCast(constraints) && !isChrome() && !isFirefox()) {
	      return Promise.reject(new TypeError('Screen sharing only supports Chrome and Firefox.'));
	    }
	    if (isVideoConstrainsForScreenCast(constraints) && typeof constraints.audio === 'object' && constraints.audio.source !== AudioSourceInfo.SCREENCAST) {
	      return Promise.reject(new TypeError('Cannot capture video from screen cast while capture audio from' + ' other source.'));
	    }

	    // Check and convert constraints.
	    if (!constraints.audio && !constraints.video) {
	      return Promise.reject(new TypeError('At least one of audio and video must be requested.'));
	    }
	    const mediaConstraints = Object.create({});
	    if (typeof constraints.audio === 'object' && constraints.audio.source === AudioSourceInfo.MIC) {
	      mediaConstraints.audio = Object.create({});
	      if (isEdge()) {
	        mediaConstraints.audio.deviceId = constraints.audio.deviceId;
	      } else {
	        mediaConstraints.audio.deviceId = {
	          exact: constraints.audio.deviceId
	        };
	      }
	    } else {
	      if (constraints.audio.source === AudioSourceInfo.SCREENCAST) {
	        mediaConstraints.audio = true;
	      } else {
	        mediaConstraints.audio = constraints.audio;
	      }
	    }
	    if (typeof constraints.video === 'object') {
	      mediaConstraints.video = Object.create({});
	      if (typeof constraints.video.frameRate === 'number') {
	        mediaConstraints.video.frameRate = constraints.video.frameRate;
	      }
	      if (constraints.video.resolution && constraints.video.resolution.width && constraints.video.resolution.height) {
	        if (constraints.video.source === VideoSourceInfo.SCREENCAST) {
	          mediaConstraints.video.width = constraints.video.resolution.width;
	          mediaConstraints.video.height = constraints.video.resolution.height;
	        } else {
	          mediaConstraints.video.width = Object.create({});
	          mediaConstraints.video.width.exact = constraints.video.resolution.width;
	          mediaConstraints.video.height = Object.create({});
	          mediaConstraints.video.height.exact = constraints.video.resolution.height;
	        }
	      }
	      if (typeof constraints.video.deviceId === 'string') {
	        mediaConstraints.video.deviceId = {
	          exact: constraints.video.deviceId
	        };
	      }
	      if (isFirefox() && constraints.video.source === VideoSourceInfo.SCREENCAST) {
	        mediaConstraints.video.mediaSource = 'screen';
	      }
	    } else {
	      mediaConstraints.video = constraints.video;
	    }
	    if (isVideoConstrainsForScreenCast(constraints)) {
	      return navigator.mediaDevices.getDisplayMedia(mediaConstraints);
	    } else {
	      return navigator.mediaDevices.getUserMedia(mediaConstraints);
	    }
	  }
	}

	// Copyright (C) <2018> Intel Corporation

	var media = /*#__PURE__*/Object.freeze({
		__proto__: null,
		AudioTrackConstraints: AudioTrackConstraints,
		VideoTrackConstraints: VideoTrackConstraints,
		StreamConstraints: StreamConstraints,
		MediaStreamFactory: MediaStreamFactory,
		AudioSourceInfo: AudioSourceInfo,
		VideoSourceInfo: VideoSourceInfo,
		TrackKind: TrackKind,
		Resolution: Resolution
	});

	let logger;
	let errorLogger;
	function setLogger() {
	  /*eslint-disable */
	  logger = console.log;
	  errorLogger = console.error;
	  /*eslint-enable */
	}
	function log(message, ...optionalParams) {
	  if (logger) {
	    logger(message, ...optionalParams);
	  }
	}
	function error(message, ...optionalParams) {
	  if (errorLogger) {
	    errorLogger(message, ...optionalParams);
	  }
	}

	class Event$1 {
	  constructor(type) {
	    this.listener = {};
	    this.type = type | '';
	  }
	  on(event, fn) {
	    if (!this.listener[event]) {
	      this.listener[event] = [];
	    }
	    this.listener[event].push(fn);
	    return true;
	  }
	  off(event, fn) {
	    if (this.listener[event]) {
	      var index = this.listener[event].indexOf(fn);
	      if (index > -1) {
	        this.listener[event].splice(index, 1);
	      }
	      return true;
	    }
	    return false;
	  }
	  offAll() {
	    this.listener = {};
	  }
	  dispatch(event, data) {
	    if (this.listener[event]) {
	      this.listener[event].map(each => {
	        each.apply(null, [data]);
	      });
	      return true;
	    }
	    return false;
	  }
	}

	var bind = function bind(fn, thisArg) {
	  return function wrap() {
	    var args = new Array(arguments.length);
	    for (var i = 0; i < args.length; i++) {
	      args[i] = arguments[i];
	    }
	    return fn.apply(thisArg, args);
	  };
	};

	// utils is a library of generic helper functions non-specific to axios

	var toString = Object.prototype.toString;

	// eslint-disable-next-line func-names
	var kindOf = (function(cache) {
	  // eslint-disable-next-line func-names
	  return function(thing) {
	    var str = toString.call(thing);
	    return cache[str] || (cache[str] = str.slice(8, -1).toLowerCase());
	  };
	})(Object.create(null));

	function kindOfTest(type) {
	  type = type.toLowerCase();
	  return function isKindOf(thing) {
	    return kindOf(thing) === type;
	  };
	}

	/**
	 * Determine if a value is an Array
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is an Array, otherwise false
	 */
	function isArray(val) {
	  return Array.isArray(val);
	}

	/**
	 * Determine if a value is undefined
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if the value is undefined, otherwise false
	 */
	function isUndefined(val) {
	  return typeof val === 'undefined';
	}

	/**
	 * Determine if a value is a Buffer
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Buffer, otherwise false
	 */
	function isBuffer(val) {
	  return val !== null && !isUndefined(val) && val.constructor !== null && !isUndefined(val.constructor)
	    && typeof val.constructor.isBuffer === 'function' && val.constructor.isBuffer(val);
	}

	/**
	 * Determine if a value is an ArrayBuffer
	 *
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is an ArrayBuffer, otherwise false
	 */
	var isArrayBuffer = kindOfTest('ArrayBuffer');


	/**
	 * Determine if a value is a view on an ArrayBuffer
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a view on an ArrayBuffer, otherwise false
	 */
	function isArrayBufferView(val) {
	  var result;
	  if ((typeof ArrayBuffer !== 'undefined') && (ArrayBuffer.isView)) {
	    result = ArrayBuffer.isView(val);
	  } else {
	    result = (val) && (val.buffer) && (isArrayBuffer(val.buffer));
	  }
	  return result;
	}

	/**
	 * Determine if a value is a String
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a String, otherwise false
	 */
	function isString(val) {
	  return typeof val === 'string';
	}

	/**
	 * Determine if a value is a Number
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Number, otherwise false
	 */
	function isNumber(val) {
	  return typeof val === 'number';
	}

	/**
	 * Determine if a value is an Object
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is an Object, otherwise false
	 */
	function isObject(val) {
	  return val !== null && typeof val === 'object';
	}

	/**
	 * Determine if a value is a plain Object
	 *
	 * @param {Object} val The value to test
	 * @return {boolean} True if value is a plain Object, otherwise false
	 */
	function isPlainObject(val) {
	  if (kindOf(val) !== 'object') {
	    return false;
	  }

	  var prototype = Object.getPrototypeOf(val);
	  return prototype === null || prototype === Object.prototype;
	}

	/**
	 * Determine if a value is a Date
	 *
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Date, otherwise false
	 */
	var isDate = kindOfTest('Date');

	/**
	 * Determine if a value is a File
	 *
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a File, otherwise false
	 */
	var isFile = kindOfTest('File');

	/**
	 * Determine if a value is a Blob
	 *
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Blob, otherwise false
	 */
	var isBlob = kindOfTest('Blob');

	/**
	 * Determine if a value is a FileList
	 *
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a File, otherwise false
	 */
	var isFileList = kindOfTest('FileList');

	/**
	 * Determine if a value is a Function
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Function, otherwise false
	 */
	function isFunction(val) {
	  return toString.call(val) === '[object Function]';
	}

	/**
	 * Determine if a value is a Stream
	 *
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a Stream, otherwise false
	 */
	function isStream(val) {
	  return isObject(val) && isFunction(val.pipe);
	}

	/**
	 * Determine if a value is a FormData
	 *
	 * @param {Object} thing The value to test
	 * @returns {boolean} True if value is an FormData, otherwise false
	 */
	function isFormData(thing) {
	  var pattern = '[object FormData]';
	  return thing && (
	    (typeof FormData === 'function' && thing instanceof FormData) ||
	    toString.call(thing) === pattern ||
	    (isFunction(thing.toString) && thing.toString() === pattern)
	  );
	}

	/**
	 * Determine if a value is a URLSearchParams object
	 * @function
	 * @param {Object} val The value to test
	 * @returns {boolean} True if value is a URLSearchParams object, otherwise false
	 */
	var isURLSearchParams = kindOfTest('URLSearchParams');

	/**
	 * Trim excess whitespace off the beginning and end of a string
	 *
	 * @param {String} str The String to trim
	 * @returns {String} The String freed of excess whitespace
	 */
	function trim(str) {
	  return str.trim ? str.trim() : str.replace(/^\s+|\s+$/g, '');
	}

	/**
	 * Determine if we're running in a standard browser environment
	 *
	 * This allows axios to run in a web worker, and react-native.
	 * Both environments support XMLHttpRequest, but not fully standard globals.
	 *
	 * web workers:
	 *  typeof window -> undefined
	 *  typeof document -> undefined
	 *
	 * react-native:
	 *  navigator.product -> 'ReactNative'
	 * nativescript
	 *  navigator.product -> 'NativeScript' or 'NS'
	 */
	function isStandardBrowserEnv() {
	  if (typeof navigator !== 'undefined' && (navigator.product === 'ReactNative' ||
	                                           navigator.product === 'NativeScript' ||
	                                           navigator.product === 'NS')) {
	    return false;
	  }
	  return (
	    typeof window !== 'undefined' &&
	    typeof document !== 'undefined'
	  );
	}

	/**
	 * Iterate over an Array or an Object invoking a function for each item.
	 *
	 * If `obj` is an Array callback will be called passing
	 * the value, index, and complete array for each item.
	 *
	 * If 'obj' is an Object callback will be called passing
	 * the value, key, and complete object for each property.
	 *
	 * @param {Object|Array} obj The object to iterate
	 * @param {Function} fn The callback to invoke for each item
	 */
	function forEach(obj, fn) {
	  // Don't bother if no value provided
	  if (obj === null || typeof obj === 'undefined') {
	    return;
	  }

	  // Force an array if not already something iterable
	  if (typeof obj !== 'object') {
	    /*eslint no-param-reassign:0*/
	    obj = [obj];
	  }

	  if (isArray(obj)) {
	    // Iterate over array values
	    for (var i = 0, l = obj.length; i < l; i++) {
	      fn.call(null, obj[i], i, obj);
	    }
	  } else {
	    // Iterate over object keys
	    for (var key in obj) {
	      if (Object.prototype.hasOwnProperty.call(obj, key)) {
	        fn.call(null, obj[key], key, obj);
	      }
	    }
	  }
	}

	/**
	 * Accepts varargs expecting each argument to be an object, then
	 * immutably merges the properties of each object and returns result.
	 *
	 * When multiple objects contain the same key the later object in
	 * the arguments list will take precedence.
	 *
	 * Example:
	 *
	 * ```js
	 * var result = merge({foo: 123}, {foo: 456});
	 * console.log(result.foo); // outputs 456
	 * ```
	 *
	 * @param {Object} obj1 Object to merge
	 * @returns {Object} Result of all merge properties
	 */
	function merge(/* obj1, obj2, obj3, ... */) {
	  var result = {};
	  function assignValue(val, key) {
	    if (isPlainObject(result[key]) && isPlainObject(val)) {
	      result[key] = merge(result[key], val);
	    } else if (isPlainObject(val)) {
	      result[key] = merge({}, val);
	    } else if (isArray(val)) {
	      result[key] = val.slice();
	    } else {
	      result[key] = val;
	    }
	  }

	  for (var i = 0, l = arguments.length; i < l; i++) {
	    forEach(arguments[i], assignValue);
	  }
	  return result;
	}

	/**
	 * Extends object a by mutably adding to it the properties of object b.
	 *
	 * @param {Object} a The object to be extended
	 * @param {Object} b The object to copy properties from
	 * @param {Object} thisArg The object to bind function to
	 * @return {Object} The resulting value of object a
	 */
	function extend(a, b, thisArg) {
	  forEach(b, function assignValue(val, key) {
	    if (thisArg && typeof val === 'function') {
	      a[key] = bind(val, thisArg);
	    } else {
	      a[key] = val;
	    }
	  });
	  return a;
	}

	/**
	 * Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
	 *
	 * @param {string} content with BOM
	 * @return {string} content value without BOM
	 */
	function stripBOM(content) {
	  if (content.charCodeAt(0) === 0xFEFF) {
	    content = content.slice(1);
	  }
	  return content;
	}

	/**
	 * Inherit the prototype methods from one constructor into another
	 * @param {function} constructor
	 * @param {function} superConstructor
	 * @param {object} [props]
	 * @param {object} [descriptors]
	 */

	function inherits(constructor, superConstructor, props, descriptors) {
	  constructor.prototype = Object.create(superConstructor.prototype, descriptors);
	  constructor.prototype.constructor = constructor;
	  props && Object.assign(constructor.prototype, props);
	}

	/**
	 * Resolve object with deep prototype chain to a flat object
	 * @param {Object} sourceObj source object
	 * @param {Object} [destObj]
	 * @param {Function} [filter]
	 * @returns {Object}
	 */

	function toFlatObject(sourceObj, destObj, filter) {
	  var props;
	  var i;
	  var prop;
	  var merged = {};

	  destObj = destObj || {};

	  do {
	    props = Object.getOwnPropertyNames(sourceObj);
	    i = props.length;
	    while (i-- > 0) {
	      prop = props[i];
	      if (!merged[prop]) {
	        destObj[prop] = sourceObj[prop];
	        merged[prop] = true;
	      }
	    }
	    sourceObj = Object.getPrototypeOf(sourceObj);
	  } while (sourceObj && (!filter || filter(sourceObj, destObj)) && sourceObj !== Object.prototype);

	  return destObj;
	}

	/*
	 * determines whether a string ends with the characters of a specified string
	 * @param {String} str
	 * @param {String} searchString
	 * @param {Number} [position= 0]
	 * @returns {boolean}
	 */
	function endsWith(str, searchString, position) {
	  str = String(str);
	  if (position === undefined || position > str.length) {
	    position = str.length;
	  }
	  position -= searchString.length;
	  var lastIndex = str.indexOf(searchString, position);
	  return lastIndex !== -1 && lastIndex === position;
	}


	/**
	 * Returns new array from array like object
	 * @param {*} [thing]
	 * @returns {Array}
	 */
	function toArray(thing) {
	  if (!thing) return null;
	  var i = thing.length;
	  if (isUndefined(i)) return null;
	  var arr = new Array(i);
	  while (i-- > 0) {
	    arr[i] = thing[i];
	  }
	  return arr;
	}

	// eslint-disable-next-line func-names
	var isTypedArray = (function(TypedArray) {
	  // eslint-disable-next-line func-names
	  return function(thing) {
	    return TypedArray && thing instanceof TypedArray;
	  };
	})(typeof Uint8Array !== 'undefined' && Object.getPrototypeOf(Uint8Array));

	var utils = {
	  isArray: isArray,
	  isArrayBuffer: isArrayBuffer,
	  isBuffer: isBuffer,
	  isFormData: isFormData,
	  isArrayBufferView: isArrayBufferView,
	  isString: isString,
	  isNumber: isNumber,
	  isObject: isObject,
	  isPlainObject: isPlainObject,
	  isUndefined: isUndefined,
	  isDate: isDate,
	  isFile: isFile,
	  isBlob: isBlob,
	  isFunction: isFunction,
	  isStream: isStream,
	  isURLSearchParams: isURLSearchParams,
	  isStandardBrowserEnv: isStandardBrowserEnv,
	  forEach: forEach,
	  merge: merge,
	  extend: extend,
	  trim: trim,
	  stripBOM: stripBOM,
	  inherits: inherits,
	  toFlatObject: toFlatObject,
	  kindOf: kindOf,
	  kindOfTest: kindOfTest,
	  endsWith: endsWith,
	  toArray: toArray,
	  isTypedArray: isTypedArray,
	  isFileList: isFileList
	};

	function encode(val) {
	  return encodeURIComponent(val).
	    replace(/%3A/gi, ':').
	    replace(/%24/g, '$').
	    replace(/%2C/gi, ',').
	    replace(/%20/g, '+').
	    replace(/%5B/gi, '[').
	    replace(/%5D/gi, ']');
	}

	/**
	 * Build a URL by appending params to the end
	 *
	 * @param {string} url The base of the url (e.g., http://www.google.com)
	 * @param {object} [params] The params to be appended
	 * @returns {string} The formatted url
	 */
	var buildURL = function buildURL(url, params, paramsSerializer) {
	  /*eslint no-param-reassign:0*/
	  if (!params) {
	    return url;
	  }

	  var serializedParams;
	  if (paramsSerializer) {
	    serializedParams = paramsSerializer(params);
	  } else if (utils.isURLSearchParams(params)) {
	    serializedParams = params.toString();
	  } else {
	    var parts = [];

	    utils.forEach(params, function serialize(val, key) {
	      if (val === null || typeof val === 'undefined') {
	        return;
	      }

	      if (utils.isArray(val)) {
	        key = key + '[]';
	      } else {
	        val = [val];
	      }

	      utils.forEach(val, function parseValue(v) {
	        if (utils.isDate(v)) {
	          v = v.toISOString();
	        } else if (utils.isObject(v)) {
	          v = JSON.stringify(v);
	        }
	        parts.push(encode(key) + '=' + encode(v));
	      });
	    });

	    serializedParams = parts.join('&');
	  }

	  if (serializedParams) {
	    var hashmarkIndex = url.indexOf('#');
	    if (hashmarkIndex !== -1) {
	      url = url.slice(0, hashmarkIndex);
	    }

	    url += (url.indexOf('?') === -1 ? '?' : '&') + serializedParams;
	  }

	  return url;
	};

	function InterceptorManager() {
	  this.handlers = [];
	}

	/**
	 * Add a new interceptor to the stack
	 *
	 * @param {Function} fulfilled The function to handle `then` for a `Promise`
	 * @param {Function} rejected The function to handle `reject` for a `Promise`
	 *
	 * @return {Number} An ID used to remove interceptor later
	 */
	InterceptorManager.prototype.use = function use(fulfilled, rejected, options) {
	  this.handlers.push({
	    fulfilled: fulfilled,
	    rejected: rejected,
	    synchronous: options ? options.synchronous : false,
	    runWhen: options ? options.runWhen : null
	  });
	  return this.handlers.length - 1;
	};

	/**
	 * Remove an interceptor from the stack
	 *
	 * @param {Number} id The ID that was returned by `use`
	 */
	InterceptorManager.prototype.eject = function eject(id) {
	  if (this.handlers[id]) {
	    this.handlers[id] = null;
	  }
	};

	/**
	 * Iterate over all the registered interceptors
	 *
	 * This method is particularly useful for skipping over any
	 * interceptors that may have become `null` calling `eject`.
	 *
	 * @param {Function} fn The function to call for each interceptor
	 */
	InterceptorManager.prototype.forEach = function forEach(fn) {
	  utils.forEach(this.handlers, function forEachHandler(h) {
	    if (h !== null) {
	      fn(h);
	    }
	  });
	};

	var InterceptorManager_1 = InterceptorManager;

	var normalizeHeaderName = function normalizeHeaderName(headers, normalizedName) {
	  utils.forEach(headers, function processHeader(value, name) {
	    if (name !== normalizedName && name.toUpperCase() === normalizedName.toUpperCase()) {
	      headers[normalizedName] = value;
	      delete headers[name];
	    }
	  });
	};

	/**
	 * Create an Error with the specified message, config, error code, request and response.
	 *
	 * @param {string} message The error message.
	 * @param {string} [code] The error code (for example, 'ECONNABORTED').
	 * @param {Object} [config] The config.
	 * @param {Object} [request] The request.
	 * @param {Object} [response] The response.
	 * @returns {Error} The created error.
	 */
	function AxiosError(message, code, config, request, response) {
	  Error.call(this);
	  this.message = message;
	  this.name = 'AxiosError';
	  code && (this.code = code);
	  config && (this.config = config);
	  request && (this.request = request);
	  response && (this.response = response);
	}

	utils.inherits(AxiosError, Error, {
	  toJSON: function toJSON() {
	    return {
	      // Standard
	      message: this.message,
	      name: this.name,
	      // Microsoft
	      description: this.description,
	      number: this.number,
	      // Mozilla
	      fileName: this.fileName,
	      lineNumber: this.lineNumber,
	      columnNumber: this.columnNumber,
	      stack: this.stack,
	      // Axios
	      config: this.config,
	      code: this.code,
	      status: this.response && this.response.status ? this.response.status : null
	    };
	  }
	});

	var prototype = AxiosError.prototype;
	var descriptors = {};

	[
	  'ERR_BAD_OPTION_VALUE',
	  'ERR_BAD_OPTION',
	  'ECONNABORTED',
	  'ETIMEDOUT',
	  'ERR_NETWORK',
	  'ERR_FR_TOO_MANY_REDIRECTS',
	  'ERR_DEPRECATED',
	  'ERR_BAD_RESPONSE',
	  'ERR_BAD_REQUEST',
	  'ERR_CANCELED'
	// eslint-disable-next-line func-names
	].forEach(function(code) {
	  descriptors[code] = {value: code};
	});

	Object.defineProperties(AxiosError, descriptors);
	Object.defineProperty(prototype, 'isAxiosError', {value: true});

	// eslint-disable-next-line func-names
	AxiosError.from = function(error, code, config, request, response, customProps) {
	  var axiosError = Object.create(prototype);

	  utils.toFlatObject(error, axiosError, function filter(obj) {
	    return obj !== Error.prototype;
	  });

	  AxiosError.call(axiosError, error.message, code, config, request, response);

	  axiosError.name = error.name;

	  customProps && Object.assign(axiosError, customProps);

	  return axiosError;
	};

	var AxiosError_1 = AxiosError;

	var transitional = {
	  silentJSONParsing: true,
	  forcedJSONParsing: true,
	  clarifyTimeoutError: false
	};

	/**
	 * Convert a data object to FormData
	 * @param {Object} obj
	 * @param {?Object} [formData]
	 * @returns {Object}
	 **/

	function toFormData(obj, formData) {
	  // eslint-disable-next-line no-param-reassign
	  formData = formData || new FormData();

	  var stack = [];

	  function convertValue(value) {
	    if (value === null) return '';

	    if (utils.isDate(value)) {
	      return value.toISOString();
	    }

	    if (utils.isArrayBuffer(value) || utils.isTypedArray(value)) {
	      return typeof Blob === 'function' ? new Blob([value]) : Buffer.from(value);
	    }

	    return value;
	  }

	  function build(data, parentKey) {
	    if (utils.isPlainObject(data) || utils.isArray(data)) {
	      if (stack.indexOf(data) !== -1) {
	        throw Error('Circular reference detected in ' + parentKey);
	      }

	      stack.push(data);

	      utils.forEach(data, function each(value, key) {
	        if (utils.isUndefined(value)) return;
	        var fullKey = parentKey ? parentKey + '.' + key : key;
	        var arr;

	        if (value && !parentKey && typeof value === 'object') {
	          if (utils.endsWith(key, '{}')) {
	            // eslint-disable-next-line no-param-reassign
	            value = JSON.stringify(value);
	          } else if (utils.endsWith(key, '[]') && (arr = utils.toArray(value))) {
	            // eslint-disable-next-line func-names
	            arr.forEach(function(el) {
	              !utils.isUndefined(el) && formData.append(fullKey, convertValue(el));
	            });
	            return;
	          }
	        }

	        build(value, fullKey);
	      });

	      stack.pop();
	    } else {
	      formData.append(parentKey, convertValue(data));
	    }
	  }

	  build(obj);

	  return formData;
	}

	var toFormData_1 = toFormData;

	/**
	 * Resolve or reject a Promise based on response status.
	 *
	 * @param {Function} resolve A function that resolves the promise.
	 * @param {Function} reject A function that rejects the promise.
	 * @param {object} response The response.
	 */
	var settle = function settle(resolve, reject, response) {
	  var validateStatus = response.config.validateStatus;
	  if (!response.status || !validateStatus || validateStatus(response.status)) {
	    resolve(response);
	  } else {
	    reject(new AxiosError_1(
	      'Request failed with status code ' + response.status,
	      [AxiosError_1.ERR_BAD_REQUEST, AxiosError_1.ERR_BAD_RESPONSE][Math.floor(response.status / 100) - 4],
	      response.config,
	      response.request,
	      response
	    ));
	  }
	};

	var cookies = (
	  utils.isStandardBrowserEnv() ?

	  // Standard browser envs support document.cookie
	    (function standardBrowserEnv() {
	      return {
	        write: function write(name, value, expires, path, domain, secure) {
	          var cookie = [];
	          cookie.push(name + '=' + encodeURIComponent(value));

	          if (utils.isNumber(expires)) {
	            cookie.push('expires=' + new Date(expires).toGMTString());
	          }

	          if (utils.isString(path)) {
	            cookie.push('path=' + path);
	          }

	          if (utils.isString(domain)) {
	            cookie.push('domain=' + domain);
	          }

	          if (secure === true) {
	            cookie.push('secure');
	          }

	          document.cookie = cookie.join('; ');
	        },

	        read: function read(name) {
	          var match = document.cookie.match(new RegExp('(^|;\\s*)(' + name + ')=([^;]*)'));
	          return (match ? decodeURIComponent(match[3]) : null);
	        },

	        remove: function remove(name) {
	          this.write(name, '', Date.now() - 86400000);
	        }
	      };
	    })() :

	  // Non standard browser env (web workers, react-native) lack needed support.
	    (function nonStandardBrowserEnv() {
	      return {
	        write: function write() {},
	        read: function read() { return null; },
	        remove: function remove() {}
	      };
	    })()
	);

	/**
	 * Determines whether the specified URL is absolute
	 *
	 * @param {string} url The URL to test
	 * @returns {boolean} True if the specified URL is absolute, otherwise false
	 */
	var isAbsoluteURL = function isAbsoluteURL(url) {
	  // A URL is considered absolute if it begins with "<scheme>://" or "//" (protocol-relative URL).
	  // RFC 3986 defines scheme name as a sequence of characters beginning with a letter and followed
	  // by any combination of letters, digits, plus, period, or hyphen.
	  return /^([a-z][a-z\d+\-.]*:)?\/\//i.test(url);
	};

	/**
	 * Creates a new URL by combining the specified URLs
	 *
	 * @param {string} baseURL The base URL
	 * @param {string} relativeURL The relative URL
	 * @returns {string} The combined URL
	 */
	var combineURLs = function combineURLs(baseURL, relativeURL) {
	  return relativeURL
	    ? baseURL.replace(/\/+$/, '') + '/' + relativeURL.replace(/^\/+/, '')
	    : baseURL;
	};

	/**
	 * Creates a new URL by combining the baseURL with the requestedURL,
	 * only when the requestedURL is not already an absolute URL.
	 * If the requestURL is absolute, this function returns the requestedURL untouched.
	 *
	 * @param {string} baseURL The base URL
	 * @param {string} requestedURL Absolute or relative URL to combine
	 * @returns {string} The combined full path
	 */
	var buildFullPath = function buildFullPath(baseURL, requestedURL) {
	  if (baseURL && !isAbsoluteURL(requestedURL)) {
	    return combineURLs(baseURL, requestedURL);
	  }
	  return requestedURL;
	};

	// Headers whose duplicates are ignored by node
	// c.f. https://nodejs.org/api/http.html#http_message_headers
	var ignoreDuplicateOf = [
	  'age', 'authorization', 'content-length', 'content-type', 'etag',
	  'expires', 'from', 'host', 'if-modified-since', 'if-unmodified-since',
	  'last-modified', 'location', 'max-forwards', 'proxy-authorization',
	  'referer', 'retry-after', 'user-agent'
	];

	/**
	 * Parse headers into an object
	 *
	 * ```
	 * Date: Wed, 27 Aug 2014 08:58:49 GMT
	 * Content-Type: application/json
	 * Connection: keep-alive
	 * Transfer-Encoding: chunked
	 * ```
	 *
	 * @param {String} headers Headers needing to be parsed
	 * @returns {Object} Headers parsed into an object
	 */
	var parseHeaders = function parseHeaders(headers) {
	  var parsed = {};
	  var key;
	  var val;
	  var i;

	  if (!headers) { return parsed; }

	  utils.forEach(headers.split('\n'), function parser(line) {
	    i = line.indexOf(':');
	    key = utils.trim(line.substr(0, i)).toLowerCase();
	    val = utils.trim(line.substr(i + 1));

	    if (key) {
	      if (parsed[key] && ignoreDuplicateOf.indexOf(key) >= 0) {
	        return;
	      }
	      if (key === 'set-cookie') {
	        parsed[key] = (parsed[key] ? parsed[key] : []).concat([val]);
	      } else {
	        parsed[key] = parsed[key] ? parsed[key] + ', ' + val : val;
	      }
	    }
	  });

	  return parsed;
	};

	var isURLSameOrigin = (
	  utils.isStandardBrowserEnv() ?

	  // Standard browser envs have full support of the APIs needed to test
	  // whether the request URL is of the same origin as current location.
	    (function standardBrowserEnv() {
	      var msie = /(msie|trident)/i.test(navigator.userAgent);
	      var urlParsingNode = document.createElement('a');
	      var originURL;

	      /**
	    * Parse a URL to discover it's components
	    *
	    * @param {String} url The URL to be parsed
	    * @returns {Object}
	    */
	      function resolveURL(url) {
	        var href = url;

	        if (msie) {
	        // IE needs attribute set twice to normalize properties
	          urlParsingNode.setAttribute('href', href);
	          href = urlParsingNode.href;
	        }

	        urlParsingNode.setAttribute('href', href);

	        // urlParsingNode provides the UrlUtils interface - http://url.spec.whatwg.org/#urlutils
	        return {
	          href: urlParsingNode.href,
	          protocol: urlParsingNode.protocol ? urlParsingNode.protocol.replace(/:$/, '') : '',
	          host: urlParsingNode.host,
	          search: urlParsingNode.search ? urlParsingNode.search.replace(/^\?/, '') : '',
	          hash: urlParsingNode.hash ? urlParsingNode.hash.replace(/^#/, '') : '',
	          hostname: urlParsingNode.hostname,
	          port: urlParsingNode.port,
	          pathname: (urlParsingNode.pathname.charAt(0) === '/') ?
	            urlParsingNode.pathname :
	            '/' + urlParsingNode.pathname
	        };
	      }

	      originURL = resolveURL(window.location.href);

	      /**
	    * Determine if a URL shares the same origin as the current location
	    *
	    * @param {String} requestURL The URL to test
	    * @returns {boolean} True if URL shares the same origin, otherwise false
	    */
	      return function isURLSameOrigin(requestURL) {
	        var parsed = (utils.isString(requestURL)) ? resolveURL(requestURL) : requestURL;
	        return (parsed.protocol === originURL.protocol &&
	            parsed.host === originURL.host);
	      };
	    })() :

	  // Non standard browser envs (web workers, react-native) lack needed support.
	    (function nonStandardBrowserEnv() {
	      return function isURLSameOrigin() {
	        return true;
	      };
	    })()
	);

	/**
	 * A `CanceledError` is an object that is thrown when an operation is canceled.
	 *
	 * @class
	 * @param {string=} message The message.
	 */
	function CanceledError(message) {
	  // eslint-disable-next-line no-eq-null,eqeqeq
	  AxiosError_1.call(this, message == null ? 'canceled' : message, AxiosError_1.ERR_CANCELED);
	  this.name = 'CanceledError';
	}

	utils.inherits(CanceledError, AxiosError_1, {
	  __CANCEL__: true
	});

	var CanceledError_1 = CanceledError;

	var parseProtocol = function parseProtocol(url) {
	  var match = /^([-+\w]{1,25})(:?\/\/|:)/.exec(url);
	  return match && match[1] || '';
	};

	var xhr = function xhrAdapter(config) {
	  return new Promise(function dispatchXhrRequest(resolve, reject) {
	    var requestData = config.data;
	    var requestHeaders = config.headers;
	    var responseType = config.responseType;
	    var onCanceled;
	    function done() {
	      if (config.cancelToken) {
	        config.cancelToken.unsubscribe(onCanceled);
	      }

	      if (config.signal) {
	        config.signal.removeEventListener('abort', onCanceled);
	      }
	    }

	    if (utils.isFormData(requestData) && utils.isStandardBrowserEnv()) {
	      delete requestHeaders['Content-Type']; // Let the browser set it
	    }

	    var request = new XMLHttpRequest();

	    // HTTP basic authentication
	    if (config.auth) {
	      var username = config.auth.username || '';
	      var password = config.auth.password ? unescape(encodeURIComponent(config.auth.password)) : '';
	      requestHeaders.Authorization = 'Basic ' + btoa(username + ':' + password);
	    }

	    var fullPath = buildFullPath(config.baseURL, config.url);

	    request.open(config.method.toUpperCase(), buildURL(fullPath, config.params, config.paramsSerializer), true);

	    // Set the request timeout in MS
	    request.timeout = config.timeout;

	    function onloadend() {
	      if (!request) {
	        return;
	      }
	      // Prepare the response
	      var responseHeaders = 'getAllResponseHeaders' in request ? parseHeaders(request.getAllResponseHeaders()) : null;
	      var responseData = !responseType || responseType === 'text' ||  responseType === 'json' ?
	        request.responseText : request.response;
	      var response = {
	        data: responseData,
	        status: request.status,
	        statusText: request.statusText,
	        headers: responseHeaders,
	        config: config,
	        request: request
	      };

	      settle(function _resolve(value) {
	        resolve(value);
	        done();
	      }, function _reject(err) {
	        reject(err);
	        done();
	      }, response);

	      // Clean up request
	      request = null;
	    }

	    if ('onloadend' in request) {
	      // Use onloadend if available
	      request.onloadend = onloadend;
	    } else {
	      // Listen for ready state to emulate onloadend
	      request.onreadystatechange = function handleLoad() {
	        if (!request || request.readyState !== 4) {
	          return;
	        }

	        // The request errored out and we didn't get a response, this will be
	        // handled by onerror instead
	        // With one exception: request that using file: protocol, most browsers
	        // will return status as 0 even though it's a successful request
	        if (request.status === 0 && !(request.responseURL && request.responseURL.indexOf('file:') === 0)) {
	          return;
	        }
	        // readystate handler is calling before onerror or ontimeout handlers,
	        // so we should call onloadend on the next 'tick'
	        setTimeout(onloadend);
	      };
	    }

	    // Handle browser request cancellation (as opposed to a manual cancellation)
	    request.onabort = function handleAbort() {
	      if (!request) {
	        return;
	      }

	      reject(new AxiosError_1('Request aborted', AxiosError_1.ECONNABORTED, config, request));

	      // Clean up request
	      request = null;
	    };

	    // Handle low level network errors
	    request.onerror = function handleError() {
	      // Real errors are hidden from us by the browser
	      // onerror should only fire if it's a network error
	      reject(new AxiosError_1('Network Error', AxiosError_1.ERR_NETWORK, config, request, request));

	      // Clean up request
	      request = null;
	    };

	    // Handle timeout
	    request.ontimeout = function handleTimeout() {
	      var timeoutErrorMessage = config.timeout ? 'timeout of ' + config.timeout + 'ms exceeded' : 'timeout exceeded';
	      var transitional$1 = config.transitional || transitional;
	      if (config.timeoutErrorMessage) {
	        timeoutErrorMessage = config.timeoutErrorMessage;
	      }
	      reject(new AxiosError_1(
	        timeoutErrorMessage,
	        transitional$1.clarifyTimeoutError ? AxiosError_1.ETIMEDOUT : AxiosError_1.ECONNABORTED,
	        config,
	        request));

	      // Clean up request
	      request = null;
	    };

	    // Add xsrf header
	    // This is only done if running in a standard browser environment.
	    // Specifically not if we're in a web worker, or react-native.
	    if (utils.isStandardBrowserEnv()) {
	      // Add xsrf header
	      var xsrfValue = (config.withCredentials || isURLSameOrigin(fullPath)) && config.xsrfCookieName ?
	        cookies.read(config.xsrfCookieName) :
	        undefined;

	      if (xsrfValue) {
	        requestHeaders[config.xsrfHeaderName] = xsrfValue;
	      }
	    }

	    // Add headers to the request
	    if ('setRequestHeader' in request) {
	      utils.forEach(requestHeaders, function setRequestHeader(val, key) {
	        if (typeof requestData === 'undefined' && key.toLowerCase() === 'content-type') {
	          // Remove Content-Type if data is undefined
	          delete requestHeaders[key];
	        } else {
	          // Otherwise add header to the request
	          request.setRequestHeader(key, val);
	        }
	      });
	    }

	    // Add withCredentials to request if needed
	    if (!utils.isUndefined(config.withCredentials)) {
	      request.withCredentials = !!config.withCredentials;
	    }

	    // Add responseType to request if needed
	    if (responseType && responseType !== 'json') {
	      request.responseType = config.responseType;
	    }

	    // Handle progress if needed
	    if (typeof config.onDownloadProgress === 'function') {
	      request.addEventListener('progress', config.onDownloadProgress);
	    }

	    // Not all browsers support upload events
	    if (typeof config.onUploadProgress === 'function' && request.upload) {
	      request.upload.addEventListener('progress', config.onUploadProgress);
	    }

	    if (config.cancelToken || config.signal) {
	      // Handle cancellation
	      // eslint-disable-next-line func-names
	      onCanceled = function(cancel) {
	        if (!request) {
	          return;
	        }
	        reject(!cancel || (cancel && cancel.type) ? new CanceledError_1() : cancel);
	        request.abort();
	        request = null;
	      };

	      config.cancelToken && config.cancelToken.subscribe(onCanceled);
	      if (config.signal) {
	        config.signal.aborted ? onCanceled() : config.signal.addEventListener('abort', onCanceled);
	      }
	    }

	    if (!requestData) {
	      requestData = null;
	    }

	    var protocol = parseProtocol(fullPath);

	    if (protocol && [ 'http', 'https', 'file' ].indexOf(protocol) === -1) {
	      reject(new AxiosError_1('Unsupported protocol ' + protocol + ':', AxiosError_1.ERR_BAD_REQUEST, config));
	      return;
	    }


	    // Send the request
	    request.send(requestData);
	  });
	};

	// eslint-disable-next-line strict
	var _null = null;

	var DEFAULT_CONTENT_TYPE = {
	  'Content-Type': 'application/x-www-form-urlencoded'
	};

	function setContentTypeIfUnset(headers, value) {
	  if (!utils.isUndefined(headers) && utils.isUndefined(headers['Content-Type'])) {
	    headers['Content-Type'] = value;
	  }
	}

	function getDefaultAdapter() {
	  var adapter;
	  if (typeof XMLHttpRequest !== 'undefined') {
	    // For browsers use XHR adapter
	    adapter = xhr;
	  } else if (typeof process !== 'undefined' && Object.prototype.toString.call(process) === '[object process]') {
	    // For node use HTTP adapter
	    adapter = xhr;
	  }
	  return adapter;
	}

	function stringifySafely(rawValue, parser, encoder) {
	  if (utils.isString(rawValue)) {
	    try {
	      (parser || JSON.parse)(rawValue);
	      return utils.trim(rawValue);
	    } catch (e) {
	      if (e.name !== 'SyntaxError') {
	        throw e;
	      }
	    }
	  }

	  return (encoder || JSON.stringify)(rawValue);
	}

	var defaults = {

	  transitional: transitional,

	  adapter: getDefaultAdapter(),

	  transformRequest: [function transformRequest(data, headers) {
	    normalizeHeaderName(headers, 'Accept');
	    normalizeHeaderName(headers, 'Content-Type');

	    if (utils.isFormData(data) ||
	      utils.isArrayBuffer(data) ||
	      utils.isBuffer(data) ||
	      utils.isStream(data) ||
	      utils.isFile(data) ||
	      utils.isBlob(data)
	    ) {
	      return data;
	    }
	    if (utils.isArrayBufferView(data)) {
	      return data.buffer;
	    }
	    if (utils.isURLSearchParams(data)) {
	      setContentTypeIfUnset(headers, 'application/x-www-form-urlencoded;charset=utf-8');
	      return data.toString();
	    }

	    var isObjectPayload = utils.isObject(data);
	    var contentType = headers && headers['Content-Type'];

	    var isFileList;

	    if ((isFileList = utils.isFileList(data)) || (isObjectPayload && contentType === 'multipart/form-data')) {
	      var _FormData = this.env && this.env.FormData;
	      return toFormData_1(isFileList ? {'files[]': data} : data, _FormData && new _FormData());
	    } else if (isObjectPayload || contentType === 'application/json') {
	      setContentTypeIfUnset(headers, 'application/json');
	      return stringifySafely(data);
	    }

	    return data;
	  }],

	  transformResponse: [function transformResponse(data) {
	    var transitional = this.transitional || defaults.transitional;
	    var silentJSONParsing = transitional && transitional.silentJSONParsing;
	    var forcedJSONParsing = transitional && transitional.forcedJSONParsing;
	    var strictJSONParsing = !silentJSONParsing && this.responseType === 'json';

	    if (strictJSONParsing || (forcedJSONParsing && utils.isString(data) && data.length)) {
	      try {
	        return JSON.parse(data);
	      } catch (e) {
	        if (strictJSONParsing) {
	          if (e.name === 'SyntaxError') {
	            throw AxiosError_1.from(e, AxiosError_1.ERR_BAD_RESPONSE, this, null, this.response);
	          }
	          throw e;
	        }
	      }
	    }

	    return data;
	  }],

	  /**
	   * A timeout in milliseconds to abort a request. If set to 0 (default) a
	   * timeout is not created.
	   */
	  timeout: 0,

	  xsrfCookieName: 'XSRF-TOKEN',
	  xsrfHeaderName: 'X-XSRF-TOKEN',

	  maxContentLength: -1,
	  maxBodyLength: -1,

	  env: {
	    FormData: _null
	  },

	  validateStatus: function validateStatus(status) {
	    return status >= 200 && status < 300;
	  },

	  headers: {
	    common: {
	      'Accept': 'application/json, text/plain, */*'
	    }
	  }
	};

	utils.forEach(['delete', 'get', 'head'], function forEachMethodNoData(method) {
	  defaults.headers[method] = {};
	});

	utils.forEach(['post', 'put', 'patch'], function forEachMethodWithData(method) {
	  defaults.headers[method] = utils.merge(DEFAULT_CONTENT_TYPE);
	});

	var defaults_1 = defaults;

	/**
	 * Transform the data for a request or a response
	 *
	 * @param {Object|String} data The data to be transformed
	 * @param {Array} headers The headers for the request or response
	 * @param {Array|Function} fns A single function or Array of functions
	 * @returns {*} The resulting transformed data
	 */
	var transformData = function transformData(data, headers, fns) {
	  var context = this || defaults_1;
	  /*eslint no-param-reassign:0*/
	  utils.forEach(fns, function transform(fn) {
	    data = fn.call(context, data, headers);
	  });

	  return data;
	};

	var isCancel = function isCancel(value) {
	  return !!(value && value.__CANCEL__);
	};

	/**
	 * Throws a `CanceledError` if cancellation has been requested.
	 */
	function throwIfCancellationRequested(config) {
	  if (config.cancelToken) {
	    config.cancelToken.throwIfRequested();
	  }

	  if (config.signal && config.signal.aborted) {
	    throw new CanceledError_1();
	  }
	}

	/**
	 * Dispatch a request to the server using the configured adapter.
	 *
	 * @param {object} config The config that is to be used for the request
	 * @returns {Promise} The Promise to be fulfilled
	 */
	var dispatchRequest = function dispatchRequest(config) {
	  throwIfCancellationRequested(config);

	  // Ensure headers exist
	  config.headers = config.headers || {};

	  // Transform request data
	  config.data = transformData.call(
	    config,
	    config.data,
	    config.headers,
	    config.transformRequest
	  );

	  // Flatten headers
	  config.headers = utils.merge(
	    config.headers.common || {},
	    config.headers[config.method] || {},
	    config.headers
	  );

	  utils.forEach(
	    ['delete', 'get', 'head', 'post', 'put', 'patch', 'common'],
	    function cleanHeaderConfig(method) {
	      delete config.headers[method];
	    }
	  );

	  var adapter = config.adapter || defaults_1.adapter;

	  return adapter(config).then(function onAdapterResolution(response) {
	    throwIfCancellationRequested(config);

	    // Transform response data
	    response.data = transformData.call(
	      config,
	      response.data,
	      response.headers,
	      config.transformResponse
	    );

	    return response;
	  }, function onAdapterRejection(reason) {
	    if (!isCancel(reason)) {
	      throwIfCancellationRequested(config);

	      // Transform response data
	      if (reason && reason.response) {
	        reason.response.data = transformData.call(
	          config,
	          reason.response.data,
	          reason.response.headers,
	          config.transformResponse
	        );
	      }
	    }

	    return Promise.reject(reason);
	  });
	};

	/**
	 * Config-specific merge-function which creates a new config-object
	 * by merging two configuration objects together.
	 *
	 * @param {Object} config1
	 * @param {Object} config2
	 * @returns {Object} New object resulting from merging config2 to config1
	 */
	var mergeConfig = function mergeConfig(config1, config2) {
	  // eslint-disable-next-line no-param-reassign
	  config2 = config2 || {};
	  var config = {};

	  function getMergedValue(target, source) {
	    if (utils.isPlainObject(target) && utils.isPlainObject(source)) {
	      return utils.merge(target, source);
	    } else if (utils.isPlainObject(source)) {
	      return utils.merge({}, source);
	    } else if (utils.isArray(source)) {
	      return source.slice();
	    }
	    return source;
	  }

	  // eslint-disable-next-line consistent-return
	  function mergeDeepProperties(prop) {
	    if (!utils.isUndefined(config2[prop])) {
	      return getMergedValue(config1[prop], config2[prop]);
	    } else if (!utils.isUndefined(config1[prop])) {
	      return getMergedValue(undefined, config1[prop]);
	    }
	  }

	  // eslint-disable-next-line consistent-return
	  function valueFromConfig2(prop) {
	    if (!utils.isUndefined(config2[prop])) {
	      return getMergedValue(undefined, config2[prop]);
	    }
	  }

	  // eslint-disable-next-line consistent-return
	  function defaultToConfig2(prop) {
	    if (!utils.isUndefined(config2[prop])) {
	      return getMergedValue(undefined, config2[prop]);
	    } else if (!utils.isUndefined(config1[prop])) {
	      return getMergedValue(undefined, config1[prop]);
	    }
	  }

	  // eslint-disable-next-line consistent-return
	  function mergeDirectKeys(prop) {
	    if (prop in config2) {
	      return getMergedValue(config1[prop], config2[prop]);
	    } else if (prop in config1) {
	      return getMergedValue(undefined, config1[prop]);
	    }
	  }

	  var mergeMap = {
	    'url': valueFromConfig2,
	    'method': valueFromConfig2,
	    'data': valueFromConfig2,
	    'baseURL': defaultToConfig2,
	    'transformRequest': defaultToConfig2,
	    'transformResponse': defaultToConfig2,
	    'paramsSerializer': defaultToConfig2,
	    'timeout': defaultToConfig2,
	    'timeoutMessage': defaultToConfig2,
	    'withCredentials': defaultToConfig2,
	    'adapter': defaultToConfig2,
	    'responseType': defaultToConfig2,
	    'xsrfCookieName': defaultToConfig2,
	    'xsrfHeaderName': defaultToConfig2,
	    'onUploadProgress': defaultToConfig2,
	    'onDownloadProgress': defaultToConfig2,
	    'decompress': defaultToConfig2,
	    'maxContentLength': defaultToConfig2,
	    'maxBodyLength': defaultToConfig2,
	    'beforeRedirect': defaultToConfig2,
	    'transport': defaultToConfig2,
	    'httpAgent': defaultToConfig2,
	    'httpsAgent': defaultToConfig2,
	    'cancelToken': defaultToConfig2,
	    'socketPath': defaultToConfig2,
	    'responseEncoding': defaultToConfig2,
	    'validateStatus': mergeDirectKeys
	  };

	  utils.forEach(Object.keys(config1).concat(Object.keys(config2)), function computeConfigValue(prop) {
	    var merge = mergeMap[prop] || mergeDeepProperties;
	    var configValue = merge(prop);
	    (utils.isUndefined(configValue) && merge !== mergeDirectKeys) || (config[prop] = configValue);
	  });

	  return config;
	};

	var data = {
	  "version": "0.27.2"
	};

	var VERSION = data.version;


	var validators$1 = {};

	// eslint-disable-next-line func-names
	['object', 'boolean', 'number', 'function', 'string', 'symbol'].forEach(function(type, i) {
	  validators$1[type] = function validator(thing) {
	    return typeof thing === type || 'a' + (i < 1 ? 'n ' : ' ') + type;
	  };
	});

	var deprecatedWarnings = {};

	/**
	 * Transitional option validator
	 * @param {function|boolean?} validator - set to false if the transitional option has been removed
	 * @param {string?} version - deprecated version / removed since version
	 * @param {string?} message - some message with additional info
	 * @returns {function}
	 */
	validators$1.transitional = function transitional(validator, version, message) {
	  function formatMessage(opt, desc) {
	    return '[Axios v' + VERSION + '] Transitional option \'' + opt + '\'' + desc + (message ? '. ' + message : '');
	  }

	  // eslint-disable-next-line func-names
	  return function(value, opt, opts) {
	    if (validator === false) {
	      throw new AxiosError_1(
	        formatMessage(opt, ' has been removed' + (version ? ' in ' + version : '')),
	        AxiosError_1.ERR_DEPRECATED
	      );
	    }

	    if (version && !deprecatedWarnings[opt]) {
	      deprecatedWarnings[opt] = true;
	      // eslint-disable-next-line no-console
	      console.warn(
	        formatMessage(
	          opt,
	          ' has been deprecated since v' + version + ' and will be removed in the near future'
	        )
	      );
	    }

	    return validator ? validator(value, opt, opts) : true;
	  };
	};

	/**
	 * Assert object's properties type
	 * @param {object} options
	 * @param {object} schema
	 * @param {boolean?} allowUnknown
	 */

	function assertOptions(options, schema, allowUnknown) {
	  if (typeof options !== 'object') {
	    throw new AxiosError_1('options must be an object', AxiosError_1.ERR_BAD_OPTION_VALUE);
	  }
	  var keys = Object.keys(options);
	  var i = keys.length;
	  while (i-- > 0) {
	    var opt = keys[i];
	    var validator = schema[opt];
	    if (validator) {
	      var value = options[opt];
	      var result = value === undefined || validator(value, opt, options);
	      if (result !== true) {
	        throw new AxiosError_1('option ' + opt + ' must be ' + result, AxiosError_1.ERR_BAD_OPTION_VALUE);
	      }
	      continue;
	    }
	    if (allowUnknown !== true) {
	      throw new AxiosError_1('Unknown option ' + opt, AxiosError_1.ERR_BAD_OPTION);
	    }
	  }
	}

	var validator = {
	  assertOptions: assertOptions,
	  validators: validators$1
	};

	var validators = validator.validators;
	/**
	 * Create a new instance of Axios
	 *
	 * @param {Object} instanceConfig The default config for the instance
	 */
	function Axios(instanceConfig) {
	  this.defaults = instanceConfig;
	  this.interceptors = {
	    request: new InterceptorManager_1(),
	    response: new InterceptorManager_1()
	  };
	}

	/**
	 * Dispatch a request
	 *
	 * @param {Object} config The config specific for this request (merged with this.defaults)
	 */
	Axios.prototype.request = function request(configOrUrl, config) {
	  /*eslint no-param-reassign:0*/
	  // Allow for axios('example/url'[, config]) a la fetch API
	  if (typeof configOrUrl === 'string') {
	    config = config || {};
	    config.url = configOrUrl;
	  } else {
	    config = configOrUrl || {};
	  }

	  config = mergeConfig(this.defaults, config);

	  // Set config.method
	  if (config.method) {
	    config.method = config.method.toLowerCase();
	  } else if (this.defaults.method) {
	    config.method = this.defaults.method.toLowerCase();
	  } else {
	    config.method = 'get';
	  }

	  var transitional = config.transitional;

	  if (transitional !== undefined) {
	    validator.assertOptions(transitional, {
	      silentJSONParsing: validators.transitional(validators.boolean),
	      forcedJSONParsing: validators.transitional(validators.boolean),
	      clarifyTimeoutError: validators.transitional(validators.boolean)
	    }, false);
	  }

	  // filter out skipped interceptors
	  var requestInterceptorChain = [];
	  var synchronousRequestInterceptors = true;
	  this.interceptors.request.forEach(function unshiftRequestInterceptors(interceptor) {
	    if (typeof interceptor.runWhen === 'function' && interceptor.runWhen(config) === false) {
	      return;
	    }

	    synchronousRequestInterceptors = synchronousRequestInterceptors && interceptor.synchronous;

	    requestInterceptorChain.unshift(interceptor.fulfilled, interceptor.rejected);
	  });

	  var responseInterceptorChain = [];
	  this.interceptors.response.forEach(function pushResponseInterceptors(interceptor) {
	    responseInterceptorChain.push(interceptor.fulfilled, interceptor.rejected);
	  });

	  var promise;

	  if (!synchronousRequestInterceptors) {
	    var chain = [dispatchRequest, undefined];

	    Array.prototype.unshift.apply(chain, requestInterceptorChain);
	    chain = chain.concat(responseInterceptorChain);

	    promise = Promise.resolve(config);
	    while (chain.length) {
	      promise = promise.then(chain.shift(), chain.shift());
	    }

	    return promise;
	  }


	  var newConfig = config;
	  while (requestInterceptorChain.length) {
	    var onFulfilled = requestInterceptorChain.shift();
	    var onRejected = requestInterceptorChain.shift();
	    try {
	      newConfig = onFulfilled(newConfig);
	    } catch (error) {
	      onRejected(error);
	      break;
	    }
	  }

	  try {
	    promise = dispatchRequest(newConfig);
	  } catch (error) {
	    return Promise.reject(error);
	  }

	  while (responseInterceptorChain.length) {
	    promise = promise.then(responseInterceptorChain.shift(), responseInterceptorChain.shift());
	  }

	  return promise;
	};

	Axios.prototype.getUri = function getUri(config) {
	  config = mergeConfig(this.defaults, config);
	  var fullPath = buildFullPath(config.baseURL, config.url);
	  return buildURL(fullPath, config.params, config.paramsSerializer);
	};

	// Provide aliases for supported request methods
	utils.forEach(['delete', 'get', 'head', 'options'], function forEachMethodNoData(method) {
	  /*eslint func-names:0*/
	  Axios.prototype[method] = function(url, config) {
	    return this.request(mergeConfig(config || {}, {
	      method: method,
	      url: url,
	      data: (config || {}).data
	    }));
	  };
	});

	utils.forEach(['post', 'put', 'patch'], function forEachMethodWithData(method) {
	  /*eslint func-names:0*/

	  function generateHTTPMethod(isForm) {
	    return function httpMethod(url, data, config) {
	      return this.request(mergeConfig(config || {}, {
	        method: method,
	        headers: isForm ? {
	          'Content-Type': 'multipart/form-data'
	        } : {},
	        url: url,
	        data: data
	      }));
	    };
	  }

	  Axios.prototype[method] = generateHTTPMethod();

	  Axios.prototype[method + 'Form'] = generateHTTPMethod(true);
	});

	var Axios_1 = Axios;

	/**
	 * A `CancelToken` is an object that can be used to request cancellation of an operation.
	 *
	 * @class
	 * @param {Function} executor The executor function.
	 */
	function CancelToken(executor) {
	  if (typeof executor !== 'function') {
	    throw new TypeError('executor must be a function.');
	  }

	  var resolvePromise;

	  this.promise = new Promise(function promiseExecutor(resolve) {
	    resolvePromise = resolve;
	  });

	  var token = this;

	  // eslint-disable-next-line func-names
	  this.promise.then(function(cancel) {
	    if (!token._listeners) return;

	    var i;
	    var l = token._listeners.length;

	    for (i = 0; i < l; i++) {
	      token._listeners[i](cancel);
	    }
	    token._listeners = null;
	  });

	  // eslint-disable-next-line func-names
	  this.promise.then = function(onfulfilled) {
	    var _resolve;
	    // eslint-disable-next-line func-names
	    var promise = new Promise(function(resolve) {
	      token.subscribe(resolve);
	      _resolve = resolve;
	    }).then(onfulfilled);

	    promise.cancel = function reject() {
	      token.unsubscribe(_resolve);
	    };

	    return promise;
	  };

	  executor(function cancel(message) {
	    if (token.reason) {
	      // Cancellation has already been requested
	      return;
	    }

	    token.reason = new CanceledError_1(message);
	    resolvePromise(token.reason);
	  });
	}

	/**
	 * Throws a `CanceledError` if cancellation has been requested.
	 */
	CancelToken.prototype.throwIfRequested = function throwIfRequested() {
	  if (this.reason) {
	    throw this.reason;
	  }
	};

	/**
	 * Subscribe to the cancel signal
	 */

	CancelToken.prototype.subscribe = function subscribe(listener) {
	  if (this.reason) {
	    listener(this.reason);
	    return;
	  }

	  if (this._listeners) {
	    this._listeners.push(listener);
	  } else {
	    this._listeners = [listener];
	  }
	};

	/**
	 * Unsubscribe from the cancel signal
	 */

	CancelToken.prototype.unsubscribe = function unsubscribe(listener) {
	  if (!this._listeners) {
	    return;
	  }
	  var index = this._listeners.indexOf(listener);
	  if (index !== -1) {
	    this._listeners.splice(index, 1);
	  }
	};

	/**
	 * Returns an object that contains a new `CancelToken` and a function that, when called,
	 * cancels the `CancelToken`.
	 */
	CancelToken.source = function source() {
	  var cancel;
	  var token = new CancelToken(function executor(c) {
	    cancel = c;
	  });
	  return {
	    token: token,
	    cancel: cancel
	  };
	};

	var CancelToken_1 = CancelToken;

	/**
	 * Syntactic sugar for invoking a function and expanding an array for arguments.
	 *
	 * Common use case would be to use `Function.prototype.apply`.
	 *
	 *  ```js
	 *  function f(x, y, z) {}
	 *  var args = [1, 2, 3];
	 *  f.apply(null, args);
	 *  ```
	 *
	 * With `spread` this example can be re-written.
	 *
	 *  ```js
	 *  spread(function(x, y, z) {})([1, 2, 3]);
	 *  ```
	 *
	 * @param {Function} callback
	 * @returns {Function}
	 */
	var spread = function spread(callback) {
	  return function wrap(arr) {
	    return callback.apply(null, arr);
	  };
	};

	/**
	 * Determines whether the payload is an error thrown by Axios
	 *
	 * @param {*} payload The value to test
	 * @returns {boolean} True if the payload is an error thrown by Axios, otherwise false
	 */
	var isAxiosError = function isAxiosError(payload) {
	  return utils.isObject(payload) && (payload.isAxiosError === true);
	};

	/**
	 * Create an instance of Axios
	 *
	 * @param {Object} defaultConfig The default config for the instance
	 * @return {Axios} A new instance of Axios
	 */
	function createInstance(defaultConfig) {
	  var context = new Axios_1(defaultConfig);
	  var instance = bind(Axios_1.prototype.request, context);

	  // Copy axios.prototype to instance
	  utils.extend(instance, Axios_1.prototype, context);

	  // Copy context to instance
	  utils.extend(instance, context);

	  // Factory for creating new instances
	  instance.create = function create(instanceConfig) {
	    return createInstance(mergeConfig(defaultConfig, instanceConfig));
	  };

	  return instance;
	}

	// Create the default instance to be exported
	var axios$1 = createInstance(defaults_1);

	// Expose Axios class to allow class inheritance
	axios$1.Axios = Axios_1;

	// Expose Cancel & CancelToken
	axios$1.CanceledError = CanceledError_1;
	axios$1.CancelToken = CancelToken_1;
	axios$1.isCancel = isCancel;
	axios$1.VERSION = data.version;
	axios$1.toFormData = toFormData_1;

	// Expose AxiosError class
	axios$1.AxiosError = AxiosError_1;

	// alias for CanceledError for backward compatibility
	axios$1.Cancel = axios$1.CanceledError;

	// Expose all/spread
	axios$1.all = function all(promises) {
	  return Promise.all(promises);
	};
	axios$1.spread = spread;

	// Expose isAxiosError
	axios$1.isAxiosError = isAxiosError;

	var axios_1 = axios$1;

	// Allow use of default import syntax in TypeScript
	var _default = axios$1;
	axios_1.default = _default;

	var axios = axios_1;

	class RTCEndpoint extends Event$1 {
	  constructor(options) {
	    super('RTCPusherPlayer');
	    this.TAG = '[RTCPusherPlayer]';
	    let defaults = {
	      element: '',
	      // html video element
	      debug: false,
	      // if output debug log
	      zlmsdpUrl: '',
	      simulcast: false,
	      useCamera: true,
	      audioEnable: true,
	      videoEnable: true,
	      recvOnly: false,
	      resolution: {
	        w: 0,
	        h: 0
	      },
	      usedatachannel: false
	    };
	    this.options = Object.assign({}, defaults, options);
	    if (this.options.debug) {
	      setLogger();
	    }
	    this.e = {
	      onicecandidate: this._onIceCandidate.bind(this),
	      ontrack: this._onTrack.bind(this),
	      onicecandidateerror: this._onIceCandidateError.bind(this),
	      onconnectionstatechange: this._onconnectionstatechange.bind(this),
	      ondatachannelopen: this._onDataChannelOpen.bind(this),
	      ondatachannelmsg: this._onDataChannelMsg.bind(this),
	      ondatachannelerr: this._onDataChannelErr.bind(this),
	      ondatachannelclose: this._onDataChannelClose.bind(this)
	    };
	    this._remoteStream = null;
	    this._localStream = null;
	    this._tracks = [];
	    this.pc = new RTCPeerConnection(null);
	    this.pc.onicecandidate = this.e.onicecandidate;
	    this.pc.onicecandidateerror = this.e.onicecandidateerror;
	    this.pc.ontrack = this.e.ontrack;
	    this.pc.onconnectionstatechange = this.e.onconnectionstatechange;
	    this.datachannel = null;
	    if (this.options.usedatachannel) {
	      this.datachannel = this.pc.createDataChannel('chat');
	      this.datachannel.onclose = this.e.ondatachannelclose;
	      this.datachannel.onerror = this.e.ondatachannelerr;
	      this.datachannel.onmessage = this.e.ondatachannelmsg;
	      this.datachannel.onopen = this.e.ondatachannelopen;
	    }
	    if (!this.options.recvOnly && (this.options.audioEnable || this.options.videoEnable)) this.start();else this.receive();
	  }
	  receive() {

	    //debug.error(this.TAG,'this not implement');
	    const AudioTransceiverInit = {
	      direction: 'recvonly',
	      sendEncodings: []
	    };
	    const VideoTransceiverInit = {
	      direction: 'recvonly',
	      sendEncodings: []
	    };
	    if (this.options.videoEnable) {
	      this.pc.addTransceiver('video', VideoTransceiverInit);
	    }
	    if (this.options.audioEnable) {
	      this.pc.addTransceiver('audio', AudioTransceiverInit);
	    }
	    this.pc.createOffer().then(desc => {
	      log(this.TAG, 'offer:', desc.sdp);
	      this.pc.setLocalDescription(desc).then(() => {
	        axios({
	          method: 'post',
	          url: this.options.zlmsdpUrl,
	          responseType: 'json',
	          data: desc.sdp,
	          headers: {
	            'Content-Type': 'text/plain;charset=utf-8'
	          }
	        }).then(response => {
	          let ret = response.data; //JSON.parse(response.data);
	          if (ret.code != 0) {
	            // mean failed for offer/anwser exchange 
	            this.dispatch(Events$1.WEBRTC_OFFER_ANWSER_EXCHANGE_FAILED, ret);
	            return;
	          }
	          let anwser = {};
	          anwser.sdp = ret.sdp;
	          anwser.type = 'answer';
	          log(this.TAG, 'answer:', ret.sdp);
	          this.pc.setRemoteDescription(anwser).then(() => {
	            log(this.TAG, 'set remote sucess');
	          }).catch(e => {
	            error(this.TAG, e);
	          });
	        });
	      });
	    }).catch(e => {
	      error(this.TAG, e);
	    });
	  }
	  start() {
	    let videoConstraints = false;
	    let audioConstraints = false;
	    if (this.options.useCamera) {
	      if (this.options.videoEnable) videoConstraints = new VideoTrackConstraints(VideoSourceInfo.CAMERA);
	      if (this.options.audioEnable) audioConstraints = new AudioTrackConstraints(AudioSourceInfo.MIC);
	    } else {
	      if (this.options.videoEnable) {
	        videoConstraints = new VideoTrackConstraints(VideoSourceInfo.SCREENCAST);
	        if (this.options.audioEnable) audioConstraints = new AudioTrackConstraints(AudioSourceInfo.SCREENCAST);
	      } else {
	        if (this.options.audioEnable) audioConstraints = new AudioTrackConstraints(AudioSourceInfo.MIC);else {
	          // error shared display media not only audio
	          error(this.TAG, 'error paramter');
	        }
	      }
	    }
	    if (this.options.resolution.w != 0 && this.options.resolution.h != 0 && typeof videoConstraints == 'object') {
	      videoConstraints.resolution = new Resolution(this.options.resolution.w, this.options.resolution.h);
	    }
	    MediaStreamFactory.createMediaStream(new StreamConstraints(audioConstraints, videoConstraints)).then(stream => {
	      this._localStream = stream;
	      this.dispatch(Events$1.WEBRTC_ON_LOCAL_STREAM, stream);
	      const AudioTransceiverInit = {
	        direction: 'sendrecv',
	        sendEncodings: []
	      };
	      const VideoTransceiverInit = {
	        direction: 'sendrecv',
	        sendEncodings: []
	      };
	      if (this.options.simulcast && stream.getVideoTracks().length > 0) {
	        VideoTransceiverInit.sendEncodings = [{
	          rid: 'h',
	          active: true,
	          maxBitrate: 1000000
	        }, {
	          rid: 'm',
	          active: true,
	          maxBitrate: 500000,
	          scaleResolutionDownBy: 2
	        }, {
	          rid: 'l',
	          active: true,
	          maxBitrate: 200000,
	          scaleResolutionDownBy: 4
	        }];
	      }
	      if (this.options.audioEnable) {
	        if (stream.getAudioTracks().length > 0) {
	          this.pc.addTransceiver(stream.getAudioTracks()[0], AudioTransceiverInit);
	        } else {
	          AudioTransceiverInit.direction = 'recvonly';
	          this.pc.addTransceiver('audio', AudioTransceiverInit);
	        }
	      }
	      if (this.options.videoEnable) {
	        if (stream.getVideoTracks().length > 0) {
	          this.pc.addTransceiver(stream.getVideoTracks()[0], VideoTransceiverInit);
	        } else {
	          VideoTransceiverInit.direction = 'recvonly';
	          this.pc.addTransceiver('video', VideoTransceiverInit);
	        }
	      }

	      /*
	      stream.getTracks().forEach((track,idx)=>{
	          debug.log(this.TAG,track);
	          this.pc.addTrack(track);
	      });
	      */
	      this.pc.createOffer().then(desc => {
	        log(this.TAG, 'offer:', desc.sdp);
	        this.pc.setLocalDescription(desc).then(() => {
	          axios({
	            method: 'post',
	            url: this.options.zlmsdpUrl,
	            responseType: 'json',
	            data: desc.sdp,
	            headers: {
	              'Content-Type': 'text/plain;charset=utf-8'
	            }
	          }).then(response => {
	            let ret = response.data; //JSON.parse(response.data);
	            if (ret.code != 0) {
	              // mean failed for offer/anwser exchange 
	              this.dispatch(Events$1.WEBRTC_OFFER_ANWSER_EXCHANGE_FAILED, ret);
	              return;
	            }
	            let anwser = {};
	            anwser.sdp = ret.sdp;
	            anwser.type = 'answer';
	            log(this.TAG, 'answer:', ret.sdp);
	            this.pc.setRemoteDescription(anwser).then(() => {
	              log(this.TAG, 'set remote sucess');
	            }).catch(e => {
	              error(this.TAG, e);
	            });
	          });
	        });
	      }).catch(e => {
	        error(this.TAG, e);
	      });
	    }).catch(e => {
	      this.dispatch(Events$1.CAPTURE_STREAM_FAILED);
	      //debug.error(this.TAG,e);
	    });

	    //const offerOptions = {};
	    /*
	    if (typeof this.pc.addTransceiver === 'function') {
	        // |direction| seems not working on Safari.
	        this.pc.addTransceiver('audio', { direction: 'recvonly' });
	        this.pc.addTransceiver('video', { direction: 'recvonly' });
	    } else {
	        offerOptions.offerToReceiveAudio = true;
	        offerOptions.offerToReceiveVideo = true;
	    }
	    */
	  }

	  _onIceCandidate(event) {
	    if (event.candidate) {
	      log(this.TAG, 'Remote ICE candidate: \n ' + event.candidate.candidate);
	      // Send the candidate to the remote peer
	    }
	  }
	  _onTrack(event) {
	    this._tracks.push(event.track);
	    if (this.options.element && event.streams && event.streams.length > 0) {
	      this.options.element.srcObject = event.streams[0];
	      this._remoteStream = event.streams[0];
	      this.dispatch(Events$1.WEBRTC_ON_REMOTE_STREAMS, event);
	    } else {
	      if (this.pc.getReceivers().length == this._tracks.length) {
	        log(this.TAG, 'play remote stream ');
	        this._remoteStream = new MediaStream(this._tracks);
	        this.options.element.srcObject = this._remoteStream;
	      } else {
	        error(this.TAG, 'wait stream track finish');
	      }
	    }
	  }
	  _onIceCandidateError(event) {
	    this.dispatch(Events$1.WEBRTC_ICE_CANDIDATE_ERROR, event);
	  }
	  _onconnectionstatechange(event) {
	    this.dispatch(Events$1.WEBRTC_ON_CONNECTION_STATE_CHANGE, this.pc.connectionState);
	  }
	  _onDataChannelOpen(event) {
	    log(this.TAG, 'ondatachannel open:', event);
	    this.dispatch(Events$1.WEBRTC_ON_DATA_CHANNEL_OPEN, event);
	  }
	  _onDataChannelMsg(event) {
	    log(this.TAG, 'ondatachannel msg:', event);
	    this.dispatch(Events$1.WEBRTC_ON_DATA_CHANNEL_MSG, event);
	  }
	  _onDataChannelErr(event) {
	    log(this.TAG, 'ondatachannel err:', event);
	    this.dispatch(Events$1.WEBRTC_ON_DATA_CHANNEL_ERR, event);
	  }
	  _onDataChannelClose(event) {
	    log(this.TAG, 'ondatachannel close:', event);
	    this.dispatch(Events$1.WEBRTC_ON_DATA_CHANNEL_CLOSE, event);
	  }
	  sendMsg(data) {
	    if (this.datachannel != null) {
	      this.datachannel.send(data);
	    } else {
	      error(this.TAG, 'data channel is null');
	    }
	  }
	  closeDataChannel() {
	    if (this.datachannel) {
	      this.datachannel.close();
	      this.datachannel = null;
	    }
	  }
	  close() {
	    this.closeDataChannel();
	    if (this.pc) {
	      this.pc.close();
	      this.pc = null;
	    }
	    if (this.options) {
	      this.options = null;
	    }
	    if (this._localStream) {
	      this._localStream.getTracks().forEach((track, idx) => {
	        track.stop();
	      });
	    }
	    if (this._remoteStream) {
	      this._remoteStream.getTracks().forEach((track, idx) => {
	        track.stop();
	      });
	    }
	    this._tracks.forEach((track, idx) => {
	      track.stop();
	    });
	    this._tracks = [];
	  }
	  get remoteStream() {
	    return this._remoteStream;
	  }
	  get localStream() {
	    return this._localStream;
	  }
	}

	const quickScan = [{
	  'label': '4K(UHD)',
	  'width': 3840,
	  'height': 2160
	}, {
	  'label': '1080p(FHD)',
	  'width': 1920,
	  'height': 1080
	}, {
	  'label': 'UXGA',
	  'width': 1600,
	  'height': 1200,
	  'ratio': '4:3'
	}, {
	  'label': '720p(HD)',
	  'width': 1280,
	  'height': 720
	}, {
	  'label': 'SVGA',
	  'width': 800,
	  'height': 600
	}, {
	  'label': 'VGA',
	  'width': 640,
	  'height': 480
	}, {
	  'label': '360p(nHD)',
	  'width': 640,
	  'height': 360
	}, {
	  'label': 'CIF',
	  'width': 352,
	  'height': 288
	}, {
	  'label': 'QVGA',
	  'width': 320,
	  'height': 240
	}, {
	  'label': 'QCIF',
	  'width': 176,
	  'height': 144
	}, {
	  'label': 'QQVGA',
	  'width': 160,
	  'height': 120
	}];
	function GetSupportCameraResolutions$1() {
	  return new Promise(function (resolve, reject) {
	    let resolutions = [];
	    let ok = 0;
	    let err = 0;
	    for (let i = 0; i < quickScan.length; ++i) {
	      let videoConstraints = new VideoTrackConstraints(VideoSourceInfo.CAMERA);
	      videoConstraints.resolution = new Resolution(quickScan[i].width, quickScan[i].height);
	      MediaStreamFactory.createMediaStream(new StreamConstraints(false, videoConstraints)).then(stream => {
	        resolutions.push(quickScan[i]);
	        ok++;
	        if (ok + err == quickScan.length) {
	          resolve(resolutions);
	        }
	      }).catch(e => {
	        err++;
	        if (ok + err == quickScan.length) {
	          resolve(resolutions);
	        }
	      });
	    }
	  });
	}
	function GetAllScanResolution$1() {
	  return quickScan;
	}
	function isSupportResolution$1(w, h) {
	  return new Promise(function (resolve, reject) {
	    let videoConstraints = new VideoTrackConstraints(VideoSourceInfo.CAMERA);
	    videoConstraints.resolution = new Resolution(w, h);
	    MediaStreamFactory.createMediaStream(new StreamConstraints(false, videoConstraints)).then(stream => {
	      resolve();
	    }).catch(e => {
	      reject(e);
	    });
	  });
	}

	console.log('build date:', BUILD_DATE);
	console.log('version:', VERSION$1);
	const Events = Events$1;
	const Media = media;
	const Endpoint = RTCEndpoint;
	const GetSupportCameraResolutions = GetSupportCameraResolutions$1;
	const GetAllScanResolution = GetAllScanResolution$1;
	const isSupportResolution = isSupportResolution$1;

	exports.Endpoint = Endpoint;
	exports.Events = Events;
	exports.GetAllScanResolution = GetAllScanResolution;
	exports.GetSupportCameraResolutions = GetSupportCameraResolutions;
	exports.Media = Media;
	exports.isSupportResolution = isSupportResolution;

	Object.defineProperty(exports, '__esModule', { value: true });

	return exports;

})({});
//# sourceMappingURL=ZLMRTCClient.js.map
