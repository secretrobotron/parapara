/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var API_DIR = "../api/";
var CHARACTERS_DIR = "../characters/";
var CHARACTER_WIDTH = 300;
var CHARACTER_HEIGHT = 300;
var CHARACTER_DURATION = 20;//sec

var Main = {

	init: function() {
		Main.timebase = document.getElementById("time-base");
		Main.main_layer = document.getElementById("main-layer");
		Main.main_layer_path = document.getElementById("main-layer-path");
		Main.current_time = 0;
		Main.start();
   	},
	
	start: function() {
		Main.loadAllCharacters(function() {
			Main.next(0);
		});
	},

	next: function(index) {
		if (index == Main.characters.length) {
			index = 0;
		}
	
		var originalCurrentTime = Main.timebase.getCurrentTime();
		var simpleDuration = Main.timebase.getSimpleDuration();
		var currentTime = originalCurrentTime - (parseInt(originalCurrentTime/simpleDuration)*simpleDuration);
		var currentRate = currentTime/simpleDuration;		
		var ANIMATE_RANGE = 0.5;
		var ANIMATE_BUFFER = 0.2;
	
		var character = Main.characters[index];

		var g = document.createElementNS("http://www.w3.org/2000/svg", "g");
		g.setAttribute("id", character.id);

		var image = document.createElementNS("http://www.w3.org/2000/svg", "image");
		var imageid = character.id+"i";
		image.setAttribute("id", imageid);
		image.setAttributeNS("http://www.w3.org/1999/xlink", "href", CHARACTERS_DIR+character.id+".svg");
		image.setAttribute("width", CHARACTER_WIDTH);
		image.setAttribute("height", CHARACTER_HEIGHT);
		image.setAttribute("transform", "translate(-"+CHARACTER_WIDTH/2+" -"+(CHARACTER_HEIGHT-20)+")");
		g.appendChild(image);

		var use = document.createElementNS("http://www.w3.org/2000/svg", "use");
		use.setAttribute("transform", "matrix(1 0 0 -0.5 0 0)");
		use.setAttribute("filter", "url(#shadow)");
		use.setAttributeNS("http://www.w3.org/1999/xlink", "href", "#"+imageid);
		g.appendChild(use);

		var duration = 10+CHARACTER_DURATION*Math.random();

		//animate motion ---------------------			
		var animateMotion = document.createElementNS("http://www.w3.org/2000/svg", "animateMotion");
		animateMotion.setAttribute("dur", duration+"s");
		animateMotion.setAttribute("rotate", "auto");
		animateMotion.setAttribute("repeatCount", "1");
		animateMotion.setAttribute("begin", originalCurrentTime+"s");
		animateMotion.setAttribute("calcMode", "linear");
		var mpath = document.createElementNS("http://www.w3.org/2000/svg", "mpath");
		mpath.setAttributeNS("http://www.w3.org/1999/xlink", "href", "#main-layer-path");
		animateMotion.appendChild(mpath);
		animateMotion.addEventListener("endEvent", function(e) {
			var imageElement = e.originalTarget.parentNode;
			imageElement.parentNode.removeChild(imageElement);
		}, true);

		var start = (1-currentRate);
		var startdif = 0;
		//find start point
		var matrix = Main.main_layer_path.getScreenCTM();
		for (;;) {
			var point = Main.main_layer_path.getPointAtLength(start).matrixTransform(matrix);
			if (point.x > window.innerWidth) {
				break;
			}
			start += 0.01;
			startdif += 0.01;
			if (start > 1) {
				start -= 1;
			}
		}
		var end = start-ANIMATE_RANGE + startdif;
		if (end >= 0) {
			animateMotion.setAttribute("keyPoints", start+";"+end);
			animateMotion.setAttribute("keyTimes", "0;1");
		} else {
			var startRate = start*(1/ANIMATE_RANGE);
			animateMotion.setAttribute("keyPoints", start+";0;1;"+(end+1));
			animateMotion.setAttribute("keyTimes", "0;"+startRate+";"+startRate+";1");
		}
		g.appendChild(animateMotion);
		Main.main_layer.appendChild(g);

		var timedif = 4500+Math.random()*3000;
		setTimeout(Main.next, timedif, index+1);
	},

	createAnimateMotion: function(duration, keyPoints, id, originalCurrentTime) {
		var animateMotion = document.createElementNS("http://www.w3.org/2000/svg", "animateMotion");
		animateMotion.setAttribute("dur", duration+"s");
		animateMotion.setAttribute("rotate", "auto");
		animateMotion.setAttribute("repeatCount", "1");
		animateMotion.setAttribute("keyPoints", keyPoints);
		animateMotion.setAttribute("id",  id);
		animateMotion.setAttribute("begin", originalCurrentTime+"s");
		var mpath = document.createElementNS("http://www.w3.org/2000/svg", "mpath");
		mpath.setAttributeNS("http://www.w3.org/1999/xlink", "href", "#main-layer-path");
		animateMotion.appendChild(mpath);
		return animateMotion;
	},
	
	loadAllCharacters: function(callback) {
		var url = API_DIR+"get_all_characters_before_restart.php?"+(new Date()).getTime();
		$.getJSON(url, function(json) {
			Main.characters = [];
			Main.appendCharacters(json);
			callback();
		});
	},
	
	appendCharacters: function(json) {
		for (var i = 0, n = json.length; i < n; i++) {
			var characterOfJson = json[i];
			var character = new Character();
			character.setup(characterOfJson);
			Main.characters.push(character);
		}
	}
}

function Character() {
}

Character.prototype = {
	setup: function(json) {
		this.x = json.x;
		this.id = json.id;
	},

	show: function(parent) {
	},
	
	hide: function(parent) {
		this.ui.remove();
	}	
}

var StyleController = {
	transform: function(target, value) {
		StyleController.apply(target, "transform", value);
	},
	duration: function(target, value) {
		StyleController.apply(target, "transition-duration", value);
	},
	origin: function(target, value) {
		StyleController.apply(target, "transform-origin", value);
	},
	apply: function(target, property, value) {
		target.css("-moz-"+property, value);
		target.css("-webkit-"+property, value);
		target.css("-o-"+property, value);
		target.css("-ms-"+property, value);
	}
}		
