window.addEventListener('load', (event) => {
    getLedSetting();
    createWebsocket();
});

var websocket;

function createWebsocket() {
    if ($("#url").val() == "")
        websocket = new WebSocket('ws://localhost:81/');
    else
        websocket = new WebSocket('ws://' + $("#url").val() + ':81/');
}

function post(jstr, uri) {
    $.ajax(getUri() + uri, {
        data: jstr,
        method: "POST",
        contentType: "text/plain"
    });
}

function getUri() {
    var uri = "http://" + $("#url").val();
    return uri;
}

function openTab(evt, tabName) {
    var i, tabcontent, tablinks;
    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";
}

function getSsids() {
    $.getJSON(getUri() + "/wifi/scan", function (data) {
        $("#ssids").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#ssids").append('<li><button class="ssiditem" onclick="ssidItemClick(\'' + data[i] + '\')">' + data[i] + '</button></li>');
        }
    });
}

function tryToConnect() {
    console.log(getUri() + "/features_get");
    $.getJSON(getUri() + "/features_get", function (data) {
        console.log(data);
        createWebsocket();
    });
}

function ssidItemClick(name) {
    var str = name;
    $('#ssid').val(str);
}

function connectToWifi() {
    var ssidstr = $("#ssid").val();
    var pwstr = $("#password").val();
    var apb = $("#ap").val();
    var jstr = JSON.stringify({ ssid: ssidstr, PW: pwstr, AP: apb ? 1 : 0 });
    post(jstr, "/wifi/connect");
}

function getBtDevices() {
    $.getJSON(getUri() + "/bt_scan", function (data) {
        $("#btdevices").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#btdevices").append('<li><button class="btitem" onclick="btItemClick(\'' + data[i]['mac'] + '\')">' + data[i]['name'] + " " + data[i]['mac'] + '</button></li>');
        }
    });
}
function getPairedBtDevices() {
    $.getJSON(getUri() + "/bt_paireddevices", function (data) {
        $("#btdevices").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#btdevices").append('<li><button class="btitem" onclick="btItemClick(\'' + data[i]['mac'] + '\')">' + data[i]['name'] + " " + data[i]['mac'] + '</button></li>');
        }
    });
}

function btItemClick(name) {
    var str = name;
    $('#mac').val(str);
}

function connectToBT() {
    var macbt = $("#mac").val();
    var psxl = $("#psx").val();
    var jstr = JSON.stringify({ mac: macbt, psx: psxl ? 1 : 0 });
    post(jstr, "/bt_connect");
}

function removePairedBtDevices() {
    var macbt = $("#mac").val();
    var jstr = JSON.stringify({ mac: macbt });
    post(jstr, "/bt_remove");
}

function getLedSetting() {
    $.getJSON(getUri() + "/ledarr_get", function (data) {
        var leds = data["ledArrNum"];
        var pin = data["ledArrPin"];
        $("#ledcount").val(leds);
        $("#ledpin").val(pin);
    });
}

function setLedSettings() {
    var ledcount = $("#ledcount").val();
    var pin = $("#ledpin").val();
    var jstr = JSON.stringify({ led: { ledArrPin: pin, ledArrNum: ledcount } });
    post(jstr, "/ledarr_set");
}

function enableLeds(cb) {
    var redc = $("#redRange").val();
    var greenc = $("#greenRange").val();
    var bluec = $("#blueRange").val();
    var jstr = JSON.stringify({ led: { LEDArrMode: 1, led_array: [{ id: 0, blue: cb.checked ? bluec : 0, red: cb.checked ? redc : 0, green: cb.checked ? greenc : 0 }] } });
    post(jstr, "/ledarr_act");
}

function updateLeds() {
    if ($("#enableled").checked) {
        var redc = $("#redRange").val();
        var greenc = $("#greenRange").val();
        var bluec = $("#blueRange").val();
        var jstr = JSON.stringify({ led: { LEDArrMode: 1, led_array: [{ id: 0, blue: bluec, red: redc, green: greenc }] } });
        websocket.send(jstr);
    }
}