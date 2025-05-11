let id
function start(){
	document.getElementById("a1").play()
	document.getElementById("a2").pause()
	console.log("点击了"+rd(0,1000))
	let span=document.getElementById("a4")
	span.innerHTML=rd(0,1000)
	span.style.color="rgb("+rd(0,255)+","+rd(0,255)+","+rd(0,255)+")"
	id=setInterval(function(){
		span.innerHTML=rd(0,1000)
		span.style.color = "rgb(" + rd(0, 255) + "," + rd(0, 255) + "," + rd(0, 255) + ")"
	},100)
	
}

function stop(){
	document.getElementById("a2").play()
	document.getElementById("a1").pause()
	clearInterval(id)
	
}

function rd(n,m){
	let c = m-n+1
	return Math.floor(Math.random()*c+n)
}


