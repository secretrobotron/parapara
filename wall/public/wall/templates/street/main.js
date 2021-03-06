/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var CHARACTER_WIDTH = 300;
var CHARACTER_HEIGHT = 300;
var CHARACTER_DURATION = 30; // sec

// Maximum number of "old" characters to show. That is, we always show newly
// added characters, even if 100 come it at one time.
// We apply this threshold and drop characters when:
// * Characters have been shown the minimum number of times, or
// * When getting the initial list of characters from the server (for characters
//   that have already been displayed in the past).
var NUM_CHARACTERS_THRESHOLD = 35;

// Number of times to show a character before shrinking/fading it
var NUM_ITERATIONS_AT_FULL_SIZE = 2;

// Minimum number of times to show a character shrunk or faded before it can be
// dropped
var MIN_NUM_ITERATIONS_AT_REDUCED_SIZE = 1;

var Main = {

  start: function() {
    Main.timebase = document.getElementById("time-base");
    Main.main_layer = document.getElementById("main-layer"); Database.start(Main.timebase, Main.showCharacter); },

  showCharacter: function(character, currentActiveTime, currentSimpleTime, currentRate, durationRate) {
    // Create a group to wrap the character and its animation
    var g = document.createElementNS("http://www.w3.org/2000/svg", "g");
    g.setAttribute("id", character.id);

    // Create the character image
    var image =
      document.createElementNS("http://www.w3.org/2000/svg", "image");
    var imageid = character.id+"i";
    image.setAttribute("id", imageid);
    image.setAttributeNS("http://www.w3.org/1999/xlink", "href", character.uri);
    image.setAttribute("width", CHARACTER_WIDTH);
    image.setAttribute("height", CHARACTER_HEIGHT);
    var yOfCharacter = 700 - CHARACTER_HEIGHT;
    g.setAttribute("transform", "translate(0, "+yOfCharacter+")");
    g.appendChild(image);

//    console.error("show:"+character.id+" "+currentSimpleTime);

    // Add a shadow to the character
    var use = document.createElementNS("http://www.w3.org/2000/svg", "use");
    use.setAttribute("transform", "matrix(1 0 0 -0.5 0 400)");
    use.setAttribute("filter", "url(#shadow)");
    use.setAttributeNS("http://www.w3.org/1999/xlink", "href", "#"+imageid);
    g.appendChild(use);

    // Create the animation
    var animateMotion =
      document.createElementNS("http://www.w3.org/2000/svg", "animateMotion");
    animateMotion.setAttribute("dur", Math.round(CHARACTER_DURATION*durationRate)+"s");
    animateMotion.setAttribute("begin", currentActiveTime+"s");
    animateMotion.setAttribute("calcMode", "linear");
    animateMotion.setAttribute("repeatCount", "1");
    animateMotion.setAttribute("fill", "freeze");
    animateMotion.setAttribute("attributeName", "transform");
    animateMotion.setAttribute("type", "transform");
    animateMotion.setAttribute("rotate", "auto");
    animateMotion.setAttribute("from", (-CHARACTER_WIDTH)+" 0");
    animateMotion.setAttribute("to", "1280 0");

    // When the animation finishes, remove the character
    animateMotion.addEventListener("endEvent", function(e) {
      var gElement = e.originalTarget.parentNode;
      gElement.parentNode.removeChild(gElement);
      //console.error("remove:"+character.id);
    }, true);

    // Add the animation to the group, then add the group to the scene
    g.appendChild(animateMotion);
    Main.main_layer.appendChild(g);
    // Update the character's status so we don't add it again
    character.repeatCount++;
  }

}
