var sigTable;
var signalDataCopy = null;
var sigHead = null;
var simuList = [];
var selectedElement = false;
var lastMouseX;
var lastMouseY;
var moveMode = -1;
var xRatio;
var yRatio;
var blinkFlag = false;
var blinkProc = -1;
var emulProc = -1;

const selectorBox = 6;



const templateSigObj = JSON.stringify({"SwitchAddr": [], "AddrMode": 2,	"TransMode": 1, "ChgDelay": 0, "Display": {"Panel": {"h": 50,"w": 50,"r": 25},"Lamps": [{"Ind": 0,"xPos": 0,"yPos": 0,"Dia": 25}],"activeLamp": 0,"activeAspect": 0},"Aspects": [{"IntVal": [0, 0],"Lamps": [{"Color": "","Blink": 0}]}, {"IntVal": [1, 3],"Lamps": [{"Color": "","Blink": 0}]}, {"IntVal": [4, 6],"Lamps": [{"Color": "","Blink": 0}]}, {"IntVal": [7, 31],"Lamps": [{"Color": "","Blink": 0}]}]});
const templateLampObj = JSON.stringify({"Ind":0,"xPos":0, "yPos":0, "Dia":25});
const templateAspectObj = JSON.stringify({"IntVal":[0,0],"Lamps":[]});
const templateAspectLampObj = JSON.stringify({"Color":"#FF0000", "Blink":0});
const templateSimuLED = JSON.stringify({"Color":{}, "CurrColor":{}, "Blink":0});

function DisplaySigSetupInitialData(JSONArray)
{
	DisplayConfigUpdateData(JSONArray);
}	

function DisplaySigSetupUpdateData(JSONArray)
{
	if(typeof JSONArray.SigHeads !== "undefined")
	{
		while (sigTable.hasChildNodes())
			sigTable.removeChild(sigTable.childNodes[0]);
		signalDataCopy = JSON.parse(JSON.stringify(JSONArray)); //make a duplicate to work with
		sigHead = signalDataCopy.SigHeads;
		console.log("Num Sig: ", sigHead.length);
		for (i=0; i < sigHead.length; i++)
		{
			if(typeof sigHead[i].Display.activeLamp === "undefined")
			{
				sigHead[i].Display.activeLamp = 0;
			}
			if(typeof sigHead[i].Display.activeAspect === "undefined")
			{
				sigHead[i].Display.activeAspect = 0;
			}

			sigTable.append(createSigTableRow("tablepaneldiv", i));
			loadSigColorSelector(sigHead[i]);
			loadSignalData(i, sigHead[i]);
			
		}
		updateNumLampsTotal();
		if (blinkProc < 0)
			blinkProc = setInterval(blinkLamps, 500);
		if (emulProc < 0)
			emulProc = setInterval(processEmulator, 100);
	}
}	

function addNewAspect(thisButton, event)
{
	var sigNum = parseInt(thisButton.getAttribute("sigNum"));
	var aspNum = sigHead[sigNum].Aspects.length;
	var numLamps = sigHead[sigNum].Display.Lamps.length;
	if (aspNum < 36)
	{
		updateAspectsAndLamps(sigHead[sigNum], aspNum+1, numLamps);
		loadTruthTable(sigNum, sigHead[sigNum]);
		document.getElementById("aspectRB_" + sigNum.toString() + "_" + aspNum.toString()).onclick();
		document.getElementById("aspectRB_" + sigNum.toString() + "_" + aspNum.toString()).checked = 1;
	}
}

function deleteThisAspect(thisAspect, event)
{
	var sigNum = parseInt(thisAspect.getAttribute("sigNum"));
	var aspNum = parseInt(thisAspect.getAttribute("aspectNum"));
	var numAsps = sigHead[sigNum].Aspects.length;
	if (numAsps > 1)
	{
		sigHead[sigNum].Aspects.splice(aspNum,1);
		loadTruthTable(sigNum, sigHead[sigNum]);
		aspNum--;
		document.getElementById("aspectRB_" + sigNum.toString() + "_" + aspNum.toString()).onclick();
		document.getElementById("aspectRB_" + sigNum.toString() + "_" + aspNum.toString()).checked = 1;
	}
	else
		alert("Sorry, the last and only aspect of a signal can't be deleted'");
}

function deleteThisSignal(thisCancelButton)
{
	var sigNum = parseInt(thisCancelButton.getAttribute("name"));
	if (confirm("Do you really want to delete signal " + (sigNum+1) + "?"))
	{
		signalDataCopy.SigHeads.splice(sigNum,1);
		var dataCopy = JSON.parse(JSON.stringify(signalDataCopy));
		DisplaySigSetupUpdateData(dataCopy);
//		updateNumLampsTotal();
	}
}

function createAddSignal()
{
	var newSignal = JSON.parse(templateSigObj);
	signalDataCopy.SigHeads.push(newSignal);
	var dataCopy = JSON.parse(JSON.stringify(signalDataCopy));
	DisplaySigSetupUpdateData(dataCopy);
//	updateNumLampsTotal();
}

function moveUp(thisSignal,event)
{
	var signal = parseInt(thisSignal.getAttribute("name"));
	if (signal > 0)
	{
		var moveSig = sigHead.splice(signal, 1);
		sigHead.splice(signal-1, 0, moveSig[0]);
		var dataCopy = JSON.parse(JSON.stringify(signalDataCopy));
		DisplaySigSetupUpdateData(dataCopy);
	}
}

function moveDown(thisSignal,event)
{
	var signal = parseInt(thisSignal.getAttribute("name"));
	if (signal < sigHead.length)
	{
		var moveSig = sigHead.splice(signal, 1);
		sigHead.splice(signal+1, 0, moveSig[0]);
		var dataCopy = JSON.parse(JSON.stringify(signalDataCopy));
		DisplaySigSetupUpdateData(dataCopy);
	}
}

function copyElement(thisSignal,event)
{
	var signal = thisSignal.getAttribute("name");
	var newSignal = JSON.parse(JSON.stringify(sigHead[parseInt(signal)]));
	var addrOfs = newSignal.SwitchAddr.length;
	for (var i = 0; i < addrOfs; i++)
		newSignal.SwitchAddr[i] += addrOfs;
	sigHead.splice(parseInt(signal)+1, 0, newSignal);
	var dataCopy = JSON.parse(JSON.stringify(signalDataCopy));
	DisplaySigSetupUpdateData(dataCopy);
}

function newNumLed(thisSignal, event)
{
	var signal = thisSignal.getAttribute("name");
	var thisSigHead = sigHead[parseInt(signal)];
	var numLamps = parseInt(document.getElementById("numled" + signal).value);
	if (isNaN(numLamps))
	{
		alert("Error in Number of Lamps. Must be a single numerical value");
		console.log(numLamps);
		return false;
	}
	while (thisSigHead.Display.Lamps.length > numLamps)
		thisSigHead.Display.Lamps.pop();
	while (thisSigHead.Display.Lamps.length < numLamps)
	{
		console.log("add new Lamp");
		var newLamp = JSON.parse(templateLampObj);
		newLamp.Ind = thisSigHead.Display.Lamps.length;
		newLamp.xPos = (5*thisSigHead.Display.Lamps.length) - (thisSigHead.Display.Panel.w>>1);
		newLamp.yPos = (5*thisSigHead.Display.Lamps.length) - (thisSigHead.Display.Panel.h>>1);
		thisSigHead.Display.Lamps.push(newLamp);
	}
	var numAspects = thisSigHead.Aspects.length;
    updateAspectsAndLamps(thisSigHead, numAspects, numLamps);
	requestAnimationFrame(function(timestamp){
		starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
		drawSignalHead(parseInt(signal));
	})
	updateNumLampsTotal();
//	console.log(thisSigHead);
}

function updateNumLampsTotal()
{
	var numLamps = 0;
	for (var i=0; i<sigHead.length; i++)
		numLamps += sigHead[i].Display.Lamps.length;
	document.getElementById("numledsinchain").innerHTML = numLamps.toString();
	while (simuList.length > numLamps)
		simuList.pop();
	while (simuList.length < numLamps)
		simuList.push(JSON.parse(templateSimuLED));
}


function newInpAddr(thisSignal, event)
{
	var signal = thisSignal.getAttribute("name");
	var thisSigHead = sigHead[parseInt(signal)];
	try
	{
		var addrStr = JSON.parse('[' + document.getElementById("inpaddr" + signal).value + ']');
	}
	catch
	{
		alert("Error in Addresses. Must be a list of numerical values separated by ','");
		console.log(addrStr);
		return false;
	}
	if (addrStr.length > 4)
	{
		alert("Error in Addresses. The maximum of supported addresses is 4");
		return false;
	}
	thisSigHead.SwitchAddr = addrStr;
	newAddrMode(thisSignal, event); //this updates number of aspects, lamps based on new address and reloads the truth table
	
//	loadTruthTable(parseInt(signal), thisSigHead);
//	console.log(thisSigHead);
}

function updateAspectsAndLamps(thisSigHead, numAspects, numLamps)
{
	while (thisSigHead.Aspects.length > numAspects)
		thisSigHead.Aspects.pop();
	while (thisSigHead.Aspects.length < numAspects)
	{
		var newAspect = JSON.parse(templateAspectObj);
		thisSigHead.Aspects.push(newAspect);
	}
	for (var i=0; i<thisSigHead.Aspects.length;i++)
	{
		while (thisSigHead.Aspects[i].Lamps.length > numLamps)
			thisSigHead.Aspects[i].Lamps.pop();
		while (thisSigHead.Aspects[i].Lamps.length < numLamps)
		{
			var newAspectLamp = JSON.parse(templateAspectLampObj);
			thisSigHead.Aspects[i].Lamps.push(newAspectLamp);
		}
	}
}

function newAddrMode(thisSignal, event)
{
	var signal = thisSignal.getAttribute("name").replace(/[^0-9]/g, '');
	var thisSigHead = sigHead[parseInt(signal)];

	var addrMode = 0;
	if (document.getElementById("rbStatSwi" + signal).checked)
		addrMode = 1;
	else
		if (document.getElementById("rbSig" + signal).checked)
			addrMode = 2;
			
	thisSigHead.AddrMode = addrMode;
	var numAspects = 0;
	switch (addrMode)
	{
		case 0 : numAspects = thisSigHead.SwitchAddr.length * 2;
				 break;
		case 1 : numAspects = Math.pow(2, thisSigHead.SwitchAddr.length);
				 break;
		case 2 : numAspects = 4;
				 break;
	}
    var numLamps = thisSigHead.Display.Lamps.length;
    updateAspectsAndLamps(thisSigHead, numAspects, numLamps);
	loadTruthTable(parseInt(signal), thisSigHead);
	console.log(thisSigHead);
}

function newTransMode(thisSignal, event)
{
	signal = thisSignal.getAttribute("name").replace(/[^0-9]/g, '');
	var thisSigHead = sigHead[parseInt(signal)];
	thisSigHead.TransMode = (document.getElementById("rbSoft" + signal).checked) ? 1:0;
}

function newchgDelay(thisSignal, event)
{
	signal = thisSignal.getAttribute("name").replace(/[^0-9]/g, '');
	var thisSigHead = sigHead[parseInt(signal)];
	var transDelay = parseInt(document.getElementById("chgdelay" +signal).value);
	if (isNaN(transDelay))
	{
		alert("Error in Change Delay time. Must be a single numerical value");
		console.log(numLamps);
		return false;
	}
	thisSigHead.ChgDelay = transDelay;
	console.log(transDelay);
}


/**
 * Draws a rounded rectangle using the current state of the canvas. 
 * If you omit the last three params, it will draw a rectangle 
 * outline with a 5 pixel border radius 
 * @param {CanvasRenderingContext2D} ctx
 * @param {Number} x The top left x coordinate
 * @param {Number} y The top left y coordinate 
 * @param {Number} width The width of the rectangle 
 * @param {Number} height The height of the rectangle
 * @param {Number} radius The corner radius. Defaults to 5;
 * @param {Boolean} fill Whether to fill the rectangle. Defaults to false.
 * @param {Boolean} stroke Whether to stroke the rectangle. Defaults to true.
 */
function roundRect(ctx, x, y, width, height, radius, fill, stroke) {
  if (typeof stroke == "undefined" ) {
    stroke = true;
  }
  if (typeof radius === "undefined") {
    radius = 5;
  }
  var topY = y - (height>>1);
  var botY = y + (height>>1);
  var leftX = x - (width>>1);
  var rightX = x + (width>>1);
  
  ctx.beginPath();
  ctx.moveTo(leftX + radius, topY);
  ctx.lineTo(rightX - radius, topY);
  ctx.arc(rightX - radius, topY + radius, radius, 1.5 * Math.PI, 2 * Math.PI);
  ctx.lineTo(rightX, botY - radius);
  ctx.arc(rightX - radius, botY - radius, radius, 0, 0.5 * Math.PI);
  ctx.lineTo(leftX + radius, botY);
  ctx.arc(leftX + radius, botY - radius, radius, 0.5 * Math.PI, Math.PI);
  ctx.lineTo(leftX, topY + radius);
  ctx.arc(leftX + radius, topY + radius, radius, Math.PI, 1.5*Math.PI);
  ctx.closePath();
  if (stroke) {
    ctx.stroke();
  }
  if (fill) {
    ctx.fill();
  }        
}

function findElement(thisCanvas, x, y)
{
	var bounds = thisCanvas.getBoundingClientRect();
	xRatio = thisCanvas.width/bounds.width;
	yRatio = thisCanvas.height/bounds.height;
	var display = sigHead[parseInt(thisCanvas.getAttribute("name"))].Display;
	for (var i=0; i<display.Lamps.length; i++)
	{
		var diaOfsX = (display.Lamps[i].Dia>>1) / xRatio;
		var diaOfsY = (display.Lamps[i].Dia>>1) / yRatio;
		if ((x>((display.Lamps[i].xPos /xRatio) - diaOfsX)) && (x<((display.Lamps[i].xPos/xRatio) + diaOfsX)) && (y>((display.Lamps[i].yPos/yRatio) - diaOfsY)) && (y<((display.Lamps[i].yPos/yRatio) + diaOfsY)))
			return display.Lamps[i];
	}
	var diaOfsX = (display.Panel.w>>1) / xRatio;
	var diaOfsY = (display.Panel.h>>1) / yRatio;
	if ((x>(-diaOfsX-(selectorBox>>1))) && (x<diaOfsX+(selectorBox>>1)) && (y>(-diaOfsY-(selectorBox>>1))) && (y<diaOfsY+(selectorBox>>1)))
		return display.Panel;
	return false;
}

function findMoveHandle(thisCanvas, x, y)
{
	var bounds = thisCanvas.getBoundingClientRect();
	xRatio = thisCanvas.width/bounds.width;
	yRatio = thisCanvas.height/bounds.height;
	var display = sigHead[parseInt(thisCanvas.getAttribute("name"))].Display;
	var diaOfsX = (display.Panel.w>>1) / xRatio;
	var diaOfsY = (display.Panel.h>>1) / yRatio;
	var pickBoxOfsX = (selectorBox>>1) / xRatio;
	var pickBoxOfsY = (selectorBox>>1) / yRatio;
	var radOfsX = display.Panel.r / xRatio;
	if ((x <= (diaOfsX + pickBoxOfsX)) && (x >= (diaOfsX - pickBoxOfsX)) && (y >= (-pickBoxOfsY)) && (y <= (pickBoxOfsY)))
		return 1; //horizontal size
	if ((x <= (pickBoxOfsX)) && (x >= (-pickBoxOfsX)) && (y >= (diaOfsY - pickBoxOfsY)) && (y <= (diaOfsY + pickBoxOfsY)))
		return 2; //vertical size
	console.log(x,y, diaOfsX - radOfsX, -diaOfsY);
	if ((x <= (diaOfsX - radOfsX + pickBoxOfsX)) && (x >= (diaOfsX - radOfsX - pickBoxOfsX)) && (y >= (-diaOfsY - pickBoxOfsY)) && (y <= (-diaOfsY + pickBoxOfsY)))
		return 3; //radius picker
    return -1;
}

function updateElementSelection(newSelection)
{
	var oldElement = selectedElement;
	if (oldElement != newSelection)
	{
		var needUpdate = false;
		selectedElement = false;
		for (var i = 0; i<sigHead.length; i++)
		{
			if (oldElement == sigHead[i].Display.Panel)
			{
				needUpdate = true;
			} else
				if (newSelection == sigHead[i].Display.Panel)
				{
					selectedElement = newSelection;
					needUpdate = true;
				};
			for (var j = 0; j<sigHead[i].Display.Lamps.length; j++)
			{
				if (newSelection == sigHead[i].Display.Lamps[j])
				{
					sigHead[i].Display.activeLamp = j;
					selectedElement = newSelection;
					if ((sigHead[i].Display.activeAspect >= 0) && (sigHead[i].Display.activeLamp >= 0))
						loadLampData(i,sigHead[i].Display.activeAspect, sigHead[i].Display.activeLamp);
					needUpdate = true;
				}
				
			}
		}
		if (needUpdate)
		{
			requestAnimationFrame(function(timestamp)
			{
				starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
				drawAllSignalHeads();
			});
		}
	}
}

function mouseClick(thisCanvas, thisEvent)
{
//	console.log("Mouse click " + thisCanvas.id);
	var rect = thisCanvas.getBoundingClientRect();
	var centerX = rect.left + (rect.width>>1);
	var centerY = rect.top + (rect.height>>1);
	updateElementSelection(findElement(thisCanvas, (thisEvent.clientX - centerX), (thisEvent.clientY - centerY)));
}

function handleMouse(thisEvent)
{
	if (thisEvent.type == "mouseup")
	{
		document.removeEventListener("mouseup", handleMouse);
		document.removeEventListener("mousemove", handleMouse);
	}
	if (thisEvent.type == "mousemove")
	{
		var deltaX = thisEvent.clientX - lastMouseX;
		var deltaY = thisEvent.clientY - lastMouseY;
		lastMouseX = thisEvent.clientX;
		lastMouseY = thisEvent.clientY;
		var needUpdate = false;
		switch (moveMode)
		{
			case 0: selectedElement.xPos = selectedElement.xPos + (deltaX * xRatio);
					selectedElement.yPos = selectedElement.yPos + (deltaY * yRatio);
					needUpdate = true;
					break;
			case 1: selectedElement.w = selectedElement.w + (deltaX * xRatio * 2);
					needUpdate = true;
					break;
			case 2: selectedElement.h = selectedElement.h + (deltaY * yRatio * 2);
					needUpdate = true;
					break;
			case 3: selectedElement.r = Math.min(Math.max(selectedElement.r - (deltaX * xRatio), 0), (selectedElement.w>>1));
					needUpdate = true;
					break;
			default: break;
		}
		if (needUpdate)
		{
			requestAnimationFrame(function(timestamp)
			{
				starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
				drawAllSignalHeads();
			});
		}
	}
}

function mouseDown(thisCanvas, thisEvent)
{
	if (selectedElement)
	{
		var rect = thisCanvas.getBoundingClientRect();
		var centerX = rect.left + (rect.width>>1);
		var centerY = rect.top + (rect.height>>1);
		if (findElement(thisCanvas, (thisEvent.clientX - centerX), (thisEvent.clientY - centerY)) == selectedElement)
		{
			lastMouseX = thisEvent.clientX;
			lastMouseY = thisEvent.clientY;
			if (sigHead[parseInt(thisCanvas.getAttribute("name"))].Display.Lamps.indexOf(selectedElement) >= 0) //this is a lamps
			{
				moveMode = 0;
			}
			else // this is a panel
			{
				if (sigHead[parseInt(thisCanvas.getAttribute("name"))].Display.Panel == selectedElement) //this is still the same lamps
				{
					moveMode = findMoveHandle(thisCanvas, (thisEvent.clientX - centerX), (thisEvent.clientY - centerY))
				}
				else
					moveMode = -1;
			}
			if (moveMode >= 0)
			{
				document.addEventListener("mouseup", handleMouse);
				document.addEventListener("mousemove", handleMouse);
			}
		}
	}
}

function blinkLamps()
{
	blinkFlag = !blinkFlag;
	requestAnimationFrame(function(timestamp)
	{
		starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
		drawAllSignalHeads();
	});
}

function processEmulator()
{
	if ((useEmulator) && (!doTestSequence))
		for (var i=0; i < simuList.length; i++)
		{
			var newCol = JSON.parse(JSON.stringify(simuList[i].Color));
			if (((simuList[i].Blink > 0) && (!blinkFlag)) || ((simuList[i].Blink < 0) && (blinkFlag)))
				newCol.RGBVal = [0,0,0];
			else
				newCol.RGBVal = simuList[i].Color.RGBVal;
			if (newCol.RGBVal != simuList[i].CurrColor.RGBVal)
			{
				simuList[i].CurrColor = JSON.parse(JSON.stringify(newCol));
				sendLEDcommand(i, newCol.RGBVal);
			}
		}
}
		

function loadRemoteSignal(signalNum, aspectNum, lampNum)
{
	var baseLamp = 0;
	for (var i=0; i < signalNum; i++)
		baseLamp += sigHead[i].Display.Lamps.length;
	var numLamps = sigHead[signalNum].Display.Lamps.length;
	for (var i=0; i < numLamps; i++)
	{
		var colHead = signalData.LEDCols;
		var colStr = sigHead[signalNum].Aspects[aspectNum].Lamps[i].Color;
		var listIndex = Math.max(colHead.findIndex(function(item, i){return item.Name === colStr}),0); //make sure it gets something in case aspect is not yet configured
//		console.log(colHead, colStr, listIndex);
		var currCol = JSON.parse(JSON.stringify(colHead[listIndex]));
		for (var j=0; j<3;j++)
			currCol.RGBVal[j] = Math.round(currCol.RGBVal[j] * signalData.Brightness.PercentVal / 100);  
		simuList[baseLamp + i].Color = currCol;
		simuList[baseLamp + i].Blink = sigHead[signalNum].Aspects[aspectNum].Lamps[i].Blink;
//		console.log(simuList[i]);
	}
}



function loadLampData(signalNum, aspectNum, lampNum)
{
	var blinkPanel = document.getElementById("blinkp" + signalNum.toString());	
	if (lampNum < 0)
		lampNum = Math.max(blinkPanel.getAttribute('currLamp'),0);
		
	//load title
	var titleText = document.getElementById("lamptitle" + signalNum.toString());
	titleText.innerHTML = "Lamp Setup for Aspect " + aspectNum.toString() + ", Lamp " + lampNum.toString();
	//load color
	var colSel = document.getElementById("lampcolor" + signalNum.toString());
	colSel.value = sigHead[signalNum].Aspects[aspectNum].Lamps[lampNum].Color; 


	//load blink rate
	var blinkRate = document.getElementById("blinkrate" + signalNum.toString());
	blinkRate.value = sigHead[signalNum].Aspects[aspectNum].Lamps[lampNum].Blink; 
	
	//load active lamp
	sigHead[signalNum].Display.activeAspect = aspectNum;
	sigHead[signalNum].Display.activeLamp = lampNum;
	requestAnimationFrame(function(timestamp)
	{
		starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
		drawAllSignalHeads();
	});
	blinkPanel.setAttribute('currLamp', lampNum.toString());
}

function selectAspect(thisAspect,event)
{
	if (doTestSequence)
    {
		doTestSequence = false; //in case test sequence is runnig, stop it as it superceeds aspect display
		document.getElementById("cbRunTest").checked = false;
	}
	var signalNum = parseInt(thisAspect.getAttribute("sigNum"));
	var aspectNum = parseInt(thisAspect.getAttribute("aspectNum"));
    loadLampData(signalNum, aspectNum, -1);
	loadRemoteSignal(signalNum, aspectNum, -1);
}

function updateBlinkRate(thisEdit, event)
{
	//save blink rate
	var sigNum = parseInt(thisEdit.getAttribute("name"));
	var blinkPanel = document.getElementById("blinkp" + sigNum.toString());	
	var lampNum = parseInt(blinkPanel.getAttribute("currLamp"));
	var aspectNum = sigHead[sigNum].Display.activeAspect;
	sigHead[sigNum].Aspects[aspectNum].Lamps[lampNum].Blink = parseInt(thisEdit.value); 
}

function changeColor(thisList, event)
{
	var sigNum = parseInt(thisList.getAttribute("name"));
	var blinkPanel = document.getElementById("blinkp" + sigNum.toString());	
	var lampNum = parseInt(blinkPanel.getAttribute("currLamp"));
	var aspectNum = sigHead[sigNum].Display.activeAspect;
	sigHead[sigNum].Aspects[aspectNum].Lamps[lampNum].Color = thisList.value; 
	console.log(sigNum, aspectNum, lampNum, thisList.value);
	drawSignalHead(sigNum);

}

function changeAspectVal(thisEdit, event)
{
	var sigNum = parseInt(thisEdit.getAttribute("sigNum"));
	var aspectNum = parseInt(thisEdit.getAttribute("aspNum"));
	var colNum = parseInt(thisEdit.getAttribute("colNum"));
	sigHead[sigNum].Aspects[aspectNum].IntVal[colNum] = parseInt(thisEdit.value);
}

function loadSigColorSelector(sigObject)
{
	function createSigOptions(dropdownlist)
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
	var rowindex = sigObject.InpNum;
	var cellInside = document.getElementById("lampcolor" + sigHead.indexOf(sigObject).toString());
	if (cellInside !== "undefined")
	{
		createSigOptions(cellInside);
		cellInside.value  = ""; 
	}
}

function drawAllSignalHeads()
{
	for (var i=0; i < sigHead.length; i++)
		drawSignalHead(i);
}

function drawSignalHead(ofRow)
{
//	console.log("Looking: ", 'sigcanvas' + ofRow.toString());
	var basePane = document.getElementById('sigcanvas' + ofRow.toString());
	var centerX = basePane.width>>1;
	var centerY = basePane.height>>1;
	var ctx = basePane.getContext("2d");
	ctx.clearRect(0, 0, basePane.width, basePane.height);
    ctx.lineWidth = 5;
    if (selectedElement == sigHead[ofRow].Display.Panel)
		ctx.strokeStyle = "rgb(255, 0, 0)";
	else
		ctx.strokeStyle = "rgb(255, 255, 255)";
	ctx.fillStyle = "rgba(0, 0, 0, 1)";
	roundRect(ctx, centerX, centerY, sigHead[ofRow].Display.Panel.w, sigHead[ofRow].Display.Panel.h, sigHead[ofRow].Display.Panel.r, true);
    if (selectedElement == sigHead[ofRow].Display.Panel)
    {
//		roundRect(ctx, centerX - (sigHead[ofRow].Display.Panel.w>>1), centerY, selectorBox, selectorBox, 0, true);
		roundRect(ctx, centerX + (sigHead[ofRow].Display.Panel.w>>1), centerY, selectorBox, selectorBox, 0, true);
//		roundRect(ctx, centerX, centerY - (sigHead[ofRow].Display.Panel.h>>1), selectorBox, selectorBox, 0, true);
		roundRect(ctx, centerX, centerY + (sigHead[ofRow].Display.Panel.h>>1), selectorBox, selectorBox, 0, true);

		ctx.strokeStyle = "rgb(255, 255, 0)";
		roundRect(ctx, centerX + (sigHead[ofRow].Display.Panel.w>>1) - sigHead[ofRow].Display.Panel.r, centerY - (sigHead[ofRow].Display.Panel.h>>1), selectorBox, selectorBox, 0, true);
	}
    ctx.lineWidth = 2;
	ctx.strokeStyle = "rgb(255, 255, 255)";
	ctx.fillStyle = "rgba(50, 50, 50, 1)";
	for (var i=0; i < sigHead[ofRow].Display.Lamps.length; i++)
	{
		if ((sigHead[ofRow].Display.activeLamp >= 0) && (sigHead[ofRow].Display.activeAspect >= 0))
			ctx.fillStyle = getColorOfName(sigHead[ofRow].Aspects[sigHead[ofRow].Display.activeAspect].Lamps[i].Color);
		if (i == sigHead[ofRow].Display.activeLamp)
		{
			ctx.lineWidth = 4;
			ctx.strokeStyle = "rgb(255, 0, 0)";
		}
		else
		{
			ctx.lineWidth = 2;
			ctx.strokeStyle = "rgb(255, 255, 255)";
		}
		if ((sigHead[ofRow].Aspects[sigHead[ofRow].Display.activeAspect].Lamps[i].Blink > 0) && (!blinkFlag))
			ctx.fillStyle = "rgba(0, 0, 0, 1)";
		if ((sigHead[ofRow].Aspects[sigHead[ofRow].Display.activeAspect].Lamps[i].Blink < 0) && (blinkFlag))
			ctx.fillStyle = "rgba(0, 0, 0, 1)";
		
		roundRect(ctx, centerX + sigHead[ofRow].Display.Lamps[i].xPos, centerY  + sigHead[ofRow].Display.Lamps[i].yPos, sigHead[ofRow].Display.Lamps[i].Dia, sigHead[ofRow].Display.Lamps[i].Dia, sigHead[ofRow].Display.Lamps[i].Dia>>1, true);
	}
}

function createStaticTruthTable(holder, ofRow)
{
	var swiArray = JSON.parse("{\"Addr\" :" + "[" + document.getElementById("inpaddr" + ofRow.toString()).value + "]}");
	var numCols = Math.min(swiArray.Addr.length, 4); //only first 4 addresses are considered
	var numRows = Math.pow(2, numCols);
	numCols++;
	var tt = document.createElement("table");
	var newRow = document.createElement("tr");
	tt.append(newRow);
	for (var j = 0; j < numCols; j++)
	{
		var newCol = document.createElement("td");
		switch (j)
		{
			case 0: var newRB = document.createTextNode("Select");
					newCol.append(newRB);
					break;
			default:var newTitle = document.createTextNode("Swi " + swiArray.Addr[j-1].toString());
					newCol.append(newTitle);
					break;
		}
		newRow.append(newCol);
	}


	for (var i = 0; i < numRows; i++)
	{
		var newText;
		newRow = document.createElement("tr");
		tt.append(newRow);
		for (var j = 0; j < numCols; j++)
		{
			newCol = document.createElement("td");
			switch (j)
			{
				case 0: var newRB = document.createElement("input");
						newRB.setAttribute("type", "radio");
						newRB.setAttribute("name", "aspectRB" + ofRow.toString());
						newRB.setAttribute("id", "aspectRB_" + ofRow.toString() + "_" + i.toString());
						newRB.setAttribute("sigNum", ofRow.toString());
						newRB.setAttribute("aspectNum", i.toString());
						newRB.setAttribute("onclick", "selectAspect(this,event)");
						newRB.checked = (i==0);
						newCol.append(newRB);
						newText =  document.createTextNode(i.toString());
						newCol.append(newText);
						break;
				default:newText = document.createTextNode((((0x0001<<(j-1)) & i) > 0) ? "Closed":"Thrown");
						newCol.append(newText);
						break;
			}
			newRow.append(newCol);
		}
	}
	holder.append(tt);
}

function createDynamicTruthTable(holder, ofRow)
{
	var swiArray = JSON.parse("{\"Addr\" :" + "[" + document.getElementById("inpaddr" + ofRow.toString()).value + "]}");
	var numCols = Math.min(swiArray.Addr.length, 4); //only first 4 addresses are considered
	var numRows = 2 * numCols;
	numCols++;
	var tt = document.createElement("table");
	var newRow = document.createElement("tr");
	tt.append(newRow);
	for (var j = 0; j < numCols; j++)
	{
		var newCol = document.createElement("td");
		switch (j)
		{
			case 0: var newRB = document.createTextNode("Select");
					newCol.append(newRB);
					break;
			default:var newTitle = document.createTextNode("Swi " + swiArray.Addr[j-1].toString());
					newCol.append(newTitle);
					break;
		}
		newRow.append(newCol);
	}


	for (var i = 0; i < numRows; i++)
	{
		var newText;
		newRow = document.createElement("tr");
		tt.append(newRow);
		for (var j = 0; j < numCols; j++)
		{
			newCol = document.createElement("td");
			switch (j)
			{
				case 0: var newRB = document.createElement("input");
						newRB.setAttribute("type", "radio");
						newRB.setAttribute("name", "aspectRB"  + ofRow.toString());
						newRB.setAttribute("id", "aspectRB_" + ofRow.toString() + "_" + i.toString());
						newRB.setAttribute("sigNum", ofRow.toString());
						newRB.setAttribute("aspectNum", i.toString());
						newRB.setAttribute("onclick", "selectAspect(this,event)");
						newRB.checked = (i==0);
						newCol.append(newRB);
						newText =  document.createTextNode(i.toString());
						newCol.append(newText);
						break;
				default:
//						if ((i>>1) == (j-1))
							newText = document.createTextNode(((i>>1) == (j-1)) ? (((i & 0x01) > 0) ? "Closed":"Thrown") : "");
//						else
//							newText = document.createTextNode("");
						newCol.append(newText);
						break;
			}
			newRow.append(newCol);
		}
	}

	holder.append(tt);
}

function createSignalTruthTable(holder, ofRow)
{
	var swiArray = JSON.parse("{\"Addr\" :" + "[" + document.getElementById("inpaddr" + ofRow.toString()).value + "]}");
	var numCols = 4; 
	var numRows = sigHead[ofRow].Aspects.length;
	var tt = document.createElement("table");
	tt.setAttribute('name', ofRow.toString());
	var newRow = document.createElement("tr");
	tt.append(newRow);
	for (var j = 0; j < numCols; j++)
	{
		var newCol = document.createElement("td");
		var newText;
		switch (j)
		{
			case 0: newText = document.createTextNode("Select");
					break;
			case 1: newText = document.createTextNode("From");
					break;
			case 2: newText = document.createTextNode("To");
					break;
			case 3: newText = document.createTextNode("Cancel");
					break;
		}
		newCol.append(newText);
		newRow.append(newCol);
	}


	for (var i = 0; i < numRows; i++)
	{
		newRow = document.createElement("tr");
		tt.append(newRow);
		for (var j = 0; j < numCols; j++)
		{
			newCol = document.createElement("td");
			switch (j)
			{
				case 0: var newRB = document.createElement("input");
						newRB.setAttribute("type", "radio");
						newRB.setAttribute("name", "aspectRB"  + ofRow.toString());
						newRB.setAttribute("id", "aspectRB_" + ofRow.toString() + "_" + i.toString());
						newRB.setAttribute("sigNum", ofRow.toString());
						newRB.setAttribute("aspectNum", i.toString());
						newRB.setAttribute("onclick", "selectAspect(this,event)");
						newCol.append(newRB);
						newRB.checked = (i==0);
						break;
				case 3: var cellInside = document.createElement("img");
						cellInside.setAttribute('src', 'cancel-button.png');
						cellInside.setAttribute('alt', 'Cancel');
						cellInside.setAttribute('class', 'img_icon');
						cellInside.setAttribute('name', i.toString());
						cellInside.setAttribute("sigNum", ofRow.toString());
						cellInside.setAttribute("aspectNum", i.toString());
						cellInside.setAttribute("id", "asp_cancel" + i.toString());
						cellInside.onclick = function() {deleteThisAspect(this, event)};
						newCol.appendChild(cellInside);
						break;
				default:var newText = sigHead[ofRow].Aspects[i].IntVal[j-1].toString();
						var inputField = document.createElement("input");
						inputField.setAttribute('class', "numinputshort");	
						inputField.setAttribute("sigNum", ofRow.toString());
						inputField.setAttribute("aspNum", i.toString());
						inputField.setAttribute("colNum", (j-1).toString());
						inputField.setAttribute("name", (j-1).toString());
						inputField.setAttribute("onchange", "changeAspectVal(this, event)");
						inputField.setAttribute("id", "Destination" + ofRow.toString() + "_" + i.toString() + "_" + j.toString());
						inputField.value = newText;
						newCol.append(inputField);
						break;
			}
			newRow.append(newCol);
		}
	}
	newRow = document.createElement("tr");
	tt.append(newRow);
	newCol = document.createElement("td");
	var cellInside = document.createElement("button");
	cellInside.setAttribute('class', 'slim-button');
	cellInside.setAttribute('name', ofRow.toString());
	cellInside.setAttribute("sigNum", ofRow.toString());
	cellInside.setAttribute("id", "asp_add" + ofRow.toString());
	cellInside.setAttribute("onclick", "addNewAspect(this, event)");
	newRow.append(cellInside);
	var textElement = document.createTextNode("Add");
	cellInside.append(textElement);
	holder.append(tt);
}


function loadSignalData(thisPos, JSONArray)
{
	var thisIndex = thisPos + 1;
	document.getElementById("rowtitle" + thisPos.toString()).innerHTML = "Signal Head #" + thisIndex.toString();
	document.getElementById("numled" + thisPos.toString()).value = JSONArray.Display.Lamps.length;
	document.getElementById("inpaddr" + thisPos.toString()).value = JSON.stringify(JSONArray.SwitchAddr).substr(1).slice(0, -1);
	document.getElementById("rbDynSwi" + thisPos.toString()).checked = JSONArray.AddrMode == 0;
	document.getElementById("rbStatSwi" + thisPos.toString()).checked = JSONArray.AddrMode == 1;
	document.getElementById("rbSig" + thisPos.toString()).checked = JSONArray.AddrMode == 2;
	document.getElementById("rbHard" + thisPos.toString()).checked = JSONArray.TransMode == 0;
	document.getElementById("rbSoft" + thisPos.toString()).checked = JSONArray.TransMode == 1;
	document.getElementById("chgdelay" + thisPos.toString()).value = JSONArray.ChgDelay;
	loadContentPanel(thisPos, JSONArray);
}
	
function loadContentPanel(thisPos, JSONArray)
{
	loadTruthTable(thisPos, JSONArray);
	loadLampData(thisPos, 0, 0);
	requestAnimationFrame(function(timestamp){
		starttime = timestamp || new Date().getTime(); //if browser doesn't support requestAnimationFrame, generate our own timestamp using Date
		drawSignalHead(thisPos);
	})
}


function loadTruthTable(thisPos, JSONArray)
{
	var truthTable = document.getElementById("truthtable" + thisPos.toString());
//	var ttMode = parseInt(truthTable.getAttribute("ttmode"));
//	if (ttMode !== JSONArray.AddrMode)
	{
//		console.log(ttMode, JSONArray.AddrMode, "delete TT", truthTable.children.length);
		while (truthTable.hasChildNodes())
			truthTable.removeChild(truthTable.childNodes[0]);
//		console.log(ttMode, JSONArray.AddrMode, "deleted TT", truthTable.children.length);
	}
	switch (JSONArray.AddrMode)
	{
		case 0: document.getElementById("truthtabletitle" + thisPos.toString()).innerHTML = "Dynamic Address Table";
//				if (ttMode !== JSONArray.AddrMode)
				{
					truthTable.setAttribute('ttmode', JSONArray.AddrMode);
					createDynamicTruthTable(truthTable, thisPos);
				}
				break;
		case 1:	document.getElementById("truthtabletitle" + thisPos.toString()).innerHTML = "Static Address Table";
//				if (ttMode !== JSONArray.AddrMode)
				{
					truthTable.setAttribute('ttmode', JSONArray.AddrMode);
					createStaticTruthTable(truthTable, thisPos);
				}
				break;
		case 2: document.getElementById("truthtabletitle" + thisPos.toString()).innerHTML = "Signal Aspect Value Table";
//				if (ttMode !== JSONArray.AddrMode)
				{
					truthTable.setAttribute('ttmode', JSONArray.AddrMode);
					createSignalTruthTable(truthTable, thisPos);
				}
				break;
	}
}

/*
function findSigByInpNr(inpNr, createIfNone)
{
	if(typeof sensorData.SignalIndicators !== "undefined")
	{
		var sigHead = sensorData.SignalIndicators;
		var index = sigHead.findIndex(function(item, i){return item.SigNum === inpNr});
		console.log(sigHead, index);
		if (index >= 0)
			return sigHead[index];
		if (createIfNone) //here we do not have this BD, so we create it
		{
			var newSigObject = JSON.parse(templateSigObj);
			newSigObject.SigNum = inpNr;
			var newIndex = sigHead.push(newSigObject);
			return sigHead[newIndex-1];
		}
	}
	return null;
}

*/
function handleSignalEvent(JSONArray)
{
//	console.log(JSONArray);
	if (JSONArray.evtType == 12)
	{
		var sigBaseNum = 2 * Math.floor(JSONArray.evtID/2);
		var thisSignal = findSigByInpNr(sigBaseNum, true);
		console.log(thisSignal);
		if (thisSignal !== null)
		{
			makeSigRowVisible(thisSignal);
			var thisElement = document.getElementById("sig_status" + thisSignal.SigNum.toString());
//			switch (JSONArray.evtVal)
//			{
//			  case 0: 	thisElement.style.backgroundColor = getColorOfName(thisBlockDet.OffCol); 
//						break;
//			  case 1: 	thisElement.style.backgroundColor = getColorOfName(thisBlockDet.OnCol); 
//						break;
//			}
		}
	}
}

function cancelSigCfg()
{
	DisplaySigSetupUpdateData(signalData);
}

function saveSigCfg()
{
	signalData.SigHeads = sigHead;
	sendJSONDataToDiskFile("signals.cfg", JSON.stringify(signalData));
}

function cancelSigSetup()
{
	ws.send("{\"Cmd\": \"Request\",\"CmdParams\": [\"SetupData\"],\"JSONData\": {}}");
}

function addText(parent, newId, rowIndex, newText, newClass)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	
	if (newId !== "")
		docElement.setAttribute('id', newId + rowIndex.toString());	
	var textElement = document.createTextNode(newText);
	docElement.append(textElement);
	parent.append(docElement);
}

function addInputField(parent, newId, rowIndex, newClass, onChange)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	
	var inputField = document.createElement("input");
	inputField.setAttribute('class', newClass);	
	inputField.setAttribute("name", rowIndex.toString());
	if (onChange != 'undefined')
		inputField.setAttribute("onchange", onChange);
	inputField.setAttribute("id", newId + rowIndex.toString());
	docElement.append(inputField);
	parent.append(docElement);
}

function addButton(parent, newId, rowIndex, newClass, newText, clickfct)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	
	var inputField = document.createElement("button");
	inputField.setAttribute('class', newClass);	
	inputField.setAttribute("id", newId + rowIndex.toString());
	inputField.setAttribute("name", rowIndex.toString());
	inputField.setAttribute("onclick", clickfct);
	var textElement = document.createTextNode(newText);
	inputField.append(textElement);
	docElement.append(inputField);
	parent.append(docElement);
}

function addIcon(parent, iconimg, newId, newText, rowIndex, newClass, onClick)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	

	var cellInside = document.createElement("img");
	cellInside.setAttribute('src', iconimg);
	cellInside.setAttribute('alt', newText);
	cellInside.setAttribute('class', 'img_icon');
	cellInside.setAttribute('name', rowIndex.toString());
	cellInside.setAttribute("id", newId + rowIndex.toString());
	if (onClick != 'undefined')
		cellInside.setAttribute("onclick", onClick);
	docElement.append(cellInside);
	parent.append(docElement);
}

function addRadioButton(parent, group, newId, newText, rowIndex, newClass, onClick)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	
	var radioButton = document.createElement("input");
	radioButton.setAttribute("type", "radio");
	radioButton.setAttribute("id", newId + rowIndex.toString());
	radioButton.setAttribute("name", group + rowIndex.toString());
	if (onClick != 'undefined')
		radioButton.setAttribute("onclick", onClick);
	docElement.append(radioButton);
	var textElement = document.createTextNode(newText);
	docElement.append(textElement);
	parent.append(docElement);
}

function addDropDownSelector(parent, newId, rowindex, newClass, onChange)
{
	var docElement = document.createElement("div");
	docElement.setAttribute('class', newClass);	
	var cellInside = document.createElement("select");
	cellInside.setAttribute("id", newId + rowindex.toString());
	cellInside.setAttribute("name", rowindex.toString());
	cellInside.setAttribute("onchange", onChange);
	docElement.append(cellInside);
	parent.append(docElement);
}

function createTitleLine(parent, rowIndex)
{
	var titleTextField = document.createElement("div");
	var thisIndex = rowIndex + 1;
	addText(titleTextField, "rowtitle", rowIndex, "Signal Head #" + thisIndex.toString(), "inputtext_title");
	addText(titleTextField, "", rowIndex, "", "break10");
	addIcon(titleTextField, "cancel-button.png", "sig_cancel", "Cancel", rowIndex, "inputtext_left", "deleteThisSignal(this, event)")
	addText(titleTextField, "", rowIndex, "", "break50");
	addInputField(titleTextField, "numled", rowIndex, "numinputshort", "newNumLed(this, event)");
	addText(titleTextField, "", rowIndex, "", "break5");
	addText(titleTextField, "", rowIndex, "Lamps", "inputtext_left");
	addText(titleTextField, "", rowIndex, "", "break50");
	addText(titleTextField, "", rowIndex, "Addresses:", "inputtext_left");
	addText(titleTextField, "", rowIndex, "", "break2");
	addInputField(titleTextField, "inpaddr", rowIndex, "numinputstandard", "newInpAddr(this, event)");
	addText(titleTextField, "", rowIndex, "", "break10");
	addText(titleTextField, "", rowIndex, "using:", "inputtext_left");
	addRadioButton(titleTextField, "addrmode", "rbDynSwi", "Dyn. Swi", rowIndex, "inputtext_left", "newAddrMode(this, event)");
	addRadioButton(titleTextField, "addrmode", "rbStatSwi", "Static Swi", rowIndex, "inputtext_left", "newAddrMode(this, event)");
	addRadioButton(titleTextField, "addrmode", "rbSig", "Signal", rowIndex, "inputtext_left", "newAddrMode(this, event)");
	addText(titleTextField, "", rowIndex, "", "break20");
	parent.append(titleTextField);
}

function createModeLine(parent, rowIndex)
{
	var transModeField = document.createElement("div");
	transModeField.setAttribute('class', "inputtext_left");	
	transModeField.setAttribute("id", "transmode" + rowIndex.toString());
	addText(transModeField, "", rowIndex, "Transistion Mode:", "inputtext_left");
	addText(transModeField, "", rowIndex, "", "break10");
	addRadioButton(transModeField, "transmode", "rbHard", "Hard", rowIndex, "inputtext_left", "newTransMode(this, event)");
	addRadioButton(transModeField, "transmode", "rbSoft", "Soft", rowIndex, "inputtext_left", "newTransMode(this, event)");
	addText(transModeField, "", rowIndex, "", "break50");
	addText(transModeField, "", rowIndex, "Change Delay:", "inputtext_left");
	addText(transModeField, "", rowIndex, "", "break10");
	addInputField(transModeField, "chgdelay", rowIndex, "numinputshort", "newchgDelay(this, event)");
	addText(transModeField, "", rowIndex, "", "break5");
	addText(transModeField, "", rowIndex, "[ms]", "inputtext_left");
	
	parent.append(transModeField);
}

function createSigTableRow(classname, rowIndex)
{
	var row = document.createElement("div");
	return detailSigTableRow(row, classname, rowIndex);
}

function detailSigTableRow(row, classname, rowIndex)
{
	row.setAttribute('class', classname);	
	row.setAttribute("id", "tblSigRow" + rowIndex.toString());
	row.setAttribute("name", rowIndex.toString());
	
	var sortPanel = document.createElement("div");
	sortPanel.setAttribute('class', "sortpaneldiv");	
	sortPanel.setAttribute('id', "sp" + rowIndex.toString());	
	addButton(sortPanel, "btnUp", rowIndex, "slim-button-top", "Up", "moveUp(this,event)");
	addButton(sortPanel, "btnCopy", rowIndex, "slim-button-bottom", "Add Copy", "copyElement(this,event)");
	addButton(sortPanel, "btnDown", rowIndex, "slim-button-bottom", "Down", "moveDown(this,event)");
	row.append(sortPanel);
	var workPanel = document.createElement("div");
	workPanel.setAttribute('id', "wp" + rowIndex.toString());	
	workPanel.setAttribute('class', "workpaneldiv");	
	row.append(workPanel);
	
	var titlePanel = document.createElement("div");
	titlePanel.setAttribute('id', "tp" + rowIndex.toString());	
	titlePanel.setAttribute('class', "titlepaneldiv");
	createTitleLine(titlePanel, rowIndex);
	workPanel.append(titlePanel);
	var modePanel = document.createElement("div");
	modePanel.setAttribute('id', "mp" + rowIndex.toString());	
	modePanel.setAttribute('class', "titlepaneldiv");
	createModeLine(modePanel, rowIndex);
	workPanel.append(modePanel);
	var contentPanel = document.createElement("div");
	contentPanel.setAttribute('id', "cp" + rowIndex.toString());	
	contentPanel.setAttribute('class', "contentpaneldiv");
	workPanel.append(contentPanel);
	var truthTableDiv = document.createElement("div");
	truthTableDiv.setAttribute('class', "truthtablepaneldiv");
	addText(truthTableDiv, "truthtabletitle", rowIndex, "Aspect Truth Table", "linecontainer_1");
	var truthTableArea = document.createElement("div");
	truthTableArea.setAttribute('class', "truthtableholder");
	truthTableArea.setAttribute('id', "truthtable" + rowIndex.toString());
	truthTableArea.setAttribute('ttmode', "-1");
	truthTableDiv.append(truthTableArea);
	contentPanel.append(truthTableDiv);
	var signalImage = document.createElement("div");
	signalImage.setAttribute('class', "headimagepaneldiv");
	addText(signalImage, "", rowIndex, "Signal Head Image", "linecontainer_1");
	contentPanel.append(signalImage);
	var signalImageContainer = document.createElement("div");
	signalImageContainer.setAttribute('id', "headimagediv"  + rowIndex.toString());
	signalImageContainer.setAttribute('class', "headimageholder");
	signalImage.append(signalImageContainer);

	var basePane = document.createElement("canvas");
	basePane.setAttribute('class', 'headimageholder');
	basePane.setAttribute('name', rowIndex.toString());
	basePane.setAttribute('id', 'sigcanvas' + rowIndex.toString());
	basePane.setAttribute("onclick", "mouseClick(this, event)");
	basePane.setAttribute("onmousedown", "mouseDown(this, event)");
//	basePane.setAttribute("onmouseup", "mouseUp(this, event)"); //not needed, in this phase we use document.onmouseup listener
	signalImageContainer.append(basePane);

	var lampSetup = document.createElement("div");
	lampSetup.setAttribute('class', "lampsetuppaneldiv");
	addText(lampSetup, "lamptitle", rowIndex, "Lamp Setup", "linecontainer_1");
	contentPanel.append(lampSetup);

	var colorPanel = document.createElement("div");
	colorPanel.setAttribute('id', "colp" + rowIndex.toString());	
	colorPanel.setAttribute('class', "colorpaneldiv");
	addText(colorPanel, "", rowIndex, "", "break10");
	addText(colorPanel, "", rowIndex, "Color:", "inputtext_left");
	addText(colorPanel, "", rowIndex, "", "break10");
	addDropDownSelector(colorPanel, "lampcolor", rowIndex, "colorlist", "changeColor(this, event)");
//	addText(colorPanel, "", rowIndex, "", "break10");
//	addInputField(colorPanel, "colorlist", rowIndex, "numinputshort");
//	addText(colorPanel, "", rowIndex, "", "break10");
//	addButton(colorPanel, "btnSelCol", rowIndex, "slim-button", "Color", "");
	lampSetup.append(colorPanel);

	var blinkPanel = document.createElement("div");
	blinkPanel.setAttribute('id', "blinkp" + rowIndex.toString());	
	blinkPanel.setAttribute('sigNum', rowIndex.toString());	
	blinkPanel.setAttribute('currLamp', "-1");	
	blinkPanel.setAttribute('class', "blinkpaneldiv");
	addText(blinkPanel, "", rowIndex, "", "break10");
	addText(blinkPanel, "", rowIndex, "Blink Rate:", "inputtext_left");
	addText(blinkPanel, "", rowIndex, "", "break10");
	addInputField(blinkPanel, "blinkrate", rowIndex, "numinputshort", "updateBlinkRate(this, event)");
	addText(blinkPanel, "", rowIndex, "", "break5");
	addText(blinkPanel, "", rowIndex, "[ms]", "inputtext_left");
	lampSetup.append(blinkPanel);


	
	
	
	
	
	return row;
}



function createSignalTable()
{
	console.log("Create Sig Table");
	var body = document.getElementById("sigtablediv");
	sigTable = document.createElement("div");
	sigTable.setAttribute("class", "tablediv");
//	sigTable.append(createSigTableRow("tablepaneldiv", 0));
	body.appendChild(sigTable);
}


