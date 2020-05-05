function fadeOut(el) {
  el.style.zIndex = 1;
	el.style.opacity = 1;
  setTimeout(function () {
	  var fadeEffect = setInterval(function() {
	  	if (el.style.opacity > 0) {
	  		el.style.opacity -= 0.025;
	  	} else {
        el.style.zIndex = -1;
	  		clearInterval(fadeEffect);
	  	}
	  }, 50);
  }, 1000);
}
