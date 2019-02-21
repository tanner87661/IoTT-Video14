const templateLEDColObject = JSON.stringify({"Name": "","RGBVal": [0, 0, 0, 0]});

var colorindex = -1;
var testProc = -1;
var currTestCol = 0;
var doTestSequence = false;

function DisplayLEDSetupUpdateData(JSONArray)
{
	if(typeof JSONArray.LEDCols !== "undefined") //do this first so that options are available
	{
		loadLEDColorSelector();
	}

//	if(typeof JSONArray.NumLED !== "undefined")
//		document.getElementById("numledsinchain").innerHTML = JSONArray.NumLED; 
	updateNumLampsTotal();
	if(typeof JSONArray.Brightness !== "undefined")
		if(typeof JSONArray.Brightness.PercentVal !== "undefined")
		{
			document.getElementById("mybrightness").value = JSONArray.Brightness.PercentVal;
			document.getElementById("globalbrightness").innerHTML = JSONArray.Brightness.PercentVal.toString() + "  [%]";
		}
	if(typeof JSONArray.Fading !== "undefined")
		if(typeof JSONArray.Fading.PercentVal !== "undefined")
		{
			document.getElementById("myfading").value = JSONArray.Fading.PercentVal;
			document.getElementById("globalfading").innerHTML = JSONArray.Fading.PercentVal.toString() + "  [%]";
		}
		if (testProc < 0)
			testProc = setInterval(runTestSequence, 1000);
	
}	

function sendLEDcommand(LEDStr, rgbVal)
{
	var jsonRequest = JSON.parse('{"LED":[' + LEDStr + '],"Color":' + JSON.stringify(rgbVal) + '}');
	var requestStr = "{\"Cmd\":\"Display\",\"CmdParams\":[\"DisplayData\"],\"JSONData\": " + JSON.stringify(jsonRequest) + "}";
	ws.send(requestStr);
}

function runTestSequence() //switch on
{
	var newCol = {"Color":[0,0,0,0]};
	var maxLED = Math.max(10, parseInt(document.getElementById("numledsinchain").innerHTML));
	var sendData = false;
	if (doTestSequence)
	{
		currTestCol = (currTestCol + 1) % 4;
		sendData = true;
		if (currTestCol > 0)
			newCol.Color[currTestCol-1] = Math.round(255 * signalData.Brightness.PercentVal / 100);
	}
	else
	  if (currTestCol > 0)
	  {
		  currTestCol = 0;
		  sendData = true;
	  }
	if (sendData)
	{	
		var jsonRequest = JSON.parse('{"Num":' + maxLED.toString() + ',"Color":' + JSON.stringify(newCol.Color) + '}');
		var requestStr = "{\"Cmd\":\"Display\",\"CmdParams\":[\"DisplayTest\"],\"JSONData\": " + JSON.stringify(jsonRequest) + "}";
		console.log(currTestCol, requestStr);
		ws.send(requestStr);
	}
}

function testLEDChain(fromElement) //switchoff
{
	if (fromElement.checked)
	{
		if (!useEmulator)
		{
			alert("Please activate Test Mode. then try again");
			fromElement.checked = false;
			return;
		}
	}
	doTestSequence = fromElement.checked; 
}

function testLEDChainColor(testButton)
{
	var newCol = {"Color":[0,0,0,0]};
	var maxLED = Math.max(10, parseInt(document.getElementById("numledsinchain").innerHTML));
	var jsonRequest = JSON.parse('{"Num":' + maxLED.toString() + ',"Color":' + JSON.stringify(newCol.Color) + '}');
	var requestStr = "{\"Cmd\":\"Display\",\"CmdParams\":[\"DisplayTest\"],\"JSONData\": " + JSON.stringify(jsonRequest) + "}";
	ws.send(requestStr);
	console.log("Test LED chain color done", requestStr);
}

function testLEDChainMouse(testButton)
{
	if (!useEmulator)
	{
		alert("Please activate Test Mode");
		return;
	}
	document.getElementById("cbRunTest").checked = false;
	doTestSequence = false;
	var newCol = {"Color":[0,0,0,0]};
	var maxLED = Math.max(10, parseInt(document.getElementById("numledsinchain").innerHTML));
	var cStr = document.getElementById("dlgColor").value;
	var hlpVal = parseInt(cStr.replace('#', ''), 16);
	newCol.Color[0] = Math.round(((hlpVal & 0xFF0000)>>16) * signalData.Brightness.PercentVal / 100);
	newCol.Color[1] = Math.round(((hlpVal & 0x00FF00)>>8) * signalData.Brightness.PercentVal / 100);
	newCol.Color[2] = Math.round((hlpVal & 0x0000FF) * signalData.Brightness.PercentVal / 100);
	var jsonRequest = JSON.parse('{"Num":' + maxLED.toString() + ',"Color":' + JSON.stringify(newCol.Color) + '}');
	var requestStr = "{\"Cmd\":\"Display\",\"CmdParams\":[\"DisplayTest\"],\"JSONData\": " + JSON.stringify(jsonRequest) + "}";
//	alert(requestStr);
	ws.send(requestStr);
	console.log("Test LED chain color", requestStr);
}

function handleBrightness(thisInput, value)
{
	signalData.Brightness.PercentVal = value;
	document.getElementById("globalbrightness").innerHTML = value.toString() + "  [%]";
}

function handleFading(thisInput, value)
{
	signalData.Fading.PercentVal = value;
	document.getElementById("globalfading").innerHTML = value.toString() + "  [%]";
}

function loadColor()
{
	var colName = document.getElementById("namedcolorlist").value;
	colorindex = signalData.LEDCols.findIndex(function(item, i){return item.Name === colName});
	document.getElementById("inp_coloredit").value = colName;
	document.getElementById("dlgColor").value = getColorOfName(colName);
//	document.getElementById("cbBlink").checked = signalData.LEDCols[colorindex].RGBVal[3] > 0;
//	document.getElementById("newblinkfreq").value = signalData.LEDCols[colorindex].RGBVal[3];
}

function addNewColor()
{
	console.log("addNewColor");
	var colHead = signalData.LEDCols;
	var newCol = document.getElementById("inp_coloredit").value;
	var index = colHead.findIndex(function(item, i){return item.Name === newCol});
	if (index < 0) //check if same color not already exists
	{
		if (confirm("Create a new Color named " + newCol + "?"))
		{
			var newColObj = JSON.parse(templateLEDColObject);
			newColObj.Name = newCol;
			var newIndex = colHead.push(newColObj);
			colorindex = newIndex;
			loadLEDColorSelector();
			document.getElementById("namedcolorlist").value = newColObj.Name;
		}
	}
	document.getElementById("namedcolorlist").value = newCol;
	loadColor();
	console.log(colHead);
}

function changeColor()
{
	console.log("Change Color");
	var colHead = signalData.LEDCols;
	var newCol = document.getElementById("inp_coloredit").value;
	var oldCol = document.getElementById("namedcolorlist").options[colorindex].text;
	var index = colHead.findIndex(function(item, i){return item.Name === newCol});
	if (index < 0)
	{
		if (confirm("Rename " + oldCol + " to " + newCol + "?"))
		{
			var listIndex = colHead.findIndex(function(item, i){return item.Name === oldCol});
			colHead[listIndex].Name = newCol;
			loadLEDColorSelector();
			document.getElementById("namedcolorlist").value = newCol;
			loadColor();
		}
	}
}

function deleteColor()
{
	console.log("deleteColor");
}

function saveColor()
{
	
	var cStr = document.getElementById("dlgColor").value;
	var hlpVal = parseInt(cStr.replace('#', ''), 16);
//	console.log(signalData.LEDCols[colorindex].RGBVal,hlpVal);
	signalData.LEDCols[colorindex].RGBVal[0] = (hlpVal & 0xFF0000)>>16;
	signalData.LEDCols[colorindex].RGBVal[1] = (hlpVal & 0x00FF00)>>8;
	signalData.LEDCols[colorindex].RGBVal[2] = (hlpVal & 0x0000FF);
//	if (document.getElementById("cbBlink").checked)
//		signalData.LEDCols[colorindex].RGBVal[3] = parseInt(document.getElementById("newblinkfreq").value);
//	else
//		signalData.LEDCols[colorindex].RGBVal[3] = 0;
//	console.log(signalData.LEDCols[colorindex].RGBVal);
	
}

function loadLEDColorSelector()
{
	function createBDOptions(dropdownlist)
	{
		while (dropdownlist.length > 0)
			dropdownlist.remove(0);
		for (var i=0; i<signalData.LEDCols.length;i++)
		{
			var option = document.createElement("option");
			option.value = signalData.LEDCols[i].Name;
			option.text = signalData.LEDCols[i].Name;
			dropdownlist.appendChild(option);
		}
	}
		
	var cellInside = document.getElementById("namedcolorlist");
	if (cellInside !== "undefined")
		createBDOptions(cellInside);
	DisplaySigSetupUpdateData(signalData);
}
