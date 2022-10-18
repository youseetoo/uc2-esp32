window.addEventListener('load', (event) => {

    getModulesAndFillTabs();
    getLedSetting();
    getMotoretting();
    laserget();
    createWebsocket();
});

var websocket;

function createWebsocket() {
    if ($("#url").val() == "")
        websocket = new WebSocket('ws://' + $(location).attr('hostname') + ':81/');
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
    var uri;
    if ($("#url").val() == "")
        uri = "http://" + $(location).attr('hostname');
    else
        uri = "http://" + $("#url").val();
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

function getModulesAndFillTabs() {
    $.getJSON(getUri() + "/modules_get", function (data) {
        $("#tab").empty();
        $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Wifi\')">Wifi</button>')
        $("#tab").append('<button class="tablinks" onclick="openTab(event, \'BT\')">BT</button>')
        $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Modules\')">Modules</button>')
        if (data["modules"]["led"] != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'LED\')">LED</button>')
        if (data["modules"]["motor"] != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Motor\')">Motor</button>')
        if (data["modules"]["laser"] != 0)
            $("#tab").append('<button class="tablinks" onclick="openTab(event, \'Laser\')">Laser</button>')


        $("#m_enable_analog").attr('checked', data["modules"]["analog"]);
        $("#m_enable_dac").attr('checked', data["modules"]["dac"]);
        $("#m_enable_digital").attr('checked', data["modules"]["digital"]);
        $("#m_enable_laser").attr('checked', data["modules"]["laser"]);
        $("#m_enable_motor").attr('checked', data["modules"]["motor"]);
        $("#m_enable_led").attr('checked', data["modules"]["led"]);
        $("#m_enable_pid").attr('checked', data["modules"]["pid"]);
        $("#m_enable_scanner").attr('checked', data["modules"]["scanner"]);
        $("#m_enable_sensor").attr('checked', data["modules"]["sensor"]);
        $("#m_enable_slm").attr('checked', data["modules"]["slm"]);
    });
}

function setModuleSettings() {
    $("#steppinxinvert:checked").val() ? 1 : 0;
    var analog = $("#m_enable_analog:checked").val() ? 1 : 0;
    var dac = $("#m_enable_dac:checked").val() ? 1 : 0;
    var digital = $("#m_enable_digital:checked").val() ? 1 : 0;
    var laser = $("#m_enable_laser:checked").val() ? 1 : 0;
    var motor = $("#m_enable_motor:checked").val() ? 1 : 0;
    var led = $("#m_enable_led:checked").val() ? 1 : 0;
    var pid = $("#m_enable_pid:checked").val() ? 1 : 0;
    var scanner = $("#m_enable_scanner:checked").val() ? 1 : 0;
    var sensor = $("#m_enable_sensor:checked").val() ? 1 : 0;
    var slm = $("#m_enable_slm:checked").val() ? 1 : 0;
    var jstr = JSON.stringify({ modules: { analog: analog, dac: dac, digital: digital, laser: laser, motor: motor, led: led, pid: pid, scanner: scanner, sensor: sensor, slm: slm } });
    post(jstr, "/modules_set");
    getModulesAndFillTabs();
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
    if ($("#enableled:checked").val()) {
        var redc = $("#redRange").val();
        var greenc = $("#greenRange").val();
        var bluec = $("#blueRange").val();
        var jstr = JSON.stringify({ led: { LEDArrMode: 1, led_array: [{ id: 0, blue: bluec, red: redc, green: greenc }] } });
        websocket.send(jstr);
    }
}


function getMotoretting() {
    $.getJSON(getUri() + "/motor_get", function (data) {
        var steppers = data["steppers"];
        for (var i = 0; i < steppers.length; i++) {
            var id = steppers[i]["stepperid"];
            var step = steppers[i]["step"];
            var dir = steppers[i]["dir"];
            var power = steppers[i]["enable"];
            var stepin = steppers[i]["step_inverted"];
            var dirin = steppers[i]["dir_inverted"];
            var powerin = steppers[i]["enable_inverted"];

            if (id == 0) {
                $("#steppina").val(step);
                $("#steppinainvert").checked = stepin;
                $("#dirpina").val(dir);
                $("#dirpinainvert").checked = dirin;
                $("#powerpina").val(power);
                $("#powerpinainvert").checked = powerin;
            }
            else if (id == 1) {
                $("#steppinx").val(step);
                $("#steppinxinvert").checked = stepin;
                $("#dirpinx").val(dir);
                $("#dirpinxinvert").checked = dirin;
                $("#powerpinx").val(power);
                $("#powerpinxinvert").checked = powerin;
            }
            else if (id == 2) {
                $("#steppiny").val(step);
                $("#steppinyinvert").checked = stepin;
                $("#dirpiny").val(dir);
                $("#dirpinyinvert").checked = dirin;
                $("#powerpiny").val(power);
                $("#powerpinyinvert").checked = powerin;
            }
            else if (id == 3) {
                $("#steppinz").val(step);
                $("#steppinzinvert").checked = stepin;
                $("#dirpinz").val(dir);
                $("#dirpinzinvert").checked = dirin;
                $("#powerpinz").val(power);
                $("#powerpinzinvert").checked = powerin;
            }
        }
    });
}

function setMotorSettings() {
    var sa = $("#steppina").val();
    var da = $("#dirpina").val();
    var ea = $("#powerpina").val();
    var sai = $("#steppinainvert:checked").val() ? 1 : 0;
    var dai = $("#dirpinainvert:checked").val() ? 1 : 0;
    var eai = $("#powerpinainvert:checked").val() ? 1 : 0;

    var sx = $("#steppinx").val();
    var dx = $("#dirpinx").val();
    var ex = $("#powerpinx").val();
    var sxi = $("#steppinxinvert:checked").val() ? 1 : 0;
    var dxi = $("#dirpinxinvert:checked").val() ? 1 : 0;
    var exi = $("#powerpinxinvert:checked").val() ? 1 : 0;

    var sy = $("#steppiny").val();
    var dy = $("#dirpiny").val();
    var ey = $("#powerpiny").val();
    var syi = $("#steppinyinvert:checked").val() ? 1 : 0;
    var dyi = $("#steppinyinvert:checked").val() ? 1 : 0;
    var eyi = $("#powerpinyinvert:checked").val() ? 1 : 0;

    var sz = $("#steppinz").val();
    var dz = $("#dirpinz").val();
    var ez = $("#powerpinz").val();
    var szi = $("#steppinzinvert:checked").val() ? 1 : 0;
    var dzi = $("#dirpinzinvert:checked").val() ? 1 : 0;
    var ezi = $("#powerpinzinvert:checked").val() ? 1 : 0;

    var jstr = JSON.stringify(
        {
            motor:
            {
                steppers: [
                    { stepperid: 0, step: sa, dir: da, enable: ea, step_inverted: sai, dir_inverted: dai, enable_inverted: eai },
                    { stepperid: 1, step: sx, dir: dx, enable: ex, step_inverted: sxi, dir_inverted: dxi, enable_inverted: exi },
                    { stepperid: 2, step: sy, dir: dy, enable: ey, step_inverted: syi, dir_inverted: dyi, enable_inverted: eyi },
                    { stepperid: 3, step: sz, dir: dz, enable: ez, step_inverted: szi, dir_inverted: dzi, enable_inverted: ezi },
                ]
            }
        });
    post(jstr, "/motor_set");
}

var motorspeeds = [-4000, -2000, -1000, -500, -200, -100, -50, -20, -10, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 4000];

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

function laserget() {
    $.getJSON(getUri() + "/laser_get", function (data) {
        var l1 = data["LASER1pin"];
        var l2 = data["LASER2pin"];
        var l3 = data["LASER3pin"];
        $("#laser1pin").val(l1);
        $("#laser2pin").val(l2);
        $("#laser3pin").val(l3);
    });
}

function setLaserPin(laserid) {
    var pin = $('#laser' + laserid + 'pin').val();
    var jstr = JSON.stringify(
        {
            LASERid: laserid,
            LASERpin: pin
        });
    post(jstr, "/laser_set");
}


function setLaserAct(laserid) {
    var value = $('#laser' + laserid + 'value').val();
    var despeckel = $('#laser' + laserid + 'despeckel').val();
    var periode = $('#laser' + laserid + 'periode').val();
    var jstr = JSON.stringify(
        {
            LASERid: laserid,
            LASERval: value,
            LASERdespeckle : despeckel,
            LASERdespecklePeriod : periode
        });
    post(jstr, "/laser_act");
}