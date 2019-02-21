	  var InitialCall = true;
	  var requestStr = "";
	  var upTime;
	  var systemTime = new Date();
	  var serverIP = "ws://" + location.hostname + "/ws";  
      var ws = null; // = new WebSocket(serverIP);
      var runtimeData = null;;
      var configData = null;
      var signalData = null;
      var currentPage;
      var useEmulator = false;
	  var requestframeref;

      
 
	  function startWebsockets()
	  {
		  document.addEventListener("visibilitychange", handleVisibilityChange, false);
		  console.log("init websockets");
		  ws = new WebSocket(serverIP);
	  
      ws.onopen = function() 
      {
		  console.log("Page Open");
		  ws.send("{\"Cmd\": \"Request\",\"CmdParams\": [\"RuntimeData\"],\"JSONData\": {}}");
      };
 
	  ws.onclose = function (evt) 
	  {
		  console.log("websock close");
		  setTimeout(startWebsockets, 3000);
//		  conStatusError();
	  };

	  ws.onerror = function (evt) 
	  {
		  console.log(evt);
//		  conStatusError();
	  };

      ws.onmessage = function(evt) 
      {
		console.log(evt.data);
  		var myArr = JSON.parse(evt.data);
 		if(typeof myArr.Cmd !== "undefined")
 		{
			if (myArr.Cmd == "Report")
			{
				if (myArr.CmdParams == "CfgData")
				{
					configData = myArr.JSONData;
					DisplayConfigUpdateData(configData);
				}
				if (myArr.CmdParams == "SignalData")
				{
					signalData = myArr.JSONData;
				}				
				if (myArr.CmdParams == "RuntimeData")
				{
					runtimeData = myArr.JSONData;
					DisplayConfigUpdateData(runtimeData);
				}				
				if (myArr.CmdParams == "EventData")				
				{
					switch (myArr.JSONData.evtType)
					{
						case 0:;
						case 1:;
						case 2: //handleButtonEvent(myArr.JSONData);
								//handleSwitchEvent(myArr.JSONData);
								break;
						case 10://console.log("Call BD Event");
//						        handleBlockDetectorEvent(myArr.JSONData);
								break;
						default:break;
					}
				}
			}
		}
	  }
      };

	  function handleVisibilityChange() 
	  {
		  console.log("Visibility Change");
//		  if (!websockConnected && !document.hidden) {
//			  restart();
	  }

//    function requestPageData()
//    {
//		if (ws === null)
//			startWebsockets();
//	    console.log("Send Request");
//		if (currentPage == "BasicCfg")
//		  getConfigData();
//		  ws.send("{\"Cmd\": \"Request\",\"CmdParams\": [\"CfgData\"],\"JSONData\": {}}");
//		if (currentPage == "CTCCfg")
//		  ws.send("{\"Cmd\": \"Request\",\"CmdParams\": [\"SensorData\"],\"JSONData\": {}}");
//	}

	function openPage(pageName, elmnt, color) 
	{
    // Hide all elements with class="tabcontent" by default */
		var i, tabcontent, tablinks;
		tabcontent = document.getElementsByClassName("tabcontent");
		for (i = 0; i < tabcontent.length; i++) {
			tabcontent[i].style.display = "none";
		}

    // Remove the background color of all tablinks/buttons
		tablinks = document.getElementsByClassName("tablink");
		for (i = 0; i < tablinks.length; i++) {
			tablinks[i].style.backgroundColor = "";
		}

    // Show the specific tab content
		document.getElementById(pageName).style.display = "block";
		currentPage = pageName;
		if (currentPage == "BasicCfg")
		{
			DisplayConfigUpdateData(configData);
			console.log("Update Config");
			console.log(configData);
		}
//		requestPageData();
    // Add the specific color to the button used to open the tab content
		elmnt.style.backgroundColor = color;
	}
	
	function getConfigData()
	{
		var request = new XMLHttpRequest();
		request.onreadystatechange = function()
		{
			if (this.readyState == 4) 
			{
				if (this.status == 200) 
				{
//					console.log(this.responseText);
					var myArr = JSON.parse(this.responseText);
					configData = myArr;
					DisplayConfigUpdateData(configData);
				}
			}
		}
		console.log("send AJAX request");
		request.open("GET", "/node.cfg", true);
		request.setRequestHeader('Cache-Control', 'no-cache');
		request.send();
//		setTimeout('GetGatewayData()', 10000); //milliseconds
	} //GetGatewayData
	
	function sendJSONDataToDiskFile(fileName, jsonString)
	{
		var request = new XMLHttpRequest();
		request.onreadystatechange = function()
		{
			if (this.readyState == 4) 
			{
				if (this.status == 200) 
				{
					console.log(this.responseText);
					alert("Page is being reloaded");
					location.reload();
				}
			}
		}
		console.log("send AJAX Post request");
		request.open("POST", "/post", true);
		request.setRequestHeader('Content-Type', 'application/json');
		request.setRequestHeader('Content-Disposition', 'attachment; filename="' + fileName + '"');
		if (confirm("Save data and restart device?"))
			request.send(jsonString);
	} //GetGatewayData



	function verifySignalData(myArr)
	{
		if(typeof myArr.LEDCols === "undefined")
			myArr.LEDCols = [];
		if(typeof myArr.Brightness === "undefined")
			myArr.Brightness = {};
		if(typeof myArr.NumLED === "undefined")
			myArr.NumLED = 10;
		if(typeof myArr.SignalStart === "undefined")
			myArr.SignalStart = 1025;
		if(typeof myArr.SigHeads === "undefined")
			myArr.SigHeads = [];
		console.log(myArr);
	}
	
	function getSignalData()
	{
		var request = new XMLHttpRequest();
		request.onreadystatechange = function()
		{
			if (this.readyState == 4) 
			{
				if (this.status == 200) 
				{
//					console.log(this.responseText);
					var myArr = JSON.parse(this.responseText);
					verifySignalData(myArr);
					signalData = myArr;
					DisplayLEDSetupUpdateData(signalData);
					DisplaySigSetupUpdateData(signalData);
				}
			}
		}
		console.log("send AJAX request");
		request.open("GET", "/signals.cfg", true);
		request.setRequestHeader('Cache-Control', 'no-cache');
		request.send();
	} //getSignalData
	

	function UpdateGatewayData() //Time Zone and NetBIOSName
	{
		requestStr = "&ntpTimeZone=" + document.getElementById("newtimezone").value;
		requestStr = "&NTPServer=" + document.getElementById("newntpserver").value;
		requestStr += "&NetBIOSName=" + document.getElementById("newnetbiosname").value;
		if (document.getElementById("newusentpserver").checked)
			requestStr += "&useNTP=1";
		else
			requestStr += "&useNTP=0";
		console.log(requestStr);
	}

window.requestAnimationFrame = window.requestAnimationFrame
                               || window.mozRequestAnimationFrame
                               || window.webkitRequestAnimationFrame
                               || window.msRequestAnimationFrame
                               || function(f){return setTimeout(f, 1000/60)}

window.cancelAnimationFrame = window.cancelAnimationFrame
                              || window.mozCancelAnimationFrame
                              || function(requestID){clearTimeout(requestID)} //fall back


function setEmulatorMode(sender)
{
	document.getElementById("cbRunTest").checked = false;
	doTestSequence = false;
	useEmulator = document.getElementById(sender.getAttribute("id")).checked;
	document.getElementById("useemulator0").checked = useEmulator;
	document.getElementById("useemulator4").checked = useEmulator;
	var requestStr = "{\"Cmd\":\"Display\",\"CmdParams\":[\"Emulator\"],\"JSONData\":{\"useEmulator\":" + useEmulator + "}}";
	ws.send(requestStr);
}

///////////////////////////helper function//////////////////////////////////////////////
function getColorOfName(colName)
{
	if (signalData !== null)
	{
		var colorData = signalData.LEDCols;
		for (var i=0; i < colorData.length; i++)
		{
			if (colorData[i].Name === colName)
			{
				var colVal = (colorData[i].RGBVal[0]<<16) + (colorData[i].RGBVal[1]<<8) + colorData[i].RGBVal[2];
				var colStr = colVal.toString(16);
				while (colStr.length < 6)
					colStr = "0" + colStr;
				colStr = "#" + colStr;
//				console.log(colorData[i], colStr);
				return colStr;
			}
		}
	}
	return "#000000"; //black
}
