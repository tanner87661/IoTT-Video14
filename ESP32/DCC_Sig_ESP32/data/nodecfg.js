	function DisplayConfigInitialData(JSONArray)
	{
		DisplayConfigUpdateData(JSONArray);
	}	

	function DisplayConfigUpdateData(JSONArray)
	{
		console.log(JSONArray);
 		  if(typeof JSONArray.SWVersion !== "undefined")
			document.getElementById("myversion").innerHTML = JSONArray.SWVersion;
 		  if(typeof JSONArray.IP !== "undefined")
			document.getElementById("myip").innerHTML = JSONArray.IP;
 		  if(typeof JSONArray.SigStrength !== "undefined")
			document.getElementById("sigstrength").innerHTML = JSONArray.SigStrength + " [dB]";
 		  if(typeof JSONArray.uptime !== "undefined")
 		  {
			upTime = JSONArray.uptime;
			displayUpTime();
		  }
// 		  if(typeof JSONArray.useNTP !== "undefined")
//		  {
//			document.getElementById("cbUseNTP").checked = JSONArray.useNTP == 1;
//			setTimeServerOptions();
//		  }
 		  if(typeof JSONArray.useWifi !== "undefined")
			document.getElementById("cbUseWifi").checked = (JSONArray.useWifi == 1); 
 		  if(typeof JSONArray.NetBIOSName !== "undefined")
			document.getElementById("newnetbiosname").value = JSONArray.NetBIOSName;
 		  if(typeof JSONArray.useDHCP !== "undefined")
 		  {
			document.getElementById("rbUseDHCP").checked = (JSONArray.useDHCP == 1); 
			document.getElementById("rbUseStatic").checked = (JSONArray.useDHCP !== 1); 
		  }
 		  if(typeof JSONArray.staticIP !== "undefined")
			document.getElementById("newstaticip").value = JSONArray.staticIP; 
 		  if(typeof JSONArray.staticGateway !== "undefined")
			document.getElementById("newstaticgateway").value = JSONArray.staticGateway; 
 		  if(typeof JSONArray.staticNetmask !== "undefined")
			document.getElementById("newnetmask").value = JSONArray.staticNetmask; 
 		  if(typeof JSONArray.staticDNS !== "undefined")
			document.getElementById("newdnsip").value = JSONArray.staticDNS; 
 
 		  if(typeof JSONArray.useAP !== "undefined")
			document.getElementById("cbUseAP").checked = (JSONArray.useAP == 1); 
 		  if(typeof JSONArray.apName !== "undefined")
			document.getElementById("newapname").value = JSONArray.apName; 
 		  if(typeof JSONArray.apGateway !== "undefined")
			document.getElementById("newapip").value = JSONArray.apGateway; 
// 		  if(typeof JSONArray.apUser !== "undefined")
//			document.getElementById("newapuser").value = JSONArray.apUser; 
 		  if(typeof JSONArray.apPassword !== "undefined")
			document.getElementById("newappassword").value = JSONArray.apPassword; 
 
 		  if(typeof JSONArray.useNTP !== "undefined")
			document.getElementById("cbUseNTP").checked = (JSONArray.useNTP == 1); 
 		  if(typeof JSONArray.ntpTimeZone !== "undefined")
			document.getElementById("newtimezone").value = JSONArray.ntpTimeZone; 
 		  if(typeof JSONArray.NTPServer !== "undefined")
			document.getElementById("newntpserver").value = JSONArray.NTPServer; 

  		  if(typeof JSONArray.useBushby !== "undefined")
			document.getElementById("supportbushby").checked = (JSONArray.useBushby == 1); 
 		  if(typeof JSONArray.useInputMode !== "undefined")
 		  {
			document.getElementById("rbUseDCC").checked = (JSONArray.useInputMode == 0); 
			document.getElementById("rbUseLocoNet").checked = (JSONArray.useInputMode == 1); 
			document.getElementById("rbUseMQTT").checked = (JSONArray.useInputMode == 2); 
		  }

 		  if(typeof JSONArray.mqttServer !== "undefined")
 		  {
			if(typeof JSONArray.mqttServer.ip !== "undefined")
				document.getElementById("newmqttserver").value = JSONArray.mqttServer.ip;
			if(typeof JSONArray.mqttServer.port !== "undefined")
				document.getElementById("newmqttport").value = JSONArray.mqttServer.port;
			if(typeof JSONArray.mqttServer.user !== "undefined")
				document.getElementById("newmqttuser").value = JSONArray.mqttServer.user;
			if(typeof JSONArray.mqttServer.password !== "undefined")
				document.getElementById("newmqttpassword").value = JSONArray.mqttServer.password;
		  }
 		  if(typeof JSONArray.lnBCTopic !== "undefined")
			document.getElementById("newbroadcasttopic").value = JSONArray.lnBCTopic;
 		  if(typeof JSONArray.lnEchoTopic !== "undefined")
			document.getElementById("newechotopic").value = JSONArray.lnEchoTopic;
		setSetupConfigOptions();
 	}	

	function displayUpTime()
	{
		var days = Math.trunc(upTime/86400);
 		var hours = Math.trunc(upTime/3600) % 24;
 		var minutes = Math.trunc(upTime/60) % 60;
 		var seconds = upTime % 60;
 	    document.getElementById("sysuptime").innerHTML = days + " Days, " + hours + " Hours, " + minutes + " Minutes, " + seconds + " Seconds";
	}
	  
	function UpdateTimeDisplay()
	{
	    upTime++;
	    displayUpTime();
	    systemTime += 1000;
//	    systemTime.setSeconds(systemTime.getSeconds() + 1);
//		  document.getElementById("sysdatetime").innerHTML = systemTime;
	}

function saveDeviceCfg()
{
	requestStr = "{"

	requestStr += "\"useWifi\":" + ((document.getElementById("cbUseWifi").checked) ? 1:0);
	requestStr += ",\"NetBIOSName\":\"" + document.getElementById("newnetbiosname").value + "\"";
	requestStr += ",\"useDHCP\":" + ((document.getElementById("rbUseDHCP").checked) ? 1:0);
	requestStr += ",\"staticIP\":\"" + document.getElementById("newstaticip").value + "\"";
	requestStr += ",\"staticGateway\":\"" + document.getElementById("newstaticgateway").value + "\"";
	requestStr += ",\"staticNetmask\":\"" + document.getElementById("newnetmask").value + "\"";
	requestStr += ",\"staticDNS\":\"" + document.getElementById("newdnsip").value + "\"";

	requestStr += ",\"useAP\":" + ((document.getElementById("cbUseAP").checked) ? 1:0);
	requestStr += ",\"apName\":\"" + document.getElementById("newapname").value + "\"";
	requestStr += ",\"apGateway\":\"" + document.getElementById("newapip").value + "\"";
//	requestStr += ",\"apUser\":\"" + document.getElementById("newapuser").value + "\"";
	requestStr += ",\"apPassword\":\"" + document.getElementById("newappassword").value + "\"";

	requestStr += ",\"useNTP\":" + ((document.getElementById("cbUseNTP").checked) ? 1:0);
	requestStr += ",\"NTPServer\":\"" + document.getElementById("newntpserver").value + "\"";
	requestStr += ",\"ntpTimeZone\":" + document.getElementById("newtimezone").value;

	requestStr += ",\"useBushby\":" + ((document.getElementById("supportbushby").checked) ? 1:0);
	
	requestStr += ",\"useInputMode\":";
	var radioButtons = document.getElementsByName("cmdSrc");
    for(var i = 0; i < radioButtons.length; i++)
    {
        if(radioButtons[i].checked == true)
        {
			requestStr += i.toString();
			break;
		}
	}	
	
	requestStr += ",\"mqttServer\":{";
	requestStr += "\"ip\":\"" + (document.getElementById("newmqttserver").value) + "\"";
	requestStr += ",\"port\":" + (document.getElementById("newmqttport").value);
	requestStr += ",\"user\":\"" + (document.getElementById("newmqttuser").value) + "\"";
	requestStr += ",\"password\":\"" + (document.getElementById("newmqttpassword").value) + "\"";
	requestStr += "}";
	requestStr += ",\"lnBCTopic\":\"" + (document.getElementById("newbroadcasttopic").value) + "\"";
	requestStr += ",\"lnEchoTopic\":\"" + (document.getElementById("newechotopic").value) + "\"";
	
	requestStr += "}";

	sendJSONDataToDiskFile("node.cfg", requestStr);

	console.log(requestStr);
	ws.send(requestStr);
}

function ResetLNStats() //Reset Byte Counters
{
	requestStr = "{\"Cmd\":\"Update\",\"CmdParams\":[\"CfgData\"],\"JSONData\": {\"ResetCtr\":true}}";
	ws.send(requestStr);
}

function cancelCfg()
{
	ws.send("{\"Cmd\": \"Request\",\"CmdParams\": [\"CfgData\"],\"JSONData\": {}}");
}

function setWifiOptions()
{
  var cbWifi = document.getElementById('cbUseWifi');
  if (!cbWifi.checked)
  {
    var cbAP = document.getElementById('cbUseAP');
    cbAP.checked = true;
	var rbUseMQTT = document.getElementById('rbUseMQTT');
	rbUseMQTT.checked = false;
	var rbUseLocoNet = document.getElementById('rbUseLocoNet');
	rbUseLocoNet.checked = true;
    var cbGW = document.getElementById('actAsGateway');
    cbGW.checked = false;
  }
  setSetupConfigOptions();
}

function setAccessPointOptions()
{
  var cbAP = document.getElementById('cbUseAP');
  if (!cbAP.checked) 
  {
	var cbWifi = document.getElementById('cbUseWifi');
	cbWifi.checked = true;
  }
  setSetupConfigOptions();
}

function setTimeServerOptions()
{
  setSetupConfigOptions();
}

function setGatewayOptions()
{
  setSetupConfigOptions();
}

function setDHCPOptions()
{
  setSetupConfigOptions();
}

function setSetupConfigOptions()
{
  var cbWifi = document.getElementById('cbUseWifi');
  var divWifi = document.getElementById('BasicCfg_UseWifiData');
  var cbAP = document.getElementById('cbUseAP');
  var divAP = document.getElementById('BasicCfg_UseAPData');
  var optNTP = document.getElementById('BasicCfg_UseNTPCB');
  var cbNTP = document.getElementById('cbUseNTP');
  var divNTP = document.getElementById('BasicCfg_UseNTPData');
  var cbActGW = document.getElementById('actAsGateway');
  var rbCmdSrcMQTT = document.getElementById('rbUseMQTT');
  var divMQTT = document.getElementById('BasicCfg_MQTTSetup');
  var rbUseStatic = document.getElementById('rbUseStatic');
  var divDHCP = document.getElementById('BasicCfg_DHCPOptions');
  var divDCCRB = document.getElementById('radioDCC');
  var divLNRB = document.getElementById('radioLocoNet');
  var divMQTTRB = document.getElementById('radioMQTT');
  var divBushbyCB = document.getElementById('checkboxBushby');
  var rbUseLN = document.getElementById('rbUseLocoNet');
  var rbUseMQTT = document.getElementById('rbUseMQTT');
  var rbUseDCC = document.getElementById('rbUseDCC');
  var cbUseBushby = document.getElementById('supportbushby');

  var showWifiOptions = cbWifi.checked;
  var showStaticOptions = cbWifi.checked && rbUseStatic.checked;
  var showAPOptions = cbAP.checked;
  var showNTPCheckbox = cbWifi.checked;
  var showNTPOptions = showNTPCheckbox && cbNTP.checked;
  var showMQTTSel = cbWifi.checked;
  var showActGWSel = cbWifi.checked;
  var showMQTTOptions = cbWifi.checked && rbUseMQTT.checked;
  var showBushby = rbUseMQTT.checked || rbUseLN.checked;

  if (showWifiOptions) {
    divWifi.style.display = 'block';
  } else {
    divWifi.style.display = 'none';
  }
  if (showStaticOptions) {
    divDHCP.style.display = 'block';
  } else {
    divDHCP.style.display = 'none';
  }
  if (showAPOptions) {
    divAP.style.display = 'block';
  } else {
    divAP.style.display = 'none';
  }
  if (showNTPCheckbox) {
    optNTP.style.display = 'block';
  } else {
    optNTP.style.display = 'none';
  }
  if (showNTPOptions) {
    divNTP.style.display = 'block';
  } else {
    divNTP.style.display = 'none';
  }
  if (showMQTTSel) {
    divMQTTRB.style.display = 'block';
  } else {
    divMQTTRB.style.display = 'none';
  }
  if (showBushby) {
    divBushbyCB.style.display = 'block';
  } else {
    divBushbyCB.style.display = 'none';
  }
  if (showMQTTOptions) {
    divMQTT.style.display = 'block';
  } else {
    divMQTT.style.display = 'none';
  }
}
