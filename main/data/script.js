window.addEventListener('load', (event) => {

    getModulesAndFillTabs();

});

var websocket;
var motor;
var led;
var analogJoystick;

function setModules(data) {
    motor = data["modules"]["motor"];
    led = data["modules"]["led"];
    analogJoystick = data["modules"]["joy"];
}

function createWebsocket() {
    if ($("#url").val() == "")
        websocket = new WebSocket('ws://' + $(location).attr('hostname') + ':81/');
    else
        websocket = new WebSocket('ws://' + $("#url").val() + ':81/');
    websocket.addEventListener('message', function (data) {
        var result = JSON.parse(data.data); // $.parseJSON(data);
        var steppers = result["steppers"];
        for (var i = 0; i < steppers.length; i++) {
            var id = steppers[i]["stepperid"];
            var curpos = steppers[i]["position"];
            if (id == 0)
                $("#posa").html(curpos);
            if (id == 1)
                $("#posx").html(curpos);
            if (id == 2)
                $("#posy").html(curpos);
            if (id == 3)
                $("#posz").html(curpos);
        }
    });
}

function post(jstr, uri) {
    $.ajax(getUri() + uri, {
        data: jstr,
        method: "POST",
        contentType: "text/plain"
    });
}

function getUri() {
    var uri;
    if ($("#url").val() == "")
        uri = "http://" + $(location).attr('hostname');
    else
        uri = "http://" + $("#url").val();
    return uri;
}

function updateUi() {
    if (led)
        getLedSetting();
    if (motor)
        getMotoretting();
    if (laser)
        laserget();
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

function getModulesAndFillTabs() {
    $.getJSON(getUri() + "/modules_get", function (data) {
        $("#tab").empty();
        $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Wifi\')">Wifi</button>')
        $("#tab").append('<button class="tablinks" onclick="openTab(event, \'BT\')">BT</button>')
        setModules(data);
        if (led != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'LED\')">LED</button>')
        if (motor != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Motor\')">Motor</button>')
        if (laser != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Laser\')">Laser</button>')

        updateUi();
        createWebsocket();
    });
}

function hideOrShow(id) {
    var x = document.getElementById(id);
    if (x.style.display == "none" || x.style.display == "") {
        x.style.display = "block";
    } else {
        x.style.display = "none";
    }
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
        updateUi();
    });

}

function ssidItemClick(name) {
    var str = name;
    $('#ssid').val(str);
}

function connectToWifi() {
    var ssidstr = $("#ssid").val();
    var pwstr = $("#password").val();
    var apb = $("#ap").checked;
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

function enableLeds(cb) {
    var redc = $("#redRange").val();
    var greenc = $("#greenRange").val();
    var bluec = $("#blueRange").val();
    var jstr = JSON.stringify({ led: { LEDArrMode: 1, led_array: [{ id: 0, blue: cb.checked ? bluec : 0, red: cb.checked ? redc : 0, green: cb.checked ? greenc : 0 }] } });
    post(jstr, "/ledarr_act");
}

function updateLeds() {
    if ($("#enableled:checked").val()) {
        var redc = $("#redRange").val();
        var greenc = $("#greenRange").val();
        var bluec = $("#blueRange").val();
        var jstr = JSON.stringify({ led: { LEDArrMode: 1, led_array: [{ id: 0, blue: bluec, red: redc, green: greenc }] } });
        websocket.send(jstr);
    }
}

var motorspeeds = [-160000, -80000, -8000, -4000, -2000, -1000, -500, -200, -100, -50, -20, -10, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 4000, 8000, 80000, 160000];

function updateMotors() {
    var xr = $("#xRange").val();
    console.log($("#xRange").val());
    var x = motorspeeds[xr];
    var y = motorspeeds[$("#yRange").val()];
    var z = motorspeeds[$("#zRange").val()];
    var a = motorspeeds[$("#aRange").val()];
    var af = (a != 0) ? 1 : 0;
    var xf = (x != 0) ? 1 : 0;
    var yf = (y != 0) ? 1 : 0;
    var zf = (z != 0) ? 1 : 0;
    var jstr = JSON.stringify(
        {
            motor:
            {
                steppers: [
                    { stepperid: 0, speed: a, isforever: af },
                    { stepperid: 1, speed: x, isforever: xf },
                    { stepperid: 2, speed: y, isforever: yf },
                    { stepperid: 3, speed: z, isforever: zf }
                ]
            }
        });
    websocket.send(jstr);
}


function setLaserAct(laserid) {
    var value = $('#laser' + laserid + 'value').val();
    var despeckel = $('#laser' + laserid + 'despeckel').val();
    var periode = $('#laser' + laserid + 'periode').val();
    var jstr = JSON.stringify(
        {
            LASERid: laserid,
            LASERval: value,
            LASERdespeckle: despeckel,
            LASERdespecklePeriod: periode
        });
    post(jstr, "/laser_act");
}