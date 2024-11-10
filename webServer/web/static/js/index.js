// consts
let filterBounds = [1.30102999566, 4.30102999566];
let maxLogEntries = 5;
let pollInterval = 1;

// state
let suppressReload = false;
let loadedConfig = {};
let logEntries = [];
let selectedTrack = () => { return $('#audioFilesInput').val(); };
let maskSliderUpdate = null;

$(document).ready(() => {
    $(`#audioFilesInput`).on('change', () => {
        if (!suppressReload) { doAction('stop'); }
    });

    inputs.forEach(i => {
        let id = i[0];

        $(`#${id}Input`).on('change', () => {
            updateLabels();
            sendConfig();
        });

        $(`#${id}Input`).on('touchstart', () => {
            maskSliderUpdate = id;
        });

        $(`#${id}Input`).on('touchend', () => {
            if (maskSlideUpdate == id) {
                maskSliderUpdate = null;
            } else {
                showError("Unexpected state in slider masking");
            }
        });

        $(document).on('input', `#${id}Input`, function() {
            updateLabels();
        });
    });

    updateLabels();
    console.log("Ready.");

    recvSongs();

    setInterval(() => { recvConfig(); }, pollInterval * 1000);
});

document.ontouchstart = function(e){ 
    e.preventDefault(); 
}

document.ontouchmove = function(e){ 
    e.preventDefault(); 
}

const inputs = [
    [ 'lpfCutoff', 'LPF cutoff (hz)', 'lpf_cutoff', true ],
    [ 'hpfCutoff', 'HPF cutoff (hz)', 'hpf_cutoff', true ],
    [ 'bodyThreshold', 'Body threshold (rms)', 'body_threshold' ],
    [ 'mouthThreshold', 'Mouth threshold (rms)', 'mouth_threshold' ],
    [ 'rmsWindow', 'RMS window (ms)', 'rms_window_ms' ],
    [ 'flipInterval', 'Flip interval (ms)', 'flip_interval_ms' ],
]

function timestamp() {
    return new Date().toLocaleTimeString();
}

let errTimer = null;
function showError(msg) {
    console.log('error', msg);
    msg = timestamp() + ": " + msg;
    logEntries.splice(0, 0, { status: 'Error', message: msg });
    reloadLog();
}

function showOk(msg) {
    msg = timestamp() + ": " + msg;
    logEntries.splice(0, 0, { status: 'OK', message: msg });
    reloadLog();
}

function reloadLog() {
    let container = $('#logView');
    container.empty();
    while (logEntries.length > maxLogEntries) { logEntries.pop() }

    logEntries.forEach(entry => {
        console.log(entry);
        $('#logView').append(`<div class="logEntry logEntry${entry.status}">${entry.message}</div>`);
    });
}

function updateLabel(target, text, key, log) {
    let inputVal = $(`#${target}Input`).val();
    loadedConfig[key] = inputVal;

    // map input value to log_10 scale
    inputVal = filterBounds[0] + (filterBounds[1] - filterBounds[0]) * inputVal; 

    if (log) {
        loadedConfig[key] = Math.floor(Math.pow(10, inputVal)) + 1;
    }

    $(`#${target}Value`).text(loadedConfig[key]);
}

function updateLabels() {
    inputs.forEach(i => updateLabel(i[0], i[1], i[2], i[3]));
}

async function query(method, endpoint, body, and_then) {
    try {
        let request = { method: method };
        
        if (body != null) {
            request.body = JSON.stringify(body);
            request.headers = {
                "Content-Type": "application/json",
            };
        }

        let response = await fetch(endpoint, request);

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        let json = await response.json();

        if (json.status == "success") {
            return and_then(json);
        } else if (json.status != null) {
            throw new Error(json.status);
        } else {
            throw new Error("(no status)");
        }
    } catch (e) {
        showError(`${endpoint}: ${e}`);
    }

    return null;
}

async function doAction(action) {
    query("POST", "/api/actions", { actions: [{ action: action, args: selectedTrack() }]}, (resp) => {
        showOk("Sent action " + action);
        recvConfig();
    });
}

async function sendConfig() {
    query("POST", "/api/config", loadedConfig, (resp) => {
        showOk("Sent config OK");
    });
}

async function recvConfig() {
    query("GET", "/api/config", null, (resp) => {
        loadPlayingState(resp.state);

        suppressReload = true;
        if (resp.activesong != "") {
            $('#audioFilesInput').val(resp.activesong);
        }
        suppressReload = false;

        inputs.forEach(i => {
            if (i[0] == maskSliderUpdate) {
                return;
            }

            let dst = $(`#${i[0]}Input`);
            let val = resp.config[i[2]];

            if (val == null) {
                showError(`Key ${i[2]} missing from config response`);
                return;
            }

            if (i[3]) {
                val = (Math.log10(val) - filterBounds[0]);
                val = val / (filterBounds[1] - filterBounds[0]);
            }

            dst.val(val);
        });

        updateLabels();
    });
}

async function recvSongs() {
    query("GET", "/api/audiofiles", null, (resp) => {
        let a = $('#audioFilesInput').empty();

        resp.files.forEach(f => {
            a.append(`<option value="${f}">${f}</option>`);
        });

        showOk(`Received ${resp.files.length} songs`);
        recvConfig();
    });
}

function loadPlayingState(state) {
    let target = $('#btnPlayPause');

    target.removeClass("bg-success");
    target.removeClass("bg-warning");

    playingState = false;

    if (state == "play_pause") {
        target.addClass("bg-success");
        playingState = true;
    } else if (state == "pause") {
        target.addClass("bg-warning");
    }
}
