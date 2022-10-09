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