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

function getSsids()
{
    $.getJSON($("#url").val()+"/wifi/scan",function(data)
    {
        $("#ssids").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#ssids").append('<li><button class="ssiditem" onclick="ssidItemClick(\''+data[i]+'\')">'+data[i]+'</button></li>');
        }
    });
}

function tryToConnect()
{
    console.log($("#url").val()+"/features_get");
    $.getJSON($("#url").val()+"/features_get",function(data)
    {
        console.log(data);
    });
}

function ssidItemClick(name)
{
    var str = name;
    $('#ssid').val(str);
}

function connectToWifi()
{
    var ssidstr =  $("#ssid").val();
    var pwstr =  $("#password").val();
    var apb = $("#ap").val();
    var jstr = JSON.stringify({ssid: ssidstr, PW : pwstr, AP:apb ? 1 : 0});
    $.ajax($("#url").val()+"/wifi/connect", {
        data: jstr,
        method: "POST",
        contentType: "text/plain"
     });
}

function getBtDevices()
{
    $.getJSON($("#url").val()+"/bt_scan",function(data)
    {
        $("#btdevices").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#btdevices").append('<li><button class="btitem" onclick="btItemClick(\''+data[i]['mac']+'\')">'+data[i]['name'] + " " +data[i]['mac']+'</button></li>');
        }
    });
}
function getPairedBtDevices()
{
    $.getJSON($("#url").val()+"/bt_paireddevices",function(data)
    {
        $("#btdevices").empty();
        for (var i = 0, len = data.length; i < len; i++) {
            $("#btdevices").append('<li><button class="btitem" onclick="btItemClick(\''+data[i]['mac']+'\')">'+data[i]['name'] + " " +data[i]['mac']+'</button></li>');
        }
    });
}

function btItemClick(name)
{
    var str = name;
    $('#mac').val(str);
}

function connectToBT()
{
    var macbt =  $("#mac").val();
    var psxl =  $("#psx").val();
    var jstr = JSON.stringify({mac: macbt, psx:psxl ? 1 : 0});
    $.ajax($("#url").val()+"/bt_connect", {
        data: jstr,
        method: "POST",
        contentType: "text/plain"
     });
}

function removePairedBtDevices()
{
    var macbt =  $("#mac").val();
    var jstr = JSON.stringify({mac: macbt});
    $.ajax($("#url").val()+"/bt_remove", {
        data: jstr,
        method: "POST",
        contentType: "text/plain"
     });
}