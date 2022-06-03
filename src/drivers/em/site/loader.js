/**
 * @license
 * 
 * Copyright (C) 2015  Valtteri "tsone" Heikkila
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// NOTE: Originally from: http://jsfiddle.net/vWx8V/
var KEY_CODE_TO_NAME = {8:"Backspace",9:"Tab",13:"Return",16:"Shift",17:"Ctrl",18:"Alt",19:"Pause/Break",20:"Caps Lock",27:"Esc",32:"Space",33:"Page Up",34:"Page Down",35:"End",36:"Home",37:"Left",38:"Up",39:"Right",40:"Down",45:"Insert",46:"Delete",48:"0",49:"1",50:"2",51:"3",52:"4",53:"5",54:"6",55:"7",56:"8",57:"9",65:"A",66:"B",67:"C",68:"D",69:"E",70:"F",71:"G",72:"H",73:"I",74:"J",75:"K",76:"L",77:"M",78:"N",79:"O",80:"P",81:"Q",82:"R",83:"S",84:"T",85:"U",86:"V",87:"W",88:"X",89:"Y",90:"Z",91:"Meta",93:"Right Click",96:"Numpad 0",97:"Numpad 1",98:"Numpad 2",99:"Numpad 3",100:"Numpad 4",101:"Numpad 5",102:"Numpad 6",103:"Numpad 7",104:"Numpad 8",105:"Numpad 9",106:"Numpad *",107:"Numpad +",109:"Numpad -",110:"Numpad .",111:"Numpad /",112:"F1",113:"F2",114:"F3",115:"F4",116:"F5",117:"F6",118:"F7",119:"F8",120:"F9",121:"F10",122:"F11",123:"F12",144:"Num Lock",145:"Scroll Lock",182:"My Computer",183:"My Calculator",186:";",187:"=",188:",",189:"-",190:".",191:"/",192:"`",219:"[",220:"\\",221:"]",222:"'"};

var hack_first_input_time = null;
var persistent_savegames = false;

var FCEM = {
soundEnabled : true,
showControls : (function(show) {
	var el = document.getElementById('controllersToggle');
	return function(show) {
		el.checked = (show === undefined) ? !el.checked : show;
	};
})(),
locStorage : (function() {
	var bestStorage = {
		getItem: function(item_name) {
			if (typeof this[item_name] == 'undefined') {
				return null;
			}
			return this[item_name];
		},
	};
	try {
		let test = localStorage.getItem('ctrlInit'); // This line with throw in Chrome incognito (maybe only when in an i-frame)
		bestStorage = localStorage;
	}catch(error) {
		console.log('local storage unavailable, use dummy storage');
	}
	return bestStorage;
})(),
toggleSound : (function() {
	var el = document.getElementById('soundIcon');
	return function() {
		FCEM.soundEnabled = !FCEM.soundEnabled;
		FCEM.silenceSound(!FCEM.soundEnabled);
		el.style.backgroundPosition = (FCEM.soundEnabled ? '-32' : '-80') + 'px -48px';
	};
})(),
  onInitialSyncFromIDB : function(er) {
    assert(!er);
    try {
      FS.mkdir('/fceux/sav');
    } catch (e) {
    }
    try {
      FS.mkdir('/fceux/rom');
    } catch (e) {
    }

    // var savs = FS.readdir('/fceux/sav');
    // savs.forEach(function(x) { console.log('!!!! sav: ' + x); });
    // Write savegame and synchronize IDBFS in intervals.
    if (persistent_savegames) {
      setInterval(Module.cwrap('FCEM_OnSaveGameInterval'), 1000);
    }
  },
  onDeleteGameSyncFromIDB : function(er) {
    assert(!er);
  },
  onSyncToIDB : function(er) {
    assert(!er);
  },
  onDOMLoaded : function() {
    FCEM.showControls(false);
  },
  createAudioContext : function() {
    if (typeof FCEM.audioContext === 'undefined' || FCEM.audioContext.state !== 'running') {
      if (typeof AudioContext !== 'undefined') {
        FCEM.audioContext = new AudioContext();
      } else if (typeof webkitAudioContext !== 'undefined') {
        FCEM.audioContext = new webkitAudioContext();
      }
      FCEM.audioContext.resume();
    }
  },
  startGame : function(path) {
    FCEM.createAudioContext();
    Module.romName = path;
    Module.romReload = 1;
  },
  _getLocalInputDefault : function(id, type) {
    var m = (type ? 'gp' : 'input') + id;
    if (FCEM.locStorage[m] === undefined) {
      if (FCEC.inputs[id] === undefined) {
        FCEM.locStorage[m] = '0'; // NOTE: fallback if the id is undefined
      } else {
        FCEM.locStorage[m] = FCEC.inputs[id][type];
      }
    }
    return parseInt(FCEM.locStorage[m]);
  },
  setLocalKey : function(id, key) {
    FCEM.locStorage['input' + id] = key;
  },
  getLocalKey : function(id) {
    return FCEM._getLocalInputDefault(id, 0);
  },
  setLocalGamepad : function(id, binding) {
    FCEM.locStorage['gp' + id] = binding;
  },
  getLocalGamepad : function(id) {
    return FCEM._getLocalInputDefault(id, 1);
  },
  clearInputBindings : function() {
    for (var id in FCEC.inputs) {
      // clear local bindings
      delete FCEM.locStorage['input' + id];
      delete FCEM.locStorage['gp' + id];
      // clear host bindings
      var key = FCEM.getLocalKey(id);
      FCEM.bindKey(0, key);
      FCEM.bindGamepad(id, 0);
    }
  },
  syncInputBindings : function() {
    for (var id in FCEC.inputs) {
      var key = FCEM.getLocalKey(id);
      FCEM.bindKey(id, key);
      var binding = FCEM.getLocalGamepad(id);
      FCEM.bindGamepad(id, binding);
    }
  },
  initInputBindings : function() {
    FCEM.syncInputBindings();
    FCEM.initKeyBind();
  },
  key2Name : function (key) {
    var keyName = (key & 0x0FF) ? KEY_CODE_TO_NAME[key & 0x0FF] : '(Unset)';
    if (keyName === undefined) keyName = '(Unknown)';
    var prefix = '';
    if (key & 0x100 && keyName !== 'Ctrl')  prefix += 'Ctrl+';
    if (key & 0x400 && keyName !== 'Alt')   prefix += 'Alt+';
    if (key & 0x200 && keyName !== 'Shift') prefix += 'Shift+';
    if (key & 0x800 && keyName !== 'Meta')  prefix += 'Meta+';
    return prefix + keyName;
  },
  gamepad2Name : function (binding) {
    var type = binding & 0x03;
    var pad = (binding & 0x0C) >> 2;
    var idx = (binding & 0xF0) >> 4;
    if (!type) return '(Unset)';
    var typeNames = [ 'Button', '-Axis', '+Axis' ];
    return 'Gamepad ' + pad + ' ' + typeNames[type-1] + ' ' + idx;
  },
  initKeyBind : function() {
    var table = document.getElementById("keyBindTable");
    var proto = document.getElementById("keyBindProto");

    while (table.lastChild != table.firstChild) {
      table.removeChild(table.lastChild);
    }

    for (id in FCEC.inputs) {
      var item = FCEC.inputs[id];
      var key = FCEM.getLocalKey(id);
      var gamepad = FCEM.getLocalGamepad(id);
      var keyName = FCEM.key2Name(key);
      var gamepadName = FCEM.gamepad2Name(gamepad);

      var el = proto.cloneNode(true);
      el.children[0].innerHTML = item[2];
      el.children[1].innerHTML = keyName;
      el.children[2].innerHTML = gamepadName;
      el.children[3].dataset.id = id;
      el.children[3].dataset.name = item[2];

      table.appendChild(el);
    }
  },
  clearBinding : function(keyBind) {
    var id = keyBind.dataset.id;
    var key = FCEM.getLocalKey(id);
    FCEM.bindKey(0, key);
    FCEM.setLocalKey(id, 0);
    FCEM.bindGamepad(id, 0);
    FCEM.setLocalGamepad(id, 0);
    FCEM.initKeyBind();
  },
  resetDefaultBindings : function() {
    FCEM.clearInputBindings();
    FCEM.initInputBindings();
  },
  setLocalController : function(id, val) {
    FCEM.locStorage['ctrl' + id] = Number(val);
    FCEM.setController(id, val);
  },
  initControllers : function(force) {
    if (force || !FCEM.locStorage.getItem('ctrlInit')) {
      for (var id in FCEC.controllers) {
        FCEM.locStorage['ctrl' + id] = FCEC.controllers[id][0];
      }
      FCEM.locStorage['ctrlInit'] = 'true';
    }
    for (var id in FCEC.controllers) {
      var val = Number(FCEM.locStorage['ctrl' + id]);
      FCEM.setController(id, val);
      FCEV.setControllerEl(id, val);
    }
  },

	// Udp socket API for C++ rainbow mapper
	udpChannel : null,
	udpInputBuffer : [],
	udpOutputBuffer : null, // null: no possible buffering, array: each element will be sent on next occasion

	createUdpSocket : function(address, port) {
		FCEM.udpOutputBuffer = [];

		wtu.get_channel(
			{'address': address, 'port': 3003, 'ssl': true}, // Relay server, port is hardcoded
			{'address': '127.0.0.1', 'port': port}           // Game server, expect it to be cohosted with the relay
		)
		.then(function(chan) {
			chan.binaryType = 'arraybuffer'; // Firefox implements the spec which says "blob" by default. Others are rebels. Let's get the same behaviour everywhere.
			FCEM.udpChannel = chan;

			chan.onmessage = function(e) {
				FCEM.udpInputBuffer.push(e.data);
			};
			chan.onclose = function() {
				FCEM.udpChannel = null;
				udpInputBuffer = [];
			};

			for (let i = 0; i < FCEM.udpOutputBuffer.length; ++i) {
				console.log('send deleayed packet');
				FCEM.udpChannel.send(FCEM.udpOutputBuffer[i]);
			}
			FCEM.udpOutputBuffer = null;
		});
		return 1;
	},

	closeUdpSocket : function(socket) {
		if (socket === 1 && FCEM.udpChannel !== null) {
			FCEM.udpChannel.close();
		}
	},

	udpSend : function(socket, payload) {
		if (socket === 1) {
			if (FCEM.udpChannel !== null) {
				FCEM.udpChannel.send(payload);
			}else if (FCEM.udpOutputBuffer !== null) {
				console.log('udpSend: delay packet');
				// Avoid buffering more than one packet, it would be calling for trouble with the client assuming packet has been lost
				FCEM.udpOutputBuffer = [payload];
			}
		}
	},

	udpReceive : function(socket) {
		if (socket !== 1 || FCEM.udpInputBuffer.length === 0) {
			return null;
		}
		return FCEM.udpInputBuffer.splice(0, 1)[0];
	},

	// Ping API for C++ rainbow mapper
	pingResult : null,
	pingSeries : 0,
	pingRunning : false,

	pingServer : function(address, port, number_of_pings) {
		if (FCEM.pingRunning) {
			return;
		}

		FCEM.pingRunning = true;
		FCEM.pingSeries = (FCEM.pingSeries + 1) % 0x100;
		FCEM.pingResult = [];
		for (let i = 0; i < number_of_pings; ++i) {
			FCEM.pingResult.push(null);
		}

		++number_of_pings; //HACK onemoreping: do one more ping because, apparently, RTT of the first message is longer in WebRTC

		wtu.get_channel(
			{'address': address, 'port': 3003, 'ssl': true}, // Relay server, port is hardcoded
			{'address': '127.0.0.1', 'port': port},          // Game server, expect it to be cohosted with the relay
		)
		.then(function(chan) {
			chan.binaryType = 'arraybuffer'; // Firefox implements the spec which says "blob" by default. Others are rebels. Let's get the same behaviour everywhere.

			// Register pong reception routine
			chan.onmessage = function(e) {
				const message_type_pong = 5;
				let v = new DataView(e.data);
				if (v.getUint8(0) == message_type_pong) {
					let receive_time = Date.now() % 0x100000000;
					let series = v.getUint8(1);
					let number = v.getUint8(2);
					let send_time = v.getUint32(3, true);
					--number; if (number < 0) {return;} //HACK onemoreping: ignore extra ping
					if (series === FCEM.pingSeries && number < FCEM.pingResult.length) {
						console.log("ping["+number+"]: "+ (receive_time - send_time) +"ms");
						FCEM.pingResult[number] = receive_time - send_time;
					}else console.error('ignored pong: bad data');
				}else console.error('ignored pong: bad type #'+ v.getUint8(0));
			};

			// Send a ping per second
			let send_clock = 0;
			let current_number = 0;
			for (let i = 0; i < number_of_pings; ++i) {
				setTimeout(function() {
					let payload = new ArrayBuffer(10);
					let v = new DataView(payload);
					v.setUint8(0, 2); // message_type
					v.setUint8(1, FCEM.pingSeries);
					v.setUint8(2, current_number);
					v.setUint32(3, Date.now() % 0x100000000, true);
					v.setUint8(7, 0); //
					v.setUint8(8, 1); // Filler
					v.setUint8(9, 2); //

					chan.send(payload);
					++current_number;
				}, send_clock);
				send_clock += 1000;
			}

			//HACK Ensure at least 20 seconds passed since first input (to have a clean perf report, with audio enabled all the time)
			if (hack_first_input_time !== null) {
				hack_delay = hack_first_input_time + 20000 - Date.now();
				if (hack_delay > send_clock) {
					console.log("delaying ping result by "+ (hack_delay-send_clock) +" ms to check browser perf");
					send_clock = hack_delay;
				}
			}

			// One second after the last ping, finalize the operation
			setTimeout(function() {
				chan.close();
				FCEM.pingRunning = false;
			}, send_clock);
		});
	},

	popPingResult : function() {
		// Return nothing if there is nothing to return
		if (FCEM.pingRunning || FCEM.pingResult === null) {
			return null;
		}

		// Warn or block online if emulator performances are too bad
		let perf_estimate = FCEM.perfEstimation();
		if (perf_estimate['summary'] === 'bad') {
			console.warn('Performance warning: '+ JSON.stringify(perf_estimate));
			GuiMessage.warn('Performance', 'Your browser seems to have difficulties running the game at real time.<br />If online is laggy, try the native emulator.');
		}else if (perf_estimate['summary'] === 'very-bad') {
			console.warn('Performance error: '+ JSON.stringify(perf_estimate));
			GuiMessage.error('Performance', 'Your browser fails to emulate the game at real time.<br />You cannot play online with it, it would be too laggy.');

			// Soft block access to online by never returning ping result
			FCEM.pingResult = null;
			return null;
		}else {
			console.log('Performance: '+ JSON.stringify(perf_estimate));
		}

		// Return ping result's summary
		let result = {
			min: 999999,
			max: 0,
			total: 0,
			lost: 0,
			number: FCEM.pingResult.length,
		};
		for (let i = 0; i < FCEM.pingResult.length; ++i) {
			let val = FCEM.pingResult[i];
			if (val === null) {
				++result.lost;
			}else {
				if (val < result.min) result.min = val;
				if (val > result.max) result.max = val;
				result.total += val;
			}
		}

		FCEM.pingResult = null;
		return result;
	},

	// Debug API
	timingInfo : function() {
		var cpp_timing_table_addr = FCEM.getTimingInfo();
		var info_names = [
			'system_fps', 'emulated_fps',
			'ideal_frame_cnt', 'frame_cnt',
			'emulated_min', 'emulated_max', 'emulated_avg', 'nb_skips'
		];
		var result = {};
		for (var info_idx = 0; info_idx < info_names.length; ++info_idx) {
			result[info_names[info_idx]] = getValue(cpp_timing_table_addr + 4*info_idx, 'i32');
		}
		return result;
	},
	perfEstimation: function() {
		var tinfo = FCEM.timingInfo();

		if (
			// Emulated system is not 50 FPS (may be NTSC, or a bug)
			Math.round(tinfo['system_fps'] / 1000) != 50 ||

			// Difference between theroetical and observed framerate is too high
			Math.abs(1 - (tinfo['ideal_frame_cnt'] / tinfo['frame_cnt'])) > .02
		)
		{
			return {summary: 'very-bad', detail: tinfo};
		}

		// Emulator sometimes has to burst big batch of frames to catchup occasional sluggyness
		if (tinfo['emulated_max'] > 6) {
			return {summary: 'bad', detail: tinfo};
		}

		return {summary: 'good', detail: tinfo};
	},
};

var FCEV = {
catchEnabled : false,
catchId : null,
catchKey : null,
catchGamepad : null,
keyBindToggle : (function() {
	var el = document.getElementById("keyBindDiv");
	return function() {
		el.style.display = (el.style.display == 'block') ? 'none' : 'block';
	};
})(),
catchStart : (function(keyBind) {
	var nameEl = document.getElementById("catchName");
	var keyEl = document.getElementById("catchKey");
	var gamepadEl = document.getElementById("catchGamepad");
	var catchDivEl = document.getElementById("catchDiv");
	return function(keyBind) {
		var id = keyBind.dataset.id;
		FCEV.catchId = id;

		nameEl.innerHTML = keyBind.dataset.name;
		var key = FCEM.getLocalKey(id);
		FCEV.catchKey = key;
		keyEl.innerHTML = FCEM.key2Name(key);

		var binding = FCEM.getLocalGamepad(id);
		FCEV.catchGamepad = binding;
		gamepadEl.innerHTML = FCEM.gamepad2Name(binding);

		catchDivEl.style.display = 'block';

		FCEV.catchEnabled = true;
	};
})(),
catchEnd : (function(save) {
	var catchDivEl = document.getElementById("catchDiv");
	return function(save) {
		FCEV.catchEnabled = false;

		if (save && FCEV.catchId) {
		  // Check/overwrite duplicates
		  for (var id in FCEC.inputs) {

                    // Skip current binding
		    if (FCEV.catchId == id) {
                      continue;
                    }

                    // Check duplicate key binding
		    var key = FCEM.getLocalKey(id);
		    if (key && FCEV.catchKey == key) {
		      if (!confirm('Key ' + FCEM.key2Name(key) + ' already bound as ' + FCEC.inputs[id][2] + '. Clear the previous binding?')) {
		        FCEV.catchEnabled = true; // Re-enable key catching
		        return;
		      }
		      FCEM.setLocalKey(id, 0);
		      FCEM.bindKey(0, key);
		    }

                    // Check duplicate gamepad binding
		    var binding = FCEM.getLocalGamepad(id);
		    if (binding && FCEV.catchGamepad == binding) {
		      if (!confirm(FCEM.gamepad2Name(binding) + ' already bound as ' + FCEC.inputs[id][2] + '. Clear the previous binding?')) {
		        FCEV.catchEnabled = true; // Re-enable key catching
		        return;
		      }
		      FCEM.setLocalGamepad(id, 0);
		      FCEM.bindGamepad(id, 0);
		    }
		  }

                  // Clear old key binding
		  var oldKey = FCEM.getLocalKey(FCEV.catchId);
		  FCEM.bindKey(0, oldKey);
		  // Set new bindings
		  FCEM.setLocalKey(FCEV.catchId, FCEV.catchKey);
		  FCEM.bindKey(FCEV.catchId, FCEV.catchKey);
		  FCEM.setLocalGamepad(FCEV.catchId, FCEV.catchGamepad);
		  FCEM.bindGamepad(FCEV.catchId, FCEV.catchGamepad);

		  FCEV.catchId = null;
		  FCEM.initKeyBind();
		}

		catchDivEl.style.display = 'none';
	};
})(),
setControllerEl : function(id, val) {
	var el = document.getElementById(FCEC.controllers[id][1]);
	if (!el) {
		return;
	}
	if (el.tagName == 'SELECT' || el.type == 'range') {
		el.value = val;
	} else if (el.type == 'checkbox') {
		el.checked = val;
	}
},
setProgress : (function(x) {
	var el = document.getElementById('progress');
	return function(x) {
		x = (x > 1) ? 1 : ((x < 0) ? 0 : x);
		el.style.width = 3 * ((42*x) |0) + 'px';
	};
})(),
scanForGamepadBinding : function() {
  if (navigator && navigator.getGamepads) {
    var gamepads = navigator.getGamepads();
    // Scan through gamepads.
    var i = gamepads.length - 1;
    if (i > 3) i = 3; // Max 4 gamepads.
    for (; i >= 0; --i) {
      var p = gamepads[i];
      if (p && p.connected) {
        // Scan for button.
        var j = p.buttons.length - 1;
        if (j > 15) j = 15; // Max 16 buttons.
        for (; j >= 0; --j) {
          var button = p.buttons[j];
          if (button.pressed || (button.value >= 0.1)) {
            return (j << 4) | (i << 2) | 1; // Produce button binding.
          }
        }
        // Scan for axis.
        var j = p.axes.length - 1;
        if (j > 15) j = 15; // Max 16 axes.
        for (; j >= 0; --j) {
          var value = p.axes[j];
          if (value <= -0.1) {
            return (j << 4) | (i << 2) | 2; // Produce -axis binding.
          } else if (value >= 0.1) {
            return (j << 4) | (i << 2) | 3; // Produce +axis binding.
          }
        }
      }
    }
  }
  return 0;
},
};

var loaderEl = document.getElementById('loader');

var Module = {
  preRun: [function() {
    // Mount IndexedDB file system (IDBFS) to /fceux.
    FS.mkdir('/fceux');
    if (persistent_savegames) {
        FS.mount(IDBFS, {}, '/fceux');
    }
  }],
  postRun: [function() {
    FCEM.setController = Module.cwrap('FCEM_SetController', null, ['number', 'number']);
    FCEM.bindKey = Module.cwrap('FCEM_BindKey', null, ['number', 'number']);
    FCEM.bindGamepad = Module.cwrap('FCEM_BindGamepad', null, ['number', 'number']);
    FCEM.silenceSound = Module.cwrap('FCEM_SilenceSound', null, ['number']);
	FCEM.getGamepadState = Module.cwrap('FCEM_GetGamepadState', 'number', ['number']);
	FCEM.getTimingInfo = Module.cwrap('FCEM_getTimingInfo', 'number', []);
    // HACK: Disable default fullscreen handlers. See Emscripten's library_browser.js
    // The handlers forces the canvas size by setting css style width and height with
    // "!important" flag. Workaround is to disable the default fullscreen handlers.
    // See Emscripten's updateCanvasDimensions() in library_browser.js for the faulty code.
    Browser.fullscreenHandlersInstalled = true;
    // Initial IDBFS sync.
    FS.syncfs(true, FCEM.onInitialSyncFromIDB); // reference to "persistent_savegames": note that we want to call the callback, even if IDBFS was not mounted
    // Setup configuration from localStorage.
    FCEM.initControllers();
    FCEM.initInputBindings();
  }],
  print: function() {
    text = Array.prototype.slice.call(arguments).join(' ');
    console.log(text);
  },
  printErr: function(text) {
    text = Array.prototype.slice.call(arguments).join(' ');
    console.error(text);
  },
  canvas: (function() {
    var el = document.getElementById('canvas');
// TODO: tsone: handle context loss, see: http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    el.addEventListener("webglcontextlost", function(e) { alert('WebGL context lost. You will need to reload the page.'); e.preventDefault(); }, false);
    return el;
  })(),
  canvas3D: (function() {
    return document.getElementById('canvas3D');
  })(),
  setStatus: function(text) {
    var dl = 'Downloading data...';
// TODO: tsone: add startswith() method?
    if (text.substring(0, dl.length) === dl) {
      var r = text.match(/\(([\d.]+)\/([\d.]+)\)/);
      var x = parseFloat(r[1]) / parseFloat(r[2]);
      FCEV.setProgress(0.75 + 0.25*x);
    }
    if (!text) {
      loaderEl.hidden = true;
    }
  },
  totalDependencies: 0,
  monitorRunDependencies: function(left) {
    this.totalDependencies = Math.max(this.totalDependencies, left);
    Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
  }
};
Module.setStatus('Loading...');
window.onerror = function(event) {
  Module.setStatus = function(text) {
    if (text) Module.printErr('[post-exception status] ' + text);
  };
};

var req = new XMLHttpRequest();
req.addEventListener('progress', function(event) {
	var x = event.loaded / FCEC.FCEUX_JS_SIZE;
	FCEV.setProgress(0.75 * x);
}, false);
req.addEventListener('load', function(event) {
	var e = event.target;
	var blob = new Blob([ e.responseText ]);
	var s = document.createElement('script');
	var url = URL.createObjectURL(blob);
	s.onload = s.onerror = function() {
		URL.revokeObjectURL(url);
	}
	s.src = url;
	document.documentElement.appendChild(s);

	FCEM.startGame('/data/games/Super Tilt Bro(E).nes');
}, false);
req.open("GET", "{{fceux.js}}", true);
req.send();

var current = 0;

function calcGameOffset() {
  return 3 * ((3*Math.random()) |0);
}

function startAudioFromPhysical() {
  /* Function to be called on the first physical event to enable audio if it was not already done on load */
  FCEM.createAudioContext();
  document.removeEventListener("keydown", startAudioFromPhysical);
  document.removeEventListener("click", startAudioFromPhysical);
  hack_first_input_time = Date.now();
}
document.addEventListener("keydown", startAudioFromPhysical);
document.addEventListener("click", startAudioFromPhysical);

document.addEventListener("keydown", function(e) {
  if (!FCEV.catchEnabled) {
    return;
  }
  var key = e.keyCode & 0x0FF;
  if (e.metaKey)  key |= 0x800;
  if (e.altKey)   key |= 0x400;
  if (e.shiftKey) key |= 0x200;
  if (e.ctrlKey)  key |= 0x100;

  var el = document.getElementById("catchKey");
  el.innerHTML = FCEM.key2Name(key);

  FCEV.catchKey = key;
});

document.getElementById('current_msg').addEventListener('click', function(e) {
	document.getElementById('current_msg').className = 'hidden';
	document.getElementById('messages').className = 'hidden';
});
GuiMessage = {
	show: function(title, body, class_name) {
		var msg = document.getElementById("current_msg");
		var container = document.getElementById('messages')
		msg.innerHTML = '<div class="msg_title">'+ title +'</div><div class="msg_body">'+ body +'</div>';
		msg.className = class_name;
		container.className = '';
	},
	warn: function(title, body) {
		GuiMessage.show(title, body, 'warning');
	},
	error: function(title, body) {
		GuiMessage.show(title, body, 'error');
	},
};

// Must scan/poll as Gamepad API doesn't send input events...
setInterval(function() {
  if (!FCEV.catchEnabled) {
    return;
  }
  var binding = FCEV.scanForGamepadBinding();
  if (!binding) {
    return;
  }
  var el = document.getElementById("catchGamepad");
  el.innerHTML = FCEM.gamepad2Name(binding);
  FCEV.catchGamepad = binding;
}, 60);

document.addEventListener('DOMContentLoaded', FCEM.onDOMLoaded, false);
